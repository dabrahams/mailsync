//--------------------- Configuration Parsing ----------------------------
#include <stdio.h>
#include <string>
#include <sys/stat.h>           // Stat()
#include "c-client.h"           // for MAILTMPLEN
#include "configuration.h"
#include "options.h"
#include "store.h"
#include "channel.h"

extern options_t options;

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
  if (options.debug) printf("\t%s\n",t->buf.c_str());
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

  if (options.debug) printf(" Parsing config...\n\n");

  t = &token;
  t->line = 0;

  // Read items from file
  while (1) {
    ConfigItem config_item;
    string name;

    // Acquire one item
    get_token(f, t);

    // End of config file reached
    if (t->eof)
      break;
    
    // Parse store
    if (t->buf == "store") {
      Store* store = new Store();
      // Read name (store name)
      get_token(f, t);
      if (t->eof) {
        die_with_fatal_parse_error( t, "Store name missing");
      }
      name = t->buf;
      store->name = t->buf;
      // Read "{"
      get_token(f, t);
      if (t->eof || t->buf != "{") {
        die_with_fatal_parse_error( t, "Expected \"{\" while parsing store");
      }
      // Read store configuration (name/value pairs)
      while (1) {
        get_token(f, t);
        // End of store config
        if (t->buf == "}")
          break;
        else if (t->eof)
          die_with_fatal_parse_error( t,
                         "Expected \"{\" while parsing store: unclosed store");
        else if (t->buf == "server") {
          get_token(f, t);
          store->server = t->buf;
        }
        else if (t->buf == "prefix") {
          get_token(f, t);
          store->prefix = t->buf;
        }
        else if (t->buf == "ref") {
          get_token(f, t);
          store->ref = t->buf;
        }
        else if (t->buf == "pat") {
          get_token(f, t);
          store->pat = t->buf;
        }
        else if (t->buf == "passwd") {
          get_token(f, t);
          store->set_passwd(t->buf);
        }
        else
          die_with_fatal_parse_error(t, "Unknown store field");
      }
      if (store->server == "") {
        store->isremote = 0;
      }
      else {
        store->isremote = 1;
      }
      config_item.store    = store;
      config_item.is_store = true;
    }
    // Parse a channel
    else if (t->buf == "channel") {
      Channel* channel = new Channel();
      // Read name (channel name)
      get_token(f, t);
      if (t->eof) {
        die_with_fatal_parse_error( t, "Channel name missing");
      }
      name = t->buf;
      channel->name = t->buf;
      // Read stores
      get_token(f, t);
      if (t->eof) {
        die_with_fatal_parse_error( t,
                        "Expected a store name while parsing channel");
      }
      channel->store_a.name = t->buf;
      get_token(f, t);
      if (t->eof) {
        die_with_fatal_parse_error( t,
                        "Expected a store name while parsing channel");
      }
      channel->store_b.name = t->buf;
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
          channel->msinfo = t->buf;
        }
        else if (t->buf == "passwd") {
          get_token(f, t);
          channel->set_passwd(t->buf);
        }
	else if (t->buf == "sizelimit") {
          get_token(f, t);
	  channel->set_sizelimit(t->buf);
	}
        else
          die_with_fatal_parse_error(t, "Unknown channel field");
      }
      if (channel->msinfo == "") {
        die_with_fatal_parse_error( t, "%s: missing msinfo",
                                    channel->name.c_str());
      }
      config_item.channel  = channel;
      config_item.is_store = false;
    }
    else
      die_with_fatal_parse_error(t, "unknow configuration element");

    if (confmap.count(name)) {
      die_with_fatal_parse_error( t,
                      "Tag (store or channel name) used twice: %s",
                      name.c_str());
    }
    confmap.insert( make_pair(name, config_item) );
  }

  if (options.debug) printf( " End parsing config. Config is OK\n\n" );

  return;
}

//////////////////////////////////////////////////////////////////////////
//
bool read_configuration( const string& config_file_name,
                               map<string, ConfigItem>& confmap)
// 
// Read and parse config file
//
// Return true on success
// 
//////////////////////////////////////////////////////////////////////////
{
  FILE* config;
  string config_file = config_file_name;
    
  if (config_file == "") {
    char *home;
    home = getenv( "HOME" );
    if (home) {
      config_file = string( home ) + "/.mailsync";
    }
    else {
      fprintf( stderr, "Error: Can't get home directory. Use `-f file'.\n" );
      return false;
    }
  }
  {
    struct stat st;
    stat( config_file.c_str(), &st );
    if ( S_ISDIR( st.st_mode ) 
         || !(config=fopen(config_file.c_str(),"r")) )
    {
      fprintf( stderr,
               "Error: Can't open config file %s\n", config_file.c_str());
      return false;
    }
  }
  parse_config( config, confmap );
  return true;
}

