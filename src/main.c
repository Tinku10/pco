#define STB_DS_IMPLEMENTATION

#include "connection.h"
#include <getopt.h>

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
    int sd = client_connect(host, dport);
    client_hello(sd, &s);
  } else {
    // start a server that others can join
    server_start(&s, sport, path);
  }

  pthread_join(s.thread, NULL);

  return 0;
}
