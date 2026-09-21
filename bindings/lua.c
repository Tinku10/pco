#include "connection.h"
#include "log.h"
#include "queue.h"
#include "diff.h"
#include <lauxlib.h>
#include <lua.h>
#include <lualib.h>
#include <stb/stb_ds.h>


// node_t build_node(lua_State* L) {
//   node_t node;

//   luaL_checktype(L, 1, LUA_TTABLE);

//   lua_getfield(L, 1, "")
// }

int lua_client_connect(lua_State *L) {
  char *host = luaL_checkstring(L, 1);
  int dport = luaL_checkint(L, 2);

  int socket = client_connect(host, dport);

  lua_pushnumber(L, socket);

  return 1;
}

int lua_client_listen(lua_State *L) {
  node_t *node = (node_t *)malloc(sizeof(node_t));

  char *host = luaL_checkstring(L, 1);
  int dport = luaL_checkint(L, 2);

  client_listen(node, host, dport);

  lua_pushnumber(L, (size_t)node);

  return 1;
}

int lua_client_hello(lua_State *L) {
  size_t address = luaL_checkinteger(L, 1);
  node_t *node = (node_t *)address;

  client_hello(node);

  return 0;
}

int lua_client_get_message(lua_State *L) {
  dbg_print("getting message\n");
  fflush(stdout);
  size_t address = luaL_checkinteger(L, 1);
  node_t *node = (node_t *)address;

  message_t *msg = qpop(&node->inbox);
  if (msg == NULL) {
    lua_pushnil(L);
    return 1;
  }

  lua_newtable(L);

  lua_pushstring(L, message_kind_str(msg->kind));
  lua_setfield(L, -2, "kind");

  lua_newtable(L);

  for (size_t i = 0; i < msg->dlen; i++) {
    lua_newtable(L);

    if (msg->diff[i].path) {
      lua_pushlstring(L, msg->diff[i].path, msg->diff[i].plen);
      lua_setfield(L, -2, "path");
    }

    lua_pushinteger(L, msg->diff[i].cursor_r);
    lua_setfield(L, -2, "cursor_r");

    lua_newtable(L);

    for (size_t j = 0; j < msg->diff[i].llen; j++) {
      lua_newtable(L);

      lua_pushstring(L, diff_kind_str(msg->diff[i].lines[j].kind));
      lua_setfield(L, -2, "kind");

      lua_pushinteger(L, msg->diff[i].lines[j].num);
      lua_setfield(L, -2, "num");

      lua_pushinteger(L, msg->diff[i].lines[j].len);
      lua_setfield(L, -2, "len");

      lua_pushlstring(L, msg->diff[i].lines[j].body, msg->diff[i].lines[j].len);
      lua_setfield(L, -2, "body");

      lua_rawseti(L, -2, j + 1);
    }

    lua_setfield(L, -2, "lines");

    lua_rawseti(L, -2, i + 1);
  }

  lua_setfield(L, -2, "diffs");

  return 1;
}

int lua_get_eventfd(lua_State *L) {
  size_t address = luaL_checkinteger(L, 1);
  node_t *node = (node_t *)address;

  lua_pushnumber(L, node->event[0]);

  return 1;
}

int lua_send_message(lua_State *L) {
  size_t address = luaL_checkinteger(L, 1);
  node_t *node = (node_t *)address;

  char *prev_state = luaL_checkstring(L, 2);
  char *curr_state = luaL_checkstring(L, 3);

  fdiff_t *diff = get_buffer_diff(prev_state, curr_state);
  message_t msg = {.kind = MSG_CONTENT, .dlen = arrlen(diff), .diff = diff};

  client_send(node, &msg);

  return 0;
}

static const struct luaL_Reg lib[] = {
    {"client_connect", lua_client_connect},
    {"client_listen", lua_client_listen},
    {"client_hello", lua_client_hello},
    {"client_get_message", lua_client_get_message},
    {"client_get_eventfd", lua_get_eventfd},
    {"client_send_message", lua_send_message},
    // {"client_hello", lua_client_hello},
    {NULL, NULL},
};

int luaopen_mylib(lua_State *L) {
  luaL_openlib(L, "pco", lib, 0);
  return 1;
}
