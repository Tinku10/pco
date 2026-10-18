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
        sprintf(buffer, "Failed to create %s function", func_id);
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
        sprintf(buffer, "Failed to export %s function", func_id);
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
        perror("Couldn't allocate sufficient memory in malloc()");
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
        sprintf(buffer, sizeof(buffer), "failed to extract named property %s", name);
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
        sprintf(buffer, sizeof(buffer), "failed to extract named property %s", name);
        napi_throw_error(env, NULL, buffer);
        return NULL;
    }

    return get_string_utf8(env, value);
}

static void get_array_from_prop(
    napi_env env,
    napi_value obj,
    const char *name,
    napi_value* out
) {
    napi_value arr = get_prop(env, obj, name);
    if (!arr) {
        char buffer[50];
        sprintf(buffer, sizeof(buffer), "[%s]: failed to extract named property", name);
        napi_throw_error(env, NULL, buffer);
        return;
    }

    bool is_array = false;
    if (napi_is_array(env, arr, &is_array) != napi_ok || !is_array) {
        char buffer[100];
        sprintf(buffer, sizeof(buffer), "[%s]: not an array or there was a failure to check if it's an array", name);
        napi_throw_error(env, NULL, buffer);
        return;
    }

    uint32_t count;
    if (napi_get_array_length(env, arr, &count) != napi_ok) {
        char buffer[100];
        sprintf(buffer, sizeof(buffer), "[%s]: error getting length of array", name);
        napi_throw_error(env, NULL, buffer);
        return;
    }

    for (uint32_t i = 0; i < count; i++) {
        napi_value line_obj;
        if (napi_get_element(env, arr, i, &line_obj) != napi_ok) {
            char buffer[100];
            sprintf(buffer, sizeof(buffer), "[%s]: error extracting %d element of array", name, i);
            napi_throw_error(env, NULL, buffer);
            return;
        }
        out[i] = line_obj;
    }
}

static napi_value js_client_connect(napi_env env, napi_callback_info info) {
    size_t sz = 2;
    napi_value args[2];
    napi_status status = napi_get_cb_info(env, info, &sz, args, NULL, NULL);
    if (status != napi_ok) {
        napi_throw_error(env, NULL, "Failed to retrieve callback info");
        return NULL;
    }

    node_t *node = (node_t*)malloc(sizeof(node_t));

    char *host;
    int32_t port;
    host = get_string_utf8(env, args[0]);
    status = napi_get_value_int32(env, args[1], &port);
    if (status != napi_ok) {
        napi_throw_error(env, NULL, "Failed to extract port from callback info");
        return NULL;
    }

    client_listen(node, host, port);
    
    free(host);

    // store reference to node ptr inside js
    napi_value result;
    status = napi_create_object(env, &result);
    if (status != napi_ok) {
        perror("Error creating node data structure javascript holder");
        free(node);
        return NULL;
    }

    status = napi_wrap(env, result, node, NULL, NULL, NULL);
    if (status != napi_ok) {
        perror("Error wrapping native object with javascript");
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
        napi_throw_error(env, NULL, "Failed to retrieve callback info");
        return NULL;
    }

    node_t *node = NULL;
    status = napi_unwrap(env, this_ptr, &node);
    if (status != napi_ok) {
        napi_throw_error(env, NULL, "Failed to unwarp native object");
        return NULL;
    }

    client_hello(node);
    return NULL;
}

