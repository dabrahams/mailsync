#ifndef __MAILSYNC_STORE__

#include <stdio.h>
#include <string>
#include <map>
#include <set>
#include <c-client.h>
#include "types.h"
#include "msgid.h"

//////////////////////////////////////////////////////////////////////////
//
class Store
//
// Structure that holds the pattern for _one_ set of mailboxes to sync
//
//////////////////////////////////////////////////////////////////////////
{
  public:
    string name, server, prefix;
    string ref, pat;
    Passwd passwd;
    int isremote;                  // I.e. allows OP_HALFOPEN
    int delim;
    MAILSTREAM* stream;            // c-client mailstream through which the
                                   // store can be reached
    MailboxMap boxes;              // boxes with their properties

    void clear();

    Store():
      boxes(), name(), server(), prefix(), ref(), pat(), passwd() { clear(); }

    void print(FILE* f);
    void set_passwd(string password) { passwd.set_passwd(password);}
    size_t acquire_mail_list( );
    void get_delim();
    string full_mailbox_name(const string& box);
    bool fetch_message_ids(MsgIdPositions& mids);
    bool list_contents();
    bool flag_message_for_removal( unsigned long msgno, const MsgId& msgid,
                                   char * place);
    MAILSTREAM* mailbox_open( const string& boxname,
                                     long c_client_options);
    MAILSTREAM* store_open( long c_client_options);
    bool mailbox_create( const string& boxname );
    char* driver_name();
    void display_driver();
    void print_error(const char * cause, const string& mailbox);
    int mailbox_expunge(string mailbox_name);
};

#define __MAILSYNC_STORE__
#endif
