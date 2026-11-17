#include "connection.h"
#include "diff.h"
#include "fileio.h"
#include "log.h"
#include "queue.h"
#include <lauxlib.h>
#include <lua.h>
#include <lualib.h>
#include <stb/stb_ds.h>

fline_t *get_lines(const char *content, diff_kind kind, range_t range) {
  char *start = content;
  char *end;
  fline_t *lines = NULL;

  while ((end = strchr(start, '\n')) != NULL) {
    size_t m_len = end - start;
    char *m_content = (char *)malloc(sizeof(char) * m_len + 1);
    memcpy(m_content, start, m_len);
    m_content[m_len] = '\0';
    fline_t m_line = {
        .body = m_content, .len = m_len, .kind = kind, .range = range};
    arrput(lines, m_line);
    start = end + 1;
  }

  if ((end = strchr(start, '\0')) != NULL) {
    size_t m_len = end - start;
    char *m_content = (char *)malloc(sizeof(char) * m_len + 1);
    memcpy(m_content, start, m_len);
    m_content[m_len] = '\0';
    fline_t m_line = {
        .body = m_content, .len = m_len, .kind = kind, .range = range};
    arrput(lines, m_line);
    start = end + 1;
  }

  return lines;
}
// node_t build_node(lua_State* L) {
//   node_t node;

//   luaL_checktype(L, 1, LUA_TTABLE);

//   lua_getfield(L, 1, "")
// }

// int lua_client_connect(lua_State *L) {
//   char *host = luaL_checkstring(L, 1);
//   int dport = luaL_checkint(L, 2);

//   int socket = client_connect(host, dport);

//   lua_pushnumber(L, socket);

//   return 1;
// }

static void push_document_state(lua_State *L, node_t *node) {
  if (!node || !node->doc) {
    lua_pushnil(L);
    return;
  }

  AMresult *root_result =
      AMmapRange(node->doc, AM_ROOT, AMstr(NULL), AMstr(NULL), NULL);

  if (!root_result || AMresultStatus(root_result) != AM_STATUS_OK) {
    AMresultFree(root_result);
    lua_pushnil(L);
    return;
  }

  // Create document-state table
  lua_newtable(L);

  AMitems items = AMresultItems(root_result);
  AMitem *item;

  while ((item = AMitemsNext(&items, 1)) != NULL) {
    AMbyteSpan key_span;

    if (!AMitemKey(item, &key_span)) {
      continue;
    }

    const AMobjId *file_id = AMitemObjId(item);

    if (!file_id || AMobjObjType(node->doc, file_id) != AM_OBJ_TYPE_LIST) {
      continue;
    }

    // Copy filename for get_list()
    char *filename = malloc(key_span.count + 1);

    if (!filename) {
      continue;
    }

    memcpy(filename, key_span.src, key_span.count);
    filename[key_span.count] = '\0';

    char **list = NULL;
    get_list(node->doc, filename, &list);

    // if (!list) {
    //   free(filename);
    //   continue;
    // }

    // Create Lua array
    lua_newtable(L);

    uint32_t count = arrlen(list);

    for (uint32_t i = 0; i < count; i++) {
      lua_pushstring(L, list[i]);
      lua_rawseti(L, -2, i + 1);

      free(list[i]);
    }

    // document[filename] = lines
    lua_setfield(L, -2, filename);

    free(filename);

    arrfree(list);
  }

  AMresultFree(root_result);
}

int lua_client_listen(lua_State *L) {
  // node_t *node = (node_t *)malloc(sizeof(node_t));

  char *host = luaL_checkstring(L, 1);
  int dport = luaL_checkint(L, 2);

  node_t *node = lua_newuserdata(L, sizeof(*node));
  // dbg_print("node address %zu", (size_t)node);

  client_listen(node, host, dport);

  luaL_getmetatable(L, "Node");
  lua_setmetatable(L, -2);
  // lua_pushinteger(L, (size_t)node);

  return 1;
}

int lua_client_hello(lua_State *L) {
  node_t *node = luaL_checkudata(L, 1, "Node");

  // size_t address = luaL_checklong(L, 1);
  // node_t *node = (node_t *)address;

  // dbg_print("node address %zu", (size_t)node);
  client_hello(node);

  return 0;
}

