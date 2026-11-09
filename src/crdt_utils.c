#include "crdt_utils.h"
#include <automerge-c/automerge.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

AMdoc *get_document(AMresult *result) {
  if (!result || AMresultStatus(result) != AM_STATUS_OK) {
    fprintf(stderr, "failed to obtain document\n");
    AMresultFree(result);
    return NULL;
  }

  AMdoc *doc = NULL;

  if (!AMitemToDoc(AMresultItem(result), &doc) || !doc) {
    fprintf(stderr, "result does not contain a document\n");
    AMresultFree(result);
    return NULL;
  }

  return doc;
}

const AMobjId *add_list_to_document(AMdoc *doc, char *key, AMresult **ptr) {
  *ptr = AMmapPutObject(doc, AM_ROOT, AMstr(key), AM_OBJ_TYPE_LIST);

  if (!*ptr || AMresultStatus(*ptr) != AM_STATUS_OK) {
    fprintf(stderr, "failed to create list object\n");
    AMresultFree(*ptr);
    return NULL;
  }

  const AMobjId *list_obj = AMitemObjId(AMresultItem(*ptr));

  if (!list_obj) {
    fprintf(stderr, "failed to get text object ID\n");
    AMresultFree(*ptr);
    return NULL;
  }

  return list_obj;
}

int put_element_into_list(AMdoc *doc, const AMobjId *obj_id, char *item,
                          int32_t position) {
  AMresult *result = AMlistPutStr(doc, obj_id, position, true, AMstr(item));

  if (!result || AMresultStatus(result) != AM_STATUS_OK) {
    fprintf(stderr, "failed to put string into list object\n");
    AMresultFree(result);
    return -1;
  }

  AMresultFree(result);
  return 0;
}

int update_list_element(AMdoc *doc, const AMobjId *obj_id, char *item,
                        int32_t position) {
  AMresult *result = AMlistPutStr(doc, obj_id, position, false, AMstr(item));

  if (!result || AMresultStatus(result) != AM_STATUS_OK) {
    fprintf(stderr, "failed to update element at the specified position\n");
    AMresultFree(result);
    return -1;
  }

  AMresultFree(result);
  return 0;
}

void get_list(AMdoc *doc, const char *filename, char ***str_list) {
  AMbyteSpan key = {.src = (const uint8_t *)filename,
                    .count = strlen(filename)};

  AMresult *file_result = AMmapGet(doc, AM_ROOT, key, NULL);

  if (!file_result) {
    fprintf(stderr, "failed to retrieve file: %s\n", filename);
    return;
  }

  // object id of the filename in the document
  const AMobjId *file_id = AMitemObjId(AMresultItem(file_result));

  if (!file_id || AMobjObjType(doc, file_id) != AM_OBJ_TYPE_LIST) {
    fprintf(stderr, "file not found or is not a List: %s\n", filename);
    AMresultFree(file_result);
    return;
  }

  size_t count = AMobjSize(doc, file_id, NULL);

  *str_list = NULL;

  for (size_t i = 0; i < count; i++) {
    AMresult *item_result = AMlistGet(doc, file_id, i, NULL);

    if (!item_result) {
      continue;
    }

    AMbyteSpan value;

    if (AMitemToStr(AMresultItem(item_result), &value)) {
      char *item = (char *)malloc(value.count * sizeof(char) + 1);
      memcpy(item, value.src, value.count);
      item[value.count] = '\0';
      arrpush(*str_list, item);
    }

    AMresultFree(item_result);
  }

  AMresultFree(file_result);
}

int serialise_document(AMdoc *doc, char **buffer) {
  if (!doc) {
    return 0;
  }

  AMresult *result = AMsaveIncremental(doc);
  AMbyteSpan bytes;

  if (!result || !AMitemToBytes(AMresultItem(result), &bytes)) {
    fprintf(stderr, "failed to serialise document\n");
    AMresultFree(result);
    return -1;
  }

  *buffer = (char *)malloc(bytes.count * sizeof(char));
  memcpy(*buffer, bytes.src, bytes.count);

  AMresultFree(result);
  return bytes.count;
}

int deserialise_document(AMdoc *doc, const char *data, size_t size) {
  if (!doc || size == 0) {
    return 0;
  }

  AMresult *result = AMloadIncremental(doc, data, size);

  if (!result || AMresultStatus(result) != AM_STATUS_OK) {
    fprintf(stderr,
            "failed to deserialise incremental changes of the document\n");
    AMresultFree(result);
    return -1;
  }

  AMresultFree(result);
  return 0;
}