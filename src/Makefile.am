EXTRA_DIST=msgstring.c
bin_PROGRAMS=mailsync

INCLUDES=@CCLIENT_INCLUDES@
LDADD=@CCLIENT_LIBS@
AM_CXXFLAGS=@CCLIENT_CXXFLAGS@

mailsync_SOURCES=mailsync_main.cc \
                 parse_config.cc parse_config.h \
                 mailsync_types.cc mailsync_types.h
