// Please use spaces instead of tabs

//////////////////////////////////////////////////////////////////////////
// ATTENTION: The terminus "mailbox" is used throughout the code in
//            accordance with c-client speak.
//
//            What c-client calls "mailbox" is commonly referred to as a
//            mail FOLDER.
//            
//            In mailsync a box containing multiple folders is described
//            by a "Store" - have a look below at the definition of "Store".
//
//////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////
// MSINFO format description
//
// ATTENTION: the msinfo format might change without warning!
//
// The msinfo file contains synchronization information for channels as
// defined in the configuration file. Each channel contains the mailboxes
// associated with it and the message ids of all the mails that have been
// seen in those mailboxes the last time a sync was done.
// 
// The msinfo file is a normal mailbox.
//
// Each email therein represents a channel. The channel is identified by
// the "Subject: " email header.
//
// The body of an email contains a series of mailbox synchronization
// entries. Each such entry starts with the name of the mailbox followed
// by all the message ids that were seen and synchronized. Message ids
// have the format <xxx@yyy>
//
// Example:
//
// From tpo@petertosh Fri Oct  4 12:18:13 2002 +0200
// From: mailsync
// Subject: all
// Status: O
// X-Status:
// X-Keywords:
// X-UID: 2095
//
// linux/apt
// <1018460459.4617.23.camel@mpav>
// <1027985024.1352.19.camel@server1>
// linux/mailsync
// <001001c1d37a$39ee3d70$e349428e@ludwig>
// <002b01c1e95c$94957390$e349428e@ludwig>
//
// From tpo@petertosh Sat Nov 30 18:21:55 2002 +0100
// Status:
// X-Status:
// X-Keywords:
// From: mailsync
// Subject: inbox
//
// INBOX
// <.AAA-25623050-05908,3118.1037623416@mail-ems-103.amazon.com>
// <000001c28e4d$da22e0a0$3201a8c0@BALROG>
// 
// We have here two channels. The first is use for synchronization
// of all my mailboxes and the second one solely for my inbox.
//
// The first channel contains the mailboxes "linux/apt" and
// "linux/mailsync". Each of those mailboxes has had two mails in it,
// whose message-ids can be seen above.
//
//////////////////////////////////////////////////////////////////////////
//
#define VERSION "4.5"

#include <ctype.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
extern int errno;               // Just in case
#include <sys/stat.h>           // Stat()

#include <string>
#include <set>
#include <map>
#include <vector>
using std::string;
using std::set;
using std::map;
using std::vector;
using std::make_pair;

#include "c-client.h"
#include "flstring.h"
#include "msgstring.c"

//------------------------------- Defines  -------------------------------

#define DEFAULT_DELIMITER '/'
#define CREATE   1
#define NOCREATE 0

//------------------------------ Type Defs -------------------------------

typedef set<string> StringSet;
typedef map<string,unsigned long> MsgIdPositions;  // Map message ids to positions
                                                   // within a mailbox

//////////////////////////////////////////////////////////////////////////
//
struct Passwd {
//
// Structure that holds the password
//
//////////////////////////////////////////////////////////////////////////
  bool nopasswd;
  string text;

  void clear() {
    text = "";
    nopasswd = true;
  }

  void set_passwd(string passwd) {
    nopasswd = false;
    text = passwd;
  }
};

void print_with_escapes(FILE* f, const string& str); // forward declaration

//////////////////////////////////////////////////////////////////////////
//
struct Store {
//
// Structure that holds the pattern for _one_ set of mailboxes to sync
//
//////////////////////////////////////////////////////////////////////////
  string tag, server, prefix;   // Tag == store name
  string ref, pat;
  Passwd passwd;
  int isremote;     // I.e. allows OP_HALFOPEN
  int delim;

  void clear() {
    tag      = "";
    server   = "";
    prefix   = "";
    ref      = "";
    pat      = "";
    isremote = 0;
    delim    = '!';
    passwd.clear();
    return;
  }

  Store() { clear(); }

  void print(FILE* f) {
    fprintf(f,"store %s {",tag.c_str());
    if (server!="") {
      fprintf(f,"\n\tserver ");
      print_with_escapes(f,server);
    }
    if (prefix!="") {
      fprintf(f,"\n\tprefix ");
      print_with_escapes(f,prefix);
    }
    if (ref!="") {
      fprintf(f,"\n\tref ");
      print_with_escapes(f,ref);
    }
    if (pat!="") {
      fprintf(f,"\n\tpat ");
      print_with_escapes(f,pat);
    }
    if (! passwd.nopasswd) {
      fprintf(f,"\n\tpasswd ");
      print_with_escapes(f,passwd.text);
    }
    fprintf(f,"\n}\n");
    return;
  }

  void set_passwd(string password) { passwd.set_passwd(password);}
};

//////////////////////////////////////////////////////////////////////////
//
struct Channel {
//
// Structure that holds two Stores (sets of mailboxes) that should be synched
//
//////////////////////////////////////////////////////////////////////////
  string tag;           // Tag == channel name
  string store[2];
  string msinfo;
  Passwd passwd;
  unsigned long sizelimit;
  string version;

  void clear() {
    tag = store[0] = store[1] = msinfo = version = "";
    passwd.clear();
    sizelimit = 0;
    return;
  }

  Channel() { clear(); }

  void print(FILE* f) {
    fprintf(f,"channel %s %s %s {",tag.c_str(),store[0].c_str(),
                                               store[1].c_str());
    if (msinfo != "") {
      fprintf(f,"\n\tmsinfo ");
      print_with_escapes(f,msinfo);
    }
    if (! passwd.nopasswd) {
      fprintf(f,"\n\tpasswd ");
      print_with_escapes(f,passwd.text);
    }
    if(sizelimit) fprintf(f,"\n\tsizelimit %lu",sizelimit);
    fprintf(f,"\n}\n");
    return;
  }

  void set_passwd(string password) { passwd.set_passwd(password);}
  void set_sizelimit(const string& sizelim) { sizelimit=strtoul(sizelim.c_str(),NULL,10); }
};

//////////////////////////////////////////////////////////////////////////
//
struct ConfigItem {
//
// A configuration item from the config file
// Can either be a Channel or a Store
//
//////////////////////////////////////////////////////////////////////////
  int is_Store;
  Store* store;
  Channel* channel;

  void clear() {
    is_Store = 0;
    store = NULL;
    channel = NULL;
    return;
  }
  ConfigItem() { clear(); }
  
  void print(FILE* f) {
    if (is_Store)
      store->print(f);
    else
      channel->print(f);
  }
};

//////////////////////////////////////////////////////////////////////////
//
struct Token {
//
// A token is a [space|newline|tab] delimited chain of characters which
// represents a syntactic element from the configuration file
//
//////////////////////////////////////////////////////////////////////////
  string buf;
  int eof;
  int line;
};


//------------------------ Global Variables ------------------------------

//////////////////////////////////////////////////////////////////////////
// Options and default settings
//////////////////////////////////////////////////////////////////////////
bool log_chatter = 0;
bool log_warn  = 0;              // Log c-client warnings
bool log_parse = 0;              // Log RFC822 parse errors
bool show_summary = 1;           // 1 line of output per mailbox
bool show_from = 0;              // 1 line of output per message
bool show_message_id = 0;        // Implies show_from
bool no_expunge = 0;
enum { mode_unknown, mode_sync, mode_list, mode_diff } mode = mode_unknown;
bool delete_empty_mailboxes = 0;
bool debug = 0;
bool report_braindammaged_msgids = 0;
bool copy_deleted_messages = 0;
bool simulate = 0;

