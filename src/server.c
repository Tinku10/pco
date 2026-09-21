#include "connection.h"
#include "log.h"
#include "queue.h"

typedef struct {

} connection_args_t;

int setup_server_socket(node_t *s, int port) {
  struct sockaddr_in server_addr;

  int socket_d = socket(AF_INET, SOCK_STREAM, 0);
  if (socket_d < 0) {
    perror("error creating socket");
    return -1;
  }

  server_addr.sin_family = AF_INET;
  server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
  server_addr.sin_port = htons(port);

  if (bind(socket_d, (struct sockaddr *)&server_addr, sizeof(server_addr)) <
      0) {
    perror("error during socket binding");
    return -1;
  }

  unsigned int ip = ntohl(server_addr.sin_addr.s_addr);
  dbg_print("server bound to ip: %u.%u.%u.%u\n", (ip >> 24) & 0xFF,
         (ip >> 16) & 0xFF, (ip >> 8) & 0xFF, ip & 0xFF);

  if (listen(socket_d, MAX_CLIENTS) < 0) {
    perror("error during listening");
    return -1;
  }

  s->socket_d = socket_d;

  return 0;
}

void *accept_connections(void *ts) {
  dbg_print("accepting new connections\n");

  struct sockaddr client_addr;

  node_t *s = (node_t *)ts;

  while (1) {
    socklen_t clen = sizeof(client_addr);
    int conn_d = accept(s->socket_d, &client_addr, &clen);

    dbg_print("new client incoming\n");

    // add the client address to the list of known clients
    s->other_connected_sockets[s->other_cnt++] = conn_d;

    dbg_print("client connection established on socket %d\n", conn_d);

    // send client hello

    receive_messages(conn_d, s);
  }

  close(s->socket_d);
}

int server_start(node_t *node, int port, char* file) {
  // node setup
  node->other_cnt = 0;
  node->file.path = file;
  // set up the pipe
  if (pipe(node->event) == -1) {
    perror("failed to set up pipe");
    return -1;
  }
  // initialize the queue for the node
  qinit(&node->inbox);

  if (setup_server_socket(node, port) < 0) {
    return -1;
  }

  if (pthread_create(&node->sthread, NULL, accept_connections, (void *)node) < 0) {
    perror("error spinning server in a separate thread");
    return -1;
  }

  return 0;
}
