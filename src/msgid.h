#ifndef __MAILSYNC_MSGID__

#include <string>
#include <c-client.h>
#include "options.h"

using namespace std;

//////////////////////////////////////////////////////////////////////////
//
class MsgId : public string
//
// MsgId - can contain various identificators for messages
//
//////////////////////////////////////////////////////////////////////////
{
  public:
    MsgId(): string() {};
    MsgId(char* m): string(m) {};
    MsgId(string m): string(m) {};
    MsgId(ENVELOPE *envelope);
    void sanitize_message_id();
    string to_msinfo_format();
    string from_msinfo_format();
};

#define __MAILSYNC_MSGID__
#endif
