/* Please use spaces instead of tabs */

#define VERSION "4.4.1"

#include <ctype.h>
#include <stdio.h>
#include <errno.h>
extern int errno;               /* Just in case */
#include <sys/stat.h>           /* Stat() */

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

#define DEFAULT_DELIMITER '/'

/*
 * Options
 */
int log_chatter = 0;
int show_summary = 1;           /* 1 line of output per mailbox */
int show_from = 0;              /* 1 line of output per message */
int show_message_id = 0;        /* Implies show_from */
int no_expunge = 0;
enum { mode_unknown, mode_sync, mode_list, mode_diff } mode = mode_unknown;
int delete_empty_mailboxes = 0;
int debug = 0;

/*
 * Mandatory options
 */
int kill_duplicates = 1;
int log_error = 1;
int log_warn = 0;

typedef set<string> StringSet;

int critical = NIL;             /* Flag saying in critical code */
char *getpass();

void save(const StringSet& set, FILE* f, const string& delim) {
  for (StringSet::iterator i=set.begin(); i!=set.end(); i++)
    fprintf(f, "%s%s", i->c_str(), delim.c_str());
}

/*
 * C-Client doesn't declare anything const
 * If you're paranoid, you can allocate a new char[] here
 */
char* nccs(const string& s) {
  return (char*) s.c_str();
}

/*
 * Prints a string into a file (or std[out|err]) while
 * replacing " " by "\ "
 */
void print_with_escapes(FILE* f, const string& str) {
  const char *s = str.c_str();
  while (*s) {
    if (isspace(*s))
      fputc('\\',f);
    fputc(*s,f);
    s++;
  }
  return;
}

struct Passwd {
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

/*
 * The password for the current context
 * Required, because in the c-client callback functions we don't know
 * in which context (store1, store2, channel) we are
 */
Passwd * current_context_passwd = NULL;

struct Store {
  string tag, server, prefix;   /* Tag == store name */
  string ref, pat;
  Passwd passwd;
  int isremote;     /* I.e. allows OP_HALFOPEN */
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
    if (server!="") { fprintf(f,"\n\tserver "); print_with_escapes(f,server); }
    if (prefix!="") { fprintf(f,"\n\tprefix "); print_with_escapes(f,prefix); }
    if (ref!="") { fprintf(f,"\n\tref "); print_with_escapes(f,ref); }
    if (pat!="") { fprintf(f,"\n\tpat "); print_with_escapes(f,pat); }
    if (! passwd.nopasswd) { fprintf(f,"\n\tpasswd ");
                             print_with_escapes(f,passwd.text); }
    fprintf(f,"\n}\n");
    return;
  }

  void set_passwd(string password) { passwd.set_passwd(password);}
};

struct Channel {
  string tag;           /* Tag == channel name */
  string store[2];
  string msinfo;
  Passwd passwd;

  void clear() {
    tag = store[0] = store[1] = msinfo = "";
    passwd.clear();
    return;
  }

  Channel() { clear(); }

  void print(FILE* f) {
    fprintf(f,"channel %s %s %s {",tag.c_str(),store[0].c_str(),store[1].c_str());
    if (msinfo != "") { fprintf(f,"\n\tmsinfo "); print_with_escapes(f,msinfo); }
    if (! passwd.nopasswd) { fprintf(f,"\n\tpasswd ");
                             print_with_escapes(f,passwd.text);} 
    fprintf(f,"\n}\n");
    return;
  }

  void set_passwd(string password) { passwd.set_passwd(password);}
};

struct ConfigItem {
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
};

/*
 * Print out a ConfigItem
 */
void print_ConfigItem(ConfigItem& conf, FILE* f) {
  if (conf.is_Store)
    conf.store->print(f);
  else
    conf.channel->print(f);
}

/*
 * A token is a [space|newline|tab] delimited chain of characters which
 * represents a syntactic element
 */
struct Token {
  string buf;
  int eof;
  int line;
};

