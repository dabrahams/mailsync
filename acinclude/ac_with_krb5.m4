dnl AC_WITH_KRB5([ACTION-IF-FOUND[,ACTION-IF-NOT-FOUND])
dnl Output:
dnl AC_SUBST: @KRB5_LDFLAGS@ @KRB5_CFLAGS@ @KRB5_CONFIG@
dnl AM_CONDITIONAL: HAVE_KRB5
AC_DEFUN(AC_WITH_KRB5,[
 KRB5LOCATIONS="/usr,/usr/local,/usr/kerberos,/usr/local/kerberos,/usr/krb5,/usr/local/krb5,/opt/kerberos,/opt/krb5"
 HAVE_KRB5="no"
 KRB5_CONFIG=""
 KRB5_CFLAGS=""
 KRB5_LDFLAGS=""
 AC_ARG_WITH(krb5,
  AC_HELP_STRING([--with-krb5=prefix],[enable use of kerberos]),
  [
   if test "$withval" = "no" ; then
    KRB5LOCATIONS=""
   else
    test "$withval" = "yes" || KRB5LOCATIONS="$withval,${KRB5LOCATIONS}"
   fi
  ]
 )
 if test -z "${KRB5LOCATIONS}" ; then
  ifelse([$2], , :, [$2])
 else
  AC_MSG_CHECKING([for krb5-config])
  for k in `eval "echo {${KRB5LOCATIONS}}{/bin,}/krb5-config"` ; do
   if test -x "${k}" ; then
    KRB5_CONFIG="${k}"
    break
   fi
  done
  if test -z "${KRB5_CONFIG}" ; then
   AC_MSG_RESULT([not found])
   ifelse([$2], , :, [$2])
  else
   AC_MSG_RESULT([found ${KRB5_CONFIG}])
   HAVE_KRB5=yes
   KRB5_CFLAGS="`${KRB5_CONFIG} --cflags`"
   KRB5_LDFLAGS="`${KRB5_CONFIG} --libs`"
   AC_SUBST(KRB5_CONFIG)
   AC_SUBST(KRB5_CFLAGS)
   AC_SUBST(KRB5_LDFLAGS)
   ifelse([$1], , :, [$1])
  fi
 fi
 AM_CONDITIONAL(HAVE_KRB5,[test "${HAVE_KRB5}" = "yes"])
]
)

dnl AC_KRB5_GSSAPI([ACTION-IF-FOUND[,ACTION-IF-NOT-FOUND]])
dnl Output:
dnl AC_SUBST: @KRB5_GSSAPI_LIBS@ @KRB5_GSSAPI_CFLAGS@
dnl AM_CONDITIONAL: HAVE_KRB5_GSSAPI
AC_DEFUN(AC_KRB5_GSSAPI,[
 HAVE_KRB5_GSSAPI="no"
 if test "${HAVE_KRB5}" = "yes" -a "${KRB5_CONFIG}" != "" ; then
  HAVE_KRB5_GSSAPI="yes"
  KRB5_GSSAPI_CFLAGS="`${KRB5_CONFIG} --cflags gssapi`"
  KRB5_GSSAPI_LIBS="`${KRB5_CONFIG} --libs gssapi`"
  AC_SUBST(KRB5_GSSAPI_CFLAGS)
  AC_SUBST(KRB5_GSSAPI_LIBS)
  ifelse([$1], , :, [$1])
 else
  ifelse([$2], , :, [$2])
 fi
 AM_CONDITIONAL(HAVE_KRB5_GSSAPI,[test "${HAVE_KRB5_GSSAPI}" = "yes" ])
])

dnl AC_KRB5_KRB4([ACTION-IF-FOUND[,ACTION-IF-NOT-FOUND]])
dnl Output:
dnl AC_SUBST: @KRB5_KRB4_LIBS@ @KRB5_KRB4_CFLAGS@
dnl AM_CONDITIONAL: HAVE_KRB5_KRB4
AC_DEFUN(AC_KRB5_KRB4,[
 HAVE_KRB5_KRB4="no"
 if test "${HAVE_KRB5}" = "yes" -a "${KRB5_CONFIG}" != "" ; then
  HAVE_KRB5_KRB4="yes"
  KRB5_KRB4_CFLAGS="`${KRB5_CONFIG} --cflags krb4`"
  KRB5_KRB4_LIBS="`${KRB5_CONFIG} --libs krb4`"
  AC_SUBST(KRB5_KRB4_CFLAGS)
  AC_SUBST(KRB5_KRB4_LIBS)
  ifelse([$1], , :, [$1])
 else
  ifelse([$2], , :, [$2])
 fi
 AM_CONDITIONAL(HAVE_KRB5_KRB4,[test "${HAVE_KRB5_KRB4}" = "yes" ])
])

dnl AC_KRB5_KADM_CLIENT([ACTION-IF-FOUND[,ACTION-IF-NOT-FOUND]])
dnl Output:
dnl AC_SUBST: @KRB5_KADM_CLIENT_LIBS@ @KRB5_KADM_CLIENT_CFLAGS@
dnl AM_CONDITIONAL: HAVE_KRB5_KADM_CLIENT
AC_DEFUN(AC_KRB5_KADM_CLIENT,[
 HAVE_KRB5_KADM_CLIENT="no"
 if test "${HAVE_KRB5}" = "yes" -a "${KRB5_CONFIG}" != "" ; then
  HAVE_KRB5_KADM_CLIENT="yes"
  KRB5_KADM_CLIENT_CFLAGS="`${KRB5_CONFIG} --cflags kadm-client`"
  KRB5_KADM_CLIENT_LIBS="`${KRB5_CONFIG} --libs kadm-client`"
  AC_SUBST(KRB5_KADM_CLIENT_CFLAGS)
  AC_SUBST(KRB5_KADM_CLIENT_LIBS)
  ifelse([$1], , :, [$1])
 else
  ifelse([$2], , :, [$2])
 fi
 AM_CONDITIONAL(HAVE_KRB5_KADM_CLIENT,[test "${HAVE_KRB5_KADM_CLIENT}" = "yes" ])
])

dnl AC_KRB5_KADM_SERVER([ACTION-IF-FOUND[,ACTION-IF-NOT-FOUND]])
dnl Output:
dnl AC_SUBST: @KRB5_KADM_SERVER_LIBS@ @KRB5_KADM_SERVER_CFLAGS@
dnl AM_CONDITIONAL: HAVE_KRB5_KADM_SERVER
AC_DEFUN(AC_KRB5_KADM_SERVER,[
 HAVE_KRB5_KADM_SERVER="no"
 if test "${HAVE_KRB5}" = "yes" -a "${KRB5_CONFIG}" != "" ; then
  HAVE_KRB5_KADM_SERVER="yes"
  KRB5_KADM_SERVER_CFLAGS="`${KRB5_CONFIG} --cflags kadm-server`"
  KRB5_KADM_SERVER_LIBS="`${KRB5_CONFIG} --libs kadm-server`"
  AC_SUBST(KRB5_KADM_SERVER_CFLAGS)
  AC_SUBST(KRB5_KADM_SERVER_LIBS)
  ifelse([$1], , :, [$1])
 else
  ifelse([$2], , :, [$2])
 fi
 AM_CONDITIONAL(HAVE_KRB5_KADM_SERVER,[test "${HAVE_KRB5_KADM_SERVER}" = "yes" ])
])

dnl AC_KRB5_KDB([ACTION-IF-FOUND[,ACTION-IF-NOT-FOUND]])
dnl Output:
dnl AC_SUBST: @KRB5_KDB_LIBS@ @KRB5_KDB_CFLAGS@
dnl AM_CONDITIONAL: HAVE_KRB5_KDB
AC_DEFUN(AC_KRB5_KDB,[
 HAVE_KRB5_KDB="no"
 if test "${HAVE_KRB5}" = "yes" -a "${KRB5_CONFIG}" != "" ; then
  HAVE_KRB5_KDB="yes"
  KRB5_KDB_CFLAGS="`${KRB5_CONFIG} --cflags kdb`"
  KRB5_KDB_LIBS="`${KRB5_CONFIG} --libs kdb`"
  AC_SUBST(KRB5_KDB_CFLAGS)
  AC_SUBST(KRB5_KDB_LIBS)
  ifelse([$1], , :, [$1])
 else
  ifelse([$2], , :, [$2])
 fi
 AM_CONDITIONAL(HAVE_KRB5_KDB,[test "${HAVE_KRB5_KDB}" = "yes" ])
])
