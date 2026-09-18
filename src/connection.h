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
#define MSG_HEADER_BYTES 4

typedef enum {
  CLIENT_HELLO,
  MSG_ACK,
  CLIENT_DISCONNECT,
  MSG_CONTENT,
  SERVER_HELLO,
} message_kind;

// attributes of file/directory
typedef struct {
  char *path;
} file_t;

typedef struct {
  uint32_t num;
  uint32_t len;
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
  fline_t* lines;
} fdiff_t;

typedef struct node {
  unsigned int id;
  // other conected socket descriptors
  int other_connected_sockets[MAX_CLIENTS];
  // listening socket of self
  int socket_d;
  unsigned int other_cnt;
  // thread on which the server is listening on
  pthread_t thread;
  // information about the file/directory
  file_t file;
} node_t;

typedef struct message {
  message_kind kind;
  // char* body;
  // size_t blen;
  fdiff_t *diff;
  size_t dlen;
} message_t;

int server_start(node_t *, int port, char *);
int client_connect(char *ip, int port);
int client_hello(int, node_t *);

// serialization
char *package_message(message_t *);
message_t *unpackage_message(char *);

// message fetch
int recv_header(int);
char *recv_message(int, int);

// send/receive handler
int send_message(int conn_d, message_t *msg);
void receive_messages(int conn_d, node_t *);

// message receive handler
void on_message_received(int conn_d, message_t *msg, node_t *);

#endif
