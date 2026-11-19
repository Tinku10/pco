#include "connection.h"
#include "errno.h"
#include "log.h"
#include "queue.h"
#include "string.h"
#include <stb/stb_ds.h>

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
    case DOCUMENT_CREATE:
      return "DOCUMENT_CREATE";
    case DOCUMENT_DELETE:
      return "DOCUMENT_DELETE";
    case DOCUMENT_UPDATE:
      return "DOCUMENT_UPDATE";
    default:
      return "UNKNOWN";
  }
}

message_kind message_kind_str_to_enum(const char *kind) {
    if (kind == NULL) {
        return UNKNOWN;
    }

    if (strcmp(kind, "MSG_CONTENT") == 0)
        return MSG_CONTENT;
    if (strcmp(kind, "MSG_ACK") == 0)
        return MSG_ACK;
    if (strcmp(kind, "CLIENT_HELLO") == 0)
        return CLIENT_HELLO;
    if (strcmp(kind, "SERVER_HELLO") == 0)
        return SERVER_HELLO;
    if (strcmp(kind, "DOCUMENT_CREATE") == 0)
        return DOCUMENT_CREATE;
    if (strcmp(kind, "DOCUMENT_DELETE") == 0)
        return DOCUMENT_DELETE;
    if (strcmp(kind, "DOCUMENT_UPDATE") == 0)
        return DOCUMENT_UPDATE;

    return UNKNOWN;
}

const char *diff_kind_str(diff_kind kind) {
  switch (kind) {
    case DIFF_ADDED:
      return "DIFF_ADDED";
    case DIFF_DELETED:
      return "DIFF_DELETED";
    case DIFF_MODIFIED:
      return "DIFF_MODIFIED";
    default:
      return "UNKNOWN";
  }
}

diff_kind diff_kind_str_to_enum(const char *kind) {
    if (kind == NULL) {
        return UNKNOWN;
    }

    if (strcmp(kind, "DIFF_ADDED") == 0)
        return DIFF_ADDED;
    if (strcmp(kind, "DIFF_DELETED") == 0)
        return DIFF_DELETED;
    if (strcmp(kind, "DIFF_MODIFIED") == 0)
        return DIFF_MODIFIED;

    return UNKNOWN;
}

// serde for range_t
int package_range_t(range_t *range, char *buf) {
  uint32_t start_pos = htonl(range->start_pos);
  uint32_t start_char = htonl(range->start_char);
  uint32_t end_pos = htonl(range->end_pos);
  uint32_t end_char = htonl(range->end_char);

  uint32_t offset = 0;
  memcpy(buf + offset, &start_pos, 4);
  offset += 4;
  memcpy(buf + offset, &start_char, 4);
  offset += 4;
  memcpy(buf + offset, &end_pos, 4);
  offset += 4;
  memcpy(buf + offset, &end_char, 4);
  offset += 4;
  return offset;
}

int unpackage_range_t(range_t *range, char *buf) {
  uint32_t offset = 0;
  memcpy(&range->start_pos, buf + offset, 4);
  offset += 4;
  memcpy(&range->start_char, buf + offset, 4);
  offset += 4;
  memcpy(&range->end_pos, buf + offset, 4);
  offset += 4;
  memcpy(&range->end_char, buf + offset, 4);
  offset += 4;

  range->start_pos = ntohl(range->start_pos);
  range->start_char = ntohl(range->start_char);
  range->end_pos = ntohl(range->end_pos);
  range->end_char = ntohl(range->end_char);
  return offset;
}

// serde for fline_t
int package_fline_t(fline_t *line, char *buf) {
  uint32_t offset = 0;
  uint32_t kind = htonl(line->kind);
  uint32_t len = htonl(line->len);

  memcpy(buf + offset, &kind, 4);
  offset += 4;
  offset += package_range_t(&line->range, buf + offset);
  
  memcpy(buf + offset, &len, 4);
  offset += 4;
  memcpy(buf + offset, line->body, line->len);
  offset += line->len;

  return offset;
}

int unpackage_fline_t(fline_t *line, char *buf) {
  uint32_t offset = 0;
  memcpy(&line->kind, buf + offset, 4);
  line->kind = ntohl(line->kind);
  offset += 4;
  
  offset += unpackage_range_t(&line->range, buf + offset);
  
  memcpy(&line->len, buf + offset, 4);
  line->len = ntohl(line->len);
  offset += 4;
  
  line->body = (char *)malloc(line->len + 1);
  memcpy(line->body, buf + offset, line->len);
  line->body[line->len] = '\0';
  offset += line->len;

  return offset;
}

