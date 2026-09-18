// #include "connection.h"
#define STB_DS_IMPLEMENTATION
#include <stdio.h>
#include <stb_ds.h>
#include <string.h>
#include "fileio.h"

fline_t* read_file(char* path) {
  printf("reading file %s\n", path);
  FILE* fd = fopen(path, "r");
  if (fd == NULL) {
    perror("cannot open file");
    return NULL;
  }

  char* line = NULL;
  size_t len = 0;
  ssize_t read;

  fline_t* lines = NULL;

  int lnum = 1;

  while ((read = getline(&line, &len, fd)) != -1) {
    char *dline = (char*)malloc(read + 1); 
    strcpy(dline, line);
    fline_t curr_line = { .len = read, .num = lnum++, .body = dline, .kind = DIFF_ADDED};
    arrput(lines, curr_line);
  }

  // for (int i = 0; i < lnum; i++) {
  //   printf("%d %d %s", lines[i].num, lines[i].len, lines[i].body);
  // }

  free(line);
  fclose(fd);

  return lines;
}
