//-------------------------- Callback functions --------------------------
//
// Below are the implementations callback functions that are required/called
// by c-client functions.
//
// If we want to manipulate a stream or a mailbox we call a c-client
// function which does whatever is necessary and will furnish us the
// results piece by piece as the come by calling the respective callback
// function.
//
// The names of the callback functions are fixed and can not be changed.
//
//////////////////////////////////////////////////////////////////////////

#include <stdio.h>      // required by c-client.h
#include <c-client.h>

#include "options.h"
#include "types.h"
#include "store.h"

extern options_t options;
extern Store*      match_pattern_store;
extern Passwd*     current_context_passwd;

// Flag saying in critical code
int critical = NIL;

//////////////////////////////////////////////////////////////////////////
//
void mm_list ( MAILSTREAM *stream, int delimiter, char *name_nc,
               long attributes)
//
// called by c-client's mail_list to give us a mailbox name that matches
// our search pattern together with it's attributes 
//
// mm_list will save the mailbox names inside the store in the boxes
// MailboxMap
//
//////////////////////////////////////////////////////////////////////////
{
  const char* name = name_nc;
  MailboxProperties mailbox_properties;

  if( options.debug) {
    fputs ("  ", stdout);
    if (stream && stream->mailbox)
      fputs (stream->mailbox, stdout);
    if (name) fputs(name, stdout);
    if (attributes & LATT_NOINFERIORS) fputs (", no inferiors",stdout);
    if (attributes & LATT_NOSELECT) fputs (", no select",stdout);
    if (attributes & LATT_MARKED) fputs (", marked",stdout);
    if (attributes & LATT_UNMARKED) fputs (", unmarked",stdout);
    putchar ('\n');
  }

  if ( attributes & LATT_NOINFERIORS )
    mailbox_properties.no_inferiors = true;
  if ( attributes & LATT_NOSELECT )
    mailbox_properties.no_select = true;
  
  if (!match_pattern_store) {
    fprintf(stderr, "Error: match_pattern_store is NULL?!");
    // Internal error
    abort();
  }

  // Delimiter
  match_pattern_store->delim = delimiter;

  if ( *name) { // TODO: is this correct?
    const char *skip;

    // Skip over server spec, if present
    skip = strchr(name,'}');
    if (skip)
      name = skip+1;

    // Remove prefix if it matches specified prefix
    skip = match_pattern_store->prefix.c_str();
    while (*skip && *skip==*name) {
      skip++;
      name++;
    }

    // Make sure that name doesn't contain default delimiter and replace
    // all occurences of the Store delimiter by our default delimiter
    string name_copy = name;
    if (delimiter != DEFAULT_DELIMITER) {
      for (int i = 0; name[i]; i++) {
        if (name_copy[i] == DEFAULT_DELIMITER) {
          printf("Error: Name of mailbox %s contains standard delimiter '%c'\n",
                 name, DEFAULT_DELIMITER);
          printf("       That means that I will not be able to synchronize it!");
          return;
        } else if (name_copy[i] == (char)delimiter)
          name_copy[i] = DEFAULT_DELIMITER;
      }
    }
    match_pattern_store->boxes[name_copy] = mailbox_properties;
  }
  else if (options.debug)
    fprintf( stderr, "Received empty name while listing contents\n");
}

//////////////////////////////////////////////////////////////////////////
//
void mm_searched (MAILSTREAM *stream,unsigned long msgno) { }
//
//////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////
//
void mm_exists (MAILSTREAM *stream,unsigned long number) { }
//
// c-client callback that notifies us, that the number of
// messages has changed.
//
//////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////
//
void mm_expunged (MAILSTREAM *stream,unsigned long number) { }
//
//////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////
//
void mm_flags (MAILSTREAM *stream,unsigned long number) { }
//
//////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////
//
void mm_lsub (MAILSTREAM *stream,int delimiter,char *name,long attributes) { }
//
//////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////
//
void mm_status (MAILSTREAM *stream,char *mailbox,MAILSTATUS *status) { }
//
//////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////
//
void mm_notify (MAILSTREAM *stream,char *s,long errflg)
//
//////////////////////////////////////////////////////////////////////////
{
  mm_log (s,errflg);    // Just do mm_log action
}

//////////////////////////////////////////////////////////////////////////
//
void mm_log (char *string,long errflg)
//
//////////////////////////////////////////////////////////////////////////
{
  switch (errflg) {  
    case BYE:
    case NIL:                     // No error
      if (options.log_chatter)
        fprintf (stderr,"[%s]\n",string);
      break;
    case PARSE:                   // Parsing problem
      if (options.log_parse) 
        fprintf (stderr,"Parsing error: %%%s\n",string);
      break;
    case WARN:                    // Warning
      if (options.log_warn) 
        fprintf (stderr,"Warning: %%%s\n",string);
      break;
    case ERROR:                   // Error
    default:
      if (options.log_error)
        fprintf (stderr,"Error: %s\n",string);
      break;
  }
}

//////////////////////////////////////////////////////////////////////////
//
void mm_dlog (char *s)
//
//////////////////////////////////////////////////////////////////////////
{
  fprintf (stderr,"%s\n",s);
}

//////////////////////////////////////////////////////////////////////////
//
void mm_login (NETMBX *mb,char *username,char *password,long trial)
//
//////////////////////////////////////////////////////////////////////////
{
  printf ("Authorizing against {%s/%s}\n",mb->host,mb->service);
  if (*mb->user) strcpy (username,mb->user);
  else {
    printf (" username: ");
    fgets (username, 30, stdin);
  }
  if( current_context_passwd == NULL || current_context_passwd->nopasswd) {
    strcpy (password,getpass (" password: "));
  } else {
    strcpy (password, current_context_passwd->text.c_str());
  }
}

//////////////////////////////////////////////////////////////////////////
//
void mm_critical (MAILSTREAM *stream)
//
//////////////////////////////////////////////////////////////////////////
{
  critical = T;                 // Note in critical code
}

//////////////////////////////////////////////////////////////////////////
//
void mm_nocritical (MAILSTREAM *stream)
//
//////////////////////////////////////////////////////////////////////////
{
  critical = NIL;               // Note not in critical code
}

//////////////////////////////////////////////////////////////////////////
//
long mm_diskerror (MAILSTREAM *stream,long errcode,long serious)
//
// TODO: disk error occured - have a look what pine does
// 
//////////////////////////////////////////////////////////////////////////
{
  mm_log( strerror(errcode), ERROR);
  return T;
}

//////////////////////////////////////////////////////////////////////////
//
void mm_fatal (char *st)
//
// Called by c-client just before crash
//
// TODO: we should cleanly terminate here :-/
//
//////////////////////////////////////////////////////////////////////////
{
  mm_log( st, ERROR);
  fprintf (stderr,"A fatal error occured - terminating\n");
  // exit(-1);
}
