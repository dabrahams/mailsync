#include <stdio.h>
#include <string>
#include "utils.h"

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
char* nccs( const string& s)
//
// C-Client doesn't declare anything const
// If you're paranoid, you can allocate a new char[] here
// 
//////////////////////////////////////////////////////////////////////////
{
  return (char*) s.c_str();
}

