#ifndef PCO_TEST_AUTOMERGE_H
#define PCO_TEST_AUTOMERGE_H
#include <greatest/greatest.h>
#include <stdlib.h>
#include <automerge-c/automerge.h>
#include <automerge-c/utils/string.h>
#include <stb/stb_ds.h>

TEST automerge_get_document(void) {
    AMresult *result = AMcreate(NULL);
    AMdoc *doc = get_document(result);
    ASSERT(doc != NULL);
    AMresultFree(result);
    PASS();
}

TEST automerge_get_and_put_list(void) {
    AMresult *result = AMcreate(NULL);
    AMdoc *doc = get_document(result);
    AMresult *new_result = NULL;
    const AMobjId *obj_id = add_list_to_document(doc, "key1", &new_result);
    ASSERT_EQ(0, put_element_into_list(doc, obj_id, "item #1", 0));
    ASSERT_EQ(0, put_element_into_list(doc, obj_id, "item #2", 1));
    ASSERT_EQ(0, put_element_into_list(doc, obj_id, "item #3", 2));

    char **str_list;
    get_list(doc, "key1", &str_list);
    ASSERT(str_list != NULL);
    ASSERT_EQ(3, arrlen(str_list));
    ASSERT_STR_EQ("item #1", str_list[0]);
    ASSERT_STR_EQ("item #2", str_list[1]);
    ASSERT_STR_EQ("item #3", str_list[2]);

    AMresultFree(new_result);
    AMresultFree(result);
    PASS();
}

TEST automerge_whole_document_serde(void) {
    // document on the sender's side
    AMresult *alice_result = AMcreate(NULL);
    AMdoc *alice_doc = get_document(alice_result);
    AMresult *doc_result = NULL;
    const AMobjId *obj_id = add_list_to_document(alice_doc, "test.txt", &doc_result);
    
    put_element_into_list(alice_doc, obj_id, "this is the first line", 0);
    put_element_into_list(alice_doc, obj_id, "second line with newline\n", 1);
    put_element_into_list(alice_doc, obj_id, "third line with tab \t", 2);
    AMresultFree(doc_result);

    char *buffer;
    int bytes_serialised = serialise_document(alice_doc, &buffer);
    ASSERT_GT(bytes_serialised, 0);
    ASSERT_FALSE(buffer == NULL);

    // document on the receiver's side
    AMresult *bob_result = AMcreate(NULL);
    AMdoc *bob_doc = get_document(bob_result);
    ASSERT_EQ(0, deserialise_document(bob_doc, buffer, bytes_serialised));

    AMresult *map_result = AMmapGet(bob_doc, AM_ROOT, AMstr("test.txt"), NULL);
    ASSERT_FALSE(map_result == NULL);
    const AMobjId *key_id = AMitemObjId(AMresultItem(map_result));
    
    AMresult *first_element = AMlistGet(bob_doc, key_id, 0, NULL);
    AMbyteSpan first_byte_span;
    AMitemToStr(AMresultItem(first_element), &first_byte_span);
    ASSERT_EQ(0, AMstrCmp(AMstr("this is the first line"), first_byte_span));

    AMresult *second_element = AMlistGet(bob_doc, key_id, 1, NULL);
    AMbyteSpan second_byte_span;
    AMitemToStr(AMresultItem(second_element), &second_byte_span);
    ASSERT_EQ(0, AMstrCmp(AMstr("second line with newline\n"), second_byte_span));

    AMresult *third_element = AMlistGet(bob_doc, key_id, 2, NULL);
    AMbyteSpan third_byte_span;
    AMitemToStr(AMresultItem(third_element), &third_byte_span);
    ASSERT_EQ(0, AMstrCmp(AMstr("third line with tab \t"), third_byte_span));

    AMresultFree(first_element);
    AMresultFree(second_element);
    AMresultFree(third_element);
    AMresultFree(map_result);
    AMresultFree(alice_result);
    AMresultFree(bob_result);
    free(buffer);
    PASS();
}

