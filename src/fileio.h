#ifndef FILEIO_H
#define FILEIO_H

char **read_file(char *path);

char** read_dir_layout(char* base_path, char *file_path, char **ignored_files);
char *create_dir_layout(char **files);

char *path_relative_from_base(char *base_path, char *file_path);

#endif
