#include "connection.h"
#include "uv.h"

int main(int argc, char **argv) {

  // start a server that others can join
  node_t s;

  switch (argc) {
  case 3: {
    char *path = argv[2];
    server_start(&s, atoi(argv[1]), path);
    break;
  }

  // join a node
  case 4: {
    int sd = client_connect(argv[2], atoi(argv[3]));
    client_hello(sd, &s);
  }

  default:
    perror("invalid command\n");
  }

  pthread_join(s.thread, NULL);

    // uv_loop_t *loop = uv_default_loop();

    // printf("Default loop.\n");
    // uv_run(loop, UV_RUN_DEFAULT);

    // uv_loop_close(loop);
    return 0;
}
