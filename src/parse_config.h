#ifndef __PARSE_CONFIG__

#include <stdio.h>
#include <map>
#include "mailsync_types.h"

//////////////////////////////////////////////////////////////////////////
//
struct ConfigItem {
//
// A configuration item from the config file
// Can either be a Channel or a Store
//
//////////////////////////////////////////////////////////////////////////
  int is_Store;
  Store* store;
  Channel* channel;

  void clear() {
    is_Store = 0;
    store = NULL;
    channel = NULL;
    return;
  }
  ConfigItem() { clear(); }
  
  void print(FILE* f) {
    if (is_Store)
      store->print(f);
    else
      channel->print(f);
  }
};

//////////////////////////////////////////////////////////////////////////
//
void parse_config(FILE* f, map<string, ConfigItem>& confmap);
// 
// Parse config file
// 
//////////////////////////////////////////////////////////////////////////

#define __PARSE_CONFIG__
#endif
