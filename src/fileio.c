#include "fileio.h"
#include "log.h"
#include <stb/stb_ds.h>
#include <stdio.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <fnmatch.h>

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

int is_file_ignored(char* file_path, char** ignore_list) {
  if (ignore_list == NULL)
    return 0;

  for (int i = 0; i < arrlen(ignore_list); i++) {
    if (fnmatch(ignore_list[i], file_path, 0) == 0) {
      return 1;
    }
  }

  return 0;
}

char **read_dir_layout(char *base_path) {
  char** files = NULL;

  char pcoignore_path[1024];
  snprintf(pcoignore_path, sizeof(pcoignore_path), "%s/%s", base_path, ".pcoignore");

  char** ignored_files = read_file(pcoignore_path);

  DIR *dir = opendir(base_path);
  if (!dir) {
    return files;
  }

  struct dirent *entry;
  char path[1024];

  while ((entry = readdir(dir)) != NULL) {
    if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
      continue;
    }

    snprintf(path, sizeof(path), "%s/%s", base_path, entry->d_name);

    struct stat statbuf;
    if (stat(path, &statbuf) == 0 && S_ISDIR(statbuf.st_mode)) {
      read_dir_layout(path);
    } else {
      if (is_file_ignored(path, ignored_files) == 0)
        arrpush(files, path);
    }
  }

  closedir(dir);

  return files;
}
