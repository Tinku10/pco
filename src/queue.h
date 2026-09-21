#ifndef QUEUE_H
#define QUEUE_H

#include "connection.h"

void qinit(message_t***);
int qput(message_t***, message_t*);
message_t* qpop(message_t***);
void qfree(message_t***);

#endif
