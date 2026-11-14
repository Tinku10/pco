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

char *path_relative_from_base(char *base_path, char *file_path) {
  uint32_t base_sz = strlen(base_path);
  uint32_t curr_sz = strlen(file_path);
  char *path_from_base = (char *)malloc(curr_sz - base_sz);
  // extra 1 to exclude the / from base
  memcpy(path_from_base, file_path + base_sz + 1, curr_sz - base_sz - 1);
  path_from_base[curr_sz - base_sz - 1] = '\0';
  return path_from_base;
}

int is_file_ignored(char *base_path, char* file_path, char** ignore_list) {
  if (ignore_list == NULL)
    return 0;

  char *path_from_base = path_relative_from_base(base_path, file_path);

  for (int i = 0; i < arrlen(ignore_list); i++) {
    if (fnmatch(ignore_list[i], path_from_base, 0) == 0) {
      free(path_from_base);
      return 1;
    }
  }
  free(path_from_base);
  return 0;
}

char **read_dir_layout(char *base_path, char *file_path, char **ignored_files) {
  char** files = NULL;

  DIR *dir = opendir(file_path);
  if (!dir) {
    return files;
  }

  struct dirent *entry;
  

  while ((entry = readdir(dir)) != NULL) {
    if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
      continue;
    }
    
    char path[1024];
    snprintf(path, 1024, "%s/%s", file_path, entry->d_name);
    if (is_file_ignored(base_path, path, ignored_files)) continue;

    struct stat statbuf;
    if (stat(path, &statbuf) == 0 && S_ISDIR(statbuf.st_mode)) {
      char **nested_files = read_dir_layout(base_path, path, ignored_files);
      for (int i = 0; i < arrlen(nested_files); i++) {
        arrpush(files, nested_files[i]);
      }
    } else {
      arrpush(files, path_relative_from_base(base_path, path));
    }
  }

  closedir(dir);

  return files;
}