TEST automerge_incremental_document_serde(void) {
    AMresult *result = AMcreate(NULL);
    AMdoc *doc = get_document(result);
    AMresult *doc_result = NULL;
    const AMobjId *obj_id = add_list_to_document(doc, "test.txt", &doc_result);
    
    put_element_into_list(doc, obj_id, "this is the first line", 0);
    put_element_into_list(doc, obj_id, "second line", 1);
    put_element_into_list(doc, obj_id, "third line", 2);

    char *buffer;
    int bytes = serialise_document(doc, &buffer);
    ASSERT_GT(bytes, 0);

    // first deseialisation 
    ASSERT_EQ(0, deserialise_document(doc, buffer, bytes));
    free(buffer);

    // change since last save
    put_element_into_list(doc, obj_id, "change #1 after last save", 3);
    put_element_into_list(doc, obj_id, "change #2 after last save", 4);

    int inc_bytes = serialise_document(doc, &buffer);
    ASSERT_GT(inc_bytes, 0);

    // second deseialisation
    ASSERT_EQ(0, deserialise_document(doc, buffer, inc_bytes));

    char **str_list;
    get_list(doc, "test.txt", &str_list);
    ASSERT(str_list != NULL);
    ASSERT_EQ(5, arrlen(str_list));
    ASSERT_STR_EQ("this is the first line", str_list[0]);
    ASSERT_STR_EQ("second line", str_list[1]);
    ASSERT_STR_EQ("third line", str_list[2]);
    ASSERT_STR_EQ("change #1 after last save", str_list[3]);
    ASSERT_STR_EQ("change #2 after last save", str_list[4]);
    

    AMresultFree(result);
    AMresultFree(doc_result);
    for (int i = 0; i < arrlen(str_list); i++) {
        free(str_list[i]);
    }
    arrfree(str_list);

    PASS();
}

TEST automerge_add_put_delete_elements(void) {
    AMresult *result = AMcreate(NULL);
    AMdoc *doc = get_document(result);
    AMresult *doc_result = NULL;
    const AMobjId *obj_id = add_list_to_document(doc, "item lists", &doc_result);
    
    // inserts
    put_element_into_list(doc, obj_id, "item #1", 0);
    put_element_into_list(doc, obj_id, "item #2", 1);
    put_element_into_list(doc, obj_id, "item #3", 2);
    put_element_into_list(doc, obj_id, "item #4", 3);
    put_element_into_list(doc, obj_id, "item #5", 4);

    char **list;
    get_list(doc, "item lists", &list);
    ASSERT_EQ(5, arrlen(list));

    for (uint32_t i = 0; i < arrlen(list); i++) {
        free(list[i]);
    }
    arrfree(list);

    // updates
    update_list_element(doc, obj_id, "modified item #2", 1);
    update_list_element(doc, obj_id, "modified item #4", 3);

    get_list(doc, "item lists", &list);
    ASSERT_STR_EQ("modified item #2", list[1]);
    ASSERT_STR_EQ("modified item #4", list[3]);
    for (uint32_t i = 0; i < arrlen(list); i++) {
        free(list[i]);
    }
    arrfree(list);

    // deletes
    delete_element_from_list(doc, obj_id, 3);
    delete_element_from_list(doc, obj_id, 2);
    get_list(doc, "item lists", &list);
    ASSERT_EQ(3, arrlen(list));
    ASSERT_STR_EQ("item #1", list[0]);
    ASSERT_STR_EQ("modified item #2", list[1]);
    ASSERT_STR_EQ("item #5", list[2]);
    for (uint32_t i = 0; i < arrlen(list); i++) {
        free(list[i]); 
    }
    arrfree(list);


    AMresultFree(result);
    AMresultFree(doc_result);
    PASS();
}

SUITE(suite_automerge) {
    RUN_TEST(automerge_get_document);
    RUN_TEST(automerge_get_and_put_list);
    RUN_TEST(automerge_whole_document_serde);
    RUN_TEST(automerge_incremental_document_serde);
    RUN_TEST(automerge_add_put_delete_elements);
}

#endif