//////////////////////////////////////////////////////////////////////////
//
enum operation_mode_t setup_channel_stores_and_mode(
                                    const string& config_file,
                                    const vector<string>& chan_stor_names,
                                    Channel& channel)
//
// Parse chan_stor_names for the desired stores or channel and setup
// store_a, store_b and channel accordingly
//
// return mode
//
//////////////////////////////////////////////////////////////////////////
{
  vector<Store> stores_to_treat;
  vector<Channel> channels_to_treat;

  operation_mode_t operation_mode = mode_unknown;

  map<string, ConfigItem> configured_items;

  if ( ! read_configuration( config_file, configured_items))
    return mode_unknown;

  // make sure the channel and store names correspond to channel and
  // store names in the config
  for ( unsigned i = 0; i < chan_stor_names.size(); i++)
  {
    if ( configured_items.count(chan_stor_names[i]) == 0 )
    {
      fprintf( stderr,
               "Error: A channel or store named \"%s\" has not"
               "been configured\n", chan_stor_names[i].c_str());
      return mode_unknown;
    }
    
    ConfigItem* config_item = &configured_items[chan_stor_names[i]];
    if ( config_item->is_store
         && config_item->store->name == chan_stor_names[i] )
    {
      stores_to_treat.push_back( *config_item->store);
    }
    else if ( ! config_item->is_store
              && config_item->channel->name == chan_stor_names[i])
    {
      channels_to_treat.push_back( *config_item->channel);
    }
  }

  // mode_sync
  if ( channels_to_treat.size() == 1 && stores_to_treat.size() == 0 )
  {
    channel = channels_to_treat[0];
    if ( ! configured_items[channel.store_a.name].is_store
         || ! configured_items[channel.store_b.name].is_store )
    {
      fprintf( stderr,
               "Error: Malconfigured channel %s\n",
               channel.name.c_str());
      if( configured_items[ channel.store_a.name ].is_store)
      {
        fprintf( stderr,
                 "The configuration doesn't contain a store named \"%s\"\n",
                 channel.store_a.name.c_str());
      }
      else
      {
        fprintf( stderr,
                 "The configuration doesn't contain a store named \"%s\"\n",
                 channel.store_b.name.c_str());
      }
      return mode_unknown;
    }
    channel.store_a = *(configured_items[channel.store_a.name].store);
    channel.store_b = *(configured_items[channel.store_b.name].store);

    operation_mode = mode_sync;
  }
  
  // mode_diff
  else if (channels_to_treat.size() == 1 && stores_to_treat.size() == 1)
  {
    options.expunge_duplicates = 0;
    channel = channels_to_treat[0];
    if ( channel.store_a.name == stores_to_treat[0].name) {
      channel.store_b = *(configured_items[channel.store_a.name]).store;
    }
    else if ( channel.store_b.name == stores_to_treat[0].name) {
      channel.store_b = *(configured_items[channel.store_b.name]).store;
    }
    else {
      fprintf( stderr,
               "Diff mode: channel %s doesn't contain store %s.\n",
               channel.name.c_str(), stores_to_treat[0].name.c_str() );
      return mode_unknown;
    }
    channel.store_a = stores_to_treat[0];

    operation_mode = mode_diff;
  }
  // mode_list
  else if (channels_to_treat.size() == 0 && stores_to_treat.size() == 1)
  {
    channel.store_a = stores_to_treat[0];

    operation_mode = mode_list;
  }
  else
  {
    fprintf( stderr,
             "Don't know what to do with %d channels and %d stores.\n",
             channels_to_treat.size(),
             stores_to_treat.size());
    return mode_unknown;
  }

  // Give feedback on the mode we're in
  switch( operation_mode ) {
    case mode_sync:
      printf( "Synchronizing stores \"%s\" <-> \"%s\"...\n",
              channel.store_a.name.c_str(),
              channel.store_b.name.c_str());
      break;
    case mode_diff:
      printf( "Comparing store \"%s\" <-> \"%s\"...\n",
              channel.store_a.name.c_str(),
              channel.store_b.name.c_str());
      break;
    case mode_list:
      printf( "Listing store \"%s\"\n",
              channel.store_a.name.c_str() );
      break;
    default:
      printf( "Panic! Unknown mode - something unexpected happened\n");
  }

  return operation_mode;
}
