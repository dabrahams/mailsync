#include "types.h"

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

