#ifndef __MAILSYNC_CHANNEL__

#include <stdio.h>
#include <string>
#include "types.h"      // Passwd
#include "store.h"

enum direction_t { a_to_b, b_to_a };

//////////////////////////////////////////////////////////////////////////
//
class Channel
//
// Structure that holds two Stores (sets of mailboxes) that should be synched
//
//////////////////////////////////////////////////////////////////////////
{
  public:
    string name;
    Store store_a;
    Store store_b;
    string msinfo;
    Passwd passwd;
    unsigned long sizelimit;

    Channel(): name(), msinfo(), passwd(), sizelimit(0) {};

    void print(FILE* f);

    void set_passwd(string password) { passwd.set_passwd(password);}
    void set_sizelimit(const string& sizelim)
    {
      sizelimit=strtoul(sizelim.c_str(),NULL,10);
    }
    bool read_lasttime_seen( MsgIdsPerMailbox& mids_per_box, 
                             MailboxMap& deleted_mailboxes);
    bool copy_message( unsigned long msgno,
                       const MsgId& msgid,
                       string fullboxbname,
                       enum direction_t direction);
    bool write_lasttime_seen( const MailboxMap& deleted_mailboxes,
                                    MsgIdsPerMailbox& lasttime);
};

#define __MAILSYNC_CHANNEL__
#endif
