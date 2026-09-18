#include "connection.h"

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

  return socket_d;
}

int client_hello(const node_t* node) {
  message_t msg = { .kind = CLIENT_HELLO};

  send_message(node->socket_d, &msg);

  return 0;
}

void *await_responses(void *ts) {
  node_t *s = (node_t *)ts;

  receive_messages(s->socket_d, s);

  close(s->socket_d);
}

int client_listen(int socket, node_t* node) {
  node->socket_d = socket;

  if (pthread_create(&node->cthread, NULL, await_responses, (void *)node) < 0) {
    perror("error spinning client listen in a separate thread");
    return -1;
  }

  return 0;
}
