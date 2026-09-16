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

int client_hello(int socket_d, node_t* node) {
  message_t msg = { .kind = CLIENT_HELLO};

  send_message(socket_d, &msg);
  receive_messages(socket_d, node);

  return 0;
}
