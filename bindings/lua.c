#include "connection.h"
#include "diff.h"
#include "log.h"
#include "queue.h"
#include <lauxlib.h>
#include <lua.h>
#include <lualib.h>
#include <stb/stb_ds.h>

fline_t* get_lines(const char* content, diff_kind kind, range_t range) {
  char* start = content;
  char* end;
  fline_t* lines = NULL;

  while ((end = strchr(start, '\n')) != NULL) {
    size_t m_len = end - start;
    char *m_content = (char *)malloc(sizeof(char) * m_len + 1);
    memcpy(m_content, start, m_len);
    m_content[m_len] = '\0';
    fline_t m_line = {.body =  m_content, .len = m_len, .kind = kind, .range = range};
    arrput(lines, m_line);
    start = end + 1;
  }
  
  if ((end = strchr(start, '\0')) != NULL) {
    size_t m_len = end - start;
    char *m_content = (char *)malloc(sizeof(char) * m_len + 1);
    memcpy(m_content, start, m_len);
    m_content[m_len] = '\0';
    fline_t m_line = {.body = m_content, .len = m_len, .kind = kind, .range = range};
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

  dbg_print("getting message from %d, left=%d\n", node->socket_d, arrlen(node->inbox));

  message_t *msg = qpop(&node->inbox);
  if (msg == NULL) {
    dbg_print("no messages found");
    lua_pushnil(L);
    return 1;
  }

  lua_newtable(L);

  lua_pushstring(L, message_kind_str(msg->kind));
  lua_setfield(L, -2, "kind");

  lua_newtable(L);

  for (size_t i = 0; i < msg->dlen; i++) {
    lua_newtable(L);

    // lua_pushstring(L, diff_kind_str(msg->diff[i].kind));
    // lua_setfield(L, -2, "kind");

    if (msg->diff[i].plen > 0) {
      lua_pushlstring(L, msg->diff[i].path, msg->diff[i].plen);
      lua_setfield(L, -2, "path");
    }

    lua_pushinteger(L, msg->diff[i].cursor_r);
    lua_setfield(L, -2, "cursor_r");

    lua_newtable(L);

    for (size_t j = 0; j < msg->diff[i].llen; j++) {
      lua_newtable(L);

      // lua_pushinteger(L, msg->diff[i].lines[j].num);
      // lua_setfield(L, -2, "num");

      lua_pushstring(L, diff_kind_str(msg->diff[i].lines[j].kind));
      lua_setfield(L, -2, "kind");

      lua_pushinteger(L, msg->diff[i].lines[j].len);
      lua_setfield(L, -2, "len");

      lua_pushlstring(L, msg->diff[i].lines[j].body, msg->diff[i].lines[j].len);
      lua_setfield(L, -2, "body");

      lua_newtable(L);

      lua_pushinteger(L, msg->diff[i].lines[j].range.start_pos);
      lua_setfield(L, -2, "start_row");

      lua_pushinteger(L, msg->diff[i].lines[j].range.start_char);
      lua_setfield(L, -2, "start_col");

      lua_pushinteger(L, msg->diff[i].lines[j].range.end_pos);
      lua_setfield(L, -2, "end_row");

      lua_pushinteger(L, msg->diff[i].lines[j].range.end_char);
      lua_setfield(L, -2, "end_col");

      lua_setfield(L, -2, "range");

      lua_rawseti(L, -2, j + 1);
    }

    lua_setfield(L, -2, "lines");

    lua_rawseti(L, -2, i + 1);
  }

  lua_setfield(L, -2, "diffs");

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
  // size_t address = luaL_checkinteger(L, 1);
  // node_t *node = (node_t *)address;

  const char *path = luaL_checkstring(L, 2);

  int start_row = luaL_checkinteger(L, 3);
  int start_col = luaL_checkinteger(L, 4);

  int end_row = luaL_checkinteger(L, 5);
  int end_col = luaL_checkinteger(L, 6);

  diff_kind kind = diff_kind_str_to_enum(luaL_checkstring(L, 7));

  const char* content = luaL_checkstring(L, 8);
  // char *prev_state = luaL_checkstring(L, 3);
  // char *curr_state = luaL_checkstring(L, 4);
  range_t range = { .start_pos = start_row, .start_char = start_col, .end_pos = end_row, .end_char = end_col };
  fline_t* lines = get_lines(content, kind, range);

  fdiff_t diff = {.path = path, .plen = strlen(path), .lines = lines, .llen = arrlen(lines)};
  // fdiff_t *diff = get_buffer_diff(path, prev_state, curr_state);
  message_t msg = {.kind = MSG_CONTENT, .dlen = 1, .diff = &diff};
  dbg_print("diff calculation ends");

  send_message(node->connected_sockets[0], &msg);
  dbg_print("message sent");

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

static const struct luaL_Reg lib[] = {
    // {"client_connect", lua_client_connect},
    {"client_listen", lua_client_listen},
    {"client_hello", lua_client_hello},
    {"get_message", lua_get_message},
    {"get_eventfd", lua_get_eventfd},
    {"send_message", lua_send_message},
    {"server_start", lua_server_start},
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
