#include "config.h"
#include <stdio.h>
#include <string>
#include <map>
#include <vector>
#include "options.h"
#include "types.h"
#include "store.h"
#include "channel.h"
#include "configuration.h"

extern options_t options;

//////////////////////////////////////////////////////////////////////////
//
void usage()
//
// Mailsync help
//
//////////////////////////////////////////////////////////////////////////
{
  printf(PACKAGE_STRING "\n\n");        // autoconf bizzarrerie
  printf("mailsync [options] channel\n");
  printf("       synchronize two stores defined by \"channel\" or\n");
  printf("mailsync [options] store\n");
  printf("       list mailboxes contained in \"store\"\n");
  printf("mailsync [options] channel store\n");
  printf("       display changes from last seen messages in \"channel\" to\n");
  printf("       those contained in \"store\"\n");
  printf("\n");
  printf("\n");
  printf("Options:\n");
  printf("  -cd      do copy deleted mailboxes (default is not)\n");
  printf("  -n       don't delete messages\n");
  printf("  -D       delete any empty mailboxes after synchronizing\n");
  printf("  -m       show from, subject, etc. of messages that get expunged or moved\n");
  printf("  -M       also show message-ids (turns on -m)\n");
  printf("  -s       simulate\n");
  printf("  -d       show debug info\n");
  printf("  -di      debug/log IMAP protocol telemetry\n");
  printf("  -dc      debug configuration\n");
  printf("  -v       show imap chatter\n");
  printf("  -vb      show warning about braindammaged message ids\n");
  printf("  -vw      show warnings\n");
  printf("  -vp      show RFC 822 parsing errors\n");
  printf("  -f conf  use alternate config file\n");
  printf("  -t [msgid|md5] msg id type\n");
  printf("\n");
  return;
}

//////////////////////////////////////////////////////////////////////////
//
// const impossible here? (tpo)               vvvv
bool read_commandline_options( const int argc,
                                     char** argv,
                                     options_t& options,
                                     vector<string>& channels_and_stores,
                                     string& config_file )
//
// Read and parse commandline options
//
//////////////////////////////////////////////////////////////////////////
{
  int optind = 1;

  // All options must be given separately

  // first parse the command line options/switches
  // that is everything that starts with a "-" like "-w", "-m", etc.
  while (optind<argc && argv[optind][0]=='-') {
    switch (argv[optind][1]) {
    case 'n':
      options.delete_messages = 0;
      break;
    case 'm':
      options.show_from = 1;
      options.show_summary = 0;
      break;
    case 'M':
      options.show_from = 1;
      options.show_summary = 0;
      options.show_message_id = 1;
      break;
    case 's':
      printf("Only simulating\n");
      options.simulate = 1;
      options.delete_messages = 0;
      break;
    case 'v':
      if (argv[optind][2] == 'b')
        options.report_braindammaged_msgids = 1;
      else if (argv[optind][2] == 'w')
        options.log_warn = 1;
      else if (argv[optind][2] == 'p')
        options.log_parse = 1;
      else
        options.log_chatter = 1;
      break;
    case 'f':
      config_file = argv[++optind];
      break;
    case 'D':
      options.delete_empty_mailboxes = 1;
      break;
    case 'd':
      if (argv[optind][2] == 'i')
        options.debug_imap = 1;
      else if (argv[optind][2] == 'c')
        options.debug_config = 1;
      else
        options.debug = 1;
      break;
    case 'c':
      if (argv[optind][2] == 'd')
        options.copy_deleted_messages = 1;
      else {
        usage();
        return false;
      }
      break;
    case 't':
      if ( strcmp( argv[++optind], "md5") == 0 ) {
#ifdef HAVE_MD5
        options.msgid_type = MD5_MSGID;
#else
        usage();
        printf("Error: mailsync was compiled without md5 support.\n");
        printf("       you need c-client >= 2002 and recompile mailsync.\n");
        return false;
#endif // HAVE_MD5
      }
      else if ( strcmp( argv[optind], "msgid" ) == 0 )
        options.msgid_type = HEADER_MSGID;
      else {
        usage();
        printf("Error: unknown message id format\n");
        return false;
      }
      break;
    default:
      usage();
      return false;
    }
    optind++;
  }

  // we've parsed all the options the rest should consist of 
  // channel and store names
  //
  // if there aren't any following then report this as an error
  if ( argc - optind < 1 ) {
    usage();
    return false;
  }

  // read channel and store names from command line
  for ( ; optind < argc; optind++) {
    channels_and_stores.push_back( argv[optind] );
  }

  return true;
}

