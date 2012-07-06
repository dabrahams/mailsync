#include "config.h"
#include <stdio.h>
#include <ctype.h>
#include <string>
#include "msgid.h"
#include "options.h"
#include "c-client-header.h"
#include <cassert>
#include <algorithm>

//////////////////////////////////////////////////////////////////////////
//
// A MsgId as in the msinfo file has one of the following forms:
//
// <abcd@x.y.z>                         HEADER_MSGID format
// <>, md5: <ABCD1234>                  MD5_MSGID format
//
//////////////////////////////////////////////////////////////////////////

extern options_t options;

#ifdef HAVE_MD5
 #define MD5BLKLEN 64           /* MD5 block length */
 #define MD5DIGLEN 16           /* MD5 digest length */

 typedef struct {
   unsigned long chigh;         /* high 32bits of byte count */
   unsigned long clow;          /* low 32bits of byte count */
   unsigned long state[4];      /* state (ABCD) */
   unsigned char buf[MD5BLKLEN];        /* input buffer */
   unsigned char *ptr;          /* buffer position */
 } MD5CONTEXT;

 extern "C" {
  void md5_init (MD5CONTEXT *ctx);
  void md5_update (MD5CONTEXT *ctx,unsigned char *data,unsigned long len);
  void md5_final (unsigned char *digest,MD5CONTEXT *ctx);
 }
#endif // HAVE_MD5

//////////////////////////////////////////////////////////////////////////
//
MsgId::MsgId(ENVELOPE *envelope)
//
// Create a message id from an envelope
//
// depending on the options that are set we use one of the methods for
// generating a message identificator.
//
// Currently there are two methods:
//
// HEADER_MSGID       - use the Message-ID header provided in the mailheader
// MD5_MSGID          - make a md5 hash from the From, To, Subject, Date
//                      and Message-ID fields
//
//////////////////////////////////////////////////////////////////////////
{
  string str;

  switch( options.msgid_type) {

   case (HEADER_MSGID) :
        if ( envelope->message_id
             && strcmp( envelope->message_id, "") != 0 
             && strcmp( envelope->message_id, "<>") != 0 )
          *this = envelope->message_id;
        else
          *this = "<>";         // empty message-id
        sanitize_message_id();
        // some software produces empty Message-IDs f.ex. for draft emails
        // which makes us unable to differentiaty between such messages
        if ( *this == "<>") {
          printf( "Warning: empty Message-ID header in message From: \"%s\" "
                  "Subject: \"%s\" - please consult the README\n",
                  (envelope->from && envelope->from->mailbox)
                  ? envelope->from->mailbox : "",
                  envelope->subject ? envelope->subject : "");
        }
        break;
#ifdef HAVE_MD5
   case (MD5_MSGID) :
        char addr[4096];

        if (envelope->date)
          str = string((char*)envelope->date);
        if (envelope->subject)
          str += string(envelope->subject);
        if (envelope->message_id)
          str += string(envelope->message_id);
        for (ADDRESS *a = envelope->from; a; a = a->next) {
          *addr = '\0';
          rfc822_address(addr,a);
          str += string(addr);
        }
        for (ADDRESS *a = envelope->to; a; a = a->next) {
          *addr = '\0';
          rfc822_address(addr,a);
          str += string(addr);
        }
        if (str.length() != 0) {
          unsigned char bdigest[MD5DIGLEN];
          MD5CONTEXT ctx;
          char cdigest[2 * MD5DIGLEN + 2];

          md5_init(&ctx);
          md5_update(&ctx, (unsigned char *) str.c_str(), strlen(str.c_str()) );
          md5_final(bdigest, &ctx);
          for (int i = 0; i < MD5DIGLEN; i++)
            sprintf(&cdigest[2 * i],"%02x",bdigest[i]);
          cdigest[2 * MD5DIGLEN] = '\0';
          *this = cdigest;
        }
        break;
#endif // HAVE_MD5
   
   default :
        assert(0);      // this should not happen
        break;
  }
}

static char const* const fixup_names[] = { 
  "removed blanks", "added angle brackets", "added square brackets around ip address" };