int lua_get_message(lua_State *L) {
  node_t *node = luaL_checkudata(L, 1, "Node");
  // size_t address = luaL_checkinteger(L, 1);
  // node_t *node = (node_t *)address;

  dbg_print("getting message from %d, left=%d\n", node->socket_d,
            arrlen(node->inbox));

  message_t *msg = qpop(&node->inbox);
  if (msg == NULL) {
    dbg_print("no messages found");
    lua_pushnil(L);
    return 1;
  }

  lua_newtable(L);

  lua_pushstring(L, message_kind_str(msg->kind));
  lua_setfield(L, -2, "kind");

  push_document_state(L, node);
  lua_setfield(L, -2, "crdt_document");

  dbg_print("got message");

  return 1;
}

int lua_get_eventfd(lua_State *L) {
  node_t *node = luaL_checkudata(L, 1, "Node");
  // size_t address = luaL_checkinteger(L, 1);
  // node_t *node = (node_t *)address;

  lua_pushnumber(L, node->event[0]);

  return 1;
}

int lua_send_message(lua_State *L) {
  dbg_print("inside send message");

  node_t *node = luaL_checkudata(L, 1, "Node");

  const char *filename = luaL_checkstring(L, 2);

  luaL_checktype(L, 3, LUA_TTABLE);

  size_t line_count = lua_objlen(L, 3);

  char **lines = NULL;

  for (size_t i = 1; i <= line_count; i++) {
    lua_rawgeti(L, 3, i);

    size_t len;
    const char *line = luaL_checklstring(L, -1, &len);

    char *copy = malloc(len + 1);
    if (!copy) {
      lua_pop(L, 1);
      return luaL_error(L, "allocation failed");
    }

    memcpy(copy, line, len);
    copy[len] = '\0';

    arrpush(lines, copy);
    lua_pop(L, 1);
  }

  dbg_print("filename: %s, lines: %zu\n", filename, arrlen(lines));

  if (overwrite_list(node->doc, filename, lines) < 0) {
    arrfree(lines);
    return luaL_error(L, "failed to update local crdt document");
  }

  message_t msg = {.kind = MSG_CONTENT, .doc = node->doc};
  send_message(node->connected_sockets[0], &msg);

  for (size_t i = 0; i < arrlen(lines); i++) {
    free(lines[i]);
  }
  arrfree(lines);

  return 0;
}

int lua_server_start(lua_State *L) {
  int port = luaL_checkint(L, 1);
  char *path = luaL_checkstring(L, 2);

  node_t *node = lua_newuserdata(L, sizeof(*node));

  server_start(node, port, path);

  luaL_getmetatable(L, "Node");
  lua_setmetatable(L, -2);

  return 1;
}

int lua_create_layout(lua_State *L) {
  luaL_checktype(L, 1, LUA_TTABLE);
  size_t cnt = lua_objlen(L, 1);

  char **files = NULL;

  for (size_t i = 1; i <= cnt; i++) {
    lua_rawgeti(L, 1, i);

    const char *file = luaL_checkstring(L, -1);

    arrpush(files, (char *)file);
  }

  char *session_path = create_dir_layout(files);

  lua_pop(L, cnt);
  arrfree(files);

  lua_pushstring(L, session_path);

  return 1;
}

static const struct luaL_Reg lib[] = {
    // {"client_connect", lua_client_connect},
    {"client_listen", lua_client_listen},
    {"client_hello", lua_client_hello},
    {"get_message", lua_get_message},
    {"get_eventfd", lua_get_eventfd},
    {"send_message", lua_send_message},
    {"server_start", lua_server_start},
    {"create_layout", lua_create_layout},
    // {"client_hello", lua_client_hello},
    {NULL, NULL},
};

int luaopen_mylib(lua_State *L) {
  // register Node userdata type
  luaL_newmetatable(L, "Node");
  lua_pop(L, 1);

  luaL_openlib(L, "pco", lib, 0);
  return 1;
}
