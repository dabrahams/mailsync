dnl AC_WITH_OPENSSL([ACTION-IF-FOUND[,ACTION-IF-NOT-FOUND]])
dnl Output:
dnl AC_SUBST: @OPENSSL_INCLUDES@ @OPENSSL_LDFLAGS@ @OPENSSL_LIBS@
dnl AM_CONDITIONAL: HAVE_OPENSSL
AC_DEFUN(AC_WITH_OPENSSL,[
 OPENSSLLOCATIONS="/usr,/usr/local,/usr/local/openssl,/opt/openssl,/usr/local/ssl,/usr/lib/ssl,/usr/ssl"
 HAVE_OPENSSL="no"
 OPENSSL_LDFLAGS=""
 OPENSSL_LIBS=""
 OPENSSL_INCLUDES=""
 AC_ARG_WITH(openssl,
  AC_HELP_STRING([--with-openssl=prefix],[enable use of OpenSSL]),
  [
   if test "$withval" = "no" ; then
    OPENSSLLOCATIONS=""
   else
    test "$withval" = "yes" || OPENSSLLOCATIONS="$withval,${OPENSSLLOCATIONS}"
   fi
  ]
 )
 if test -z "${OPENSSLLOCATIONS}" ; then
  ifelse([$2], , :, [$2])
 else
  AC_MSG_CHECKING([for libssl])
  for p in `eval "echo {${OPENSSLLOCATIONS}}{/lib,}"` ; do
   if test -r "${p}/libssl.a" -o -r "${p}/libssl.so" ; then
    OPENSSL_LDFLAGS="-L${p}"
    AC_MSG_RESULT([found in ${p}])
    break
   fi
  done
  if test -z "${OPENSSL_LDFLAGS}" ; then
   AC_MSG_RESULT([not found])
   ifelse([$2], , :, [$2])
  else
   xLDFLAGS="${LDFLAGS}"
   LDFLAGS="${LDFLAGS} ${OPENSSL_LDFLAGS}"
   AC_CHECK_LIB(ssl,main,[
    OPENSSL_LIBS="${OPENSSL_LIBS} -lssl"
   ],[
    OPENSSL_LIBS=""
   ])
   AC_CHECK_LIB(crypto,main,[
    OPENSSL_LIBS="${OPENSSL_LIBS} -lcrypto"
   ],[
    OPENSSL_LIBS=""
   ])
   LDFLAGS="${xLDFLAGS}"
   if test -z "${OPENSSL_LIBS}" ; then
    ifelse([$2], , :, [$2])
   else
    AC_MSG_CHECKING([for openssl/ssl.h])
    for p in `eval "echo {${OPENSSLLOCATIONS}}{/include,}"` ; do
     if test -r "${p}/openssl/ssl.h" ; then
      OPENSSL_INCLUDES="-I${p}"
      AC_MSG_RESULT([found in ${p}])
      break
     fi
    done
    if test -z "${OPENSSL_INCLUDES}" ; then
     AC_MSG_RESULT([not found])
     ifelse([$2], , :, [$2])
    else
     HAVE_OPENSSL=yes
     AC_SUBST(OPENSSL_INCLUDES)
     AC_SUBST(OPENSSL_LDFLAGS)
     AC_SUBST(OPENSSL_LIBS)
     ifelse([$1], , :, [$1])
    fi
   fi
  fi
 fi
 AM_CONDITIONAL(HAVE_OPENSSL,[test "${HAVE_OPENSSL}" = "yes"])
])
