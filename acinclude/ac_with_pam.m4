dnl AC_WITH_PAM([ACTION-IF-FOUND[,ACTION-IF-NOT-FOUND]])
dnl Output:
dnl AC_SUBST: @PAM_LDFLAGS@ @PAM_INCLUDES@
dnl AM_CONDITIONAL: HAVE_PAM
AC_DEFUN(AC_WITH_PAM,[
 PAMLOCATIONS="/lib,/usr,/usr/local,/opt/pam"
 HAVE_PAM="no"
 PAM_LDFLAGS=""
 PAM_INCLUDES=""
 AC_ARG_WITH(pam,
  AC_HELP_STRING([--with-pam],[enable use of PAM]),
  [
   if test "$withval" = "no" ; then
    PAMLOCATIONS=""
   else
    test "$withval" = "yes" || PAMLOCATIONS="$withval,${PAMLOCATIONS}"
   fi
  ]
 )
 if test -z "${PAMLOCATIONS}" ; then
  ifelse([$2], , :, [$2])
 else
  AC_MSG_CHECKING([for pam library])
  for p in `eval "echo {${PAMLOCATIONS}}{/lib,/lib64}"` ; do
   if test -r "${p}/libpam.a" -o -r "${p}/libpam.so" ; then
    PAM_LDFLAGS="-L${p}"
    AC_MSG_RESULT([found in ${p}])
    break
   fi
  done
  if test -z "${PAM_LDFLAGS}" ; then
   AC_MSG_RESULT([not found])
   ifelse([$2], , :, [$2])
  else
   AC_MSG_CHECKING([for pam headers])
   for p in `eval "echo {${PAMLOCATIONS}}{/include,/include/pam,,/pam}"` ; do
    if test -r "${p}/security/pam_appl.h" ; then
     PAM_INCLUDES="-I${p}"
     AC_MSG_RESULT([found in ${p}])
     break
    fi
   done
   if test -z "${PAM_INCLUDES}" ; then
    AC_MSG_RESULT([not found])
    ifelse([$2], , :, [$2])
   else
    AC_LANG_PUSH(C)
     AC_MSG_CHECKING([if pam test program compiles])
     xLIBS="${LIBS}"
     xCPPFLAGS="${CPPFLAGS}"
     CPPFLAGS="${CPPFLAGS} ${PAM_INCLUDES}"
     LIBS="${LIBS} ${PAM_LDFLAGS} -lpam"
     AC_LINK_IFELSE(
      AC_LANG_SOURCE([
       #include <security/pam_appl.h>
       main(int argc,char **argv) {
       }
      ]),[
       AC_MSG_RESULT([yes])
       HAVE_PAM="yes"
      ],[
       AC_MSG_RESULT([no])
      ]
     )
     CPPFLAGS="${xCPPFLAGS}"
     LIBS="${xLIBS}"
    AC_LANG_POP(C)
    if test "${HAVE_PAM}" = "yes" ; then
     AC_SUBST(PAM_INCLUDES)
     AC_SUBST(PAM_LDFLAGS)
     ifelse([$1], , :, [$1])
    fi
   fi
  fi
 fi
 AM_CONDITIONAL(HAVE_PAM,[test "${HAVE_PAM}" = "yes"])
])
