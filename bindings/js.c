/**
 * @file js.c
 * @brief Contains the javascript bindings of pco lib for C
 * @date 2026-10-12
 * 
 * @copyright Copyright (c) 2026
 */

#include <node_api.h>
#include "connection.h"
#include <stdlib.h>
#include "diff.h"
#include "log.h"
#include "queue.h"
#include "crdt_utils.h"

static napi_value js_client_hello(napi_env, napi_callback_info);
static napi_value js_get_pipe_fd(napi_env, napi_callback_info);
static napi_value js_client_connect(napi_env, napi_callback_info);
static napi_value js_get_message(napi_env, napi_callback_info); 
static napi_value js_send_message(napi_env, napi_callback_info);
static napi_value js_server_start(napi_env, napi_callback_info);

/**
 * @brief Utility method to attach a method to a javascript object
 * 
 * @param env 
 * @param js_object javascript object on which the method is to be attached
 * @param func_id name of the function literal 
 * @param func_handle function to be attached
 * @return 0 on success or -1 otherwise
 */
int handle_func_create(napi_env env, napi_value js_object, char* func_id, napi_callback func_handle) {
    napi_value fn;

    napi_status status = napi_create_function(
        env,
        func_id,
        NAPI_AUTO_LENGTH,
        func_handle,
        NULL,
        &fn
    );

    if (status != napi_ok) {
        char buffer[50];
        fprintf("failed to create %s function", func_id);
        napi_throw_error(env, NULL, buffer);
        return -1;
    }

    status = napi_set_named_property(
        env,
        js_object,
        func_id,
        fn
    );

    if (status != napi_ok) {
        char buffer[50];
        snprintf(buffer, "failed to export %s function", func_id);
        napi_throw_error(env, NULL, buffer);
        return -1;
    }
    return 0;
}

/**
 * @brief Utility method to copy a utf-8 javascript string into a buffer
 * 
 * @param env 
 * @param value a javascript string 
 * @return a buffer holding the utf-8 characters
 */
static char *get_string_utf8(napi_env env, napi_value value) {
    size_t length = 0;

    // first call to determine the UTF-8 byte length.
    napi_status status = napi_get_value_string_utf8(
        env, value, NULL, 0, &length
    );

    if (status != napi_ok) {
        return NULL;
    }

    char *buffer = malloc(length + 1);
    if (buffer == NULL) {
        perror("couldn't allocate sufficient memory in malloc()");
        return NULL;
    }

    // second call to copy the contents into the allocated buffer.
    size_t copied = 0;
    status = napi_get_value_string_utf8(env, value, buffer, length + 1, &copied);

    if (status != napi_ok) {
        free(buffer);
        return NULL;
    }

    return buffer;
}

/**
 * @brief Extracts named property from a Javascript object.
 * Returns NULL when it does not exist
 * 
 * @param env 
 * @param obj javascript object
 * @param name name of the property to be extracted
 */
static napi_value get_prop(
    napi_env env,
    napi_value obj,
    const char *name
) {
    napi_value value;
    if (napi_get_named_property(env, obj, name, &value) != napi_ok) {
        return NULL;
    }
    return value;
}

/**
 * @brief Extracts a named int32_t type property from a Javascript object.
 * 
 * @param env 
 * @param javascript object
 * @param name name of the property to be extracted
 */
static int get_u32_from_prop(
    napi_env env,
    napi_value obj,
    const char *name
) {
    napi_value value = get_prop(env, obj, name);
    if (!value) return 0;

    double number;
    if (napi_get_value_double(env, value, &number) != napi_ok ||
        number < 0 || number > UINT32_MAX ||
        number != (double)(uint32_t)number) {

        char buffer[50];
        snprintf(buffer, sizeof(buffer), "failed to extract named property %s", name);
        napi_throw_error(env, NULL, buffer);
        return NULL;
    }

    return (uint32_t)number;
}

/**
 * @brief Extracts a named string type property from a javascript object.
 * 
 * @param env 
 * @param javascript object
 * @param name name of the property to be extracted
 */
