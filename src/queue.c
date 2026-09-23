#include "queue.h"
#include <stb/stb_ds.h>
#include "log.h"

void qinit(message_t ***q) {
  *q = NULL; 
}

int qput(message_t ***q, message_t *msg) {
  // add mutex around write?
  arrpush(*q, msg);
  dbg_print("message pushed, new size %d", arrlen(*q));
  return 0;
}

message_t *qpop(message_t ***q) {
  if (arrlen(*q) == 0)
    return NULL;
  return arrpop(*q);
}

void qfree(message_t ***q) { arrfree(*q); }
