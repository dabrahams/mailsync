#include <stdio.h>  // required by c-client.h

#include "c-client.h"
#include "options.h"
#include "types.h"
#include "msgstring.h"
#include "utils.h"
#include "flstring.h"

extern options_t options;
extern Passwd*     current_context_passwd;

//------------------------- Helper functions -----------------------------


//////////////////////////////////////////////////////////////////////////
//
void print_list_with_delimiter( const MsgIdSet& msgIds,
                                FILE* f,
                                const string& delim)
//
// Print the set of strings contained in "msgIds" and terminate each msgId
// with the delimiter "delim"
// 
//////////////////////////////////////////////////////////////////////////
{
    for ( MsgIdSet::iterator msgId = msgIds.begin() ;
          msgId != msgIds.end() ;
          msgId++ )
    {
      fprintf(f, "%s%s", msgId->c_str(), delim.c_str());
    }
}

//////////////////////////////////////////////////////////////////////////
//
void print_list_with_delimiter( const MailboxMap& mailboxes,
                                FILE* f,
                                const string& delim)
//
// Print the set of strings contained in "mailboxes" and terminate each
// item with the delimiter "delim"
// 
//////////////////////////////////////////////////////////////////////////
{
    for (MailboxMap::const_iterator mailbox = mailboxes.begin() ;
         mailbox != mailboxes.end() ;
         mailbox++ ) {
      // fprintf(f, "%s%s", mailbox->first.c_str(), delim.c_str());
    }
}

//--------------------------- Mail handling ------------------------------

//////////////////////////////////////////////////////////////////////////
//
void sanitize_message_id(string& msgid)
//
// Some mail software goes out of its way to produce braindammaged
// message-ids.
//
// This function attempts to bring it back to a sane form following RFC822
// that is "<blabla@somedomain>". Spaces inside the Message-ID are forbidden
// and are replaced by dots: ' '->'.'.
//
// Iif our msgid starts with '<' we find the corresponding '>' and throw
// everything after that away (let's hope there is no braindead sw that has
// significant msgid stuff _after_ the '>' or isn't using the '>' _inside_
// the msgid). If we don't find the ending '>' we just put a ">" at the end
// of the msgid.
//
//////////////////////////////////////////////////////////////////////////
{
  int removed_blanks = 0;
  int added_brackets = 0;
  unsigned i;
  if (msgid[0] != '<') {
    msgid = '<'+msgid;
    added_brackets = 1;
  }
  for (i=0; (i < msgid.size()) && (msgid[i] != '>'); i++) {
    if (isspace(msgid[i]) || iscntrl(msgid[i])) {
      msgid[i] = '.';
      removed_blanks = 1;
    }
  }

  // Have we reached end of string?
  if (i == msgid.size()) {
  // if there's no '>' we need to attach
    if(msgid[i-1] != '>') {
      msgid = msgid+'>';
      added_brackets = 1;
    }
  }
  // we've found a '>', so let's cut the rubbish that follows
  else
    msgid = msgid.substr( 0, i+1);

  if (options.report_braindammaged_msgids)
    if (removed_blanks)
      fprintf(stderr,"Warning: added brackets <> around message id %s\n",
                     msgid.c_str());
    else if (added_brackets)
      fprintf(stderr,"Warning: replaced blanks with . in message id %s\n",
                     msgid.c_str());
}

//////////////////////////////////////////////////////////////////////////
// Print formats that are being used when showin, what mails are being
// transfered or expunged:
//
// $ mailsync -m inbox
// Synchronizing stores "local-inbox" <-> "remote-inbox"...
// Authorizing against {my.imap-server/imap}
//
//  *** INBOX ***
//
// "  copied     <-  Mon Sep  9 Tomas Pospisek         test2
//                     <Pine.LNX.4.44.0209092123140.24399-100000@petertosh>"
//
// |--lead_format--|-------------from_format------------------|
//                     |--------------------msgid_format-------------------|
//
// 
//  "lead_format"  and
//  "from_format"  are only used/displayed with options -m and/or -M
//  "msgid_format" is only used/displayed with option -M
//
//////////////////////////////////////////////////////////////////////////
char lead_format[] = "  %-10s %s  ";    // arguments:  action, direction
char from_format[] = "%-61s";
char msgid_format[] = "%-65s";            // argument :  message-id

//////////////////////////////////////////////////////////////////////////
//
void print_lead( const char* action, const char* direction)
//
//////////////////////////////////////////////////////////////////////////
{
  printf( lead_format, action, direction);
}

//////////////////////////////////////////////////////////////////////////
//
void print_from(MAILSTREAM* stream, const unsigned long msgno)
//
//////////////////////////////////////////////////////////////////////////
{
  static char from[66] = "";
  MESSAGECACHE* elt;

  elt = mail_elt(stream, msgno);
  if (!elt)
    fprintf(stderr,"Error: Couldn't access message #%lu from mailbox box %s\n",
                   msgno, stream->mailbox);
  else {
    mail_cdate(&from[0], elt);
    mail_fetchfrom(&from[11], stream, msgno, 22);
    from[33] = ' ';
    mail_fetchsubject(&from[34], stream, msgno, 65-35-1);
    from[65] = '\0';
  }
  printf( from_format, &from[0]);
}

//////////////////////////////////////////////////////////////////////////
//
void print_msgid( const char* msgid)
//
//////////////////////////////////////////////////////////////////////////
{
  printf( msgid_format, msgid);
}

//////////////////////////////////////////////////////////////////////////
//
/* why can't I use size_type here? g++ will give me a syntax error! */
//////////////////////////////////////////////////////////////////////////
//
MAILSTREAM* mailbox_open( MAILSTREAM* stream, 
                          const string& fullboxname,
                          long c_client_options)
//
// Opens mailbox "fullboxname" with "c_client_options" options.
//
// If possible use the higherlevel function below
//
// Returns NIL on failure.
//
//////////////////////////////////////////////////////////////////////////
{
  return stream = mail_open( stream, nccs(fullboxname), c_client_options );
}
