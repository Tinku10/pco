#ifndef CONNECTION_H
#define CONNECTION_H

#include <arpa/inet.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#define MAX_CLIENTS 1
#define LISTEN_BACKLOG 10
#define MSG_HEADER_BYTES 4

typedef enum {
  CLIENT_HELLO,
  MSG_ACK,
  CLIENT_DISCONNECT,
  MSG_CONTENT,
  SERVER_HELLO,
  DOCUMENT_CREATE,
  DOCUMENT_DELETE,
  DOCUMENT_UPDATE,
  UNKNOWN
} message_kind;

typedef enum {
  DIFF_ADDED,
  DIFF_MODIFIED,
  DIFF_DELETED,
} diff_kind;

// attributes of file/directory
typedef struct {
  char *path;
} file_t;

// indexes are 0 based.
// Normalise it if text editor's default is 1 based
typedef struct {
  uint32_t start_pos;
  uint32_t start_char;
  uint32_t end_pos;
  uint32_t end_char;
} range_t;

typedef struct {
  diff_kind kind;
  uint32_t num;
  uint32_t len;
  range_t range;
  char *body;
} fline_t;

typedef struct {
  // file path
  char *path;
  uint32_t plen;
  // cursor position
  uint32_t cursor_r;
  // body
  // char *body;
  uint32_t llen;
  fline_t *lines;
} fdiff_t;

typedef enum {
  NODE_ADMIN,
  NODE_CONTRIBUTOR,
} node_kind;


typedef struct message {
  message_kind kind;
  // char* body;
  // size_t blen;
  fdiff_t *diff;
  size_t dlen;
} message_t;

typedef struct node {
  node_kind kind;
  unsigned int id;
  // other conected socket descriptors
  int connected_sockets[MAX_CLIENTS];
  // listening socket of self
  int socket_d;
  unsigned int connections;
  // thread on which the server is listening on
  pthread_t sthread;
  // thread on which the client is listening on
  pthread_t cthread;
  // information about the file/directory
  file_t file;
  // queue to put received messages
  message_t** inbox;
  // pipe to send event on data available
  int event[2];
} node_t;

const char *message_kind_str(message_kind);
const char *diff_kind_str(diff_kind);
message_kind message_kind_str_to_enum(const char *);
diff_kind diff_kind_str_to_enum(const char *);

int server_start(node_t *, int port, char *);
int client_connect(char *ip, int port);
int client_hello(const node_t *);
// int client_listen(int socket, node_t *node);
int client_listen(node_t*, char* ip, int port);

// serialization
char *package_message(message_t *);
message_t *unpackage_message(char *);

int package_range_t(range_t *, char *);
int unpackage_range_t(range_t *, char *);

int package_fline_t(fline_t *, char *);
int unpackage_fline_t(fline_t *, char *);

int package_fdiff_t(fdiff_t *, char *);
int unpackage_fdiff_t(fdiff_t *, char *);


// message fetch
int recv_header(int);
char *recv_message(int, int);

// send/receive handler
int send_message(int conn_d, message_t *msg);
void receive_messages(int conn_d, const node_t *);

// message receive handler
void on_message_received(int conn_d, message_t *msg, const node_t *);

#endif
