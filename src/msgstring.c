/* Message string driver for message stringstructs */
/* by Mark Crispin */
/* changes by Tomas Pospisek */

#include <stdio.h>      // required by c-client.h
#include <c-client.h>
#include "msgstring.h"

STRINGDRIVER msg_string = {
  msg_string_init,		/* initialize string structure */
  msg_string_next,		/* get next byte in string structure */
  msg_string_setpos		/* set position in string structure */
};

void msg_string_init (STRING *s,void *data,unsigned long size)
{
  MSGDATA *md = (MSGDATA *) data;
  s->data = data;		/* note stream/msgno and header length */
  mail_fetchheader_full (md->stream,md->msgno,NIL,&s->data1,FT_PREFETCHTEXT);
#if 0
  s->size = size;		/* message size */
#else	/* This kludge is necessary because of broken IMAP servers (sigh!) */
  mail_fetchtext_full (md->stream,md->msgno,&s->size,FT_PEEK);
                                /* FT_PEEK - we don't want to modify read
                                 * messages in _any_ way */
  s->size += s->data1;		/* header + body size */
#endif
  SETPOS (s,0);
}

char msg_string_next (STRING *s)
{
  char c = *s->curpos++;	/* get next byte */
  SETPOS (s,GETPOS (s));	/* move to next chunk */
  return c;			/* return the byte */
}

void msg_string_setpos (STRING *s,unsigned long i)
{
  MSGDATA *md = (MSGDATA *) s->data;
  if (i < s->data1) {		/* want header? */
    s->chunk = mail_fetchheader (md->stream,md->msgno);
    s->chunksize = s->data1;	/* header length */
    s->offset = 0;		/* offset is start of message */
  }
  else if (i < s->size) {	/* want body */
    s->chunk = mail_fetchtext_full (md->stream,md->msgno,NIL,FT_PEEK);
                                /* FT_PEEK - see above */
    //s->chunk = mail_fetchtext (md->stream,md->msgno);
    s->chunksize = s->size - s->data1;
    s->offset = s->data1;	/* offset is end of header */
  }
  else {			/* off end of message */
    s->chunk = NIL;		/* make sure that we crack on this then */
    s->chunksize = 1;		/* make sure SNX cracks the right way... */
    s->offset = i;
  }
				/* initial position and size */
  s->curpos = s->chunk + (i -= s->offset);
  s->cursize = s->chunksize - i;
}