static char *get_string_from_prop(
    napi_env env,
    napi_value obj,
    const char *name
) {
    napi_value value = get_prop(env, obj, name);
    if (!value) {
        char buffer[50];
        snprintf(buffer, sizeof(buffer), "failed to extract named property %s", name);
        napi_throw_error(env, NULL, buffer);
        return NULL;
    }

    return get_string_utf8(env, value);
}

static void get_array_from_prop(
    napi_env env,
    napi_value obj,
    const char *name,
    napi_value** out
) {
    napi_value arr = get_prop(env, obj, name);
    if (!arr) {
        char buffer[50];
        snprintf(buffer, sizeof(buffer), "[%s]: failed to extract named property", name);
        napi_throw_error(env, NULL, buffer);
        return;
    }

    bool is_array = false;
    if (napi_is_array(env, arr, &is_array) != napi_ok || !is_array) {
        char buffer[100];
        snprintf(buffer, sizeof(buffer), "[%s]: not an array or there was a failure to check if it's an array", name);
        napi_throw_error(env, NULL, buffer);
        return;
    }

    uint32_t count;
    if (napi_get_array_length(env, arr, &count) != napi_ok) {
        char buffer[100];
        snprintf(buffer, sizeof(buffer), "[%s]: error getting length of array", name);
        napi_throw_error(env, NULL, buffer);
        return;
    }

    for (uint32_t i = 0; i < count; i++) {
        napi_value line_obj;
        if (napi_get_element(env, arr, i, &line_obj) != napi_ok) {
            char buffer[100];
            snprintf(buffer, sizeof(buffer), "[%s]: error extracting %d element of array", name, i);
            napi_throw_error(env, NULL, buffer);
            return;
        }
        arrput(out, line_obj);
    }
}

static napi_value js_client_connect(napi_env env, napi_callback_info info) {
    size_t sz = 2;
    napi_value args[2];
    napi_status status = napi_get_cb_info(env, info, &sz, args, NULL, NULL);
    if (status != napi_ok) {
        napi_throw_error(env, NULL, "failed to retrieve callback info");
        return NULL;
    }

    node_t *node = (node_t*)malloc(sizeof(node_t));

    char *host;
    int32_t port;
    host = get_string_utf8(env, args[0]);
    status = napi_get_value_int32(env, args[1], &port);
    if (status != napi_ok) {
        napi_throw_error(env, NULL, "failed to extract port from callback info");
        return NULL;
    }

    client_listen(node, host, port);
    
    free(host);

    // store reference to node ptr inside js
    napi_value result;
    status = napi_create_object(env, &result);
    if (status != napi_ok) {
        perror("error creating node data structure javascript holder");
        free(node);
        return NULL;
    }

    status = napi_wrap(env, result, node, NULL, NULL, NULL);
    if (status != napi_ok) {
        perror("error wrapping native object with javascript");
        free(node);
        return NULL;
    }

    // attach the common methods 
    handle_func_create(env, result, "getMessage", js_get_message);
    handle_func_create(env, result, "sendMessage", js_send_message);
    handle_func_create(env, result, "getPipeFd", js_get_pipe_fd);
    handle_func_create(env, result, "clientHello", js_client_hello);
    
    return result;
}

static napi_value js_client_hello(napi_env env, napi_callback_info info) {
    napi_value this_ptr; // ref to the method invoker
    napi_status status = napi_get_cb_info(env, info, NULL, NULL, &this_ptr, NULL);
    if (status != napi_ok) {
        napi_throw_error(env, NULL, "failed to retrieve callback info");
        return NULL;
    }

    node_t *node = NULL;
    status = napi_unwrap(env, this_ptr, &node);
    if (status != napi_ok) {
        napi_throw_error(env, NULL, "failed to unwarp native object");
        return NULL;
    }

    client_hello(node);
    return NULL;
}

