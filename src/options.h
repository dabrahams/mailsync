#ifndef __MAILSYNC_OPTIONS__

using namespace std;

// hierarchy delimiter for IMAP
#define DEFAULT_DELIMITER '/'

typedef enum { HEADER_MSGID, MD5_MSGID } msgid_t;

//////////////////////////////////////////////////////////////////////////
// Options, commandline parsing and default settings
//////////////////////////////////////////////////////////////////////////

typedef struct options_t {
  bool log_chatter;
  bool log_warn;               // Show c-client warnings and warnings about
  bool log_parse;              // Log RFC822 parse errors
  bool show_summary;           // 1 line of output per mailbox
  bool show_from;              // 1 line of output per message
  bool show_message_id;        // Implies show_from
  bool no_expunge;
  bool delete_empty_mailboxes;
  bool debug;
  bool debug_imap;
  bool debug_config;
  bool report_braindammaged_msgids;
  bool copy_deleted_messages;
  bool simulate;
  msgid_t msgid_type;

  // the following options are mandatory
  bool expunge_duplicates;     // Should duplicates be deleted?
  bool log_error;              // Log serious errors

  options_t(): log_chatter(0),
               log_warn(0),
               log_parse(0),
               show_summary(1),
               show_from(0),
               show_message_id(0),
               no_expunge(0),
               delete_empty_mailboxes(0),
               debug(0),
               debug_imap(0),
               debug_config(0),
               report_braindammaged_msgids(0),
               copy_deleted_messages(0),
               simulate(0),
               msgid_type(HEADER_MSGID),
               expunge_duplicates(1),
               log_error(1) {};
};

#define __MAILSYNC_OPTIONS__
#endif
