#ifndef __MAILSYNC_MAILHANDLING__
#include "c-client.h"

//------------------------- Helper functions -----------------------------

//////////////////////////////////////////////////////////////////////////
//
void print_list_with_delimiter( const MsgIdSet& msgIds,
                                FILE* f,
                                const string& delim);
//
// Print the set of strings contained in "msgIds" and terminate each msgId
// with the delimiter "delim"
// 
//////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////
//
void print_list_with_delimiter( const MailboxMap& mailboxes,
                                FILE* f,
                                const string& delim);
//
// Print the set of strings contained in "mailboxes" and terminate each
// item with the delimiter "delim"
// 
//////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////
//
char* nccs( const string& s);
//
// C-Client doesn't declare anything const
// If you're paranoid, you can allocate a new char[] here
// 
//////////////////////////////////////////////////////////////////////////

//--------------------------- Mail handling ------------------------------

//////////////////////////////////////////////////////////////////////////
//
void sanitize_message_id(string& msgid);
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

//////////////////////////////////////////////////////////////////////////
//
void print_lead( const char* action, const char* direction);
//
//////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////
//
void print_from(MAILSTREAM* stream, const unsigned long msgno);
//
//////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////
//
void print_msgid( const char* msgid);
//
//////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////
//
MAILSTREAM* mailbox_open( MAILSTREAM* stream, 
                          const string& fullboxname,
                          long c_client_options);
//
// Opens mailbox "fullboxname" with "c_client_options" options.
//
// If possible use the higherlevel function below
//
// Returns NIL on failure.
//
//////////////////////////////////////////////////////////////////////////

#define __MAILSYNC_MAILHANDLING__
#endif
