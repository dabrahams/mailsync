#include "mailsync_types.h"

//////////////////////////////////////////////////////////////////////////
//
void print_with_escapes( FILE* f, const string& str)
//
// Prints a string into a file (or std[out|err]) while
// replacing " " by "\ "
// 
//////////////////////////////////////////////////////////////////////////
{
  const char *s = str.c_str();
  while ( *s ) {
    if ( isspace( *s ) ) {
      fputc( '\\', f);
    }
    fputc( *s, f);
    s++;
  }
}

//////////////////////////////////////////////////////////////////////////
//
// Passwd
//
//////////////////////////////////////////////////////////////////////////
void Passwd::clear() {
  text = "";
  nopasswd = true;
}

void Passwd::set_passwd(string passwd) {
  nopasswd = false;
  text = passwd;
}

//////////////////////////////////////////////////////////////////////////
//
// Store
//
//////////////////////////////////////////////////////////////////////////
void Store::clear() {
  tag      = "";
  server   = "";
  prefix   = "";
  ref      = "";
  pat      = "";
  isremote = 0;
  delim    = '!';
  passwd.clear();
}

void Store::print(FILE* f) {
  fprintf( f, "store %s {", tag.c_str() );
  if (server != "") {
    fprintf( f, "\n\tserver ");
    print_with_escapes( f, server);
  }
  if (prefix != "") {
    fprintf( f, "\n\tprefix ");
    print_with_escapes( f, prefix);
  }
  if (ref != "") {
    fprintf( f, "\n\tref ");
    print_with_escapes( f, ref);
  }
  if (pat != "") {
    fprintf( f, "\n\tpat ");
    print_with_escapes( f, pat);
  }
  if (! passwd.nopasswd) {
    fprintf( f, "\n\tpasswd ");
    print_with_escapes( f, passwd.text);
  }
  fprintf( f, "\n}\n");
}

//////////////////////////////////////////////////////////////////////////
//
// Channel
//
//////////////////////////////////////////////////////////////////////////
void Channel::clear() {
  tag = store[0] = store[1] = msinfo = "";
  passwd.clear();
  sizelimit = 0;
}

void Channel::print(FILE* f) {
  fprintf( f, "channel %s %s %s {", tag.c_str(), store[0].c_str(),
                                    store[1].c_str());
  if (msinfo != "") {
    fprintf( f, "\n\tmsinfo ");
    print_with_escapes( f, msinfo);
    }
  if (! passwd.nopasswd) {
    fprintf( f, "\n\tpasswd ");
    print_with_escapes( f, passwd.text);
  }
  if ( sizelimit ) fprintf( f, "\n\tsizelimit %lu", sizelimit);
  fprintf( f, "\n}\n");
}

