#include "fileio.h"
#include "log.h"
#include <stb/stb_ds.h>
#include <stdio.h>
#include <string.h>

char **read_file(char *path) {
  dbg_print("reading file %s\n", path);
  FILE *fd = fopen(path, "r");
  if (fd == NULL) {
    perror("cannot open file");
    return NULL;
  }

  char *line = NULL;
  size_t len = 0;
  ssize_t read;

  char **lines = NULL;

  while ((read = getline(&line, &len, fd)) != -1) {
    // keep lines without trailing newline characters
    if (line[read - 1] == '\n') {
      line[read - 1] = '\0';
      read--;
    }
    char *dline = (char *)malloc(read + 1);
    strcpy(dline, line);
    arrput(lines, dline);
  }

  free(line);
  fclose(fd);

  return lines;
}
