// #include "connection.h"
#include <stdio.h>
#include <stb/stb_ds.h>
#include <string.h>
#include "fileio.h"
#include "log.h"

fline_t* read_file(char* path) {
  dbg_print("reading file %s\n", path);
  FILE* fd = fopen(path, "r");
  if (fd == NULL) {
    perror("cannot open file");
    return NULL;
  }

  char* line = NULL;
  size_t len = 0;
  ssize_t read;

  fline_t* lines = NULL;

  int lnum = 0;

  while ((read = getline(&line, &len, fd)) != -1) {
    // keep lines without trailing newline characters
    if (line[read - 1] == '\n') {
      line[read - 1] = '\0';
      read--;
    }
    char *dline = (char*)malloc(read + 1); 
    strcpy(dline, line);
    fline_t curr_line = { .len = read, .num = lnum++, .body = dline};
    arrput(lines, curr_line);
  }

  // for (int i = 0; i < lnum; i++) {
  //   printf("%d %d %s", lines[i].num, lines[i].len, lines[i].body);
  // }

  free(line);
  fclose(fd);

  return lines;
}