//////////////////////////////////////////////////////////////////////////
// Mandatory options
//////////////////////////////////////////////////////////////////////////
bool expunge_duplicates = 1;     // Should duplicates be deleted?
bool log_error = 1;              // Log serious errors

int critical = NIL;             // Flag saying in critical code

//////////////////////////////////////////////////////////////////////////
// The password for the current context
// Required, because in the c-client callback functions we don't know
// in which context (store1, store2, channel) we are
Passwd * current_context_passwd = NULL;
//////////////////////////////////////////////////////////////////////////

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


//------------------------ Forward Declarations --------------------------

void sanitize_message_id(string& msgid);
bool read_lasttime_seen(Channel channel,
                       StringSet& boxes, 
                       map<string, StringSet>& mids, 
                       StringSet& extra);
bool write_lasttime_seen(Channel channel,
                         const StringSet& boxes, 
                         const StringSet& deleted_boxes,
                         map<string, StringSet>& lasttime);
MAILSTREAM* mailbox_open_create(MAILSTREAM* stream, 
                                Store& store,
                                const string& fullboxname, 
                                long options,
                                bool create);
MAILSTREAM* mailbox_open_create(MAILSTREAM* stream, 
                                const string& fullboxname, 
                                long options,
                                bool create);
string summary(MAILSTREAM* stream, unsigned long msgno);


//------------------------- Helper functions -----------------------------

//////////////////////////////////////////////////////////////////////////
//
void print_list_with_delimiter(const StringSet& set, FILE* f,
                               const string& delim) {
//
// Print the set of strings contained in "set" and terminate each item with
// the delimiter "delim"
// 
//////////////////////////////////////////////////////////////////////////
  for (StringSet::iterator i=set.begin(); i!=set.end(); i++)
    fprintf(f, "%s%s", i->c_str(), delim.c_str());
}

//////////////////////////////////////////////////////////////////////////
//
char* nccs(const string& s) {
//
// C-Client doesn't declare anything const
// If you're paranoid, you can allocate a new char[] here
// 
//////////////////////////////////////////////////////////////////////////
  return (char*) s.c_str();
}

//////////////////////////////////////////////////////////////////////////
//
void print_with_escapes(FILE* f, const string& str) {
//
// Prints a string into a file (or std[out|err]) while
// replacing " " by "\ "
// 
//////////////////////////////////////////////////////////////////////////
  const char *s = str.c_str();
  while (*s) {
    if (isspace(*s))
      fputc('\\',f);
    fputc(*s,f);
    s++;
  }
  return;
}

//////////////////////////////////////////////////////////////////////////
//
void usage() {
//
// Mailsync help
//
//////////////////////////////////////////////////////////////////////////
  printf("mailsync %s\n\n", VERSION);
  printf("usage: mailsync [-cd] [-db] [-nDdmMs] [-v[bwp]] [-f conf] channel\n");
  printf("usage: mailsync               [-dmM]  [-v[bwp]] [-f conf] store\n");
  printf("\n");
  printf("synchronize two stores defined by \"channel\" or\n");
  printf("list mailboxes contained in \"store\"\n");
  printf("\n");
  printf("Options:\n");
  printf("  -cd      do copy deleted mailboxes (default is not)\n");
  printf("  -n       don't expunge mailboxes\n");
  printf("  -D       delete any empty mailboxes after synchronizing\n");
  printf("  -m       show from, subject, etc. of messages that get expunged or moved\n");
  printf("  -M       also show message-ids (turns on -m)\n");
  printf("  -s       simulate\n");
  printf("  -d       show debug info\n");
  printf("  -v       show imap chatter\n");
  printf("  -vb      show warning about braindammaged message ids\n");
  printf("  -vw      show warnings\n");
  printf("  -vp      show RFC 822 parsing errors\n");
  printf("  -f conf  use alternate config file\n");
  printf("\n");
  return;
}


//--------------------- Configuration Parsing ----------------------------

//////////////////////////////////////////////////////////////////////////
//
void die_with_fatal_parse_error(Token* t, char * errorMessage,
                                const char * insertIntoMessage = NULL) {
//
// Fatal error while parsing the config file
// Display error message and quit
//
// errorMessage       must contain an error message to be displayed
// insertIntoMessage  is optional. errorMessage can act as a sprintf format
//                    string in which case insertIntoMessage will be used
//                    as replacement for "%s" inside errorMessage
//
//////////////////////////////////////////////////////////////////////////
  fprintf(stderr, "Error: ");
  fprintf(stderr, errorMessage, insertIntoMessage ? insertIntoMessage : "");
  fprintf(stderr, "\n");
  if (t->eof) {
    fprintf(stderr, "       Unexpected EOF while parsing configuration file");
    fprintf(stderr, "       While parsing line %d:\n", t->line);
  } else {
    fprintf(stderr, "       While parsing line %d at token \"%s\"\n",
            t->line, t->buf.c_str());
  }
  fprintf(stderr,   "       Quitting!\n");

  exit(1);
}

//////////////////////////////////////////////////////////////////////////
//
void get_token(FILE* f, Token* t) {
//
// Read a token from the config file
//
//////////////////////////////////////////////////////////////////////////
  int c;
  t->buf = "";
  t->eof = 0;
  while (1) {
    if (t->buf.size() > MAILTMPLEN)
      // Token too long
      die_with_fatal_parse_error(t, "Line too long");
    c = getc(f);
   
    // Ignore comments
    if ( t->buf.size() == 0 && c=='#') {
      while( c != '\n' && c !=EOF) {
        c = getc(f);
      }
    }
    
    // Skip newline
    if (c=='\n')
      t->line++;

    // We're done reading the config file
    if (c==EOF) {
      t->eof = 1;
      break;

    // We've reached a token boundary
    } else if (isspace(c)) {
      // The token is non empty so we've acquired something and can return
      if (t->buf.size()) {
        break;
      // Ignore spaces
      } else {
        continue;
      }
    // Continue on next line...
    } else if (c=='\\') {
      t->buf += (char)getc(f);
    // Add to token
    } else {
      t->buf += (char)c;
    }
  }
  if (debug) printf("\t%s\n",t->buf.c_str());
  return;
}

