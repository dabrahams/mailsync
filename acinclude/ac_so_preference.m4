dnl AC_SO_PREFERENCE([DEFAULT[,ACTION-IF-SO-PREFERRED[,ACTION-IF-STATIC-PREFERRED]]])
dnl Output:
dnl Environment: PREFER_SO=yes
dnl AM_CONDITIONAL: PREFER_SO
AC_DEFUN(AC_SO_PREFERENCE,[
 PREFER_SO="$1"
 AC_ARG_WITH(so-preference,
  AC_HELP_STRING([--with-so-preference],[prefer dynamic linking over static (default $1)]),
  [
   PREFER_SO="$withval"
  ]
 )
 if test "${PREFER_SO}" = "shared" -o "${PREFER_SO}" = "so" -o "${PREFER_SO}" = "yes" ; then
  PREFER_SO="yes"
  ifelse([$2], , :, [$2])
 else
  PREFER_SO="no"
  ifelse([$3], , :, [$3])
 fi
 AM_CONDITIONAL(PREFER_SO,[test "${PREFER_SO}" = "yes"])
])
