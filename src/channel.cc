#include "channel.h"
#include "utils.h"
#include "options.h"
#include "types.h"
#include "mail_handling.h"
#include <c-client.h>
#include "msgstring.h"
#include <flstring.h>
#include "msgid.h"
#include <cassert>
#include <errno.h>

extern Passwd*     current_context_passwd;
extern options_t options;

void Channel::print(FILE* f) {
  fprintf( f, "channel %s %s %s {",
              name.c_str(),
              store_a.name.c_str(),
              store_b.name.c_str());
  if (msinfo != "") {
    fprintf( f, "\n\tmsinfo ");
    print_with_escapes( f, msinfo );
  }
  if (! passwd.nopasswd) {
    fprintf( f, "\n\tpasswd ");
    print_with_escapes( f, passwd.text );
  }
    if(sizelimit) fprintf( f, "\n\tsizelimit %lu", sizelimit );
    fprintf( f, "\n}\n" );
    return;
}

//////////////////////////////////////////////////////////////////////////
//
bool Channel::read_lasttime_seen( MsgIdsPerMailbox& mids_per_box, 
                                  MailboxMap& deleted_mailboxes)
//
// Read from msinfo all the message ids that have been seen during the last
// synchronization of this channel and return a hash-map that contains those
// message ids indexed with the names of the currently seen mailboxes in
// store_a and store_b.
//
// "deleted_mailboxes" contains mailboxes that were seen at the last sync but
// were not included in the set of current mailboxes in store_a and store_b
// this time
//
// If msinfo doesn't exist yet, it will be created.
//
// This function is used in order to determine which messages to transfer
// or to expunge.
//
// See the msinfo format description for more info.
//
// returns:
//              1                 - success
//              0                 - failure
//
//              mids_per_box      - hash of lists with message-ids per mailbox
//                                  (indexed by mailbox)
//              deleted_mailboxes - mailboxes that are not contained in the
//                                  mailboxes set
//
//////////////////////////////////////////////////////////////////////////
{
  MAILSTREAM* msinfo_stream;
  ENVELOPE* envelope;
  unsigned long msgno;
  unsigned long k;
  string currentbox = "";    // the box whose msg-id's we're currently reading
  char* text;
  unsigned long textlen;

  if (options.debug) printf( " Reading lasttime of channel \"%s\"\n",
                             this->name.c_str());

  // msinfo is the name of the mailbox that contains the sync info
  current_context_passwd = &this->passwd;
  {
    bool tmp_log_error = options.log_error;
    options.log_error = false;
    mail_create( NULL, nccs( this->msinfo )); // if one exists no problem
    options.log_error = tmp_log_error;
  }
  if (! (msinfo_stream = mail_open( NULL, nccs( this->msinfo ), OP_READONLY)))
  {
    fprintf( stderr, "Error: Couldn't open msinfo box %s.\n",
                     this->msinfo.c_str());
    fprintf( stderr, "       Aborting!\n");
    return 0;
  }

  for ( msgno=1; msgno<=msinfo_stream->nmsgs; msgno++ ) {
    envelope = mail_fetchenvelope( msinfo_stream, msgno );
    if (! envelope) {
      fprintf( stderr,
               "Error: Couldn't fetch enveloppe #%lu from msinfo box %s\n",
                       msgno, this->msinfo.c_str() );
      fprintf( stderr, "       Aborting!\n" );
      return 0;
    }
    // Make sure that the mail is from mailsync 
    if ( ! ( envelope->from && envelope->from->mailbox && envelope->subject) ) {
      // Mail with missing headers
      fprintf( stderr, "Info: The msinfo box %s contains a message with"
                       " missing \"From\" or \"Subject\" header information\n");
      continue;
    }
    if ( strncmp( envelope->from->mailbox, "mailsync", 8) ) {
      // This is not an email describing a mailsync channel!
      fprintf( stderr, "Info: The msinfo box %s contains the non-mailsync"
                       "mail: \"From: %s\"\n",
                       this->msinfo.c_str(), envelope->from->mailbox );
      continue;
    }

    // The subject line contains the name of the channel
    if ( this->name == envelope->subject ) {
      // Found our lasttime

      text = mail_fetchtext_full( msinfo_stream, msgno, &textlen, FT_INTERNAL);
      if ( text )
        text = strdup( text );
      else {
        fprintf( stderr, "Error: Couldn't fetch body #%lu from msinfo box %s\n",
                         msgno, this->msinfo.c_str() );
        fprintf( stderr, "       Aborting!\n" );
        return 0;
      }
      // Replace newlines with '\0'
      // I.e. we transform the text body into c-strings, where
      // each string is either a mailbox or a message id
      for ( k=0 ; k<textlen ; k++ ) {
        if ( text[k] == '\n' )    // can we assume that newlines are allways \n?
          text[k] = '\0';
      }
      bool instring = 0;
      // Check each box against `boxes'
      for ( k=0 ; k<textlen ; ) {
        if( !instring && text[k] == '\0' ) { // skip nulls, empty mailbox names
          k++;                               // are allowed so we have to check
          instring = 1;                      // <- them here
          continue;
        }
        if ( text[k] != '<' ) {
          currentbox = &text[k];
          // if the mailbox is unknown
          if ( store_a.boxes.find(currentbox) == store_a.boxes.end()
               && store_b.boxes.find(currentbox) == store_b.boxes.end()) {
            deleted_mailboxes[currentbox]; //-% creates a new MailboxProperties
          }
        }
        else {                             // it's a message-id
          mids_per_box[currentbox].insert(
                                      MsgId( &text[k] ).from_msinfo_format() );
        }
        for( ; k<textlen && text[k] ; k++); // fastforward to next string
        instring = 0;
      }

      free( text );
      break;    // Stop searching for message
    }
  }
  
  if ( options.debug ) {
    printf( "   Store %s:\n", store_a.name.c_str() );
    for ( MailboxMap::iterator box = store_a.boxes.begin();
          box!=store_a.boxes.end(); box++ ) {
      printf( "    %s(%d) \n",
              box->first.c_str(), mids_per_box[box->first].size() );
    }
    printf( "\n" );
    printf( "   Store %s:\n", store_b.name.c_str() );
    for ( MailboxMap::iterator box = store_b.boxes.begin();
          box!=store_b.boxes.end(); box++ ) {
      printf( "    %s(%d) \n",
              box->first.c_str(), mids_per_box[box->first].size() );
    }
    printf( "\n" );
  }

  mail_close( msinfo_stream );
  return 1;
  exit( 0 );
}


