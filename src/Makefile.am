EXTRA_DIST=msgstring.c
bin_PROGRAMS=mailsync

INCLUDES=@CCLIENT_INCLUDES@
LDADD=@CCLIENT_LIBS@
AM_CXXFLAGS=@CCLIENT_CXXFLAGS@

mailsync_SOURCES=mailsync_main.cc \
                 options.h \
                 commandline.cc commandline.h \
                 configuration.cc configuration.h \
                 types.cc types.h \
                 mail_handling.cc mail_handling.h \
                 c-client_callbacks.cc \
                 c-client-header.h \
                 utils.cc utils.h \
                 store.cc store.h \
                 channel.cc channel.h \
                 msgid.cc msgid.h \
                 msgstring.c msgstring.h