//////////////////////////////////////////////////////////////////////////
//
void parse_config(FILE* f, map<string, ConfigItem>& confmap) {
// 
// Parse config file
// 
//////////////////////////////////////////////////////////////////////////
  Token token;
  Token *t;

  if (debug) printf(" Parsing config...\n\n");

  t = &token;
  t->line = 0;

  // Read items from file
  while (1) {
    ConfigItem* conf = new ConfigItem();
    string tag;
    conf->is_Store = -1;

    // Acquire one item
    get_token(f, t);

    // End of config file reached
    if (t->eof)
      break;
    
    // Parse store
    if (t->buf == "store") {
      conf->is_Store = 1;
      conf->store = new Store();
      // Read tag (store name)
      get_token(f, t);
      if (t->eof)
        die_with_fatal_parse_error(t, "Store name missing");
      tag = t->buf;
      conf->store->tag = t->buf;
      // Read "{"
      get_token(f, t);
      if (t->eof || t->buf != "{")
        die_with_fatal_parse_error(t, "Expected \"{\" while parsing store");
      // Read store configuration (name/value pairs)
      while (1) {
        get_token(f, t);
        // End of store config
        if (t->buf == "}")
          break;
        else if (t->eof)
          die_with_fatal_parse_error(t, "Expected \"{\" while parsing store: unclosed store");
        else if (t->buf == "server") {
          get_token(f, t);
          conf->store->server = t->buf;
        }
        else if (t->buf == "prefix") {
          get_token(f, t);
          conf->store->prefix = t->buf;
        }
        else if (t->buf == "ref") {
          get_token(f, t);
          conf->store->ref = t->buf;
        }
        else if (t->buf == "pat") {
          get_token(f, t);
          conf->store->pat = t->buf;
        }
        else if (t->buf == "passwd") {
          get_token(f, t);
          conf->store->set_passwd(t->buf);
        }
        else
          die_with_fatal_parse_error(t, "Unknown store field");
      }
      if (conf->store->server == "") {
        conf->store->isremote = 0;
      }
      else {
        conf->store->isremote = 1;
      }

    // Parse a channel
    }
    else if (t->buf == "channel") {
      conf->is_Store = 0;
      conf->channel = new Channel;
      // Read tag (channel name)
      get_token(f, t);
      if (t->eof) {
        die_with_fatal_parse_error(t, "Channel name missing");
      }
      tag = t->buf;
      conf->channel->tag = t->buf;
      // Read stores
      get_token(f, t);
      if (t->eof)
        die_with_fatal_parse_error(t, "Expected a store name while parsing channel");
      conf->channel->store[0] = t->buf;
      get_token(f, t);
      if (t->eof)
        die_with_fatal_parse_error(t, "Expected a store name while parsing channel");
      conf->channel->store[1] = t->buf;
      // Read "{"
      get_token(f, t);
      if (t->eof || t->buf != "{")
        die_with_fatal_parse_error(t, "Expected \"{\" while parsing channel");
      // Read channel config (name/value pairs)
      while (1) {
        get_token(f, t);
        if (t->buf == "}")
          break;
        else if (t->eof)
          die_with_fatal_parse_error(t, "Unclosed channel");
        else if (t->buf == "msinfo") {
          get_token(f, t);
          conf->channel->msinfo = t->buf;
        }
        else if (t->buf == "passwd") {
          get_token(f, t);
          conf->channel->set_passwd(t->buf);
        }
	else if (t->buf == "sizelimit") {
          get_token(f, t);
	  conf->channel->set_sizelimit(t->buf);
	}
        else
          die_with_fatal_parse_error(t, "Unknown channel field");
      }
      if (conf->channel->msinfo == "") {
        die_with_fatal_parse_error(t, "%s: missing msinfo", conf->channel->tag.c_str());
      }
    }
    else
      die_with_fatal_parse_error(t, "unknow configuration element");

    if (confmap.count(tag)) {
      die_with_fatal_parse_error(t, "Tag (store or channel name) used twice: %s", tag.c_str());
    }
    confmap.insert(make_pair(tag, *conf));
    delete conf;
  }

  if (debug) printf(" End parsing config. Config is OK\n\n");

  return;
}


//--------------------------- Mail handling ------------------------------

//////////////////////////////////////////////////////////////////////////
//
string full_mailbox_name(Store s, const string& box) {
//
// Given the name of a mailbox in a Store, return its full IMAP name.
//
//////////////////////////////////////////////////////////////////////////
  string boxname = box;
  string fullbox;
  // Replace our DEFAULT_DELIMITER by the respective store delimiter
  for (unsigned int i=0; i<boxname.size(); i++)
    if( boxname[i] == DEFAULT_DELIMITER) boxname[i] = s.delim;
  fullbox = s.server + s.prefix + boxname;
  return fullbox;
}

static StringSet*   matching_mailbox_names;
static Store*       match_pattern_store;

//////////////////////////////////////////////////////////////////////////
//
void get_mail_list(MAILSTREAM *stream, Store& store, StringSet& mailbox_names) {
//
// Fetch a list of mailbox-names from "stream" that match the pattern
// given by "store" and put them into "mailbox_names".
//
// "mailbox_names" is filled up through the callback function "mm_list"
// by c-client's mail_list function
//
// This function corresponds to c-client's mail_list
//
//////////////////////////////////////////////////////////////////////////
  match_pattern_store = &store;
  matching_mailbox_names = &mailbox_names;
  current_context_passwd = &(store.passwd);
  mail_list(stream, nccs(store.ref), nccs(store.pat));
  match_pattern_store = NULL;
  matching_mailbox_names = NULL;
}

//////////////////////////////////////////////////////////////////////////
//
void get_delim(MAILSTREAM*& stream, Store& store_in) {
//
// Determine the mailbox delimiter that "stream" is using.
// 
//////////////////////////////////////////////////////////////////////////
  Store store = store_in;
  store.prefix = "";
  store.pat = "INBOX";
  StringSet boxes;
  get_mail_list(stream, store, boxes);
  store_in.delim = store.delim;
  return;
}

//////////////////////////////////////////////////////////////////////////
//
bool read_lasttime_seen(Channel channel,
                       StringSet& boxes, 
                       map<string, StringSet>& mids_per_box, 
                       StringSet& extra) {
//
// Read from msinfo the message ids of all the messages that have been
// seen last time the channel was synchronized and return a hash that
// contains those message ids indexed by "boxes"' names.
//
// Extra contains another hash containing the names of mailboxes that
// were seen last time but were not included in "boxes".
//
// If msinfo doesn't exist yet, it will be created.
//
// This function is used in order to determine which messages to transfer
// or to expunge.
//
// See the msinfo format description for more info.
//
// arguments:
//              channel      - the channel that is being synched
//              boxes        - the mailboxes that will be synched
// returns:
//              1            - success
//              0            - failure
//
//              mids_per_box - hash of lists with message-ids per mailbox
//                             (indexed by mailbox)
//              extra        - mailboxes that are not contained in the boxes
//                             set
//
//////////////////////////////////////////////////////////////////////////
  MAILSTREAM* msinfo_stream;
  ENVELOPE* envelope;
  unsigned long msgno;
  unsigned long k;
  string* currentbox = NULL;    // the box whose msg-id's we're currently reading
  char* text;
  unsigned long textlen;

  if (debug) printf(" Reading lasttime of channel \"%s\":\n", channel.tag.c_str());

  // msinfo is the name of the mailbox that contains the sync info
  current_context_passwd = &(channel.passwd);
  msinfo_stream = mailbox_open_create(NULL, nccs(channel.msinfo), OP_READONLY, CREATE);
  if (!msinfo_stream) {
    fprintf(stderr,"Error: Couldn't open msinfo box %s.\n",
                   channel.msinfo.c_str());
    fprintf(stderr,"       Aborting!\n");
    return 0;
  }

  for (msgno=1; msgno<=msinfo_stream->nmsgs; msgno++) {
    envelope = mail_fetchenvelope(msinfo_stream, msgno);
    if (! envelope) {
      fprintf(stderr,"Error: Couldn't fetch enveloppe #%lu from msinfo box %s\n",
                     msgno, channel.msinfo.c_str());
      fprintf(stderr,"       Aborting!\n");
      return 0;
    }
    // Make sure that the mail is from mailsync and read the mailsync version
    if (strncmp(envelope->from->mailbox, "mailsync", 8)) {
      // This is not an email describing a mailsync channel!
      fprintf(stderr,"Info: The msinfo box %s contains the non-mailsync mail: \"From: %s\"\n",
              channel.msinfo.c_str(), envelope->from->mailbox );
      continue;
    }
    else {
      channel.version = &envelope->from->mailbox[8];
    }

    // The subject line contains the name of the channel
    if (channel.tag == envelope->subject) {
      // Found our lasttime

      text = mail_fetchtext_full(msinfo_stream, msgno, &textlen, FT_INTERNAL);
      if (text)
        text = strdup(text);
      else {
        fprintf(stderr,"Error: Couldn't fetch body #%lu from msinfo box %s\n",
                       msgno, channel.msinfo.c_str());
        fprintf(stderr,"       Aborting!\n");
        return 0;
      }
      // Replace newlines with '\0'
      // I.e. we transform the text body into c-strings, where
      // each string is either a mailbox or a message id
      for (k=0; k<textlen; k++) {
        if (text[k] == '\n' || text[k] == '\r')
          text[k] = '\0';
      }
      // Check each box against `boxes'
      for (k=0;k<textlen;) {
        if (text[k] == '\0') { // skip nulls
          k++;
          continue;
        }
        if (text[k] != '<') {     // is it a mailbox name?
          delete(currentbox);
          if (boxes.count(&text[k])) { // is the mailbox allready known?
            currentbox = new string(&text[k]);
          } else {
            currentbox = NULL;
            extra.insert(&text[k]);
          }
        }
        else {                    // it's a message-id
          if (currentbox)
            mids_per_box[*currentbox].insert(&text[k]);
          else {}; // box has been deleted since last sync
        }
        for(;k<textlen && text[k]; k++); // fastforward to next string
      }

      free(text);
      break;    // Stop searching for message
    }
  }
  
  if (show_message_id) {
    printf("lasttime %s: ", channel.msinfo.c_str());
    for (StringSet::iterator b = boxes.begin(); b!=boxes.end(); b++) {
      printf("%s(%d) ", b->c_str(), mids_per_box[*b].size());
    }
    printf("\n");
  }

  mail_close(msinfo_stream);
  return 1;
  exit(0);
}

