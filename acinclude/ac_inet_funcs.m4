dnl AC_INET_FUNCS([ACTION-IF-FOUND[,ACTION-IF-NOT-FOUND]])
dnl Output:
dnl  -l* added to LIBS
AC_DEFUN(AC_INET_FUNCS,[
 no_connect="no"
 no_gethostbyname="no"
 AC_CHECK_FUNC(connect,,[
  AC_CHECK_LIB(socket,connect,,[
   AC_CHECK_LIB(inet,connect,,[
    no_connect="yes"
   ])
  ])
 ])
 AC_CHECK_FUNC(gethostbyname,,[
  AC_CHECK_LIB(nsl_s,gethostbyname,,[
   AC_CHECK_LIB(nsl,gethostbyname,,[
    no_gethostbyname="yes"
   ])
  ])
 ])
 if test "$no_gethostbyname" = "yes" -o "$no_connect" = "yes" ; then
  ifelse([$2], , :, [$2])
 else
  ifelse([$1], , :, [$1])
 fi
])
