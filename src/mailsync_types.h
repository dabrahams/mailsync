#ifndef __MAILSYNC_TYPES__

#include <stdio.h>
#include <string>

//////////////////////////////////////////////////////////////////////////
//
class Passwd
//
// Structure that holds the password
//
//////////////////////////////////////////////////////////////////////////
{
  public:
    bool nopasswd;
    string text;

    void clear();
    void set_passwd(string passwd);
};

//////////////////////////////////////////////////////////////////////////
//
class Store
//
// Structure that holds the pattern for _one_ set of mailboxes to sync
//
//////////////////////////////////////////////////////////////////////////
{
  public:
    string tag, server, prefix;   // Tag == store name
    string ref, pat;
    Passwd passwd;
    int isremote;     // I.e. allows OP_HALFOPEN
    int delim;

    void clear();

    Store() { clear(); }

    void print(FILE* f);

    void set_passwd(string password) { passwd.set_passwd(password);}
};

//////////////////////////////////////////////////////////////////////////
//
class Channel
//
// Structure that holds two Stores (sets of mailboxes) that should be synched
//
//////////////////////////////////////////////////////////////////////////
{
  public:
    string tag;           // Tag == channel name
    string store[2];
    string msinfo;
    Passwd passwd;
    unsigned long sizelimit;

    void clear();

    Channel() { clear(); }

    void print(FILE* f);

    void set_passwd(string password) { passwd.set_passwd(password);}
    void set_sizelimit(const string& sizelim) { sizelimit=strtoul(sizelim.c_str(),NULL,10); }
};

#define __MAILSYNC_TYPES__
#endif
