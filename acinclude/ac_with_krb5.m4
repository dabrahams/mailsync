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
  fi
 fi
 AM_CONDITIONAL(HAVE_KRB5,[test "${HAVE_KRB5}" = "yes"])
]
)