static napi_value js_get_message(napi_env env, napi_callback_info info) {
    napi_value this_ptr; // ref to the method invoker
    napi_status status = napi_get_cb_info(env, info, NULL, NULL, &this_ptr, NULL);
    if (status != napi_ok) {
        napi_throw_error(env, NULL, "failed to retrieve callback info");
        return NULL;
    }

    node_t *node = NULL;
    status = napi_unwrap(env, this_ptr, &node);
    if (status != napi_ok) {
        napi_throw_error(env, NULL, "failed to unwarp native object");
        return NULL;
    }

    // TODO: free mem later
    message_t *msg = qpop(&node->inbox);
    if (msg == NULL) {
        dbg_print("no messages found");
        return NULL;
    }

    napi_value result; // javascript object that represents message_t
    napi_create_object(env, &result);
    
    napi_value msg_kind;
    napi_create_string_utf8(env, message_kind_str(msg->kind), NAPI_AUTO_LENGTH, &msg_kind);
    napi_set_named_property(env, result, "kind", msg_kind);

    napi_value am_doc_obj;
    napi_create_object(env, &am_doc_obj);

    AMresult *root_result = AMmapRange(node->doc, AM_ROOT, AMstr(NULL), AMstr(NULL), NULL);

    if (!root_result || AMresultStatus(root_result) != AM_STATUS_OK) {
        napi_throw_error(env, NULL, "failed to retrieve root objects");
        return NULL;
    }

    AMitems items = AMresultItems(root_result);
    AMitem *item;

    while ((item = AMitemsNext(&items, 1)) != NULL) {
        AMbyteSpan key_span;

        if (!AMitemKey(item, &key_span)) {
            continue;
        }

        const AMobjId *file_id = AMitemObjId(item);

        if (!file_id) {
            continue;
        }

        if (AMobjObjType(node->doc, file_id) != AM_OBJ_TYPE_LIST) {
            continue;
        }

        // Copy filename
        char *filename = (char *)malloc(key_span.count + 1);
        if (!filename) {
            continue;
        }

        memcpy(filename, key_span.src, key_span.count);
        filename[key_span.count] = '\0';

        // Create JS property name
        napi_value napi_filename;
        napi_create_string_utf8(
            env,
            filename,
            key_span.count,
            &napi_filename
        );

        // Get file contents
        char **list;
        get_list(node->doc, filename, &list);

        if (!list) {
            free(filename);
            continue;
        }

        // Create JS array
        napi_value arr;
        napi_create_array_with_length(env, arrlen(list), &arr);

        for (uint32_t i = 0; i < arrlen(list); i++) {
            napi_value line;

            napi_create_string_utf8(
                env,
                list[i],
                NAPI_AUTO_LENGTH,
                &line
            );

            napi_set_element(env, arr, i, line);
        }

        // Add filename as property to the document object
        napi_set_property(env, am_doc_obj, napi_filename, arr);

        free(filename);
    }

    napi_set_named_property(env, result, "crdtDocument", am_doc_obj);

    AMresultFree(root_result);

    return result;
}

static napi_value js_get_pipe_fd(napi_env env, napi_callback_info info) {
    napi_value this_ptr; // ref to the method invoker
    node_t *node = NULL;
    napi_status status = napi_get_cb_info(env, info, NULL, NULL, &this_ptr, NULL);
    if (status != napi_ok) {
        napi_throw_error(env, NULL, "failed to retreive callback info");
        return NULL;
    }

    status = napi_unwrap(env, this_ptr, &node);
    if (status != napi_ok) {
        napi_throw_error(env, NULL, "failed to unwarp native object");
        return NULL;
    }

    napi_value result;
    napi_create_int32(env, node->event[0], &result);
    return result;
}