// serde for diff_t
int package_fdiff_t(fdiff_t *diff, char *buf) {
  uint32_t offset = 0;
  int32_t plen = htonl(diff->plen);
  int32_t cursor = htonl(diff->cursor_r);
  int32_t llen = htonl(diff->llen);

  memcpy(buf + offset, &plen, 4);
  offset += 4;
  memcpy(buf + offset, diff->path, diff->plen);
  offset += diff->plen;
  memcpy(buf + offset, &cursor, 4);
  offset += 4;
  memcpy(buf + offset, &llen, 4);
  offset += 4;
  for (int i = 0; i < diff->llen; i++) {
    offset += package_fline_t(diff->lines + i, buf + offset);
  }

  return offset;
}

int unpackage_fdiff_t(fdiff_t *diff, char *buf) {
  uint32_t offset = 0;

  memcpy(&diff->plen, buf + offset, 4);
  diff->plen = ntohl(diff->plen);
  offset += 4;

  diff->path = (char *)malloc(diff->plen + 1);
  memcpy(diff->path, buf + offset, diff->plen);
  diff->path[diff->plen] = '\0';
  offset += diff->plen;

  memcpy(&diff->cursor_r, buf + offset, 4);
  diff->cursor_r = ntohl(diff->cursor_r);
  offset += 4;

  memcpy(&diff->llen, buf + offset, 4);
  diff->llen = ntohl(diff->llen);
  offset += 4;

  diff->lines = (fline_t *)malloc(sizeof(fline_t) * diff->llen);
  for (int i = 0; i < diff->llen; i++) {
    offset += unpackage_fline_t(diff->lines + i, buf + offset);
  }

  return offset;
}

// structure of a message_t
// | header:          4 bytes |
// |==========================|
// | kind:            4 bytes |
// | doc_size:        4 bytes |
// | crdt:     doc_size bytes |
// | req_size:        4 bytes |
// | req_file: req_size bytes |

char *package_message(message_t *msg) {
  dbg_print("packing message\n");

  char *buffer;
  int bytes_serialised = serialise_document(msg->doc, &buffer);
  if (bytes_serialised < 0) {
    free(buffer);
    fprintf(stderr, "error serialising automerge document");
    return NULL;
  }
  const uint32_t total_bytes = 4 + 4 + bytes_serialised + 4 + msg->req_size; // excluding header
  char *buf = (char *)malloc(MSG_HEADER_BYTES + total_bytes);

  dbg_print("message size %d", total_bytes);

  uint32_t total = htonl(total_bytes);
  uint32_t kind = htonl(msg->kind);
  uint32_t doc_size = htonl(bytes_serialised);
  uint32_t req_size = htonl(msg->req_size);

  dbg_print("header %d", total_bytes);

  memcpy(buf, &total, MSG_HEADER_BYTES);
  memcpy(buf + MSG_HEADER_BYTES, &kind, 4);
  memcpy(buf + MSG_HEADER_BYTES + 4, &doc_size, 4);
  memcpy(buf + MSG_HEADER_BYTES + 4 + 4, buffer, bytes_serialised);
  memcpy(buf + MSG_HEADER_BYTES + 4 + 4 + bytes_serialised, &req_size, 4);
  memcpy(buf + MSG_HEADER_BYTES + 4 + 4 + bytes_serialised + 4, &msg->req_file, msg->req_size);

  dbg_print("packing complete\n");

  return buf;
}

// call this method skipping the first 4 bytes of the Header
message_t *unpackage_message(char *buf, AMdoc *doc) {
  dbg_print("unpacking message\n");

  message_t *msg = (message_t *)malloc(sizeof(message_t));

  uint32_t kind;
  memcpy(&kind, buf, 4);
  msg->kind = ntohl(kind);

  uint32_t doc_size;
  memcpy(&doc_size, buf + 4, 4);
  doc_size = ntohl(doc_size);

  if (deserialise_document(doc, buf + 4 + 4, doc_size) < 0) {
    fprintf(stderr, "error deserialising automerge document");
    return NULL;
  }

  uint32_t req_size;
  memcpy(&req_size, buf + 4 + 4 + doc_size, 4);
  msg->req_size = ntohl(req_size);

  char* req_file = (char*) malloc(msg->req_size + 1);
  memcpy(&req_file, buf + 4 + 4 + doc_size + 4, msg->req_size);
  req_file[msg->req_size] = '\0';

  msg->req_file = req_file;

  return msg;
}