//////////////////////////////////////////////////////////////////////////
//
bool list_contents(MAILSTREAM*& mailbox_stream) {
//
// Display contents of mailbox
// 
// The mailbox is defined by the name in "box" and by the "store" where the
// mailbox resides
//
// returns:
//              0                    - failure
//              1                    - success
//              mailbox_stream       - a stream "to" the mailbox
//
// This was copied from fetch_message_ids
//
//////////////////////////////////////////////////////////////////////////
  MsgIdPositions mids;

  // loop and fetch all the message ids from a mailbox
  unsigned long n = mailbox_stream->nmsgs;
  for (unsigned long msgno=1; msgno<=n; msgno++) {
    string msgid;
    ENVELOPE *envelope;
    bool isdup;

    envelope = mail_fetchenvelope(mailbox_stream, msgno);
    if (! envelope) {
      fprintf(stderr,"Error: Couldn't fetch enveloppe #%lu from mailbox box %s\n",
                     msgno, mailbox_stream->mailbox);
      fprintf(stderr,"       Aborting!\n");
      return 0;
    }
    if (!(envelope->message_id))
      printf(lead_format,"no msg-id", "");
    else {
      msgid = string(envelope->message_id);
      isdup = mids.count(msgid);
      if (isdup)
        printf(lead_format,"duplicate", "");
      else
        mids.insert(make_pair(msgid, msgno));
    
      if (show_message_id) printf(msgid_format, msgid.c_str());
    }
    printf(from_format, summary(mailbox_stream,msgno).c_str());
    printf("\n");
  }
  return 1;
}

//////////////////////////////////////////////////////////////////////////
//
bool fetch_message_ids(MAILSTREAM*& mailbox_stream, string store_tag, 
                      MsgIdPositions& mids) {
//
// Fetch all the message ids that a mailbox contains.
// 
// The mailbox is defined by the name in "box" and by the "store" where the
// mailbox resides
//
// If there are duplicates they will be deleted (depending on the compile
// time option expunge_duplicates)
//
// If the mailbox doesn't exist yet, it will be created.
//
// returns:
//              0              - failure
//              1              - success
//              mailbox_stream - a mailbox_stream "to" the store
//              store_tag      - the name of the store
//              mids           - a hash indexed by msgid containing the
//                               position of the message in the mailbox
//
//////////////////////////////////////////////////////////////////////////
  // loop and fetch all the message ids from a mailbox
  unsigned long n = mailbox_stream->nmsgs;
  unsigned long nabsent = 0, nduplicates = 0;

  if (debug) printf(" Fetching message id's in mailbox \"%s\":\n", mailbox_stream->mailbox);

  for (unsigned long msgno=1; msgno<=n; msgno++) {
    string msgid;
    ENVELOPE *envelope;
    bool isdup;

    envelope = mail_fetchenvelope(mailbox_stream, msgno);
    if (! envelope) {
      fprintf(stderr,"Error: Couldn't fetch enveloppe #%lu from mailbox box %s\n",
                     msgno, mailbox_stream->mailbox);
      fprintf(stderr,"       Aborting!\n");
      return 0;
    }
    if (!(envelope->message_id)) {
      nabsent++;
      // Absent message-id.  Don't touch message.
      continue;
    }
    msgid = string(envelope->message_id);
    sanitize_message_id(msgid);
    isdup = mids.count(msgid);
    if (isdup) {
      if (expunge_duplicates) {
        char seq[30];
        sprintf(seq,"%lu",msgno);
        if (!simulate) mail_setflag(mailbox_stream, seq, "\\Deleted");
      }
      nduplicates++;
      if (show_from) printf(lead_format,"duplicate", "");
    } else {
      mids.insert(make_pair(msgid, msgno));
    }
    if (isdup && show_from) {
      printf(from_format, summary(mailbox_stream,msgno).c_str());
      if (show_message_id) printf(msgid_format, msgid.c_str());
      printf("\n");
    }
  }

  if (nduplicates) {
    if (show_summary) {
      printf("%lu duplicate%s", nduplicates, nduplicates==1 ? "" : "s");
      if (mode!=mode_diff)
        printf(" in %s", store_tag.c_str());
      printf(", ");
    } else {
      printf("%lu duplicates deleted from %s/%s\n",
             nduplicates, store_tag.c_str(), mailbox_stream->mailbox);
    }
    fflush(stdout);
  }
  return 1;
}

