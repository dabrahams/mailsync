#ifndef __MAILSYNC_COMMANDLINE__
#include <string>
#include <vector>
#include "options.h"
#include "types.h"
#include "channel.h"

int read_commandline_options( const int argc,
                                    char** argv,
                                    options_t& options,
                                    vector<string>& channels_and_stores,
                                    string& config_file);

#define __MAILSYNC_COMMANDLINE__
#endif