int recv_header(int conn_d) {
  // header is of four bytes
  size_t received = 0;
  ssize_t n = 0;

  uint8_t buf[MSG_HEADER_BYTES] = {0, 0, 0, 0};

  dbg_print("checking for %d bytes header\n", MSG_HEADER_BYTES);

  while (received < MSG_HEADER_BYTES) {
    dbg_print("conn_d=%d buf=%p received=%zu len=%zu\n", conn_d, (void *)buf,
              received, MSG_HEADER_BYTES - received);
    n = recv(conn_d, buf + received, MSG_HEADER_BYTES - received, 0);
    if (n < 0) {
      perror("problem in receiving bytes");
      return -1;
    }

    if (n == 0) {
      fprintf(stderr, "peer closed connection\n");
      return -1;
    }

    received += n;
    dbg_print("read %d bytes\n", n);
  }

  uint32_t header;
  memcpy(&header, buf, sizeof(header));
  header = ntohl(header);

  dbg_print("header: %d\n", header);

  return header;
}

char *recv_message(int conn_d, int req) {
  int received = 0;
  int n = 0;

  dbg_print("checking for %d bytes message body\n", req);

  char *buf = (char *)malloc(req);

  while (received < req) {
    n = recv(conn_d, buf + received, req - received, 0);
    if (n < 0) {
      perror("problem in receiving bytes");
      return NULL;
    }

    if (n == 0) {
      fprintf(stderr, "peer closed connection\n");
      return NULL;
    }

    received += n;
    dbg_print("read %d bytes\n", n);
  }

  return buf;
}

void receive_messages(int conn_d, const node_t *node) {
  // message_t buf;
  dbg_print("waiting on data\n");

  int32_t header;
  while ((header = recv_header(conn_d)) > 0) {
    char *buf = recv_message(conn_d, header);
    if (buf == NULL)
      break;
    
    message_t *msg = unpackage_message(buf, node->doc);
    on_message_received(conn_d, msg, node);
  }

  dbg_print("closing connection on %d", conn_d);

  close(conn_d);
}

int send_message(int conn_d, message_t *msg) {
  dbg_print("sending message %s\n", message_kind_str(msg->kind));

  char *buf = package_message(msg);
  uint32_t total_bytes_excluding_header;
  memcpy(&total_bytes_excluding_header, buf, 4);

  const uint32_t total_bytes = MSG_HEADER_BYTES + ntohl(total_bytes_excluding_header);

  dbg_print("sending message %d", total_bytes);

  int sent = 0;
  int n = 0;

  while (sent < total_bytes) {
    n = send(conn_d, buf + sent, total_bytes - sent, 0);
    if (n < 0) {
      if (errno == EINTR)
        continue;

      perror("unable to send message");
      free(buf);
      return -1;
    }

    if (n == 0) {
      fprintf(stderr, "send failed\n");
      free(buf);
      return -1;
    }

    sent += n;
  }

  dbg_print("sent %d bytes\n", sent);

  free(buf);
  return 0;
}

void on_message_received(int conn_d, message_t *msg, const node_t *node) {
  dbg_print("message received of type %s\n", message_kind_str(msg->kind));
  switch (msg->kind) {
  case MSG_CONTENT:
  case SERVER_HELLO: {
    message_t re_msg = {.kind = MSG_ACK, .doc = NULL};
    send_message(conn_d, &re_msg);

    uint8_t byte = 1;
    qput(&node->inbox, msg);
    if (write(node->event[1], &byte, 1) < 0) {
      perror("failed to write wake up");
      return;
    }
    break;
  }
  case CLIENT_HELLO: {
    message_t re_msg = {
      .kind = SERVER_HELLO,
      .doc = node->doc
    };

    send_message(conn_d, &re_msg);

    uint8_t byte = 1;
    qput(&node->inbox, msg);
    if (write(node->event[1], &byte, 1) < 0) {
      perror("failed to write wake up");
      return;
    }

    break;
  }
  case FILE_REQUEST: {
    message_t re_msg = {.kind = FILE_RESPONSE, .doc = node->doc};
    send_message(conn_d, &re_msg);

    uint8_t byte = 1;
    qput(&node->inbox, msg);
    if (write(node->event[1], &byte, 1) < 0) {
      perror("failed to write wake up");
      return;
    }

    break;
  }
  default:
    break;
  }
}
