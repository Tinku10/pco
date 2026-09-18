#include <stb_ds.h>
#include "connection.h"
#include "fileio.h"
#include "string.h"

const char *message_kind_str(message_kind kind) {
  switch (kind) {
  case MSG_CONTENT:
    return "MSG_CONTENT";
  case MSG_ACK:
    return "MSG_ACK";
  case CLIENT_HELLO:
    return "CLIENT_HELLO";
  case SERVER_HELLO:
    return "SERVER_HELLO";
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
//                | llen:     4 bytes |
//                | lines:
//                        | line (0)
//                                | kind:   4 bytes |
//                                | num:    4 bytes |
//                                | len:    4 bytes |
//                                | body: len bytes |
//                        | line (1) ...    |
//                        ...
//                        | line (llen) ... |
//                | cursor_r: 4 bytes |
//        | diff (2) ...    |
//        ...
//        | diff (dlen) ... |
// a header of 4 bytes will be added to help `recv()` know how much to read
char *package_message(message_t *msg) {
  printf("packing message\n");

  // total bytes excluding header
  uint32_t total_bytes = 4 + 4;
  for (size_t i = 0; i < msg->dlen; i++) {
    total_bytes += 4 + msg->diff[i].plen + 4;
    for (size_t j = 0; j < msg->diff[i].llen; j++) {
      total_bytes += 4 + 4 + 4 + msg->diff[i].lines[j].len;
    }
  }
  char *buf = (char *)malloc(MSG_HEADER_BYTES + total_bytes);

  // serialize message_t
  uint32_t total = htonl(total_bytes);
  uint32_t kind = htonl(msg->kind);
  uint32_t dlen = htonl(msg->dlen);

  memcpy(buf, &total, MSG_HEADER_BYTES);
  memcpy(buf + MSG_HEADER_BYTES, &kind, 4);
  memcpy(buf + MSG_HEADER_BYTES + 4, &dlen, 4);

  int shift = MSG_HEADER_BYTES + 8;

  // serialize fdiff_t
  for (size_t i = 0; i < msg->dlen; i++) {
    uint32_t plen = htonl(msg->diff[i].plen);
    uint32_t llen = htonl(msg->diff[i].llen);

    memcpy(buf + shift, &plen, 4);
    shift += 4;
    memcpy(buf + shift, msg->diff[i].path, msg->diff[i].plen);
    shift += msg->diff[i].plen;
    memcpy(buf + shift, &llen, 4);
    shift += 4;

    // serialize fline_t
    for (size_t j = 0; j < msg->diff[i].llen; j++) {
      uint32_t dkind = htonl(msg->diff[i].lines[i].kind);
      uint32_t num = htonl(msg->diff[i].lines[j].num);
      uint32_t len = htonl(msg->diff[i].lines[j].len);

      memcpy(buf + shift, &dkind, 4);
      shift += 4;
      memcpy(buf + shift, &num, 4);
      shift += 4;
      memcpy(buf + shift, &len, 4);
      shift += 4;
      memcpy(buf + shift, msg->diff[i].lines[j].body, msg->diff[i].lines[j].len);
      shift += msg->diff[i].lines[j].len;
    }
  }

  // printf("Received message: kind=%d diff_len=%zu body=%s\n", msg->kind,
  // msg->blen, msg->body); printf("Packaged message: "); for (size_t i = 0; i <
  // total_bytes; i++) {
  //   printf("%02x ", (unsigned char)buf[i]);
  // }
  printf("packing complete\n");

  return buf;
}

// message should not have the header (initial 4 bytes)
// | kind:   4 bytes |
// | dlen:   4 bytes |
// | diff:
//        | diff (1)
//                | plen:     4 bytes |
//                | path:  plen bytes |
//                | llen:     4 bytes |
//                | lines:
//                        | line (0)
//                                | kind:   4 bytes |
//                                | num:    4 bytes |
//                                | len:    4 bytes |
//                                | body: len bytes |
//                        | line (1) ...    |
//                        ...
//                        | line (llen) ... |
//                | cursor_r: 4 bytes |
//        | diff (2) ...    |
//        ...
//        | diff (dlen) ... |
message_t *unpackage_message(char *buf) {
  printf("unpacking message\n");

  message_t *msg = (message_t *)malloc(sizeof(message_t));

  // deserialize to message_t
  uint32_t kind;
  uint32_t dlen;

  memcpy(&kind, buf, 4);
  memcpy(&dlen, buf + 4, 4);

  msg->kind = ntohl(kind);
  msg->dlen = ntohl(dlen);

  msg->diff = (fdiff_t *)malloc(msg->dlen * sizeof(fdiff_t));

  int shift = 8;

  // deserialize to fdiff_t
  for (size_t i = 0; i < msg->dlen; i++) {
    uint32_t plen;
    uint32_t llen;

    memcpy(&plen, buf + shift, 4);
    msg->diff[i].plen = ntohl(plen);
    shift += 4;

    msg->diff[i].path = (char *)malloc(msg->diff[i].plen);
    memcpy(msg->diff[i].path, buf + shift, msg->diff[i].plen);
    shift += msg->diff[i].plen;

    memcpy(&llen, buf + shift, 4);
    msg->diff[i].llen = ntohl(llen);
    shift += 4;

    msg->diff[i].lines = (fline_t*)malloc(msg->diff[i].llen * sizeof(fline_t));

    for (size_t j = 0; j < msg->diff[i].llen; j++) {
      uint32_t num;
      uint32_t len;
      diff_kind dkind;

      memcpy(&dkind, buf + shift, 4);
      msg->diff[i].lines[i].kind = ntohl(dkind);
      shift += 4;

      memcpy(&num, buf + shift, 4);
      msg->diff[i].lines[j].num = ntohl(num);
      shift += 4;

      memcpy(&len, buf + shift, 4);
      msg->diff[i].lines[j].len = ntohl(len);
      shift += 4;

      msg->diff[i].lines[j].body = (char*)malloc(msg->diff[i].lines[j].len);
      memcpy(msg->diff[i].lines[j].body, buf + shift, msg->diff[i].lines[j].len);
      shift += msg->diff[i].lines[j].len;

      printf("diff=%zu line=%d body=%s path=%s\n", i, msg->diff[i].lines[j].num, msg->diff[i].lines[j].body, msg->diff[i].path);

    }
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

void receive_messages(int conn_d, const node_t *node) {
  // message_t buf;
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
  uint32_t total_bytes =  + MSG_HEADER_BYTES + 4 + 4;
  for (size_t i = 0; i < msg->dlen; i++) {
    total_bytes += 4 + msg->diff[i].plen + 4;
    for (size_t j = 0; j < msg->diff[i].llen; j++) {
      total_bytes += 4 + 4 + 4 + msg->diff[i].lines[j].len;
    }
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

  printf("sent %d bytes\n", sent);

  if (n < 0) {
    perror("error sending bytes");
    return -1;
  }

  return 0;
}

void on_message_received(int conn_d, message_t *msg, const node_t *node) {
  printf("message received of type %s\n", message_kind_str(msg->kind));
  switch (msg->kind) {
  case MSG_CONTENT: 
  case SERVER_HELLO: {
    message_t re_msg = {.kind = MSG_ACK};
    send_message(conn_d, &re_msg);
    break;
  }
  case CLIENT_HELLO: {
    fline_t* lines = read_file(node->file.path);
    fdiff_t diff = {.plen = strlen(node->file.path), .path = node->file.path, .llen = arrlen(lines), .lines = lines };
    message_t re_msg = {
        .kind = SERVER_HELLO,
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
