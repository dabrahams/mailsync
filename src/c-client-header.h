// This serves as a wrap around to c-client.h
// It is necessary, since c-client.h defines a lot of stuff in
// a incompatible way that is used elsewhere (STL f.ex.)

#ifndef __MAILSYNC_C_CLIENT_HEADER__
#define __MAILSYNC_C_CLIENT_HEADER__

#include <c-client.h>
// C-Client defines the following symbols, which messes up STL,
// since it needs them as well
#undef max
#undef min

#endif
