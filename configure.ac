AC_INIT([mailsync], [5.2.1], [tpo_deb@sourcepole.ch])
AM_INIT_AUTOMAKE
AM_CONFIG_HEADER(config.h)
AC_CONFIG_SRCDIR([src/mailsync_main.cc])


AC_PROG_CC
AC_PROG_CXX
AC_PROG_INSTALL
AC_PROG_AWK
AC_PROG_CPP

AC_HEADER_STDC
AC_CHECK_HEADERS([string.h])

AC_C_CONST

AC_FUNC_STAT
AC_CHECK_FUNCS([getpass memset strchr strdup strerror strtoul])
# default action of AC_CHECK_LIB is to add item to LIBS
AC_CHECK_LIB(crypt,crypt)

AC_INET_FUNCS(,[
 AC_MSG_ERROR([essential functions missing])
])

AC_SO_PREFERENCE([yes])
AC_WITH_OPENSSL
AC_WITH_KRB5
AC_KRB5_GSSAPI
AC_WITH_PAM

AC_WITH_CCLIENT(,[
 AC_MSG_ERROR([a working c-client installation is required for building mailsync])
])

AC_WITH_MD5

AC_CONFIG_FILES([
 Makefile
 src/Makefile
])
AC_OUTPUT
