/// Please use spaces instead of tabs

// See the documentation: "HACKING" and "ABSTRACT"

#include "config.h"             // include autoconf settings

#include <ctype.h>
#include <stdio.h>
#include <errno.h>
extern int errno;               // Just in case

#include <string>
#include <set>
#include <map>
#include <vector>
#include <cassert>
using std::string;
using std::set;
using std::map;
using std::vector;
using std::make_pair;

#include "c-client.h"
// C-Client defines these and it messes up STL, since the
// use it as well
#undef max
#undef min

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
// Required, because we don't know inside the c-client callback functions
// which context (store1, store2, channel) we are in
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

  //
  // Parse arguments, read config file, choose operation mode
  // --------------------------------------------------------
  {
    string config_file;
    vector<string> channels_and_stores;
    // bad command line parameters
    if (! read_commandline_options( argc, argv, options,
                                   channels_and_stores, config_file) )
      exit(1);         
    operation_mode = setup_channel_stores_and_mode( config_file,
                                                    channels_and_stores,
                                                    channel);
    if ( operation_mode == mode_unknown )
      exit(1);
  }

  store_a.boxes.clear();
  store_b.boxes.clear();

  // initialize c-client environment (~/.imparc etc.)
  env_init( getenv("USER"), getenv("HOME"));

  // open a read only the connection to the first store
  if ( store_a.isremote ) {
    if (! store_a.store_open( OP_HALFOPEN | OP_READONLY) )
      return 1;
  }
  else
  {
    store_a.stream = NULL;
  }

  // in case we want to sync - open a read only the connection
  // to the second store
  if (operation_mode == mode_sync && store_b.isremote)
  {
    if (! store_b.store_open( OP_HALFOPEN | OP_READONLY) )
      return 1;
  }
  else
  {
    store_b.stream = NULL;
  }

  
  // Get list of all mailboxes from first store
  //
  if (debug) printf( " Items in store \"%s\":\n", store_a.name.c_str() );
  if (! store_a.acquire_mail_list() && options.log_warn) {
    printf( " Store pattern doesn't match any selectable mailbox\n");
  }
  if (store_a.delim == '!') {
    store_a.get_delim();
  }
  if (store_a.delim == '!') {  // this should not happen
    assert(0);
  }
  else if (debug) {
    // store_b.delim can be '' for INBOXes
    if ( ! store_a.delim )
      printf(" No delimiter found for store \"%s\"\n", store_a.name.c_str());
    else
      printf( " Delimiter for store \"%s\" is '%c'\n",
              store_a.name.c_str(), store_a.delim );
  }


  // Display which drivers we're using for accessing the first store
  if (debug) {
    store_a.display_driver();
  }

  ///////////////////////////// mode_list //////////////////////////////

  // Display listing of the first mail store in case we're in list mode
  if ( operation_mode == mode_list ) {
    if ( options.show_from | options.show_message_id ) {
      for ( MailboxMap::iterator curr_mbox = store_a.boxes.begin() ; 
            curr_mbox != store_a.boxes.end() ;
            curr_mbox++ )
      {
        printf("\nMailbox: %s\n", curr_mbox->first.c_str());
        if( curr_mbox->second.no_select )
          printf("  not selectable\n");
        else {
          store_a.stream = store_a.mailbox_open( curr_mbox->first, 0);
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

  //////////////////////////////////////////////////////////////////////
  //////////// from this point on we are only dealing with /////////////
  ////////////////// mode_diff or mode_sync ////////////////////////////
  //////////////////////////////////////////////////////////////////////

  ///////////////////////////// mode_sync //////////////////////////////

  // Get list of all mailboxes and delimiter from second store
  //
  if ( operation_mode == mode_sync ) {

    store_b.boxes.clear();

    if (debug) printf( " Items in store \"%s\":\n", store_b.name.c_str() );
    // Get a list of mailboxes from the second store
    if (! store_b.acquire_mail_list() && options.log_warn )
    {
      printf( " Store pattern doesn't match any selectable mailbox\n");
    }
    // Display which drivers we're using for accessing the second store
    if (debug) store_b.display_driver();

    // Making sure we get ahold of the mailbox-hierarchy delimiter
    if (store_b.delim == '!')
      store_b.get_delim();
    if (store_a.delim == '!') {  // this should not happen
      assert(0);
    }
    else if (debug) {
      if (! store_b.delim )
        printf(" No delimiter found for store \"%s\"\n", store_b.name.c_str());
      else
        printf( " Delimiter for store \"%s\" is '%c'\n",
                store_b.name.c_str(), store_b.delim);
    }
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
  //
  // our comparison operator for our stores compares lenghts
  // that means that we're traversing the store from longest to
  // shortest mailbox name - this makes sure that we'll first see
  // and create mailboxes with longer "path"names that means 
  // submailboxes first
  success = 1; // TODO: this is bogus isn't it?
  for ( MailboxMap::iterator curr_mbox = store_a.boxes.begin(); 
        curr_mbox != store_b.boxes.end();
        curr_mbox++ )
  {
    if ( curr_mbox == store_a.boxes.end()) { // if we're done with store_a
      curr_mbox = store_b.boxes.begin();     // continue with store_b
      if ( curr_mbox == store_b.boxes.end()) break;
    }

    // skip if the current mailbox has allready been synched
    if ( curr_mbox->second.done)
      continue;
    
    // if mailbox doesn't exist in either one of the stores -> create it
    if ( store_a.boxes.find( curr_mbox->first ) == store_a.boxes.end() )
      if ( ! store_a.mailbox_create( curr_mbox->first ) )
        continue;
    if ( store_b.boxes.find( curr_mbox->first ) == store_b.boxes.end() )
      if ( ! store_b.mailbox_create( curr_mbox->first ) )
        continue;

    // when traversing store_a's boxes we don't need to worry about
    // whether it has been synched yet or not.  It isn't unless we're
    // in store_b that it matters whether the current mailbox has been
    // traversed in store_a allready
    store_b.boxes.find(curr_mbox->first)->second.done = true;

    // skip unselectable (== can't contain mails) boxes
    if ( store_a.boxes.find( curr_mbox->first )->second.no_select ) {
      if ( debug )
        printf( "%s is not selectable: skipping\n", curr_mbox->first.c_str() );
      continue;
    }
    if ( store_b.boxes.find( curr_mbox->first )->second.no_select ) {
      if ( debug )
        printf( "%s is not selectable: skipping\n", curr_mbox->first.c_str() );
      continue;
    }

    if (options.show_from)
      printf("\n *** %s ***\n", curr_mbox->first.c_str());

    MsgIdSet msgids_lasttime( lasttime[curr_mbox->first] ), msgids_union, msgids_now;
    MsgIdPositions msgidpos_a, msgidpos_b;

    if (options.show_summary) {
      printf("%s: ",curr_mbox->first.c_str());
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
   
    // open and fetch message ID's from the mailbox in the first store
    store_a.stream = store_a.mailbox_open( curr_mbox->first, 0 );
    if (! store_a.stream)
    {
      store_a.print_error( "opening and writing", curr_mbox->first);
      continue;
    }
    if (! store_a.fetch_message_ids( msgidpos_a) )
    {
      store_a.print_error( "fetching of mail ids", curr_mbox->first);
      continue;
    }

    // if we're in sync mode open and fetch message IDs from the
    // mailbox in the second store
    if( operation_mode == mode_sync ) {
      store_b.stream = store_b.mailbox_open( curr_mbox->first, 0);
      if (! store_b.stream) {
        store_b.print_error( "fetching of mail ids", curr_mbox->first);
        continue;
      }
      if (! store_b.fetch_message_ids( msgidpos_b )) {
        store_b.print_error( "fetching of mail ids", curr_mbox->first);
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

    // Create the set of all seen message IDs in a mailbox:
    // + message IDs seen the last time
    // + message IDs seen in the mailbox from store_a
    // + message IDs seen in the mailbox from store_b
    // 
    // msgids_union = union(msgids_lasttime, msgids_a, msgids_b)
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

    // Messages that should be copied from store_a to store_b,
    // from store_b to store_a, that should be removed in store_a and
    // finally that should be removed in store_b
    MsgIdSet copy_a_b, copy_b_a, remove_a, remove_b;

    // Iterate over all messages that were seen in a mailbox last time,
    // in store_a and in store_b
    for ( MsgIdSet::iterator i=msgids_union.begin();
          i!=msgids_union.end();
          i++ )
    {
      // determine first what to do with a message
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
    
     /////////////////////////// mode_sync ///////////////////////////
    
     case mode_sync:
      {
        bool success;
        unsigned long removed_a = 0, removed_b = 0, copied_a_b = 0,
                      copied_b_a = 0;

        //////////////////// removing messages ///////////////////////
        
        // We're first flagging messages for removal. If we'd first copy
        // and then remove, c-client would add a "Status: O" line to each
        // mail. We don't want this because of MUA's who interpret such
        // a line as "not new" (in particular mutt!)

        if (debug) printf( " Removing messages from store \"%s\"\n",
                           store_a.name.c_str() );

        for( MsgIdSet::iterator i =remove_a.begin(); i !=remove_a.end(); i++) {
          success = store_a.flag_message_for_removal( msgidpos_a[*i], *i, "< ");
          if (success) removed_a++;
        }

        if (debug) printf( " Removing messages from store \"%s\"\n",
                           store_b.name.c_str() );

        for( MsgIdSet::iterator i =remove_b.begin(); i !=remove_b.end(); i++) {
          success = store_b.flag_message_for_removal( msgidpos_b[*i], *i, "> ");
          if (success) removed_b++;
        }

        //////////////////// copying messages ///////////////////////
        
        if (debug)
          printf( " Copying messages from store \"%s\" to store \"%s\"\n",
                  store_a.name.c_str(), store_b.name.c_str() );

        if (! channel.open_for_copying( curr_mbox->first, a_to_b) )
          exit(1);
        for ( MsgIdSet::iterator i =copy_a_b.begin(); i !=copy_a_b.end(); i++) {
          success = channel.copy_message( msgidpos_a[*i], *i,
                                          curr_mbox->first, a_to_b );
          if (success) copied_a_b++;
          else         msgids_now.erase(*i);
          // if we've failed to copy the message over we'll pretend that we
          // haven't seen it at all. That way mailsync will have to rediscover
          // and resync the same message again next time
        }

        if (debug)
          printf( " Copying messages from store \"%s\" to store \"%s\"\n",
                  store_b.name.c_str(), store_a.name.c_str() );

        if (! channel.open_for_copying( curr_mbox->first, b_to_a) )
          exit(1);
        for ( MsgIdSet::iterator i=copy_b_a.begin(); i !=copy_b_a.end(); i++) {
          success = channel.copy_message( msgidpos_b[*i], *i,
                                          curr_mbox->first, b_to_a );
          if (success) copied_b_a++;
          else         msgids_now.erase(*i);
        }
        
        printf("\n");
        if (copied_a_b) printf( "%lu copied %s->%s.\n", copied_a_b,
                                store_a.name.c_str(), store_b.name.c_str() );
        if (copied_b_a) printf( "%lu copied %s->%s.\n", copied_b_a,
                                store_b.name.c_str(), store_a.name.c_str() );
        if (removed_a)  printf( "%lu deleted on %s.\n",
                                removed_a, store_a.name.c_str() );
        if (removed_b)  printf( "%lu deleted on %s.\n",
                                removed_b, store_b.name.c_str() );
        if (options.show_summary) {
          printf( "%lu remain%s.\n", now_n, now_n != 1 ? "" : "s");
          fflush(stdout);
        } else {
          printf( "%lu messages remain in %s\n",
                  now_n, curr_mbox->first.c_str() );
        }

        //////////////////////// expunging emails /////////////////////////
        // this *needs* to be done *after* coying as the *last* step
        // otherwise the order of the mails will get messed up since
        // some random messages inbewteen have been deleted in the mean
        // time and the message numbers we know don't correspond to
        // messages in the mailbox/store any more
        
        if (debug) printf( " Expunging messages\n" );

        if (! (options.no_expunge || options.simulate) ) {
          int n_expunged_a = store_a.mailbox_expunge( curr_mbox->first );
          int n_expunged_b = store_b.mailbox_expunge( curr_mbox->first );
          if (n_expunged_a) printf( "Expunged %d mail%s in store %s\n"
                                    , n_expunged_a
                                    , n_expunged_a == 1 ? "" : "s"
                                    , store_a.name.c_str() );
          if (n_expunged_b) printf( "Expunged %d mail%s in store %s\n"
                                    , n_expunged_b
                                    , n_expunged_b == 1 ? "" : "s"
                                    , store_b.name.c_str() );
        }

        //////////////////////// deleting empty mailboxes /////////////////////////
        
        if (options.delete_empty_mailboxes) {
          if (now_n == 0) {
            // add empty mailbox to empty_mailboxes
            empty_mailboxes[ curr_mbox->first ];
            deleted_mailboxes[ curr_mbox->first ];
          }
        }
      } // end case mode_sync
      break;

     /////////////////////////// mode_diff ///////////////////////////
    
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

    thistime[curr_mbox->first] = msgids_now;

    // close local boxes
    if (!store_a.isremote)
      store_a.stream = mail_close(store_a.stream);
    if (store_b.stream && !store_b.isremote)
      store_b.stream = mail_close(store_b.stream);

  } // end loop over all mailboxes

  if (store_a.isremote) store_a.stream = mail_close(store_a.stream);
  if (store_b.isremote) store_b.stream = mail_close(store_b.stream);

  // TODO: which success are we talking about? Above there are two instances
  //       of "success" declared which mask each other out...
  if (!success)
    return 1;

  if ( options.delete_empty_mailboxes && operation_mode==mode_sync )
  {
    string fullboxname;

    if (store_a.isremote) {
      store_a.stream = NIL;
      store_a.store_open( OP_HALFOPEN );
    } else {
      store_a.stream = NULL;
    }
    if (store_b.isremote) {
      store_b.stream = NIL;
      store_b.store_open( OP_HALFOPEN );
    } else {
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
      channel.write_thistime_seen( deleted_mailboxes, thistime);

  return 0;
}
