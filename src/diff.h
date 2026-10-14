#ifndef DIFF_H
#define DIFF_H
#include "connection.h"

typedef struct {
    char* content; // content of ith line
    size_t len;    // length of ith line
} file_line;

typedef struct {
    file_line* lines; // array of lines
    size_t len;       // length of array
} file_state;

fdiff_t* diff_files(file_state*, file_state*);
file_state *wrap_buffer(char*);
fdiff_t* get_buffer_diff(char*, char*, char*);
#endif