//////////////////////////////////////////////////////////////////////////
//
bool Channel::open_for_copying( string mailbox_name,
                                enum direction_t direction)
//
// Opens both stores in the appropriate modes for copying
//
//////////////////////////////////////////////////////////////////////////
{
  Store& store_from = (direction == a_to_b) ? store_a : store_b;
  Store& store_to   = (direction == a_to_b) ? store_b : store_a;

  if(! store_from.mailbox_open( mailbox_name, OP_READONLY ) ) {
    fprintf( stderr,
             "Error: Couldn't open mailbox %s in store %s\n",
             mailbox_name.c_str(), store_from.name.c_str());
    return 0;
  }

  // strangely enough, following Mark Crispin, if you write into
  // an open stream then it'll mark new messages as seen.
  //
  // So if we want to write to a remote mailbox we have to
  // HALF_OPEN the stream and if we're working on a local
  // mailbox then we have to use a NIL stream.
  if (store_to.isremote) {
    if(! store_to.store_open( OP_HALFOPEN) ) {
       fprintf( stderr, "Error: Couldn't open store %s\n",
                        store_to.name.c_str());
       return 0;
    }
  } else {
    mail_close( store_to.stream );
    store_to.stream = NIL;
  }
  return 1;
}

//////////////////////////////////////////////////////////////////////////
//
bool Channel::copy_message( unsigned long msgno,
                            const MsgId& msgid,
                            string mailbox_name,
                            enum direction_t direction)