static napi_value js_get_message(napi_env env, napi_callback_info info) {
    napi_value this_ptr; // ref to the method invoker
    napi_status status = napi_get_cb_info(env, info, NULL, NULL, &this_ptr, NULL);
    if (status != napi_ok) {
        napi_throw_error(env, NULL, "Failed to retrieve callback info");
        return NULL;
    }

    node_t *node = NULL;
    status = napi_unwrap(env, this_ptr, &node);
    if (status != napi_ok) {
        napi_throw_error(env, NULL, "Failed to unwarp native object");
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

    napi_value arr;
    napi_create_array_with_length(env, msg->dlen, &arr);
    napi_set_named_property(env, result, "diff", arr);

    for (int i = 0; i < msg->dlen; i++) {
        napi_value arr_obj; // javascript object that represents fdiff_t
        napi_create_object(env, &arr_obj);

        napi_value kind;
        napi_create_string_utf8(env, diff_kind_str((msg->diff + i)->kind), NAPI_AUTO_LENGTH, &kind);
        napi_set_named_property(env, arr_obj, "kind", kind);

        napi_value path;
        napi_create_string_utf8(env, (msg->diff + i)->path, NAPI_AUTO_LENGTH, &path);
        napi_set_named_property(env, arr_obj, "path", path);

        napi_value cursor;
        napi_create_uint32(env, (msg->diff + i)->cursor_r, &cursor);
        napi_set_named_property(env, arr_obj, "cursor", cursor);

        napi_value arr_lines;
        napi_create_array_with_length(env, (msg->diff + i)->llen, &arr_lines);
        napi_set_named_property(env, arr_obj, "lines", arr_lines);

        // range_t
        {
            napi_value range_obj; // javascript object that represents range_t
            napi_create_object(env, &range_obj);
            
            napi_value start_pos;
            napi_create_uint32(env, (msg->diff + i)->range.start_pos, &start_pos);
            napi_set_named_property(env, range_obj, "start_pos", start_pos);

            napi_value start_char;
            napi_create_uint32(env, (msg->diff + i)->range.start_char, &start_char);
            napi_set_named_property(env, range_obj, "start_char", start_char);

            napi_value end_pos;
            napi_create_uint32(env, (msg->diff + i)->range.end_pos, &end_pos);
            napi_set_named_property(env, range_obj, "end_pos", end_pos);

            napi_value end_char;
            napi_create_uint32(env, (msg->diff + i)->range.end_char, &end_char);
            napi_set_named_property(env, range_obj, "end_char", end_char);
            napi_set_named_property(env, arr_obj, "range", range_obj);
        }

        for (int j = 0; j < (msg->diff + i)->llen; j++) {
            napi_value line; // javascript object that represents fline_t
            napi_create_object(env, &line);

            napi_value num;
            napi_create_uint32(env, ((msg->diff + i)->lines + j)->num, &num);
            napi_set_named_property(env, line, "num", num);

            napi_value len;
            napi_create_uint32(env, ((msg->diff + i)->lines + j)->len, &len);
            napi_set_named_property(env, line, "len", len);

            napi_value body;
            napi_create_string_utf8(env, ((msg->diff + i)->lines + j)->body, NAPI_AUTO_LENGTH, &body);
            napi_set_named_property(env, line, "body", body);

            napi_set_element(env, arr_lines, j, line);
        }
        
        napi_set_element(env, arr, i, arr_obj);
    }

    return result;
}

static napi_value js_get_pipe_fd(napi_env env, napi_callback_info info) {
    napi_value this_ptr; // ref to the method invoker
    node_t *node = NULL;
    napi_status status = napi_get_cb_info(env, info, NULL, NULL, &this_ptr, NULL);
    if (status != napi_ok) {
        napi_throw_error(env, NULL, "Failed to retreive callback info");
        return NULL;
    }

    status = napi_unwrap(env, this_ptr, &node);
    if (status != napi_ok) {
        napi_throw_error(env, NULL, "Failed to unwarp native object");
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
        napi_throw_error(env, NULL, "Failed to retrieve callback info");
        return NULL;
    }
    
    node_t *node = NULL;
    status = napi_unwrap(env, this_ptr, &node);
    if (status != napi_ok) {
        napi_throw_error(env, NULL, "Failed to unwarp native object");
        return NULL;
    }

    message_t msg = {0};
    char *msg_kind_str = get_string_from_prop(env, args[0], "kind");
    msg.kind = message_kind_str_to_enum(msg_kind_str);
    free(msg_kind_str);
    msg.dlen = get_u32_from_prop(env, args[0], "dlen");
    
    // diff array
    msg.diff = (fdiff_t *)malloc(sizeof(fdiff_t) * msg.dlen);
    napi_value *diff_js_arr = (napi_value *)malloc(sizeof(napi_value) * msg.dlen);
    get_array_from_prop(env, args[0], "diff", diff_js_arr); // each element of this is a line_t

    for (uint32_t i = 0; i < msg.dlen; i++) {
        fdiff_t diff = {0};
        char *diff_kind_str = get_string_from_prop(env, diff_js_arr[i], "kind");
        diff.kind = diff_kind_str_to_enum(diff_kind_str);
        free(diff_kind_str);

        diff.cursor_r = get_u32_from_prop(env, diff_js_arr[i], "cursor_position");
        diff.path = get_string_from_prop(env, diff_js_arr[i], "path");
        diff.plen = get_u32_from_prop(env, diff_js_arr[i], "plen");
        diff.llen = get_u32_from_prop(env, diff_js_arr[i], "llen");
        
        // lines
        diff.lines = (fline_t *)malloc(sizeof(fline_t) * diff.llen);
        napi_value *lines = (napi_value *)malloc(sizeof(napi_value) * diff.llen);
        get_array_from_prop(env, diff_js_arr[i], "lines", lines);
        for (uint32_t j = 0; j < diff.llen; j++) {
            fline_t line = {0};
            line.len = get_u32_from_prop(env, lines[j], "len");
            line.body = get_string_from_prop(env, lines[j], "body");
            diff.lines[0] = line;
        }
        free(lines);

        // range
        napi_value range = get_prop(env, diff_js_arr[i], "range");
        range_t range_value;
        range_value.start_pos = get_u32_from_prop(env, range, "start_pos");
        range_value.start_char = get_u32_from_prop(env, range, "start_char");
        range_value.end_pos = get_u32_from_prop(env, range, "end_pos");
        range_value.end_char = get_u32_from_prop(env, range, "end_char");
        diff.range = range_value;

        msg.diff[i] = diff;
    }
    free(diff_js_arr);

    send_message(node->socket_d, &msg);
    
    // free allocated mem
    for (uint32_t i = 0; i < msg.dlen; i++) {        
        free(msg.diff[i].path);
        for (uint32_t j = 0; j < msg.diff[i].llen; j++) {
            free(msg.diff[i].lines->body);
        }
    }
    free(msg.diff);
    
    return NULL;
}

static napi_value js_server_start(napi_env env, napi_callback_info info) {
    size_t sz = 2;
    napi_value args[2];
    napi_status status = napi_get_cb_info(env, info, &sz, args, NULL, NULL);
    if (status != napi_ok) {
        napi_throw_error(env, NULL, "Failed to get callback info");
        return NULL;
    }

    char *path;
    int32_t port;

    path = get_string_utf8(env, args[0]);
    status = napi_get_value_int32(env, args[1], &port);
    if (status != napi_ok) {
        napi_throw_error(env, NULL, "Failed to extract port from callback info");
        return NULL;
    }

    node_t *node = (node_t*)malloc(sizeof(node_t));
    int res = server_start(node, port, path);

    if (res == -1) {
        perror("Error starting server");
        free(node);
        return NULL;
    }

    // store reference to node ptr inside js
    napi_value result;
    status = napi_create_object(env, &result);

    if (status != napi_ok) {
        perror("Error creating node data structure js holder");
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