//////////////////////////////////////////////////////////////////////////
//
bool copy_message(MAILSTREAM*& mailboxa_stream, Passwd& passwda, 
                 unsigned long msgno, const string& msgid,
                 MAILSTREAM*& mailboxb_stream, Passwd& passwdb,
                 string fullboxbname, char * direction, const Channel& channel) {
//
// Copies the message "msgno" with "msgid" from the stream "mailboxa_stream"
// which is connected to the mailboxa to the respective "mailboxb_stream".
//
// When appending to a HALF_OPEN stream or to a local mailbox, mailboxb_stream
// will be NIL. Therefore we need the full name of the box.
//
// To access the stream it uses the passwords passwd[a|b].
//
// returns !0 for success
//
// TODO: ideally sanitize_message_id should not have a side effect, but just
//       return 1 or 0 if the message had to be modified or it should have
//       enough information to print a sufficient error message, i.e. the
//       mailbox where the message is originating from
//
//////////////////////////////////////////////////////////////////////////
  MSGDATA md;
  STRING CCstring;
  char flags[MAILTMPLEN];
  char msgdate[MAILTMPLEN];
  ENVELOPE *envelope;
  MESSAGECACHE *elt;
  bool success = 1;

  current_context_passwd = &passwda;
  envelope = mail_fetchenvelope(mailboxa_stream, msgno);
  string msgid_fetched;

  if (! envelope) {
    fprintf(stderr,"Error: Couldn't fetch enveloppe #%lu from mailbox box %s\n",
                   msgno, mailboxa_stream->mailbox);
    return 0;
  }

  // Check message-id.
  if (! envelope->message_id) {
    printf("Warning: missing message-id from mailbox %s, message #%lu.\n",
           mailboxa_stream->mailbox, msgno);
  } else {
    msgid_fetched = envelope->message_id;
    sanitize_message_id(msgid_fetched);
    if (msgid_fetched != msgid) {
      printf("Warning: suspicious message-id from mailbox %s, message #%lu.",
             mailboxa_stream->mailbox, msgno);
      printf("         msgid expected: %s, msgid found: %s. \n",
             msgid.c_str(), msgid_fetched.c_str());
      printf("Please report this to http://sourceforge.net/tracker/?group_id=6374&atid=106374\n");
    }
  }

  elt = mail_elt(mailboxa_stream, msgno); // the c-client docu says not to do
                                          // this :-/ ?
  assert(elt->valid);                     // Should be valid because of
                                          // fetchenvelope()

  // we skip deleted messages unless copying deleted messages is explicitly
  // demanded
  if (elt->deleted & ! copy_deleted_messages) {
    printf("Not copying deleted message #%lu from mailbox %s.",
            msgno, mailboxa_stream->mailbox);
    return 0;
  }
      
  // Copy message over with all the flags that the original has
  memset(flags, 0, MAILTMPLEN);
  if (elt->seen) strcat (flags," \\Seen");
  if (elt->deleted) strcat (flags," \\Deleted");
  if (elt->flagged) strcat (flags," \\Flagged");
  if (elt->answered) strcat (flags," \\Answered");
  if (elt->draft) strcat (flags," \\Draft");

  md.stream = mailboxa_stream;
  md.msgno = msgno;
  if( channel.sizelimit && elt->rfc822_size > channel.sizelimit ) {
    printf(lead_format, "too big" , direction);
    printf(from_format, summary(mailboxa_stream, msgno).c_str());
    if (show_message_id) printf(msgid_format, msgid.c_str());
    printf("\n");
    return 0;
  }
  INIT (&CCstring, msg_string, (void*) &md, elt->rfc822_size);
  current_context_passwd = &passwdb;

  if (!simulate) 
    success = mail_append_full(mailboxb_stream, nccs(fullboxbname),
                               &flags[1], mail_date(msgdate,elt), 
                               &CCstring);
  if (show_from) {
    printf(lead_format, success ? "copied" : "copyfail", direction);
    printf(from_format, summary(mailboxa_stream, msgno).c_str());
    if (show_message_id) printf(msgid_format, msgid.c_str());
    printf("\n");
  }

  if (debug) printf(" Flags: \"%s\"\n", flags);
  return success;
}

//////////////////////////////////////////////////////////////////////////
//
bool remove_message(MAILSTREAM*& stream, Store& store, 
                   unsigned long msgno, const string& msgid,
                   char * place) {
//
// returns !0 on success
//
//////////////////////////////////////////////////////////////////////////
  string msgid_fetched;
  ENVELOPE *envelope;
  bool success = 1;
  
  current_context_passwd = &(store.passwd);
  envelope = mail_fetchenvelope(stream, msgno);
  if (! envelope) {
    fprintf(stderr,"Error: Couldn't fetch enveloppe #%lu from mailbox box %s\n",
                   msgno, stream->mailbox);
    return 0;
  }
  if (!(envelope->message_id)) {
    printf("Error: no message-id, so I won't delete the message.\n");
    // Possibly indicates concurrent access?
    success = 0;
  } else {
    msgid_fetched = envelope->message_id;
    sanitize_message_id(msgid_fetched);
    if (msgid_fetched != msgid) {
      printf("Error: message-ids %s and %s don't match, so I won't delete the message.\n",
             msgid_fetched.c_str(), msgid.c_str());
      success = 0;
    }
  }
  
  if (success) {
    char seq[30];
    sprintf(seq,"%lu",msgno);
    if (!simulate) mail_setflag(stream, seq, "\\Deleted");
  }
  if (show_from) {
    if (success) {
      printf(lead_format, "deleted", place);
    } else
      printf(lead_format, "deletefail", "");
    printf(from_format, summary(stream,msgno).c_str());
    printf("\n");
  }
  return success;
}

//////////////////////////////////////////////////////////////////////////
//
bool write_lasttime_seen(Channel channel,
                         const StringSet& boxes, 
                         const StringSet& deleted_boxes,
                         map<string, StringSet>& lasttime) {
//
// Save in channel.msinfo all the mail-"boxes" with all their corresponding
// msgids (found in "lasttime").
//
// channel        - contains the name of the msinfo box
// boxes          - the boxes that were synchronized and whose see msgid's
//                  should be saved in the msinfo box
// deleted_boxes  - ?
// lasttime       - hash indexed by mailbox name containing a list of msgids
//                  for each box
//
// returns !0 on success
//
//////////////////////////////////////////////////////////////////////////
  MAILSTREAM* stream;
  ENVELOPE* envelope;
  unsigned long msgno;

  // open the msinfo box
  current_context_passwd = &(channel.passwd);
  stream = mailbox_open_create(NIL, nccs(channel.msinfo), 0, NOCREATE);
  if (!stream) {
    return 0;
  }

  // first delete all the info for the channel we're reading
  // it will be replaced by a newly created set
  for (msgno=1; msgno<=stream->nmsgs; msgno++) {
    envelope = mail_fetchenvelope(stream, msgno);
    if (! envelope) {
      fprintf(stderr,"Error: Couldn't fetch enveloppe #%lu from mailbox box %s\n",
                     msgno, stream->mailbox);
      return 0;
    }
    if (envelope->subject == channel.tag) {
      char seq[30];
      sprintf(seq,"%lu",msgno);
      mail_setflag(stream, seq, "\\Deleted");
    }
  }
  mail_expunge(stream);
  
  // Construct a temporary email that contains the new set msgid's that
  // are alive (not deleted) for each mailbox and then copy that email over
  // to the msinfo box
  { 
    FILE* f;
    long flen;
    STRING CCstring;

    // create temporary file containing the email
    f = tmpfile();
    fprintf(f,"From: mailsync\nSubject: %s\n\n",channel.tag.c_str());

    // for each box - if it's not a deleted mailbox:
    // * first write a line containing it's name
    // * then dump all the message-id's we've seen the last time into it
    for (StringSet::iterator i=boxes.begin(); i!=boxes.end(); i++) {
      if (! deleted_boxes.count(*i)) {
        fprintf(f,"%s\n", i->c_str());
        print_list_with_delimiter(lasttime[*i], f, "\n");
      }
    }

    // append the constructed email into the msinfo box
    flen = ftell(f);
    rewind(f);
    INIT(&CCstring, file_string, (void*) f, flen);
    if (!mail_append(stream, nccs(channel.msinfo), &CCstring)) {
      fprintf(stderr,"Error: Can't append lasttime to msinfo \"%s\"\n",
                     channel.msinfo.c_str());
      fclose(f);
      mail_close(stream);
      return 0;
    }
    fclose(f);
  }

  mail_close(stream);
  return 1;
}

//////////////////////////////////////////////////////////////////////////
//
string summary(MAILSTREAM* stream, unsigned long msgno) {
//
//////////////////////////////////////////////////////////////////////////
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
  return string(&from[0]);
}

