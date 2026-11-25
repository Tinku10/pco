#ifndef PCO_TEST_MESSAGE_H
#define PCO_TEST_MESSAGE_H
#include <greatest/greatest.h>
#include <connection.h>

TEST serde_client_hello_message(void) {
    // client
    message_t msg = { .kind = CLIENT_HELLO, .doc = NULL};
    char *buffer = package_message(&msg);

    // server
    node_t node = {.kind = NODE_ADMIN, .doc = NULL};
    message_t *output_msg = unpackage_message(buffer + 4, &node);
    ASSERT_ENUM_EQ(CLIENT_HELLO, output_msg->kind, "CLIENT_HELLO");

    // cleanup
    free(buffer);
    free(output_msg);

    PASS();
}

TEST serde_server_hello_message(void) {
    // server
    AMresult *result = AMcreate(NULL);
    AMdoc *doc = get_document(result);
    AMresult *server_obj_result = NULL;
    const AMobjId *obj_id = add_list_to_document(doc, "test.txt", &server_obj_result);
    put_element_into_list(doc, obj_id, "item #1", 0);
    put_element_into_list(doc, obj_id, "item #2", 1);
    put_element_into_list(doc, obj_id, "item #3", 2);

    message_t msg1 = {
      .kind = SERVER_HELLO,
      .doc = doc
    };

    char *buffer1 = package_message(&msg1);

    put_element_into_list(doc, obj_id, "item #4", 3);
    put_element_into_list(doc, obj_id, "item #5", 4);
    put_element_into_list(doc, obj_id, "item #6", 5);

    message_t msg2 = {
      .kind = MSG_CONTENT,
      .doc = doc
    };

    char *buffer2 = package_message(&msg2);

    message_t msg3 = {
      .kind = SERVER_HELLO,
      .doc = doc
    };

    char *buffer3 = package_message(&msg3);

    // client 
    AMresult *client_result = AMcreate(NULL);
    AMdoc *client_doc = get_document(result);

    node_t client_node = {.kind = NODE_CONTRIBUTOR, .doc = client_doc};
    message_t *output_msg = unpackage_message(buffer3 + 4, &client_node);
    // after server hello, client's document should sync back with server's
    ASSERT_ENUM_EQ(SERVER_HELLO, output_msg->kind, "SERVER_HELLO");
    char **list = NULL;
    get_list(client_doc, "test.txt", &list);
    ASSERT(list != NULL);
    ASSERT_EQ(6, arrlen(list));


    // cleanup
    AMresultFree(result);
    AMresultFree(server_obj_result);
    AMresultFree(client_result);
    free(buffer1);
    free(buffer2);
    free(buffer3);
    for (int i = 0; i < arrlen(list); i++) free(list[i]);
    arrfree(list);
    
    PASS();
}

TEST serde_ack_message(void) {

    PASS();
}

TEST serde_content_message(void) {

    PASS();
}

SUITE(suite_message_serde) {
    RUN_TEST(serde_client_hello_message);
    RUN_TEST(serde_server_hello_message);
}

#endif
