/**
 * @file crdt_utils.h
 * @brief utilities to work with AMdoc/CRDT
 * @version 0.1
 * @date 2026-11-08
 *
 * @copyright Copyright (c) 2026
 */
#ifndef CRDT_UTILS_H
#define CRDT_UTILS_H
#include <stb/stb_ds.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/**
 * @brief get the AMdoc from result
 *
 * @return AMdoc* pointer to the AMdoc if operation is sucessful, NULL otherwise
 * @warning free AMresult to avoid memory leak
 */
AMdoc *get_document(AMresult *result);

/**
 * @brief adds an empty list to the root map of an AMdoc
 *
 * The document uses a root-map structure where each key represents a filename
 * and its corresponding value is a list of strings (e.g., lines of a file)
 *
 * example document structure:
 *
 *     root
 *       |-- "file1.txt" -> ["apple banana", "cherry", "mango"]
 *       |-- "file2.txt" -> ["grape", "pear", "peach", "strawberry"]
 *
 * @param[in]  doc     the Automerge document to modify
 * @param[in]  key     the key under which the new list will be created
 * @param[out] result  address of a pointer that receives the AMresult
 *                     returned by the Automerge operation
 *
 * @return a borrowed pointer to the AMobjId of the newly created list
 *
 * @warning free AMresult when AMobjId goes out of scope to avoid a memory leak
 */
const AMobjId *add_list_to_document(AMdoc *doc, char *key, AMresult **result);

/**
 * @brief inserts an element at a particular position of a list in an Automerge
 * document
 *
 * @param[in] doc the Automerge document
 * @param[in] obj_id pointer to the list object where item is to be inserted
 * @param[in] item a null terminated C-string
 * @param[in] position integer position in the list
 * @return 0 if the operation is successful, -1 otherwise
 */
int put_element_into_list(AMdoc *doc, const AMobjId *obj_id, char *item,
                          int32_t position);

/**
 * @brief updates an element at a particular position of a list in an Automerge
 * document
 *
 * @param[in] doc the Automerge document
 * @param[in] obj_id pointer to the list object where item is to be inserted
 * @param[in] item a null terminated C-string
 * @param[in] position integer position in the list
 * @return 0 if the operation is successful, -1 otherwise
 */
int update_list_element(AMdoc *doc, const AMobjId *obj_id, char *item,
                        int32_t position);

/**
 * @brief get the list against a key from an Automerge document
 *
 * @param[in] doc the Automerge document
 * @param[in] key name of the key in root of Automerge document
 * @param[out] str_list the list of null terminated C-string stored against the
 * key
 */
void get_list(AMdoc *doc, const char *key, char ***str_list);

/**
 * @brief serialises the incremental changes made to a document
 *
 *
 * @param[in] doc pointer to the Automerge document to be serialised
 * @param[in] buffer pointer to the memory address of the serialised bytes
 * @return number of bytes serialised or -1 in case of any error
 */
int serialise_document(AMdoc *doc, char **buffer);

/**
 * @brief deserialises the incremental changes made to a document
 *
 * @param[in] doc pointer to the existing AMdoc
 * @param[in] data pointer to the buffer to be deserialised
 * @param[in] size number of raw bytes to be deserialised
 * @return 0 if the operation is successful, -1 otherwise
 */
int deserialise_document(AMdoc *doc, const char *data, size_t size);

#endif