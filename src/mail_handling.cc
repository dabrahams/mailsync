#include <stdio.h>  // required by c-client.h
#include <ctype.h>
#include "c-client-header.h"
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
    MsgId tmp_msgId;

    for ( MsgIdSet::iterator msgId = msgIds.begin() ;
          msgId != msgIds.end() ;
          msgId++ )
    {
      tmp_msgId = *msgId;
      fprintf(f, "%s%s", tmp_msgId.to_msinfo_format().c_str(), delim.c_str());
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
      fprintf(f, "%s%s", mailbox->first.c_str(), delim.c_str());
    }
}

//--------------------------- Mail handling ------------------------------


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
  return stream = mail_open( stream, nccs(fullboxname),
                             c_client_options
                             | ( options.debug_imap ? OP_DEBUG : 0 ) );
}
