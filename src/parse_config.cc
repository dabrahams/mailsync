//--------------------- Configuration Parsing ----------------------------
#include <stdio.h>
#include <string>
#include "c-client.h"               // for MAILTMPLEN
#include "parse_config.h"

extern bool debug;

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


//////////////////////////////////////////////////////////////////////////
//
void die_with_fatal_parse_error( Token* t,
                                 char * errorMessage,
                                 const char * insertIntoMessage = NULL)
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
{
  fprintf(stderr, "Error: ");
  fprintf(stderr, errorMessage, insertIntoMessage ? insertIntoMessage : "");
  fprintf(stderr, "\n");
  if (t->eof) {
    fprintf(stderr, "       Unexpected EOF while parsing configuration file");
    fprintf(stderr, "       While parsing line %d:\n", t->line);
  }
  else {
    fprintf(stderr, "       While parsing line %d at token \"%s\"\n",
            t->line, t->buf.c_str());
  }
  fprintf(stderr,   "       Quitting!\n");

  exit(1);
}

//////////////////////////////////////////////////////////////////////////
//
void get_token( FILE* f, Token* t )
//
// Read a token from the config file
//
//////////////////////////////////////////////////////////////////////////
{
  int c;
  t->buf = "";
  t->eof = 0;
  while (1) {
    if (t->buf.size() > MAILTMPLEN) {
      // Token too long
      die_with_fatal_parse_error(t, "Line too long");
    }
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
    }
    else if (isspace(c)) {
      // The token is non empty so we've acquired something and can return
      if (t->buf.size()) {
        break;
      // Ignore spaces
      }
      else {
        continue;
      }
    // Continue on next line...
    }
    else if (c=='\\') {
      t->buf += (char)getc(f);
    // Add to token
    }
    else {
      t->buf += (char)c;
    }
  }
  if (debug) printf("\t%s\n",t->buf.c_str());
  return;
}

//////////////////////////////////////////////////////////////////////////
//
void parse_config(FILE* f, map<string, ConfigItem>& confmap)
// 
// Parse config file
// 
//////////////////////////////////////////////////////////////////////////
{
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
      if (t->eof) {
        die_with_fatal_parse_error(t, "Store name missing");
      }
      tag = t->buf;
      conf->store->tag = t->buf;
      // Read "{"
      get_token(f, t);
      if (t->eof || t->buf != "{") {
        die_with_fatal_parse_error(t, "Expected \"{\" while parsing store");
      }
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
      if (t->eof) {
        die_with_fatal_parse_error(t, "Expected a store name while parsing channel");
      }
      conf->channel->store[0] = t->buf;
      get_token(f, t);
      if (t->eof) {
        die_with_fatal_parse_error(t, "Expected a store name while parsing channel");
      }
      conf->channel->store[1] = t->buf;
      // Read "{"
      get_token(f, t);
      if (t->eof || t->buf != "{") {
        die_with_fatal_parse_error(t, "Expected \"{\" while parsing channel");
      }
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