/*
 * Fatal error while parsing the config file
 * Display error message and quit
 *
 * errorMessage       must contain an error message to be displayed
 * insertIntoMessage  is optional. errorMessage can act as a sprintf format
 *                    string in which case insertIntoMessage will be used
 *                    as replacement for "%s" inside errorMessage
 */
void die_with_fatal_parse_error(Token* t, char * errorMessage, const char * insertIntoMessage = NULL) {
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

/* 
 * Read a token from the config file
 */
void get_token(FILE* f, Token* t) {
  int c;
  t->buf = "";
  t->eof = 0;
  while (1) {
    if (t->buf.size() > MAILTMPLEN)
      /* Token too long */
      die_with_fatal_parse_error(t, "Line too long");
    c = getc(f);
   
    /* Ignore comments */
    if ( t->buf.size() == 0 && c=='#') {
      while( c != '\n' && c !=EOF) {
        c = getc(f);
      }
    }
    
    /* Skip newline */
    if (c=='\n')
      t->line++;

    /* We're done reading the config file */
    if (c==EOF) {
      t->eof = 1;
      break;

    /* We've reached a token boundary */
    } else if (isspace(c)) {
      /* The token is non empty so we've acquired something and can return */
      if (t->buf.size()) {
        break;
      /* Ignore spaces */
      } else {
        continue;
      }
    /* Continue on next line... */
    } else if (c=='\\') {
      t->buf += (char)getc(f);
    /* Add to token */
    } else {
      t->buf += (char)c;
    }
  }
  if (debug) printf("\t%s\n",t->buf.c_str());
  return;
}

/* 
 * Parse config file
 */
void Config_parse(FILE* f, map<string, ConfigItem>& confmap) {
  Token token;
  Token *t;

  if (debug) printf(" Parsing config...\n\n");

  t = &token;
  t->line = 0;

  /* Read items from file */
  while (1) {
    ConfigItem* conf = new ConfigItem();
    string tag;
    conf->is_Store = -1;

    /* Acquire one item */
    get_token(f, t);

    /* End of config file reached */
    if (t->eof)
      break;
    
    /* Parse store */
    if (t->buf == "store") {
      conf->is_Store = 1;
      conf->store = new Store();
      /* Read tag (store name) */
      get_token(f, t);
      if (t->eof)
        die_with_fatal_parse_error(t, "Store name missing");
      tag = t->buf;
      conf->store->tag = t->buf;
      /* Read "{" */
      get_token(f, t);
      if (t->eof || t->buf != "{")
        die_with_fatal_parse_error(t, "Expected \"{\" while parsing store");
      /* Read store configuration (name/value pairs) */
      while (1) {
        get_token(f, t);
        /* End of store config */
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

    /* Parse a channel */
    }
    else if (t->buf == "channel") {
      conf->is_Store = 0;
      conf->channel = new Channel;
      /* Read tag (channel name) */
      get_token(f, t);
      if (t->eof) {
        die_with_fatal_parse_error(t, "Channel name missing");
      }
      tag = t->buf;
      conf->channel->tag = t->buf;
      /* Read stores */
      get_token(f, t);
      if (t->eof)
        die_with_fatal_parse_error(t, "Expected a store name while parsing channel");
      conf->channel->store[0] = t->buf;
      get_token(f, t);
      if (t->eof)
        die_with_fatal_parse_error(t, "Expected a store name while parsing channel");
      conf->channel->store[1] = t->buf;
      /* Read "{" */
      get_token(f, t);
      if (t->eof || t->buf != "{")
        die_with_fatal_parse_error(t, "Expected \"{\" while parsing channel");
      /* Read channel config (name/value pairs) */
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

/* 
 * Given the name of a mailbox on a Store, return its full IMAP name.
 */
string Store_mailbox(Store s, const string& box) {
  string boxname = box;
  string fullbox;
  /* Replace our DEFAULT_DELIMITER by the respective store delimiter */
  for (unsigned int i=0; i<boxname.size(); i++)
    if( boxname[i] == DEFAULT_DELIMITER) boxname[i] = s.delim;
  fullbox = s.server + s.prefix + boxname;
  return fullbox;
}

static StringSet*   matching_mailbox_names;
static Store*       match_pattern_store;

/*
 * Fetch a list of mailbox-names from "stream" that match the pattern
 * given by "store" and put them into "mailbox_names".
 *
 * "mailbox_names" is filled up through the callback function "mm_list"
 * by c-client's mail_list function
 *
 * This function corresponds to c-client's mail_list
 */
void get_mail_list(MAILSTREAM *stream, Store& store, StringSet& mailbox_names) {
  match_pattern_store = &store;
  matching_mailbox_names = &mailbox_names;
  current_context_passwd = &(store.passwd);
  mail_list(stream, nccs(store.ref), nccs(store.pat));
  match_pattern_store = NULL;
  matching_mailbox_names = NULL;
}

/*
 * Print formats that are being used when showin, what mails are being
 * transfered or killed:
 *
 * "  copied     <-  Mon Sep  9 Tomas Pospisek         test2
 *                     <Pine.LNX.4.44.0209092123140.24399-100000@petertosh>"
 * |--lead_format--|-------------from_format------------------|
 *                     |--------------------msgid_format-------------------|
 *
 * 
 *  "lead_format"  and
 *  "from_format"  are only used/displayed with options -m and/or -M
 *  "msgid_format" is only used/displayed with option -M
 *
 */
char lead_format[] = "  %-10s %s  ";    /* arguments:  action, direction */
char from_format[] = "%-61s";
char msgid_format[] = "%-65s";            /* argument :  message-id */

/*
 * Some forward declarations
 */
void simplify_message_id(string& msgid);
int read_lasttime(Channel channel,
                  StringSet& boxes, 
                  map<string, StringSet>& mids, 
                  StringSet& extra);
int write_lasttime(Channel channel,
                   const StringSet& boxes, 
                   const StringSet& deleted_boxes,
                   map<string, StringSet>& lasttime);
MAILSTREAM* tdc_mail_open_create_if_nec(MAILSTREAM* stream, 
                                        const string& fullboxname, 
                                        long options);
string summary(MAILSTREAM* stream, long msgno);

/*
 * Mailsync help
 */
void usage() {
  printf("mailsync %s\n\n", VERSION);
  printf("usage: mailsync [options] channel\n");
  printf("\nOptions:\n");
  printf("  -f file  use alternate config file\n");
  printf("  -n       don't expunge mailboxes\n");
  printf("  -D       delete any empty mailboxes after synchronizing\n");
  printf("  -m       show from, subject, etc. of messages that are killed or moved\n");
  printf("  -M       also show message-ids (turns on -m)\n");
  printf("  -v       show imap chatter\n");
  printf("  -d       show debug info\n");
  return;
}

/*
 * Determine the mailbox delimiter that "stream" is using.
 */
void get_delim(MAILSTREAM*& stream, Store& store_in) {
  Store store = store_in;
  store.prefix = "";
  store.pat = "INBOX";
  StringSet boxes;
  get_mail_list(stream, store, boxes);
  store_in.delim = store.delim;
  return;
}

/*
 * Find out what we've seen the last time we did a sync
 *
 * returns:
 *              1 - success
 *              0 - failure
 */
int read_lasttime(Channel channel,
                  StringSet& boxes, 
                  map<string, StringSet>& mids, 
                  StringSet& extra) {
  MAILSTREAM* msinfo_stream;
  ENVELOPE* envelope;
  unsigned long j, k;
  string currentbox;
  char* text;
  unsigned long textlen;

  /* msinfo is the name of the mailbox that contains the sync info */
  current_context_passwd = &(channel.passwd);
  msinfo_stream = tdc_mail_open_create_if_nec(NULL, nccs(channel.msinfo), 
                                              OP_READONLY);
  if (!msinfo_stream) {
    fprintf(stderr,"Error: Couldn't open lasttime box %s\n",
                   channel.msinfo.c_str());
    return 0;
  }

  for (j=1; j<=msinfo_stream->nmsgs; j++) {
    envelope = mail_fetchenvelope(msinfo_stream, j);
    /* The subject line contains the name of the channel */
    if (channel.tag == envelope->subject) {
      /* Found our lasttime */
      int instring;

      text = mail_fetchtext_full(msinfo_stream, j, &textlen, FT_INTERNAL);
      if (text)
        text = strdup(text);
      else
        return 0;
      instring = 0;
      /* Replace spaces with '\0' */
      for (k=0; k<textlen; k++) {
        if (isspace(text[k])) {
          text[k] = '\0';
          instring = 0;
        } else {
          if (!instring) {
            /* Do nothing in particular */
          }
          instring = 1;
        }
      }
      /* Check each box against `boxes' */
      instring = 0;
      for (k=0; k<textlen; k++) {
        if (text[k] == '\0') {
          instring = 0;
        } else {
          if (!instring) {
            if (text[k] != '<') {
              /* Mailbox name */
              if (boxes.count(&text[k])) {
                currentbox = string(&text[k]);
              } else {
                currentbox = "";
                extra.insert(&text[k]);
              }
            } else {
              /* Message-id */
              if (currentbox != "") {
                mids[currentbox].insert(&text[k]);
              }
            }
          }
          instring = 1;
        }
      }
      free(text);
      break;    /* Stop searching for message */
    }
  }
  
  if (show_message_id) {
    printf("lasttime %s: ", channel.msinfo.c_str());
    for (StringSet::iterator b = boxes.begin(); b!=boxes.end(); b++) {
      printf("%s(%d) ", b->c_str(), mids[*b].size());
    }
    printf("\n");
  }

  mail_close(msinfo_stream);
  return 1;
}

int fetch_message_ids(MAILSTREAM*& stream, Store& store, 
                      const string& box, map<string,int>& m) {
  string fullboxname = Store_mailbox(store, box);
  current_context_passwd = &(store.passwd);
  stream = tdc_mail_open_create_if_nec(stream, fullboxname, 0);
  if (!stream) {
    fprintf(stderr,"Error: Couldn't open %s\n", fullboxname.c_str());
    return 0;
  }
  int n = stream->nmsgs;
  int nabsent = 0, nduplicates = 0;
  for (int j=1; j<=n; j++) {
    string msgid;
    ENVELOPE *envelope;
    bool isdup;

    envelope = mail_fetchenvelope(stream, j);
    if (!(envelope->message_id)) {
      nabsent++;
      /* Absent message-id.  Don't touch message. */
      continue;
    }
    msgid = string(envelope->message_id);
    simplify_message_id(msgid);
    isdup = m.count(msgid);
    if (isdup) {
      if (kill_duplicates) {
        char seq[30];
        sprintf(seq,"%d",j);
        mail_setflag(stream, seq, "\\Deleted");
      }
      nduplicates++;
      if (show_from) printf(lead_format,"duplicate", "");
    } else {
      m.insert(make_pair(msgid, j));
    }
    if (isdup && show_from) {
      printf(from_format, summary(stream,j).c_str());
      if (show_message_id) printf(msgid_format, msgid.c_str());
      printf("\n");
    }
  }
  if (nduplicates) {
    if (show_summary) {
      printf("%d duplicate%s", nduplicates, nduplicates==1 ? "" : "s");
      if (mode!=mode_diff)
        printf(" in %s", store.tag.c_str());
      printf(", ");
    } else {
      printf("%d duplicates deleted from %s/%s\n",
             nduplicates, store.tag.c_str(), box.c_str());
    }
    fflush(stdout);
  }
  return 1;
}

int copy_message(MAILSTREAM*& storea_stream, Store& storea, 
                 int msgno, const string& msgid,
                 MAILSTREAM*& storeb_stream, Store& storeb,
                 char * direction) {
  MSGDATA md;
  STRING CCstring;
  char flags[MAILTMPLEN];
  char msgdate[MAILTMPLEN];
  ENVELOPE *envelope;
  MESSAGECACHE *elt;

  current_context_passwd = &(storea.passwd);
  envelope = mail_fetchenvelope(storea_stream, msgno);
  string msgid_fetched;

  /* Check message-id. */
  if (!(envelope->message_id) || *(envelope->message_id)!='<') {
    printf("Warning: missing message-id: %s, message %d %s\n",
           storea_stream->mailbox, msgno,
           envelope->message_id ? envelope->message_id : "NULL");
  } else {
    msgid_fetched = envelope->message_id;
    simplify_message_id(msgid_fetched);
    if (msgid_fetched != msgid) {
      printf("Warning: suspicious message-id: %s, message %d %s\n",
             storea_stream->mailbox, msgno, msgid_fetched.c_str());
    }
  }

  elt = mail_elt(storea_stream, msgno);
  assert(elt->valid); /* Should be valid because of fetchenvelope() */

  memset(flags, 0, MAILTMPLEN);
  if (elt->seen) strcat (flags," \\Seen");
  if (elt->deleted) strcat (flags," \\Deleted");
  if (elt->flagged) strcat (flags," \\Flagged");
  if (elt->answered) strcat (flags," \\Answered");
  if (elt->draft) strcat (flags," \\Draft");
  md.stream = storea_stream;
  md.msgno = msgno;
  INIT (&CCstring, msg_string, (void*) &md, elt->rfc822_size);
  current_context_passwd = &(storeb.passwd);
  int rv = mail_append_full(storeb_stream, storeb_stream->mailbox,
                            &flags[1], mail_date(msgdate,elt), 
                            &CCstring);
  if (show_from) {
    printf(lead_format, rv ? "copied" : "copyfail", direction);
    printf(from_format, summary(storea_stream, msgno).c_str());
    if (show_message_id) printf(msgid_format, msgid.c_str());
    printf("\n");
  }
  return rv;
}


int remove_message(MAILSTREAM*& stream, Store& store, 
                   int msgno, const string& msgid,
                   char * place) {
  string msgid_fetched;
  ENVELOPE *envelope;
  int ok = 1;
  
  current_context_passwd = &(store.passwd);
  envelope = mail_fetchenvelope(stream, msgno);
  if (!(envelope->message_id)) {
    printf("Error: no message-id, so I won't kill the message.\n");
    /* Possibly indicates concurrent access? */
    ok = 0;
  } else {
    msgid_fetched = envelope->message_id;
    simplify_message_id(msgid_fetched);
    if (msgid_fetched != msgid) {
      printf("Error: message-ids don't match, so I won't kill the message.\n");
      ok = 0;
    }
  }
  
  if (ok) {
    char seq[30];
    sprintf(seq,"%d",msgno);
    mail_setflag(stream, seq, "\\Deleted");
  }
  if (show_from) {
    if (ok) {
      printf(lead_format, "killed", place);
    } else
      printf(lead_format, "killfail", "");
    printf(from_format, summary(stream,msgno).c_str());
    printf("\n");
  }
  return ok;
}


int main(int argc, char** argv) {
  Channel channel;
  Store storea, storeb;
  StringSet allboxes;
  MAILSTREAM *storea_stream, *storeb_stream;
  map<string, StringSet> lasttime, thistime;
  StringSet deleted_boxes;   /* present lasttime, but not this time */
  StringSet empty_mailboxes;
  int ok;

#include "linkage.c"

  /* Parse arguments, read config file, choose mode */
  {
    string config_file;
    int optind;
    vector<string> args;

    /* All options must be given separately */
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
      case 'v':
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
    Config_parse(config, confmap);

    /* Search for the desired store or channel; determine mode. */
    
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
      kill_duplicates = 0;
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

  /* Open remote connections. */

  if (storea.isremote) {
    current_context_passwd = &(storea.passwd);
    storea_stream = mail_open(NIL, nccs(storea.server), OP_HALFOPEN);
    if (!storea_stream) {
      fprintf(stderr,"Error: Can't contact first server %s\n",storea.server.c_str());
      return 1;
    }
  } else {
    storea_stream = NULL;
  }

  if (mode == mode_sync && storeb.isremote) {
    current_context_passwd = &(storeb.passwd);
    storeb_stream = mail_open(NIL, nccs(storeb.server), OP_HALFOPEN);
    if (!storeb_stream) {
      fprintf(stderr,"Error: Can't contact second server %s\n",storeb.server.c_str());
      return 1;
    }
  } else {
    storeb_stream = NULL;
  }

  /* Get list of all mailboxes from each server. */
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
    save(allboxes, stdout, "\n");
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
    save(allboxes, stdout, " ");
    printf("\n");
  }

  /* Read the lasttime lists. */
  {
    int success;
    success = read_lasttime(channel, allboxes, lasttime, deleted_boxes);
    if (!success) 
      exit(1);
  }

  /* Process each mailbox. */
  
  ok = 1;
  for (StringSet::iterator box = allboxes.begin(); 
       box != allboxes.end(); box++) {

    if (show_from)
      printf("\n *** %s ***\n", box->c_str());

    StringSet l(lasttime[*box]), u, now;
    map<string, int> a, b;

    if (show_summary) {
      printf("%s: ",box->c_str());
      fflush(stdout);
    } else {
      printf("\n");
    }

    /* fetch_message_ids(): map message-ids to message numbers.
       And optionally delete duplicates. 
       Also, fetch_message_ids opens/creates the mailbox.
    */
    current_context_passwd = &(storea.passwd);
    ok = fetch_message_ids(storea_stream, storea, *box, a);
    if (!ok) break;
    if (mode==mode_sync) {
      current_context_passwd = &(storeb.passwd);
      ok = fetch_message_ids(storeb_stream, storeb, *box, b);
      if (!ok) break;
    } else if (mode==mode_diff) {
      for (StringSet::iterator i=l.begin(); i!=l.end(); i++)
        b[*i] = 0;
    }

    /* u = union(l, a, b) */
    u = l;
    for (map<string,int>::iterator i=a.begin(); i!=a.end(); i++)
      u.insert(i->first);
    for (map<string,int>::iterator i=b.begin(); i!=b.end(); i++)
      u.insert(i->first);

    StringSet copy_a_b, copy_b_a, remove_a, remove_b;

    for (StringSet::iterator i=u.begin(); i!=u.end(); i++) {
      bool in_a = a.count(*i);
      bool in_b = b.count(*i);
      bool in_l = l.count(*i);

      int abl = (  (in_a ? 0x100 : 0) 
                 + (in_b ? 0x010 : 0)
                 + (in_l ? 0x001 : 0) );

      switch (abl) {

      case 0x100:  /* New message on a */
        copy_a_b.insert(*i);
        now.insert(*i);
        break;

      case 0x010:  /* New message on b */
        copy_b_a.insert(*i);
        now.insert(*i);
        break;

      case 0x111:  /* Kept message */
      case 0x110:  /* New message, no copying necessary */
        now.insert(*i);
        break;

      case 0x101:  /* Deleted on b */
        remove_a.insert(*i);
        break;

      case 0x011:  /* Deleted on a */
        remove_b.insert(*i);
        break;

      case 0x001:  /* Deleted on both */
        break;

      case 0x000:  /* Shouldn't happen */
      default:
        assert(0);
        break;
      }


    }

    int now_n = now.size();

    switch (mode) {
    case mode_sync:
      {
        for (StringSet::iterator i=copy_a_b.begin(); i!=copy_a_b.end(); i++)
          copy_message(storea_stream, storea, a[*i], *i,
                       storeb_stream, storeb, "->");
        for (StringSet::iterator i=copy_b_a.begin(); i!=copy_b_a.end(); i++)
          copy_message(storeb_stream, storeb, b[*i], *i,
                       storea_stream, storea, "<-");
        for (StringSet::iterator i=remove_a.begin(); i!=remove_a.end(); i++)
          remove_message(storea_stream, storea, a[*i], *i, "< ");
        for (StringSet::iterator i=remove_b.begin(); i!=remove_b.end(); i++)
          remove_message(storeb_stream, storeb, b[*i], *i, "> ");

        printf("\n");
        if (copy_a_b.size())
          printf("%d copied %s->%s.\n", copy_a_b.size(),
                 storea.tag.c_str(), storeb.tag.c_str());
        if (copy_b_a.size())
          printf("%d copied %s->%s.\n", copy_b_a.size(),
                 storeb.tag.c_str(), storea.tag.c_str());
        if (remove_a.size())
          printf("%d deleted on %s.\n", remove_a.size(), storea.tag.c_str());
        if (remove_b.size())
          printf("%d deleted on %s.\n", remove_b.size(), storeb.tag.c_str());
        if (show_summary) {
          printf("%d remain%s.\n", now_n, now_n!=1 ? "" : "s");
          fflush(stdout);
        } else {
          printf("%d messages remain in %s\n", now_n, box->c_str());
        }
      }
      break;

    case mode_diff:
      {
        if (copy_a_b.size())
          printf("%d new, ", copy_a_b.size());
        if (remove_b.size())
          printf("%d deleted, ", remove_b.size());
        printf("%d currently at store %s.\n", now.size(), storeb.tag.c_str());
      }
      break;

    default:
      break;
    }

    thistime[*box] = now;

    if (!no_expunge) {
      current_context_passwd = &(storea.passwd);
      mail_expunge(storea_stream);
      if (storeb_stream) {
        current_context_passwd = &(storeb.passwd);
        mail_expunge(storeb_stream);
      }
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

  if (!ok)
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
      fullboxname = Store_mailbox(storea, *i);
      printf("%s: deleting\n", i->c_str());
      printf("  %s", fullboxname.c_str());
      fflush(stdout);
      current_context_passwd = &(storea.passwd);
      if (mail_delete(storea_stream, nccs(fullboxname)))
        printf("\n");
      else
        printf(" failed\n");
      fullboxname = Store_mailbox(storeb, *i);
      printf("  %s", fullboxname.c_str());
      fflush(stdout);
      current_context_passwd = &(storeb.passwd);
      if (mail_delete(storeb_stream, nccs(fullboxname))) 
        printf("\n");
      else
        printf(" failed\n");
    }
  }

  if (mode==mode_sync) {
    write_lasttime(channel, allboxes, deleted_boxes, thistime);
  }
  return 0;
}
         
int write_lasttime(Channel channel,
                   const StringSet& boxes, 
                   const StringSet& deleted_boxes,
                   map<string, StringSet>& lasttime) {
  MAILSTREAM* stream;
  ENVELOPE* envelope;
  unsigned long j;

  current_context_passwd = &(channel.passwd);
  stream = tdc_mail_open_create_if_nec(NIL, nccs(channel.msinfo), 0);
  if (!stream) {
    return 0;
  }
  for (j=1; j<=stream->nmsgs; j++) {
    envelope = mail_fetchenvelope(stream, j);
    if (envelope->subject == channel.tag) {
      char seq[30];
      sprintf(seq,"%lu",j);
      mail_setflag(stream, seq, "\\Deleted");
    }
  }
  mail_expunge(stream);
  
  /* Write a temporary file. */
  { 
    FILE* f;
    long flen;
    STRING CCstring;

    f = tmpfile();
    fprintf(f,"From: mailsync\nSubject: %s\n\n",channel.tag.c_str());
    for (StringSet::iterator i=boxes.begin(); i!=boxes.end(); i++) {
      if (! deleted_boxes.count(*i)) {
        fprintf(f,"%s\n", i->c_str());
        save(lasttime[*i], f, "\n");
      }
    }
    flen = ftell(f);
    rewind(f);
    INIT(&CCstring, file_string, (void*) f, flen);
    if (!mail_append(stream, nccs(channel.msinfo), &CCstring)) {
      fprintf(stderr,"Error: Can't append lasttime to %s\n",channel.msinfo.c_str());
      fclose(f);
      mail_close(stream);
      return 0;
    }
    fclose(f);
  }

  mail_close(stream);
  return 1;
}

string summary(MAILSTREAM* stream, long msgno) {
  static char from[66];
  MESSAGECACHE* elt;

  elt = mail_elt(stream, msgno);
  mail_cdate(&from[0], elt);
  mail_fetchfrom(&from[11], stream, msgno, 22);
  from[33] = ' ';
  mail_fetchsubject(&from[34], stream, msgno, 65-35-1);
  from[65] = '\0';
  return string(&from[0]);
}

void simplify_message_id(string& msgid) {
  int removed_blanks = 0;
  for (unsigned i=0; i<msgid.size(); i++) {
    if (isspace(msgid[i]) || iscntrl(msgid[i])) {
      msgid[i] = '.';
      removed_blanks = 1;
    }
  }
  if (removed_blanks)
    fprintf(stderr,"%%Warning: replaced blanks with . in %s\n",msgid.c_str());
}

MAILSTREAM* tdc_mail_open_create_if_nec(MAILSTREAM* stream, 
                                        const string& fullboxname,
                                        long options) {
  int o;
  
  o = log_error;
  log_error = 0;
  mail_create(stream, nccs(fullboxname));
  log_error = o;
  stream = mail_open(stream, nccs(fullboxname), options);
  return stream;
}

/*
 * Callback function
 *
 * called by c-client's mail_list to give us a mailbox name that matches
 * our search pattern together with it's attributes 
 *
 * mm_list will save the mailbox names in the "matching_mailbox_names"
 * StringSet
 */
void mm_list (MAILSTREAM *stream,int delimiter,char *name_nc,long attributes)
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
    /* Internal error */
    abort();
  }

  /* Delimiter */

  match_pattern_store->delim = delimiter;

  /* If the mailbox is "selectable", i.e. contains messages,
     store the name in the appropriate StringSet. */
  if (!(attributes & LATT_NOSELECT) && *name) {
    const char *skip;

    /* Skip over server spec, if present */
    skip = strchr(name,'}');
    if (skip)
      name = skip+1;

    /* Remove prefix if it matches specified prefix */
    skip = match_pattern_store->prefix.c_str();
    while (*skip==*name) {
      skip++;
      name++;
    }

    /* Make sure that name doesn't contain default delimiter and replace
     * all occurences of the Store delimiter by our default delimiter */
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

void mm_searched (MAILSTREAM *stream,unsigned long msgno) { }
void mm_exists (MAILSTREAM *stream,unsigned long number) { }
void mm_expunged (MAILSTREAM *stream,unsigned long number) { }
void mm_flags (MAILSTREAM *stream,unsigned long number) { }
void mm_lsub (MAILSTREAM *stream,int delimiter,char *name,long attributes) { }
void mm_status (MAILSTREAM *stream,char *mailbox,MAILSTATUS *status) { }

void mm_notify (MAILSTREAM *stream,char *s,long errflg)
{
  mm_log (s,errflg);    /* Just do mm_log action */
}

void mm_log (char *string,long errflg)
{
  switch (errflg) {  
  case BYE:
  case NIL:                     /* No error */
    if (log_chatter)
      fprintf (stderr,"[%s]\n",string);
    break;
  case PARSE:                   /* Parsing problem */
  case WARN:                    /* Warning */
    if (log_warn) 
      fprintf (stderr,"%%%s\n",string);
    break;
  case ERROR:                   /* Error */
  default:
    if (log_error)
      fprintf (stderr,"?%s\n",string);
    break;
  }
}

void mm_dlog (char *s)
{
  fprintf (stderr,"%s\n",s);
}

void mm_login (NETMBX *mb,char *username,char *password,long trial)
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

void mm_critical (MAILSTREAM *stream)
{
  critical = T;                 /* Note in critical code */
}

void mm_nocritical (MAILSTREAM *stream)
{
  critical = NIL;               /* Note not in critical code */
}

long mm_diskerror (MAILSTREAM *stream,long errcode,long serious)
{
  return T;
}

void mm_fatal (char *st)
{
  char s[80];
  strncpy(s, st, 79);
  s[79]='\0';
  fprintf (stderr,"?%s\n",s);
}