//
// Copies the message "msgno" with "msgid" from one store to the other
// depending on "direction"
//
// When appending to a HALF_OPEN stream or to a local mailbox, mailboxb_stream
// will be NIL. Therefore we need the full name of the box.
//
// returns !0 for success
//
// TODO: ideally sanitize_message_id should not have a side effect, but just
//       return 1 or 0 if the message had to be modified or it should have
//       enough information to print a sufficient error message, i.e. the
//       mailbox where the message is originating from
//
//////////////////////////////////////////////////////////////////////////
{
  MSGDATA md;
  STRING CCstring;
  char flags[MAILTMPLEN];
  char msgdate[MAILTMPLEN];
  ENVELOPE *envelope;
  MESSAGECACHE *elt;
  bool success = 1;

  Store& store_from = (direction == a_to_b) ? store_a : store_b;
  Store& store_to   = (direction == a_to_b) ? store_b : store_a;

  current_context_passwd = &store_from.passwd;
  envelope = mail_fetchenvelope(store_from.stream, msgno);
  MsgId msgid_fetched;

  if (! envelope) {
    fprintf( stderr,
             "Error: Couldn't fetch enveloppe #%lu from mailbox box %s\n",
             msgno, store_from.stream->mailbox );
    return 0;
  }

  // Check message-id.
  msgid_fetched = MsgId(envelope);
  if (msgid_fetched.length() == 0) {
    printf( "Warning: missing message-id from mailbox %s, message #%lu.\n",
            store_from.stream->mailbox, msgno);
  }
  else
  {
    if ( msgid_fetched != msgid )
    {
      printf( "Warning: suspicious message-id from mailbox %s, message #%lu.",
              store_from.stream->mailbox, msgno);
      printf( "         msgid expected: %s, msgid found: %s. \n",
              msgid.c_str(), msgid_fetched.c_str());
      printf( "Please report this to http://sourceforge.net/tracker/"
              "?group_id=6374&atid=106374\n");
    }
  }

  elt = mail_elt(store_from.stream, msgno); // the c-client docu says not to do
                                            // this :-/ ?
  assert(elt->valid);                       // Should be valid because of
                                            // fetchenvelope()

  // we skip deleted messages unless copying deleted messages is explicitly
  // demanded
  if (elt->deleted & ! options.copy_deleted_messages) {
    print_lead( "ign. del" , direction == a_to_b ? ">" : "<" );
    print_from( store_from.stream, msgno );
    print_msgid( msgid.c_str() );
    return 0;
  }
      
  // Copy message over with all the flags that the original has
  memset( flags, 0, MAILTMPLEN );
  if (elt->seen)     strcat (flags," \\Seen");
  if (elt->deleted)  strcat (flags," \\Deleted");
  if (elt->flagged)  strcat (flags," \\Flagged");
  if (elt->answered) strcat (flags," \\Answered");
  if (elt->draft)    strcat (flags," \\Draft");

  md.stream = store_from.stream;
  md.msgno = msgno;
  if( this->sizelimit
      && elt->rfc822_size > this->sizelimit )
  {
    print_lead( "too big" , direction == a_to_b ? "->" : "<-" );
    print_from( store_from.stream, msgno );
    if ( options.show_message_id ) {
      print_msgid( msgid.c_str() );
    }
    printf("\n");
    return 0;
  }
  INIT ( &CCstring, msg_string, (void*) &md, elt->rfc822_size );
  current_context_passwd = &store_to.passwd;

  if (!options.simulate) 
    success = mail_append_full( store_to.stream,
                                nccs( store_to.full_mailbox_name(mailbox_name)),
                                &flags[1], mail_date(msgdate,elt), 
                                &CCstring );
  if (options.show_from) {
    print_lead( success ? "copied" : "copyfail",
                direction == a_to_b ? "->" : "<-" );
    print_from( store_from.stream, msgno );
    if (options.show_message_id)
      print_msgid( msgid.c_str() );
    printf("\n");
  }

  if (options.debug)
    printf(" Flags: \"%s\"\n", flags);
  return success;
}

//////////////////////////////////////////////////////////////////////////
//
bool Channel::write_thistime_seen( const MailboxMap& deleted_mailboxes,
                                         MsgIdsPerMailbox& thistime)
//
// Save in channel.msinfo all mailboxes with all msgids (found in
// "thistime") they contain.
//
// deleted_mailboxes  - the mailboxes that were deleted since the last sync
// thistime           - hash indexed by mailbox name containing a list of
//                      msgids for each box
//
// returns !0 on success
//
//////////////////////////////////////////////////////////////////////////
{
  MAILSTREAM* stream;
  ENVELOPE* envelope;
  unsigned long msgno;

  // open the msinfo box
  current_context_passwd = &this->passwd;
  stream = mailbox_open(NIL, nccs(this->msinfo), 0);
  if (!stream) {
    return 0;
  }

  // first delete all the info for the channel we're reading
  // it will be replaced by a newly created set
  for ( msgno=1; msgno <= stream->nmsgs; msgno++)
  {
    envelope = mail_fetchenvelope( stream, msgno);
    if (! envelope)
    {
      fprintf( stderr,
               "Error: Couldn't fetch enveloppe #%lu from mailbox box %s\n",
               msgno, stream->mailbox);
      return 0;
    }
    if ( envelope->subject == this->name )
    {
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
    fprintf( f, "From: mailsync\nSubject: %s\n\n", this->name.c_str() );
    if (! f) {
      fprintf( stderr, "Error: Can't create tmp file for new set of msgid's\n");
      if (errno) perror( strerror(errno) );
      mail_close(stream);
      return 0;
    }

    // for each box - if it's not a deleted mailbox:
    // * first write a line containing it's name
    // * then dump all the message-id's we've seen the last time into it
    for ( MsgIdsPerMailbox::const_iterator mailbox = thistime.begin() ;
          mailbox != thistime.end() ;
          mailbox++)
    {
      if ( deleted_mailboxes.find(mailbox->first)
           == deleted_mailboxes.end()) { // not found
        fprintf( f, "%s\n", mailbox->first.c_str() );
        print_list_with_delimiter( thistime[ mailbox->first ], f, "\n");
      }
    }

    // append the constructed email into the msinfo box
    flen = ftell(f);
    rewind(f);
    INIT(&CCstring, file_string, (void*) f, flen);
    if (!mail_append(stream, nccs(this->msinfo), &CCstring))
    {
      fprintf( stderr, "Error: Can't append thistime to msinfo \"%s\"\n",
                       this->msinfo.c_str() );
      fclose(f);
      mail_close(stream);
      return 0;
    }
    fclose(f);
  }

  mail_close(stream);
  return 1;
}