static napi_value js_send_message(napi_env env, napi_callback_info info) {
    napi_value this_ptr; // ref to the method invoker
    size_t sz = 1;
    napi_value args[1];
    napi_status status = napi_get_cb_info(env, info, &sz, args, &this_ptr, NULL);
    if (status != napi_ok) {
        napi_throw_error(env, NULL, "failed to retrieve callback info");
        return NULL;
    }
    
    node_t *node = NULL;
    status = napi_unwrap(env, this_ptr, &node);
    if (status != napi_ok) {
        napi_throw_error(env, NULL, "failed to unwarp native object");
        return NULL;
    }

    char *filename = get_string_from_prop(env, args[0], "fileName");
    AMresult *result = AMmapGet(node->doc, AM_ROOT, AMstr(filename), NULL);
    
    if (!result || AMresultStatus(result) != AM_STATUS_OK) {
        napi_throw_error(env, NULL, "error looking up key in the crdt document");
        AMresultFree(result);
        return NULL;
    }

    AMobjId *obj_id = NULL; // pointer to the list in the document
    AMresult *list_obj_result;
    AMitem *item = AMresultItem(result);

    if (!item || AMitemIdxType(item) == AM_VAL_TYPE_VOID) {
        obj_id = add_list_to_document(node->doc, filename, &list_obj_result);
    }
    else {
        obj_id = AMitemObjId(AMresultItem(result));
    }

    napi_value *operations = NULL;
    get_array_from_prop(env, args[0], "operations", &operations);

    // apply the operations to local crdt document
    for (int i = 0; arrlen(operations); i++) {
        char *diff_kind = get_string_from_prop(env, operations[i], "kind");
        uint32_t line_number = get_u32_from_prop(env, operations[i], "line");
        if (strcmp(diff_kind, "DIFF_MODIFIED") == 0) {
            char *content = get_string_from_prop(env, operations[i], "content");
            update_list_element(node->doc, obj_id, content, line_number);
            free(content);
        }
        else if (strcmp(diff_kind, "DIFF_ADDED") == 0) {
            char *content = get_string_from_prop(env, operations[i], "content");
            put_element_into_list(node->doc, obj_id, content, line_number);
            free(content);
        }
        else {
            delete_element_from_list(node->doc, obj_id, line_number);
        }

        free(diff_kind);
    }

    message_t msg = {.kind = MSG_CONTENT, .doc = node->doc};
    send_message(node->connected_sockets[0], &msg);
    
    arrfree(operations);
    free(filename);
    AMresultFree(result);
    AMresultFree(list_obj_result);
    return NULL;
}

static napi_value js_server_start(napi_env env, napi_callback_info info) {
    size_t sz = 2;
    napi_value args[2];
    napi_status status = napi_get_cb_info(env, info, &sz, args, NULL, NULL);
    if (status != napi_ok) {
        napi_throw_error(env, NULL, "failed to get callback info");
        return NULL;
    }

    char *path;
    int32_t port;

    path = get_string_utf8(env, args[0]);
    status = napi_get_value_int32(env, args[1], &port);
    if (status != napi_ok) {
        napi_throw_error(env, NULL, "failed to extract port from callback info");
        return NULL;
    }

    node_t *node = (node_t*)malloc(sizeof(node_t));
    int res = server_start(node, port, path);

    if (res == -1) {
        perror("error starting server");
        free(node);
        return NULL;
    }

    // store reference to node ptr inside js
    napi_value result;
    status = napi_create_object(env, &result);

    if (status != napi_ok) {
        perror("error creating node data structure js holder");
        free(node);
        return NULL;
    }

    status = napi_wrap(env, result, node, NULL, NULL, NULL);

    if (status != napi_ok) {
        free(node);
        return NULL;
    }

    // attach the common methods 
    handle_func_create(env, result, "getMessage", js_get_message);
    handle_func_create(env, result, "sendMessage", js_send_message);
    handle_func_create(env, result, "getPipeFd", js_get_pipe_fd);
    
    return result;
}

static napi_value Init(napi_env env, napi_value exports) {
    handle_func_create(env, exports, "serverStart", js_server_start);
    handle_func_create(env, exports, "clientConnect", js_client_connect);
    return exports;
}

NAPI_MODULE(NODE_GYP_MODULE_NAME, Init)