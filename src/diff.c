#include "diff.h"
#include "connection.h"
#include <stb/stb_ds.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define MIN(x, y) (x > y ? y : x)

fdiff_t *diff_files(file_state *previous_state, file_state *current_state) {
  fdiff_t *diffs = (fdiff_t *)malloc(sizeof(fdiff_t));
  diffs->llen = 0;
  diffs->lines = NULL;
  diffs->path = "";
  diffs->plen = 0;
  for (int i = 0; i < MIN(previous_state->len, current_state->len); i++) {
    size_t p_i_len = (previous_state->lines + i)->len;
    size_t c_i_len = (current_state->lines + i)->len;

    if (p_i_len != c_i_len) {
      char *line_copy = (char *)malloc(sizeof(char) * c_i_len + 1);
      strcpy(line_copy, (current_state->lines + i)->content);
      fline_t d = {.num = i,
                   .body = line_copy,
                   .kind = DIFF_MODIFIED,
                   .len = (current_state->lines + i)->len};
      line_copy[c_i_len] = '\0';
      arrput(diffs->lines, d);
      diffs->llen++;
    } else {
      for (int j = 0; j < p_i_len; j++) {
        if (*((previous_state->lines + i)->content + j) !=
            *((current_state->lines + i)->content + j)) {
          char *line_copy = (char *)malloc(sizeof(char) * c_i_len + 1);
          strcpy(line_copy, (current_state->lines + i)->content);
          line_copy[c_i_len] = '\0';
          fline_t d = {.num = i,
                       .body = line_copy,
                       .kind = DIFF_MODIFIED,
                       .len = (current_state->lines + i)->len};
          arrput(diffs->lines, d);
          diffs->llen++;
          break;
        }
      }
    }
  }

  // extra lines in current state
  for (int i = previous_state->len; i < current_state->len; i++) {
    char *line_copy =
        (char *)malloc(sizeof(char) * (current_state->lines + i)->len + 1);
    strcpy(line_copy, (current_state->lines + i)->content);
    line_copy[(current_state->lines + i)->len] = '\0';
    fline_t d = {.num = i,
                 .body = line_copy,
                 .kind = DIFF_ADDED,
                 .len = (current_state->lines + i)->len};
    arrput(diffs->lines, d);
    diffs->llen++;
  }
  // lines deleted from previous state
  for (int i = current_state->len; i < previous_state->len; i++) {
    fline_t d = {.num = i, .body = NULL, .kind = DIFF_DELETED, .len = 0};
    arrput(diffs->lines, d);
    diffs->llen++;
  }
  return diffs;
}

file_state *wrap_buffer(char *buffer) {
  file_state *state = (file_state *)malloc(sizeof(file_state));
  state->lines = NULL;
  state->len = 0;
  char* start = buffer;
  char* end;

  while ((end = strchr(start, '\n')) != NULL) {
    size_t m_len = end - start;
    char *m_content = (char *)malloc(sizeof(char) * m_len + 1);
    memcpy(m_content, start, m_len);
    m_content[m_len] = '\0';
    file_line m_line = {.content =  m_content, .len = m_len};
    arrput(state->lines, m_line);
    state->len++;
    start = end + 1;
  }
  
  if ((end = strchr(start, '\0')) != NULL) {
    size_t m_len = end - start;
    char *m_content = (char *)malloc(sizeof(char) * m_len + 1);
    memcpy(m_content, start, m_len);
    m_content[m_len] = '\0';
    file_line m_line = {.content = m_content, .len = m_len};
    arrput(state->lines, m_line);
    state->len++;
    start = end + 1;
  }
  return state;
}

fdiff_t *get_buffer_diff(char *buffer_previous, char *buffer_current) {
  file_state *previous_state = wrap_buffer(buffer_previous);
  file_state *current_state = wrap_buffer(buffer_current);

  fdiff_t *diff = diff_files(previous_state, current_state);

  // free state buffers
  // arrfree(previous_state->lines);
  // arrfree(current_state->lines);
  // free(previous_state);
  // free(current_state);

  return diff;
}
