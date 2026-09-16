#include "connection.h"
#include "string.h"

const char *message_kind_str(message_kind kind) {
  switch (kind) {
  case MSG_CONTENT:
    return "MSG_CONTENT";
  case MSG_ACK:
    return "MSG_ACK";
  case CLIENT_HELLO:
    return "CLIENT_HELLO";
  default:
    return "UNKNOWN";
  }
}

// simple serialization of the message
// | header: 4 bytes | kind: 4 bytes | len: 4 bytes | body: len bytes |
// a header of 4 bytes will be added to help `recv()` know how much to read
char *package_message(message_t *msg) {
  uint32_t total_bytes = 4 + 4 + msg->len;
  char *buf = (char *)malloc(total_bytes);

  uint32_t total = htonl(total_bytes);
  uint32_t kind = htonl(msg->kind);
  uint32_t len = htonl(msg->len);

  memcpy(buf, &total, MSG_HEADER_BYTES);
  memcpy(buf + MSG_HEADER_BYTES, &kind, 4);
  memcpy(buf + MSG_HEADER_BYTES + 4, &len, 4);
  memcpy(buf + MSG_HEADER_BYTES + 8, msg->body, msg->len);

  printf("Received message: kind=%d len=%zu body=%s\n", msg->kind, msg->len,
         msg->body);
  printf("Packaged message: ");
  for (size_t i = 0; i < total_bytes; i++) {
    printf("%02x ", (unsigned char)buf[i]);
  }
  printf("\n");

  return buf;
}

// message should not have the header (initial 4 bytes)
message_t *unpackage_message(char *buf) {
  printf("unpacking message\n");

  message_t *msg = (message_t *)malloc(sizeof(message_t));

  uint32_t kind;
  uint32_t len;

  memcpy(&kind, buf, 4);
  memcpy(&len, buf + 4, 4);

  msg->kind = ntohl(kind);
  msg->len = ntohl(len);

  msg->body = (char *)malloc(msg->len);
  memcpy(msg->body, buf + 8, msg->len);

  printf("message body: %s\n", msg->body);

  return msg;
}

int recv_header(int conn_d) {
  // header is of four bytes
  int received = 0;
  int n = 0;

  uint8_t buf[MSG_HEADER_BYTES] = {0};

  printf("checking for %d bytes header\n", MSG_HEADER_BYTES);

  while (received < MSG_HEADER_BYTES) {
    n = recv(conn_d, buf + received, MSG_HEADER_BYTES - received, 0);
    if (n <= 0)
      break;

    received += n;
    printf("read %d bytes\n", n);
  }

  if (n < 0) {
    perror("error while reading header");
    return -1;
  }

  uint32_t header;
  memcpy(&header, buf, sizeof(header));
  header = ntohl(header);

  printf("header: %d\n", header);

  return header;
}

char *recv_message(int conn_d, int req) {
  int received = 0;
  int n = 0;

  printf("checking for %d bytes message body\n", req);

  char *buf = (char *)malloc(req);

  while (received < req) {
    n = recv(conn_d, buf + received, req - received, 0);
    if (n <= 0)
      break;

    received += n;
    printf("read %d bytes\n", n);
  }

  if (n < 0) {
    perror("error while reading message body");
    return NULL;
  }

  return buf;
}

void receive_messages(int conn_d, node_t *node) {
  // message_t buf;
  int n = 0;

  printf("waiting on data\n");
  // pthread_t conn_thread;

  size_t header;
  while ((header = recv_header(conn_d)) > 0) {
    char *buf = recv_message(conn_d, header);
    if (buf == NULL)
      break;
    message_t *msg = unpackage_message(buf);
    on_message_received(conn_d, msg, node);
  }

  printf("closing server connection\n");

  close((int)conn_d);
}

int send_message(int conn_d, message_t *msg) {
  printf("sending message\n");
  uint32_t total_bytes = MSG_HEADER_BYTES + 4 + 4 + msg->len;
  char *buf = package_message(msg);

  int sent = 0;
  int n = 0;

  while (sent < total_bytes) {
    n = send(conn_d, buf, total_bytes - sent, 0);
    if (n <= 0)
      break;

    sent += n;
  }

  if (n < 0) {
    perror("error sending bytes");
    return -1;
  }

  return 0;
}

void on_message_received(int conn_d, message_t *msg, node_t *node) {
  printf("received: kind=%s body=%s\n", message_kind_str(msg->kind), msg->body);
  switch (msg->kind) {
  case MSG_CONTENT: {
    message_t re_msg = {.kind = MSG_ACK};
    send_message(conn_d, &re_msg);
    break;
  }
  case CLIENT_HELLO: {
    message_t re_msg = {.kind = MSG_CONTENT,
                        .body = node->file.path,
                        .len = strlen(node->file.path)};
    send_message(conn_d, &re_msg);
    break;
  }
  default:
    break;
  }
}
