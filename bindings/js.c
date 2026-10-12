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
    handle_func_create(env, result, "get_message", js_get_message);
    handle_func_create(env, result, "send_message", js_send_message);
    handle_func_create(env, result, "get_pipe_fd", js_get_pipe_fd);
    handle_func_create(env, result, "client_hello", js_client_hello);
    
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

    message_t *msg = qpop(&node->inbox);
    if (msg == NULL) {
        dbg_print("no messages found");
        return NULL;
    }

    napi_value result; // javascript object that represents message_t
    napi_create_object(env, &result);
    
    napi_value msg_kind;
    napi_create_string_utf8(env, message_kind_str(msg->kind), NAPI_AUTO_LENGTH, &msg_kind);
    napi_set_named_property(env, result, "message_kind", msg_kind);

    napi_value dlen;
    napi_create_int64(env, msg->dlen, &dlen);
    napi_set_named_property(env, result, "dlen", dlen);

    napi_value arr;
    napi_create_array_with_length(env, msg->dlen, &arr);
    napi_set_named_property(env, result, "diffs", arr);

    for (int i = 0; i < msg->dlen; i++) {
        napi_value arr_obj; // javascript object that represents fdiff_t
        napi_create_object(env, &arr_obj);

        napi_value path;
        napi_create_string_utf8(env, (msg->diff + i)->path, NAPI_AUTO_LENGTH, &path);
        napi_set_named_property(env, arr_obj, "path", path);

        napi_value plen;
        napi_create_uint32(env, (msg->diff + i)->plen, &plen);
        napi_set_named_property(env, arr_obj, "plen", dlen);

        napi_value cursor;
        napi_create_uint32(env, (msg->diff + i)->cursor_r, &cursor);
        napi_set_named_property(env, arr_obj, "cursor", cursor);

        napi_value llen;
        napi_create_uint32(env, (msg->diff + i)->llen, &llen);
        napi_set_named_property(env, arr_obj, "llen", llen);

        napi_value arr_lines;
        napi_create_array_with_length(env, (msg->diff + i)->llen, &arr_lines);
        napi_set_named_property(env, arr_obj, "lines", arr_lines);

        for (int j = 0; j < (msg->diff + i)->llen; j++) {
            napi_value lines; // javascript object that represents fline_t
            napi_create_object(env, &lines);

            napi_value msg_kind;
            napi_create_string_utf8(env, message_kind_str(((msg->diff + i)->lines + j)->kind), NAPI_AUTO_LENGTH, &msg_kind);
            napi_set_named_property(env, lines, "message_kind", msg_kind);

            napi_value num;
            napi_create_uint32(env, ((msg->diff + i)->lines + j)->num, &num);
            napi_set_named_property(env, lines, "num", num);

            napi_value len;
            napi_create_uint32(env, ((msg->diff + i)->lines + j)->len, &len);
            napi_set_named_property(env, lines, "len", len);

            napi_value body;
            napi_create_string_utf8(env, ((msg->diff + i)->lines + j)->body, NAPI_AUTO_LENGTH, &body);
            napi_set_named_property(env, lines, "body", body);

            napi_set_element(env, arr_lines, j, lines);
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
    size_t sz = 2;
    napi_value args[2];
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

    char *prev_state, *current_state;
    prev_state = get_string_utf8(env, args[0]);
    current_state = get_string_utf8(env, args[1]);

    fdiff_t *diff = get_buffer_diff(prev_state, current_state);
    message_t msg = {.kind = MSG_CONTENT, .dlen = 1, .diff = diff};

    send_message(node->socket_d, &msg);
    free(prev_state);
    free(current_state);
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
    handle_func_create(env, result, "get_message", js_get_message);
    handle_func_create(env, result, "send_message", js_send_message);
    handle_func_create(env, result, "get_pipe_fd", js_get_pipe_fd);
    
    return result;
}

static napi_value Init(napi_env env, napi_value exports) {
    handle_func_create(env, exports, "server_start", js_server_start);
    handle_func_create(env, exports, "client_connect", js_client_connect);
    return exports;
}

NAPI_MODULE(NODE_GYP_MODULE_NAME, Init)