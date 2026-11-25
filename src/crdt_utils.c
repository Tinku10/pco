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

int delete_element_from_list(AMdoc *doc, const AMobjId *obj_id, int32_t position) {
  AMresult *result = AMlistDelete(doc, obj_id, position);

  if (!result || AMresultStatus(result) != AM_STATUS_OK) {
    fprintf(stderr, "failed to delete element from the specified position\n");
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

int get_list_size(AMdoc *doc, const char *filename) {
  AMbyteSpan key = {.src = (const uint8_t *)filename,
                    .count = strlen(filename)};

  AMresult *file_result = AMmapGet(doc, AM_ROOT, key, NULL);

  if (!file_result) {
    fprintf(stderr, "failed to retrieve file: %s\n", filename);
    return -1;
  }

  // object id of the filename in the document
  const AMobjId *file_id = AMitemObjId(AMresultItem(file_result));

  if (!file_id || AMobjObjType(doc, file_id) != AM_OBJ_TYPE_LIST) {
    fprintf(stderr, "file not found or is not a List: %s\n", filename);
    AMresultFree(file_result);
    return -1;
  }

  return AMobjSize(doc, file_id, NULL);
}

int overwrite_list(AMdoc *doc, const char *key, char **new_list) {
  AMresult *result = AMmapGet(doc, AM_ROOT, AMstr(key), NULL);

  if (!result || AMresultStatus(result) != AM_STATUS_OK) {
    AMresultFree(result);
    return -1;
  }

  AMobjId *obj_id = NULL; // pointer to the list in the document
  AMresult *list_obj_result;
  bool list_obj_allocated = false;
  AMitem *item = AMresultItem(result);

  if (!item || AMitemValType(item) == AM_VAL_TYPE_VOID) {
    obj_id = add_list_to_document(doc, key, &list_obj_result);
    list_obj_allocated = true;
  } else {
    obj_id = AMitemObjId(AMresultItem(result));
  }

  uint32_t doc_sz = get_list_size(doc, key);

  // elements updated
  for (int i = 0; i < MIN(arrlen(new_list), doc_sz); i++) {
    if (update_list_element(doc, obj_id, new_list[i], i) < 0) {
      AMresultFree(result);
      if (list_obj_allocated)
        AMresultFree(list_obj_result);
      return -1;
    }
  }
  // elements deleted
  for (int i = arrlen(new_list); i < doc_sz; i++) {
    // once element at position i is deleted, all elements to its right will
    // shift left by one position. So effectively the position of elements
    // to be deleted will stay same which is arrlen(new_list)
    if (delete_element_from_list(doc, obj_id, arrlen(new_list)) < 0) {
      AMresultFree(result);
      if (list_obj_allocated)
        AMresultFree(list_obj_result);
      return -1;
    }
  }
  // elements added
  for (int i = doc_sz; i < arrlen(new_list); i++) {
    if (put_element_into_list(doc, obj_id, new_list[i], i) < 0) {
      AMresultFree(result);
      if (list_obj_allocated)
        AMresultFree(list_obj_result);
      return -1;
    }
  }
  return 0;
}

int serialise_document(AMdoc *doc, char **buffer, int incremental) {
  if (!doc) {
    return 0;
  }

  AMresult *result = (incremental ? AMsaveIncremental(doc) : AMsave(doc));
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