//////////////////////////////////////////////////////////////////////////
//
void sanitize_message_id(string& msgid) {
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
  int removed_blanks = 0;
  int added_brackets = 0;
  unsigned i;
  if (msgid[0] != '<') {
    msgid = '<'+msgid;
    added_brackets = 1;
  }
  for (i=0; (i < msgid.size()) && (msgid[i] != '>'); i++)
    if (isspace(msgid[i]) || iscntrl(msgid[i])) {
      msgid[i] = '.';
      removed_blanks = 1;
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

  if (report_braindammaged_msgids)
    if (removed_blanks)
      fprintf(stderr,"Warning: added brackets <> around message id %s\n",
                     msgid.c_str());
    else if (added_brackets)
      fprintf(stderr,"Warning: replaced blanks with . in message id %s\n",
                     msgid.c_str());
}

//////////////////////////////////////////////////////////////////////////
//
MAILSTREAM* mailbox_open_create(MAILSTREAM* stream, 
                                const string& fullboxname,
                                long c_client_options,
                                bool create) {
//
// Opens mailbox "fullboxname" with "c_client_options" options.
//
// If "create" is set it will try to create the mailbox. If the mailbox
// allready exists nothing will happen.
//
// If possible use the higherlevel function below
//
// Returns NIL on failure.
//
//////////////////////////////////////////////////////////////////////////
  bool o;
  
  o = log_error;
  log_error = 0;
  if (create)
    mail_create(stream, nccs(fullboxname));       // creates new if not
                                                  // existent, fails otherwise
  log_error = o;
  stream = mail_open(stream, nccs(fullboxname), c_client_options);
  return stream;
}

//////////////////////////////////////////////////////////////////////////
//
MAILSTREAM* mailbox_open_create(MAILSTREAM* stream,
                                Store& store,
                                const string& boxname,
                                long c_client_options,
                                bool create) {
//
// Opens the mailbox "boxname" inside "store" with "c_client_options" options.
//
// If "create" is set it will try to create the mailbox. If the mailbox
// allready exists nothing will happen.
//
// The function will complain to STDERR on error.
//
// Returns NIL on failure.
//
//////////////////////////////////////////////////////////////////////////
  string fullboxname = full_mailbox_name(store, boxname);
  current_context_passwd = &(store.passwd);
  
  stream = mailbox_open_create(stream, fullboxname, c_client_options, create);
  if (!stream) {
    fprintf(stderr,"Error: Couldn't open %s\n", fullboxname.c_str());
  }
  
  return stream;
}

//-------------------------------- main ----------------------------------

//////////////////////////////////////////////////////////////////////////
//
int main(int argc, char** argv) {
//
//////////////////////////////////////////////////////////////////////////
  Channel channel;
  Store storea, storeb;
  StringSet allboxes;
  MAILSTREAM *storea_stream, *storeb_stream;
  map<string, StringSet> lasttime, thistime;
  StringSet deleted_boxes;   // present lasttime, but not this time
  StringSet empty_mailboxes;
  int success;

#include "linkage.c"

  // Parse arguments, read config file, choose mode
  {
    string config_file;
    int optind;
    vector<string> args;

    // All options must be given separately
    optind = 1;
    while (optind<argc && argv[optind][0]=='-') {
      switch (argv[optind][1]) {
      case 'n':
        no_expunge = 1;
        break;
      case 'm':
        show_from = 1;
        show_summary = 0;
        break;
      case 'M':
        show_from = 1;
        show_summary = 0;
        show_message_id = 1;
        break;
      case 's':
        printf("Only simulating\n");
        simulate = 1;
        no_expunge = 1;
        break;
      case 'v':
        if (argv[optind][2] == 'b')
          report_braindammaged_msgids = 1;
        else if (argv[optind][2] == 'w')
          log_warn = 1;
        else if (argv[optind][2] == 'p')
          log_parse = 1;
        else
          log_chatter = 1;
        break;
      case 'f':
        config_file = argv[++optind];
        break;
      case 'D':
        delete_empty_mailboxes = 1;
        break;
      case 'd':
        debug = 1;
        break;
      case 'c':
        if (argv[optind][2] == 'd')
          copy_deleted_messages = 1;
        else {
          usage();
          return 1;
        }
        break;
      default:
        usage();
        return 1;
      }
      optind++;
    }
    if (argc-optind<1) {
      usage();
      return 1;
    }
    for ( ; optind < argc; optind++) {
      args.push_back(argv[optind]);
    }

    map<string, ConfigItem> confmap;
    FILE* config;
    Store s;
    
    if (config_file == "") {
      char *home;
      home = getenv("HOME");
      if (home) {
        config_file = string(home) + "/.mailsync";
      } else {
        fprintf(stderr,"Error: Can't get home directory. Use `-f file'.\n");
        return 1;
      }
    }
    {
      struct stat st;
      stat(config_file.c_str(), &st);
      if (S_ISDIR(st.st_mode) || !(config=fopen(config_file.c_str(),"r"))) {
        fprintf(stderr,"Error: Can't open config file %s\n",config_file.c_str());
        return 1;
      }
    }
    parse_config(config, confmap);

    // Search for the desired store or channel; determine mode.
    
    vector<Store> stores;
    vector<Channel> channels;

    for (unsigned i=0; i<args.size(); i++) {
      if (confmap.count(args[i])==0) {
        fprintf(stderr, "Error: A channel or store named \"%s\" has not been configured\n", args[i].c_str());
        return 1;
      }
      ConfigItem* p = &(confmap[args[i]]);
      if (p->is_Store && p->store->tag == args[i]) {
        stores.push_back(*p->store);
      } else if (! p->is_Store && p->channel->tag == args[i]) {
        channels.push_back(*p->channel);
      }
    }

    if (channels.size() == 1 && stores.size() == 0) {
      mode = mode_sync;
      channel = channels[0];
      if (confmap[channel.store[0]].is_Store != 1 ||
          confmap[channel.store[1]].is_Store != 1) {
        fprintf(stderr,"Error: Malconfigured channel %s\n", channel.tag.c_str());
        if(confmap[channel.store[0]].is_Store != 1)
          fprintf(stderr,"The configuration doesn't contain a store named \"%s\"\n", channel.store[0].c_str());
        else
          fprintf(stderr,"The configuration doesn't contain a store named \"%s\"\n", channel.store[1].c_str());
        return 1;
      }
      storea = *confmap[channel.store[0]].store;
      storeb = *confmap[channel.store[1]].store;

      printf("Synchronizing stores \"%s\" <-> \"%s\"...\n",
             storea.tag.c_str(), storeb.tag.c_str());

    } else if (channels.size() == 1 && stores.size() == 1) {
      mode = mode_diff;
      expunge_duplicates = 0;
      channel = channels[0];
      storea = stores[0];
      if (channel.store[0] == storea.tag) {
        storeb = *confmap[channel.store[0]].store;
      } else if (channel.store[1] == storea.tag) {
        storeb = *confmap[channel.store[1]].store;
      } else {
        fprintf(stderr,"Diff mode: channel %s doesn't contain store %s.\n",
                channel.tag.c_str(), argv[optind+1]);
        return 1;
      }
      printf("Comparing store \"%s\" <-> \"%s\"...\n",
             storea.tag.c_str(), storeb.tag.c_str());

    } else if (channels.size() == 0 && stores.size() == 1) {
      mode = mode_list;
      storea = stores[0];

      printf("Listing store \"%s\"\n", storea.tag.c_str());

    } else {
      fprintf(stderr, "Don't know what to do with %d channels and %d stores.\n", channels.size(), stores.size());
      return 1;
    }
  }

  // initialize c-client environment (~/.imparc etc.)
  env_init(getenv("USER"),getenv("HOME"));

  // Open remote connections.
  if (storea.isremote) {
    current_context_passwd = &(storea.passwd);
    storea_stream = mail_open(NIL, nccs(storea.server), OP_HALFOPEN | OP_READONLY);
    if (!storea_stream) {
      fprintf(stderr,"Error: Can't contact first server %s\n",storea.server.c_str());
      return 1;
    }
  } else {
    storea_stream = NULL;
  }

  if (mode == mode_sync && storeb.isremote) {
    current_context_passwd = &(storeb.passwd);
    storeb_stream = mail_open(NIL, nccs(storeb.server), OP_HALFOPEN | OP_READONLY);
    if (!storeb_stream) {
      fprintf(stderr,"Error: Can't contact second server %s\n",storeb.server.c_str());
      return 1;
    }
  } else {
    storeb_stream = NULL;
  }

  // Get list of all mailboxes from each server.
  allboxes.clear();
  if (debug) printf(" Items in store \"%s\":\n", storea.tag.c_str());
  get_mail_list(storea_stream, storea, allboxes);
  if (debug)
    if (storea.delim)
      printf(" Delimiter for store \"%s\" is '%c'.\n",
             storea.tag.c_str(), storea.delim);
    else printf(" No delimiter found for store \"%s\".\n", storea.tag.c_str());
  if (storea.delim == '!')
    get_delim(storea_stream, storea);
  assert(storea.delim != '!');
  if (mode==mode_list) {
    if (show_from | show_message_id)
      for (StringSet::iterator box = allboxes.begin(); 
           box != allboxes.end(); box++) {
        printf("\nMailbox: %s\n\n", (*box).c_str());
        storea_stream = mailbox_open_create(storea_stream, storea, *box, 0, NOCREATE);
        if (! storea_stream) break;
        if (! list_contents(storea_stream))
          exit(1);
      }
    else
      print_list_with_delimiter(allboxes, stdout, "\n");
    return 0;
  }
  if (mode==mode_sync) {
    if (debug) printf(" Items in store \"%s\":\n", storeb.tag.c_str());
    get_mail_list(storeb_stream, storeb, allboxes);
    if (debug)
      if (storeb.delim)
        printf(" Delimiter for store \"%s\" is '%c'.\n",
               storeb.tag.c_str(), storeb.delim);
      else printf(" No delimiter found for store \"%s\"\n", storeb.tag.c_str());

    if (storeb.delim == '!')
      get_delim(storeb_stream, storeb);
    assert(storeb.delim != '!');
  }
  if (show_message_id) {
    printf(" All seen mailboxes: ");
    print_list_with_delimiter(allboxes, stdout, " ");
    printf("\n");
  }

  // Read the lasttime lists.
  {
    int success;
    success = read_lasttime_seen(channel, allboxes, lasttime, deleted_boxes);
    if (!success) 
      exit(1);
  }

  // Process each mailbox.
  
  success = 1;
  for (StringSet::iterator box = allboxes.begin(); 
       box != allboxes.end(); box++) {

    if (show_from)
      printf("\n *** %s ***\n", box->c_str());

    StringSet msgids_lasttime(lasttime[*box]), msgids_union, msgids_now;
    MsgIdPositions msgidpos_a, msgidpos_b;

    if (show_summary) {
      printf("%s: ",box->c_str());
      fflush(stdout);
    } else {
      printf("\n");
    }

    // fetch_message_ids(): map message-ids to message numbers.
    // And optionally delete duplicates. 
    //
    // Attention: from here on we're operating on streams to single
    //            _mailboxes_! That means that from here on
    //            streamx_stream is connected to _one_ specific
    //            mailbox.
   
    storea_stream = mailbox_open_create(storea_stream, storea, *box, 0, CREATE);
    if (! storea_stream) break;
    if (! fetch_message_ids(storea_stream, storea.tag, msgidpos_a))
      exit(1);
    if (mode==mode_sync) {
      storeb_stream = mailbox_open_create(storeb_stream, storeb, *box, 0, CREATE);
      if (!storeb_stream) break;
      if (! fetch_message_ids(storeb_stream, storeb.tag, msgidpos_b))
        exit(1);
    } else if (mode==mode_diff) {
      for (StringSet::iterator i=msgids_lasttime.begin(); i!=msgids_lasttime.end(); i++)
        msgidpos_b[*i] = 0;
    }

    // u = union(msgids_lasttime, a, b)
    msgids_union = msgids_lasttime;
    for (MsgIdPositions::iterator i=msgidpos_a.begin(); i!=msgidpos_a.end(); i++)
      msgids_union.insert(i->first);
    for (MsgIdPositions::iterator i=msgidpos_b.begin(); i!=msgidpos_b.end(); i++)
      msgids_union.insert(i->first);

    StringSet copy_a_b, copy_b_a, remove_a, remove_b;


    for (StringSet::iterator i=msgids_union.begin(); i!=msgids_union.end(); i++) {
      bool in_a = msgidpos_a.count(*i);
      bool in_b = msgidpos_b.count(*i);
      bool in_l = msgids_lasttime.count(*i);

      int abl = (  (in_a ? 0x100 : 0) 
                 + (in_b ? 0x010 : 0)
                 + (in_l ? 0x001 : 0) );

      switch (abl) {

      case 0x100:  // New message on a
        copy_a_b.insert(*i);
        msgids_now.insert(*i);
        break;

      case 0x010:  // New message on b
        copy_b_a.insert(*i);
        msgids_now.insert(*i);
        break;

      case 0x111:  // Kept message
      case 0x110:  // New message, present in a and b, no copying necessary
        msgids_now.insert(*i);
        break;

      case 0x101:  // Deleted on b
        remove_a.insert(*i);
        break;

      case 0x011:  // Deleted on a
        remove_b.insert(*i);
        break;

      case 0x001:  // Deleted on both
        break;

      case 0x000:  // Shouldn't happen
      default:
        assert(0);
        break;
      }


    }

    unsigned long now_n = msgids_now.size();

    switch (mode) {
    case mode_sync:
      {
        bool success;

        // we're first removing messages
        // if we'd first copy and the remove, mailclient would add a "Status: 0" line to
        // each mail. We don't want this wrt to MUA's who interpret such a line as "not new"
        // (in particular mutt!)

        if (debug) printf(" Removing messages from store \"%s\"\n", storea.tag.c_str() );

        unsigned long removed_a = 0;
        for (StringSet::iterator i=remove_a.begin(); i!=remove_a.end(); i++) {
          success = remove_message(storea_stream, storea, msgidpos_a[*i], *i, "< ");
          if (success) removed_a++;
        }

        if (debug) printf(" Removing messages from store \"%s\"\n", storeb.tag.c_str() );

        unsigned long removed_b = 0;
        for (StringSet::iterator i=remove_b.begin(); i!=remove_b.end(); i++) {
          success = remove_message(storeb_stream, storeb, msgidpos_b[*i], *i, "> ");
          if (success) removed_b++;
        }

        string fullboxnamea = full_mailbox_name(storea, *box);
        string fullboxnameb = full_mailbox_name(storeb, *box);

        // strangely enough, following Mark Crispin, if you write into an open
        // stream then it'll mark new messages as seen.
        //
        // So if we want to write to a remote mailbox we have to HALF_OPEN the stream
        // and if we're working on a local mailbox then we have to use a NIL stream.

        if (debug) printf(" Copying messages from store \"%s\" to store \"%s\"\n",
                          storea.tag.c_str(), storeb.tag.c_str() );

        if (! storeb.isremote) {
          mail_close(storeb_stream);
          storeb_stream = NIL;
        } else 
          storeb_stream = mailbox_open_create(storeb_stream, storeb, *box, 0, NOCREATE);
        unsigned long copied_a_b = 0;
        for (StringSet::iterator i=copy_a_b.begin(); i!=copy_a_b.end(); i++) {
          success = copy_message(storea_stream, storea.passwd, msgidpos_a[*i], *i,
                                storeb_stream, storeb.passwd, fullboxnameb, "->", channel);
          if (success)
            copied_a_b++;
          else                    // we've failed to copy the message over
            msgids_now.erase(*i); // as we should've had. So let's just assume
                                  // that we haven't seen it at all. That way
                                  // mailsync will have to rediscover and resync
                                  // the same message again next time
        }

        if (debug) printf(" Copying messages from store \"%s\" to store \"%s\"\n",
                          storeb.tag.c_str(), storea.tag.c_str() );

        if (!storeb.isremote)   // reopen the stream if it was closed before
          storeb_stream = mailbox_open_create(NIL, storeb, *box, OP_READONLY, NOCREATE);
        if (!storea.isremote) { // close the stream for writing (sic - the beauty of c-client!) !!
          mail_close(storea_stream);
          storea_stream = NIL;
        } else 
          storea_stream = mailbox_open_create(storea_stream, storea, *box, 0, NOCREATE);
        unsigned long copied_b_a = 0;
        for (StringSet::iterator i=copy_b_a.begin(); i!=copy_b_a.end(); i++) {
          success = copy_message(storeb_stream, storeb.passwd, msgidpos_b[*i], *i,
                                 storea_stream, storea.passwd, fullboxnamea, "<-", channel);
          if (success)
            copied_b_a++;
          else
            msgids_now.erase(*i);
        }
        
        printf("\n");
        if (copied_a_b)
          printf("%lu copied %s->%s.\n", copied_a_b,
                 storea.tag.c_str(), storeb.tag.c_str());
        if (copied_b_a)
          printf("%lu copied %s->%s.\n", copied_b_a,
                 storeb.tag.c_str(), storea.tag.c_str());
        if (removed_a)
          printf("%lu deleted on %s.\n", removed_a, storea.tag.c_str());
        if (removed_b)
          printf("%lu deleted on %s.\n", removed_b, storeb.tag.c_str());
        if (show_summary) {
          printf("%lu remain%s.\n", now_n, now_n!=1 ? "" : "s");
          fflush(stdout);
        } else {
          printf("%lu messages remain in %s\n", now_n, box->c_str());
        }
      }
      break;

    case mode_diff:
      {
        if (copy_a_b.size())
          printf("%d new, ", copy_a_b.size());
        if (remove_b.size())
          printf("%d deleted, ", remove_b.size());
        printf("%d currently at store %s.\n", msgids_now.size(), storeb.tag.c_str());
      }
      break;

    default:
      break;
    }

    thistime[*box] = msgids_now;

    if (!no_expunge) {
      if (!storea.isremote)   // reopen the stream if it was closed before - needed for expunge
        storea_stream = mailbox_open_create(NIL, storea, *box, 0, NOCREATE);
      current_context_passwd = &(storea.passwd);
      if (!simulate) mail_expunge(storea_stream);
      if (storeb_stream) {
        if (!storeb.isremote)   // reopen the stream in write mode it was closed before - needed for expunge
          storeb_stream = mailbox_open_create(NIL, storeb, *box, 0, NOCREATE);
        current_context_passwd = &(storeb.passwd);
        if (!simulate) mail_expunge(storeb_stream);
      }
      printf("Mails expunged\n");
    }

    if (!storea.isremote)
      storea_stream = mail_close(storea_stream);
    if (storeb_stream && !storeb.isremote)
      storeb_stream = mail_close(storeb_stream);

    if (delete_empty_mailboxes) {
      if (now_n == 0) {
        empty_mailboxes.insert(*box);
      }
    }

  }

  if (storea.isremote) storea_stream = mail_close(storea_stream);
  if (storeb.isremote) storeb_stream = mail_close(storeb_stream);

  if (!success)
    return 1;

  if (delete_empty_mailboxes) {
    string fullboxname;

    if (storea.isremote) {
      current_context_passwd = &(storea.passwd);
      storea_stream = mail_open(NIL, nccs(storea.server), OP_HALFOPEN);
    } else {
      storea_stream = NULL;
    }
    if (storeb.isremote) {
      current_context_passwd = &(storeb.passwd);
      storeb_stream = mail_open(NIL, nccs(storeb.server), OP_HALFOPEN);
    } else {
      storeb_stream = NULL;
    }
    for (StringSet::iterator i=empty_mailboxes.begin(); 
         i!=empty_mailboxes.end(); i++) {
      fullboxname = full_mailbox_name(storea, *i);
      printf("%s: deleting\n", i->c_str());
      printf("  %s", fullboxname.c_str());
      fflush(stdout);
      current_context_passwd = &(storea.passwd);
      if (mail_delete(storea_stream, nccs(fullboxname)))
        printf("\n");
      else
        printf(" failed\n");
      fullboxname = full_mailbox_name(storeb, *i);
      printf("  %s", fullboxname.c_str());
      fflush(stdout);
      current_context_passwd = &(storeb.passwd);
      if (mail_delete(storeb_stream, nccs(fullboxname))) 
        printf("\n");
      else
        printf(" failed\n");
    }
  }

  if (mode==mode_sync)
    if (!simulate)
      write_lasttime_seen(channel, allboxes, deleted_boxes, thistime);

  return 0;
}
         

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
//////////////////////////////////////////////////////////////////////////


//////////////////////////////////////////////////////////////////////////
//
void mm_list (MAILSTREAM *stream,int delimiter,char *name_nc,long attributes)
//
// called by c-client's mail_list to give us a mailbox name that matches
// our search pattern together with it's attributes 
//
// mm_list will save the mailbox names in the "matching_mailbox_names"
// StringSet
//
//////////////////////////////////////////////////////////////////////////
{
  const char* name = name_nc;
  
  if( debug) {
    fputs ("  ", stdout);
    if (stream&&stream->mailbox)
      fputs (stream->mailbox, stdout);
    if (name) fputs(name, stdout);
    if (attributes & LATT_NOINFERIORS) fputs (", leaf",stdout);
    if (attributes & LATT_NOSELECT) fputs (", no select",stdout);
    if (attributes & LATT_MARKED) fputs (", marked",stdout);
    if (attributes & LATT_UNMARKED) fputs (", unmarked",stdout);
    putchar ('\n');
  }

  if (!match_pattern_store) {
    // Internal error
    abort();
  }

  // Delimiter
  match_pattern_store->delim = delimiter;

  // If the mailbox is "selectable", i.e. contains messages,
  // store the name in the appropriate StringSet.
  if (!(attributes & LATT_NOSELECT) && *name) {
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
    matching_mailbox_names->insert(string(name_copy));
  }
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
      if (log_chatter)
        fprintf (stderr,"[%s]\n",string);
      break;
    case PARSE:                   // Parsing problem
      if (log_parse) 
        fprintf (stderr,"Parsing error: %%%s\n",string);
      break;
    case WARN:                    // Warning
      if (log_warn) 
        fprintf (stderr,"Warning: %%%s\n",string);
      break;
    case ERROR:                   // Error
    default:
      if (log_error)
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
