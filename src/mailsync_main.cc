/// Please use spaces instead of tabs

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
// Take care - mailboxes with *empty* names *are* allowed.
//
//////////////////////////////////////////////////////////////////////////

#include "config.h"             // include autoconf settings

#include <ctype.h>
#include <stdio.h>
#include <errno.h>
extern int errno;               // Just in case

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

#include "configuration.h"     // configuration parsing and setup
#include "options.h"           // options and default settings
#include "commandline.h"       // commandline parsing
#include "types.h"             // MailboxMap, Passwd
#include "store.h"             // Store
#include "channel.h"           // Channel
#include "mail_handling.h"     // functions implementing various
                               // synchronization steps and helper functions

//------------------------------- Defines  -------------------------------

#define CREATE   1
#define NOCREATE 0

//------------------------ Global Variables ------------------------------

// current operation mode
enum operation_mode_t operation_mode = mode_unknown;
// options and default settings 
options_t options;

// won't link correctly if this is static - why?
Store*       match_pattern_store;

//////////////////////////////////////////////////////////////////////////
// The password for the current context
// Required, because in the c-client callback functions we don't know
// in which context (store1, store2, channel) we are
Passwd * current_context_passwd = NULL;
//////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////
//
int main(int argc, char** argv)
//
//////////////////////////////////////////////////////////////////////////
{
  Channel channel;
  Store& store_a = channel.store_a;
  Store& store_b = channel.store_b;
  MsgIdsPerMailbox lasttime, thistime;
  MailboxMap deleted_mailboxes;   // present lasttime, but not this time
  MailboxMap empty_mailboxes;
  int success;
  bool& debug = options.debug;

#include "linkage.c"

  // Parse arguments, read config file, choose operation mode
  {
    string config_file;
    vector<string> channels_and_stores;

    // bad command line parameters
    if(! read_commandline_options( argc, argv, options,
                                   channels_and_stores, config_file) )
      exit(1);         
    
    operation_mode = setup_channel_stores_and_mode( config_file,
                                                    channels_and_stores,
                                                    channel);

    if( operation_mode == mode_unknown )
        // errors are handled by setup_stores_channels_and_operation_mode
        exit(1);
  }

  store_a.boxes.clear();
  store_b.boxes.clear();

  // initialize c-client environment (~/.imparc etc.)
  env_init( getenv("USER"), getenv("HOME"));

  // open the connection to the first store
  if ( store_a.isremote ) {
    current_context_passwd = &store_a.passwd;
    store_a.stream = mail_open( NIL, nccs(store_a.server),
                               OP_HALFOPEN | OP_READONLY);
    if (! store_a.stream) {
      fprintf( stderr, 
               "Error: Can't contact first server %s\n",
               store_a.server.c_str() );
      return 1;
    }
  }
  else
  {
    store_a.stream = NULL;
  }

  // open the connection to the second store
  if (operation_mode == mode_sync && store_b.isremote)
  {
    current_context_passwd = &(store_b.passwd);
    store_b.stream = mail_open( NIL, nccs(store_b.server),
                               OP_HALFOPEN | OP_READONLY);
    if (!store_b.stream) {
      fprintf( stderr,
               "Error: Can't contact second server %s\n",
               store_b.server.c_str() );
      return 1;
    }
  }
  else
  {
    store_b.stream = NULL;
  }

  
  // Get list of all mailboxes from first store
  if (debug) printf( " Items in store \"%s\":\n", store_a.name.c_str() );
  if (! store_a.acquire_mail_list() ) {
    printf( " Store pattern doesn't match any selectable mailbox");
    exit(1);
  }
  if (store_a.delim == '!') {
    store_a.get_delim();
  }
  if (store_a.delim == '!') {  // this should not happen
    assert(0);
  }
  else if (debug) {
    if ( ! store_a.delim ) {
      // TODO: this won't happen for INBOXES
      printf(" No delimiter found for store \"%s\"\n", store_a.name.c_str());
    }
    else {
      printf( " Delimiter for store \"%s\" is '%c'\n",
              store_a.name.c_str(), store_a.delim);
    }
  }


  // Display which drivers we're using for accessing the first store
  if (debug) {
    store_a.display_driver();
  }

  ///////////////////////////// mode_list //////////////////////////////

  // Display listing of the mail store if user requested it
  if ( operation_mode == mode_list ) {
    if ( options.show_from | options.show_message_id ) {
      for ( MailboxMap::iterator box = store_a.boxes.begin() ; 
            box != store_a.boxes.end() ;
            box++ )
      {
        printf("\nMailbox: %s\n", box->first.c_str());
        if( box->second.no_select )
          printf("  not selectable\n");
        else {
          store_a.stream = store_a.mailbox_open( box->first, 0);
          if (! store_a.stream) break;
          if (! store_a.list_contents() )
            exit(1);
        }
      }
    }
    else {
      print_list_with_delimiter(store_a.boxes, stdout, "\n");
    } 
   exit(0);
  }

  //////////////////////// mode_diff or mode_sync //////////////////////

  ///////////////////////////// mode_sync //////////////////////////////

  // Sync two stores
  if ( operation_mode == mode_sync ) {

    store_b.boxes.clear();

    if (debug) printf( " Items in store \"%s\":\n", store_b.name.c_str() );
    // Get a list of mailboxes from the second store
    if (! store_b.acquire_mail_list() )
    {
      printf( " Pattern doesn't match any selectable mailbox");
      exit(1);
    }
    // Display which drivers we're using for accessing the second store
    if (debug) store_b.display_driver();

    // Making sure we get ahold of the mailbox-hierarchy delimiter
    if (store_b.delim == '!')
      store_b.get_delim();
    if (! store_b.delim || store_b.delim == '!') {
      printf(" No delimiter found for store \"%s\"\n", store_b.name.c_str());
      exit(1);
    }
    else if (debug)
      printf( " Delimiter for store \"%s\" is '%c'.\n",
              store_b.name.c_str(), store_b.delim);
  }


  //////////////////////// mode_diff or mode_sync //////////////////////

  // Display all the mailboxes we've found
  if (debug)
  {
    printf(" All seen mailboxes: \n");
    printf("  in first store: \n");
    print_list_with_delimiter( store_a.boxes, stdout, " " );
    printf("  in second store: \n");
    print_list_with_delimiter( store_b.boxes, stdout, " " );
    printf("\n");
  }

  // Read in what mailboxes and messages we've seen the last time
  // we've synchronized
  if (! channel.read_lasttime_seen( lasttime, deleted_mailboxes) )
    exit(1);    // failed to read in msinfo or similar


  // Iterate over all mailboxes and sync or diff each
  success = 1; // TODO: this is bogus isn't it?
  for ( MailboxMap::iterator box = store_a.boxes.begin(); 
        box != store_b.boxes.end();
        box++ )
  {
    if ( box == store_a.boxes.end()) { // if we're done with store_a
      box = store_b.boxes.begin();     // continue with store_b
      continue;
    }
    // skip if the box has allready been treated
    if ( box->second.done)
      continue;
    
    // if mailbox doesn't exist in either store -> create
    // if we fail, just continue with next mailbox
    if ( store_a.boxes.find( box->first ) == store_a.boxes.end() )
      if ( ! store_a.mailbox_create( box->first ) )
        continue;
    if ( store_b.boxes.find( box->first ) == store_b.boxes.end() )
      if ( ! store_b.mailbox_create( box->first ) )
        continue;
      
    box->second.done = true;

    if ( store_a.boxes.find( box->first )->second.no_select ) {
      if ( debug )
        printf( "%s is not selectable: skipping\n", box->first.c_str() );
      continue;
    }
    if ( store_b.boxes.find( box->first )->second.no_select ) {
      if ( debug )
        printf( "%s is not selectable: skipping\n", box->first.c_str() );
      continue;
    }

    if (options.show_from)
      printf("\n *** %s ***\n", box->first.c_str());

    MsgIdSet msgids_lasttime( lasttime[box->first] ), msgids_union, msgids_now;
    MsgIdPositions msgidpos_a, msgidpos_b;

    if (options.show_summary) {
      printf("%s: ",box->first.c_str());
      fflush(stdout);
    }
    else {
      printf("\n");
    }

    // fetch_message_ids(): map message-ids to message numbers
    //                      and optionally delete duplicates. 
    //
    // Attention: from here on we're operating on streams to single
    //            _mailboxes_! That means that from here on
    //            streamx_stream is connected to _one_ specific
    //            mailbox.
   
    store_a.stream = store_a.mailbox_open( box->first, 0 );
    if (! store_a.stream)
    {
      store_a.print_error( "opening and writing", box->first);
      continue;
    }
    if (! store_a.fetch_message_ids( msgidpos_a) )
    {
      store_a.print_error( "fetching of mail ids", box->first);
      continue;
    }
    if( operation_mode == mode_sync ) {
      store_b.stream = store_b.mailbox_open( box->first, 0);
      if (! store_b.stream) {
        store_b.print_error( "fetching of mail ids", box->first);
        continue;
      }
      if (! store_b.fetch_message_ids( msgidpos_b )) {
        store_b.print_error( "fetching of mail ids", box->first);
        continue;
      }
    } else if( operation_mode == mode_diff ) {
      for( MsgIdSet::iterator i=msgids_lasttime.begin();
           i!=msgids_lasttime.end();
           i++ )
      {
           msgidpos_b[*i] = 0;
      }
    }

    // u = union(msgids_lasttime, a, b)
    msgids_union = msgids_lasttime;
    for( MsgIdPositions::iterator i = msgidpos_a.begin();
         i != msgidpos_a.end() ;
         i++ )
    {
      msgids_union.insert(i->first);
    }
    for( MsgIdPositions::iterator i = msgidpos_b.begin();
         i != msgidpos_b.end();
         i++)
    {
      msgids_union.insert(i->first);
    }
    MsgIdSet copy_a_b, copy_b_a, remove_a, remove_b;


    for ( MsgIdSet::iterator i=msgids_union.begin();
          i!=msgids_union.end();
          i++ )
    {
      bool in_a = msgidpos_a.count(*i);
      bool in_b = msgidpos_b.count(*i);
      bool in_l = msgids_lasttime.count(*i);

      int a_b_l = (  (in_a ? 0x100 : 0) 
                   + (in_b ? 0x010 : 0)
                   + (in_l ? 0x001 : 0) );

      switch (a_b_l) {

      case 0x100:  // New message on a
        copy_a_b.insert(*i);
        msgids_now.insert(*i);
        break;

      case 0x010:  // New message on b
        copy_b_a.insert(*i);
        msgids_now.insert(*i);
        break;

      case 0x111:  // Kept message
      case 0x110:  // New message, present in a and b, no copying
                   // necessary
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

    switch (operation_mode) {
    case mode_sync:
      {
        bool success;

        // we're first removing messages
        // if we'd first copy and the remove, mailclient would add a
        // "Status: 0" line to each mail. We don't want this wrt to
        // MUA's who interpret such a line as "not new" (in particular
        // mutt!)

        if (debug) {
          printf( " Removing messages from store \"%s\"\n",
                  store_a.name.c_str() );
        }
        unsigned long removed_a = 0;
        for ( MsgIdSet::iterator i = remove_a.begin() ;
              i != remove_a.end() ;
              i++)
        {
          success = store_a.remove_message( msgidpos_a[*i], *i, "< ");
          if (success) removed_a++;
        }

        if (debug) {
          printf( " Removing messages from store \"%s\"\n",
                  store_b.name.c_str() );
        }

        unsigned long removed_b = 0;
        for ( MsgIdSet::iterator i = remove_b.begin();
              i != remove_b.end();
              i++ )
        {
          success = store_b.remove_message( msgidpos_b[*i], *i, "> ");
          if (success) removed_b++;
        }

        string fullboxname_a = store_a.full_mailbox_name(box->first);
        string fullboxname_b = store_b.full_mailbox_name(box->first);

        // strangely enough, following Mark Crispin, if you write into
        // an open stream then it'll mark new messages as seen.
        //
        // So if we want to write to a remote mailbox we have to
        // HALF_OPEN the stream and if we're working on a local
        // mailbox then we have to use a NIL stream.

        if (debug) {
          printf( " Copying messages from store \"%s\" to store \"%s\"\n",
                  store_a.name.c_str(), store_b.name.c_str() );
        }
        if (! store_b.isremote) {
          mail_close(store_b.stream);
          store_b.stream = NIL;
        } else {
          store_b.stream = store_b.mailbox_open( box->first, 0);
        }
        unsigned long copied_a_b = 0;
        for ( MsgIdSet::iterator i=copy_a_b.begin(); i!=copy_a_b.end(); i++)
        {
          success = channel.copy_message( msgidpos_a[*i], *i,
                                          fullboxname_b, a_to_b );
          if (success)
            copied_a_b++;
          else                    // we've failed to copy the message over
            msgids_now.erase(*i); // as we should've had. So let's just assume
                                  // that we haven't seen it at all. That way
                                  // mailsync will have to rediscover and resync
                                  // the same message again next time
        }

        if (debug)
          printf( " Copying messages from store \"%s\" to store \"%s\"\n",
                  store_b.name.c_str(), store_a.name.c_str() );

        if (!store_b.isremote) { // reopen the stream if it was closed before
          store_b.stream = NIL;
          store_b.stream = store_b.mailbox_open( box->first, OP_READONLY);
        }
        if (!store_a.isremote) { // close the stream for writing
                                // (sic - the beauty of c-client!) !!
          mail_close(store_a.stream);
          store_a.stream = NIL;
        } else 
          store_a.stream = store_a.mailbox_open( box->first, 0);
        unsigned long copied_b_a = 0;
        for ( MsgIdSet::iterator i = copy_b_a.begin() ;
              i != copy_b_a.end() ;
              i++)
        {
          success = channel.copy_message( msgidpos_b[*i], *i,
                                          fullboxname_a, b_to_a );
          if (success)
            copied_b_a++;
          else
            msgids_now.erase(*i);
        }
        
        printf("\n");
        if (copied_a_b)
          printf( "%lu copied %s->%s.\n", copied_a_b,
                  store_a.name.c_str(), store_b.name.c_str() );
        if (copied_b_a)
          printf( "%lu copied %s->%s.\n", copied_b_a,
                  store_b.name.c_str(), store_a.name.c_str() );
        if (removed_a)
          printf( "%lu deleted on %s.\n", removed_a, store_a.name.c_str() );
        if (removed_b)
          printf( "%lu deleted on %s.\n", removed_b, store_b.name.c_str() );
        if (options.show_summary) {
          printf( "%lu remain%s.\n", now_n, now_n != 1 ? "" : "s");
          fflush(stdout);
        } else {
          printf( "%lu messages remain in %s\n", now_n, box->first.c_str() );
        }
      }
      break;

    case mode_diff:
      {
        if ( copy_a_b.size() )
          printf( "%d new, ", copy_a_b.size() );
        if (remove_b.size())
          printf( "%d deleted, ", remove_b.size() );
        printf( "%d currently at store %s.\n",
                msgids_now.size(), store_b.name.c_str());
      }
      break;

    default:
      break;
    }

    thistime[box->first] = msgids_now;

    if (! options.no_expunge ) {
      if (! store_a.isremote ) { // reopen the stream in write mode if it was
                                 // closed before - needed for expunge
        store_a.stream = NIL;
        store_a.stream = store_a.mailbox_open( box->first, 0);
      }
      current_context_passwd = &store_a.passwd;
      if (!options.simulate && !options.no_expunge)
         mail_expunge( store_a.stream );
      if (store_b.stream) {
        if (!store_b.isremote) { // reopen the stream in write mode it was
                                // closed before - needed for expunge
          store_b.stream = NIL;
          store_b.stream = store_b.mailbox_open( box->first, 0);
        }
        current_context_passwd = &store_b.passwd;
        if (!options.simulate && !options.no_expunge)
	   mail_expunge( store_b.stream );
      }
      printf("Mails expunged\n");
    }

    if (!store_a.isremote)
      store_a.stream = mail_close(store_a.stream);
    if (store_b.stream && !store_b.isremote)
      store_b.stream = mail_close(store_b.stream);

    if (options.delete_empty_mailboxes) {
      if (now_n == 0) {
        empty_mailboxes[ box->first ] = MailboxProperties();
      }
    }

  }

  if (store_a.isremote) store_a.stream = mail_close(store_a.stream);
  if (store_b.isremote) store_b.stream = mail_close(store_b.stream);

  // TODO: which success are we talking about? Above there are two instances
  //       of "success" declared which mask each other out...
  if (!success)
    return 1;

  if ( options.delete_empty_mailboxes )
  {
    string fullboxname;

    if (store_a.isremote)
    {
      current_context_passwd = &store_a.passwd;
      store_a.stream = mail_open(NIL, nccs(store_a.server), OP_HALFOPEN);
    }
    else
    {
      store_a.stream = NULL;
    }
    if (store_b.isremote)
    {
      current_context_passwd = &(store_b.passwd);
      store_b.stream = mail_open(NIL, nccs(store_b.server), OP_HALFOPEN);
    }
    else
    {
      store_b.stream = NULL;
    }
    for ( MailboxMap::iterator mailbox = empty_mailboxes.begin() ; 
          mailbox != empty_mailboxes.end() ;
          mailbox++ )
    {
      fullboxname = store_a.full_mailbox_name( mailbox->first);
      printf("%s: deleting\n", mailbox->first.c_str());
      printf("  %s", fullboxname.c_str());
      fflush(stdout);
      current_context_passwd = &(store_a.passwd);
      if (mail_delete(store_a.stream, nccs(fullboxname)))
        printf("\n");
      else
        printf(" failed\n");
      fullboxname = store_b.full_mailbox_name( mailbox->first);
      printf("  %s", fullboxname.c_str());
      fflush(stdout);
      current_context_passwd = &(store_b.passwd);
      if (mail_delete(store_b.stream, nccs(fullboxname))) 
        printf("\n");
      else
        printf(" failed\n");
    }
  }

  if (operation_mode==mode_sync)
    if (!options.simulate)
      channel.write_lasttime_seen( deleted_mailboxes, thistime);

  return 0;
}
