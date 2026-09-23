#include "connection.h"
#include <getopt.h>
#include "diff.h"

int main(int argc, char **argv) {
  setbuf(stdout, NULL);

  int opt;
  int sport;
  int dport;
  char *host = NULL;
  char *path = NULL;

  struct option flags[] = {
      {"sport", required_argument, NULL, 's'},
      {"dhost", optional_argument, NULL, 'h'},
      {"dport", optional_argument, NULL, 'd'},
      {"path", optional_argument, NULL, 'p'},
      {0, 0, 0, 0},
  };

  while ((opt = getopt_long(argc, argv, "s:h:d:p:", flags, NULL)) != -1) {
    switch (opt) {
    case 's':
      sport = atoi(optarg);
      break;
    case 'h':
      host = optarg;
      break;
    case 'd':
      dport = atoi(optarg);
      break;
    case 'p':
      path = optarg;
      break;
    default:
      fprintf(stderr,
              "usage: %s -s srcport [-h dsthost] [-d dstport] [-p path]",
              "pco");
      exit(EXIT_FAILURE);
    }
  }

  node_t s;

  if (host != NULL) {
    // join a node
    // int sd = client_connect(host, dport);
    client_listen(&s, host, dport);
    client_hello(&s);
    fdiff_t* d = get_buffer_diff("abc", "ab");
    message_t msg = { .kind = MSG_CONTENT, .diff = d, .dlen = 1};
    send_message(s.socket_d, &msg);

    pthread_join(s.cthread, NULL);
  } else {
    // start a server that others can join
    server_start(&s, sport, path);
    pthread_join(s.sthread, NULL);
  }

  return 0;
}
