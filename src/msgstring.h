#ifndef __MSGSTRING__
/* Message string driver for message stringstructs */
/* by Mark Crispin */

void msg_string_init (STRING *s,void *data,unsigned long size);
char msg_string_next (STRING *s);
void msg_string_setpos (STRING *s,unsigned long i);
typedef struct msg_data {
  MAILSTREAM *stream;		/* stream */
  long msgno;			/* message number */
} MSGDATA;

extern STRINGDRIVER msg_string;

#define __MSGSTRING__
#endif 
