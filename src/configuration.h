#ifndef __MAILSYNC_CONFIGURATION__

#include <stdio.h>
#include <map>
#include <vector>
#include <string>
#include "types.h"
#include "store.h"
#include "channel.h"


//////////////////////////////////////////////////////////////////////////
//
struct ConfigItem {
//
//////////////////////////////////////////////////////////////////////////
    Store* store;
    Channel* channel;
    bool is_store;
};

//////////////////////////////////////////////////////////////////////////
//
bool read_configuration( const string& config_file_name,
                               map<string, ConfigItem>& confmap);
// 
// Read and parse config file
//
// Return true on success
// 
//////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////
//
enum operation_mode_t setup_channel_stores_and_mode(
                                    const string& config_file,
                                    const vector<string>& channels_and_stores,
                                    Channel& channel);
//
// Parse channels_and_stores for the desired stores or channel and setup
// store_a, store_b and channel accordingly
//
// return mode
//
//////////////////////////////////////////////////////////////////////////

#define __MAILSYNC_CONFIGURATION__
#endif
