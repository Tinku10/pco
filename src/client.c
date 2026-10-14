#include "connection.h"
#include "log.h"
#include "queue.h"

int client_connect(char *ip, int port) {
  int socket_d = socket(AF_INET, SOCK_STREAM, 0);
  if (socket_d < 0) {
    perror("error creating socket");
    return -1;
  }

  struct sockaddr_in addr = {.sin_addr.s_addr = inet_addr(ip),
                             .sin_family = AF_INET,
                             .sin_port = htons(port)};

  socklen_t slen = sizeof(addr);
  if (connect(socket_d, (const struct sockaddr *)&addr, slen) < 0) {
    perror("failed to connect to server");
    return -1;
  }

  dbg_print("created socket %d", socket_d);

  return socket_d;
}

int client_hello(const node_t* node) {
  message_t msg = { .kind = CLIENT_HELLO, .dlen = 0};

  send_message(node->connected_sockets[0], &msg);

  return 0;
}

void *await_responses(void *ts) {
  node_t *s = (node_t *)ts;

  receive_messages(s->connected_sockets[0], s);

  return NULL;
}

// int client_listen(int socket, node_t* node) {
//   node->socket_d = socket;

//   if (pthread_create(&node->cthread, NULL, await_responses, (void *)node) < 0) {
//     perror("error spinning client listen in a separate thread");
//     return -1;
//   }

//   return 0;
// }

int client_listen(node_t* node, char* ip, int port) {
  int socket = client_connect(ip, port);

  // setup node
  node->connections = 0;
  node->connected_sockets[node->connections++] = socket;
  node->kind = NODE_CONTRIBUTOR;
  // set up the pipe
  if (pipe(node->event) == -1) {
    perror("failed to set up pipe");
    return -1;
  }
  // initialize the queue for the node
  qinit(&node->inbox);


  if (pthread_create(&node->cthread, NULL, await_responses, (void *)node) < 0) {
    perror("error spinning client listen in a separate thread");
    return -1;
  }

  return 0;
}