//////////////////////////////////////////////////////////////////////////
//
void MsgId::sanitize_message_id()
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
{
  enum { 
    removed_blanks, added_angle_brackets, added_square_brackets, num_fixups };
  bool fixup[num_fixups] = {};
  unsigned i;
  if ((*this)[0] != '<') {
    *this = '<'+ *this;
    fixup[added_angle_brackets] = true;
  }
  for (i=0; (i < this->length()) && ((*this)[i] != '>'); i++) {
    if (isspace((*this)[i]) || iscntrl((*this)[i])) {
      (*this)[i] = '.';
      fixup[removed_blanks] = true;
    }
  }

  // Have we reached end of string?
  if (i == this->size()) {
  // if there's no '>' we need to attach
    if((*this)[i-1] != '>') {
      *this = *this + '>';
      fixup[added_angle_brackets] = true;
    }
  }
  // we've found a '>', so let's cut the rubbish that follows
  else
    *this = this->substr( 0, i+1);

  // Look for un-bracketed ip addresses
  size_type atsign_pos = this->rfind('@');
  if (atsign_pos != npos) {
    size_type tail_pos = atsign_pos + 1;
    size_type tail_end = this->size() - 1;
    size_type tail_length = tail_end - tail_pos;
    if (tail_length >= strlen("1.1.1.1") 
        && tail_length <= strlen("255.255.255.255")
        && this->find_first_not_of("0123456789.", tail_pos) == tail_end
        && std::count(this->begin() + tail_pos, this->end(), '.') == 3
        && this->find("..", tail_pos) == npos
    ) {
      *this = this->substr(0,tail_pos) + '[' + this->substr(tail_pos,tail_length) + "]>";
      fixup[added_square_brackets] = true;
    }
  }

  if (
    options.report_braindammaged_msgids 
    && std::find(fixup, fixup + num_fixups, true) != fixup+num_fixups
  ) {
    fprintf( stderr, "Warning: repaired brain-damaged message id %s (", this->c_str());
    bool comma = false;
    for (unsigned i = 0; i < num_fixups; ++i) {
      if (fixup[i]) {
        if (comma) fprintf( stderr, ", ");
        fprintf( stderr, fixup_names[i] );
        comma = true;
      }
    }
    fprintf( stderr, ")\n");
  }
}

//////////////////////////////////////////////////////////////////////////
//
string MsgId::from_msinfo_format()
//
// Read from a msinfo entry the relevant msgid depending on which 
// format is currently chosen
//
// Currently there are two formats:
//
// HEADER_MSGID       - message id as in the Message-ID header in a mailheader
// MD5_MSGID          - message id in md5 hash format
//
//////////////////////////////////////////////////////////////////////////
{
  int start;

  switch( options.msgid_type) {

   case (HEADER_MSGID) :
        return this->substr(0, this->find(">") + 1);
        break;

#ifdef HAVE_MD5
   case (MD5_MSGID) :
        start = this->find(">, md5: <") + 9; // 9 == length of ">, md5: <"
        return this->substr( start, this->find( ">", start) - start);
        break;
#endif // HAVE_MD5
   
   default :
        assert(0);      // this should not happen
        break;
  }
}

//////////////////////////////////////////////////////////////////////////
//
string MsgId::to_msinfo_format()
//
// Return the message id in msinfo format
//
// Currently there are two formats:
//
// HEADER_MSGID       - message id as in the Message-ID header in a mailheader
// MD5_MSGID          - message id in md5 hash format
//
//////////////////////////////////////////////////////////////////////////
{
  switch( options.msgid_type) {

   case (HEADER_MSGID) :
        return *this;
        break;

#ifdef HAVE_MD5
   case (MD5_MSGID) :
        return "<>, md5: <" + *this + ">";
        break;
#endif // HAVE_MD5
   
   default :
        assert(0);      // this should not happen
        break;
  }
}

//////////////////////////////////////////////////////////////////////////
//
bool MsgId::empty()
//
// Say whether Message-ID is empty
//
//
//////////////////////////////////////////////////////////////////////////
{
  return string::empty() || *this == "<>";
}
