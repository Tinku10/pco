#include "fileio.h"
#include "log.h"
#include <dirent.h>
#include <errno.h>
#include <fnmatch.h>
#include <stb/stb_ds.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <uuid/uuid.h>

#define MAX_PATH_LENGTH 1024

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

int is_file_ignored(char *base_path, char *file_path, char **ignore_list) {
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
  char **files = NULL;

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
    if (is_file_ignored(base_path, path, ignored_files))
      continue;

    struct stat statbuf;
    if (stat(path, &statbuf) == 0 && S_ISDIR(statbuf.st_mode)) {
      char **nested_files = read_dir_layout(base_path, path, ignored_files);
      for (int i = 0; i < arrlen(nested_files); i++) {
        arrpush(files, strdup(nested_files[i]));
        free(nested_files[i]);
      }
      arrfree(nested_files);
    } else {
      arrpush(files, path_relative_from_base(base_path, path));
    }
  }

  closedir(dir);

  return files;
}

int create_path(const char *path) {
  char *start = path;
  char *end;

  char curr_path[MAX_PATH_LENGTH];
  size_t curr_path_len = 0;

  while ((end = strchr(start, '/')) != NULL) {
    size_t len = end - start;
    memcpy(curr_path + curr_path_len, start, len);
    curr_path_len += len;
    curr_path[curr_path_len++] = '/';
    curr_path[curr_path_len] = '\0';
    start = end + 1;

    if (mkdir(curr_path, 0755) < 0) {
      // handle file exists error
      if (errno != EEXIST) {
        return -1;
      }
    }
  }

  if ((end = strchr(start, '\0')) != NULL) {
    size_t len = end - start;
    memcpy(curr_path + curr_path_len, start, len);
    curr_path_len += len;
    curr_path[curr_path_len] = '\0';

    FILE *file = fopen(curr_path, "w");

    if (file == NULL) {
      perror("failed during directory layout");
      return -1;
    }
    start = end + 1;
    fclose(file);
  }

  return 0;
}

char *create_dir_layout(char **files) {
  // TODO: handle OS level tmp paths
  char *tmp_path = "/tmp";

  uuid_t uuid;
  uuid_generate(&uuid);
  char uuid_str[100];

  uuid_unparse(uuid, uuid_str);

  dbg_print("session id [%s]\n", uuid_str);

  for (int i = 0; i < arrlen(files); i++) {
    char new_path[MAX_PATH_LENGTH];
    snprintf(new_path, MAX_PATH_LENGTH, "%s/pco-%s/%s", tmp_path, uuid_str,
             files[i]);
    if (create_path(new_path) < 0) {
      return NULL;
    }
  }

  char *base_path = (char *)malloc(MAX_PATH_LENGTH);
  snprintf(base_path, MAX_PATH_LENGTH, "%s/pco-%s", tmp_path, uuid_str);
  return base_path;
}
