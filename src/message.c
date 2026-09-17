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
// | header: 4 bytes |
// |=================|
// | kind:   4 bytes |
// | dlen:   4 bytes |
// | diff:
//        | diff (1)
//                | plen:     4 bytes |
//                | path:  plen bytes |
//                | blen:     4 bytes |
//                | body:  blen bytes |
//                | cursor_r: 4 bytes |
//        | diff (2) ...    |
//        | diff (dlen) ... |
// a header of 4 bytes will be added to help `recv()` know how much to read
char *package_message(message_t *msg) {
  // total bytes excluding header
  uint32_t total_bytes = 4 + 4;
  for (size_t i = 0; i < msg->dlen; i++) {
    total_bytes += 4 + msg->diff[i].plen + 4 + msg->diff[i].blen + 4;
  }
  char *buf = (char *)malloc(total_bytes);

  uint32_t total = htonl(total_bytes);
  uint32_t kind = htonl(msg->kind);
  uint32_t dlen = htonl(msg->dlen);

  memcpy(buf, &total, MSG_HEADER_BYTES);
  memcpy(buf + MSG_HEADER_BYTES, &kind, 4);
  memcpy(buf + MSG_HEADER_BYTES + 4, &dlen, 4);

  int shift = MSG_HEADER_BYTES + 8;
  for (size_t i = 0; i < msg->dlen; i++) {
    uint32_t plen = htonl(msg->diff[i].plen);
    uint32_t blen = htonl(msg->diff[i].blen);

    memcpy(buf + shift, &plen, 4);
    shift += 4;
    memcpy(buf + shift, msg->diff[i].path, msg->diff[i].plen);
    shift += msg->diff[i].plen;
    memcpy(buf + shift, &blen, 4);
    shift += 4;
    memcpy(buf + shift, msg->diff[i].body, msg->diff[i].blen);
    shift += msg->diff[i].blen;
  }

  // printf("Received message: kind=%d diff_len=%zu body=%s\n", msg->kind,
  // msg->blen, msg->body); printf("Packaged message: "); for (size_t i = 0; i <
  // total_bytes; i++) {
  //   printf("%02x ", (unsigned char)buf[i]);
  // }
  // printf("\n");

  return buf;
}

// message should not have the header (initial 4 bytes)
// | kind:   4 bytes |
// | dlen:   4 bytes |
// | diff:
//        | diff (1)
//                | plen:     4 bytes |
//                | path:  plen bytes |
//                | blen:     4 bytes |
//                | body:  blen bytes |
//                | cursor_r: 4 bytes |
//        | diff (2) ...    |
//        | diff (dlen) ... |
message_t *unpackage_message(char *buf) {
  printf("unpacking message\n");

  message_t *msg = (message_t *)malloc(sizeof(message_t));

  uint32_t kind;
  uint32_t dlen;

  memcpy(&kind, buf, 4);
  memcpy(&dlen, buf + 4, 4);

  msg->kind = ntohl(kind);
  msg->dlen = ntohl(dlen);

  msg->diff = (fdiff_t *)malloc(msg->dlen * sizeof(fdiff_t));

  int shift = 8;
  for (size_t i = 0; i < msg->dlen; i++) {
    uint32_t plen;
    uint32_t blen;

    memcpy(&plen, buf + shift, 4);
    msg->diff[i].plen = ntohl(plen);
    shift += 4;

    msg->diff[i].path = (char *)malloc(msg->diff[i].plen);
    memcpy(msg->diff[i].path, buf + shift, msg->diff[i].plen);
    shift += msg->diff[i].plen;

    memcpy(&blen, buf + shift, 4);
    msg->diff[i].blen = ntohl(blen);
    shift += 4;

    msg->diff[i].body = (char *)malloc(msg->diff[i].blen);
    memcpy(msg->diff[i].body, buf + shift, msg->diff[i].blen);
    shift += msg->diff[i].blen;


    printf("diff-%zu body=%s path=%s\n", i, msg->diff[i].body, msg->diff[i].path);
  }

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
  uint32_t total_bytes = MSG_HEADER_BYTES + 4 + 4;
  for (size_t i = 0; i < msg->dlen; i++) {
    total_bytes += 4 + msg->diff[i].plen + 4 + msg->diff[i].blen + 4;
  }
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
  switch (msg->kind) {
  case MSG_CONTENT: {
    message_t re_msg = {.kind = MSG_ACK};
    send_message(conn_d, &re_msg);
    break;
  }
  case CLIENT_HELLO: {
    fdiff_t diff = {.plen = 5, .path = "abcde", .blen = 5, .body = "12345"};
    message_t re_msg = {
        .kind = MSG_CONTENT,
        .dlen = 1,
        .diff = (fdiff_t[]){diff},
    };
    send_message(conn_d, &re_msg);
    break;
  }
  default:
    break;
  }
}
