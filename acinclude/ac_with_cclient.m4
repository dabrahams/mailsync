dnl AC_WITH_CCLIENT([ACTION-IF-FOUND[,ACTION-IF-NOT-FOUND]])
dnl Output:
dnl AC_SUBST: @CCLIENT_INCLUDES@ @CCLIENT_LIBS@ @CCLIENT_CXXFLAGS@
dnl AM_CONDITIONAL: HAVE_CCLIENT
AC_DEFUN(AC_WITH_CCLIENT,[
 CCLIENTLOCATIONS="/usr,/usr/local"
 HAVE_CCLIENT="no"
 CCLIENT_LIBS=""
 CCLIENT_INCLUDES=""
 CCLIENT_CXXFLAGS=""
 CCLIENT_LINKAGE_C=""
 CCLIENT_LINKAGE_H=""
 AC_ARG_WITH(c-client,
  AC_HELP_STRING([--with-c-client=path],[enable use of c-client]),
  [
   if test "$withval" = "no" ; then
    CCLIENTLOCATIONS=""
   else
    test "$withval" = "yes" || CCLIENTLOCATIONS="$withval,${CCLIENTLOCATIONS}"
   fi
  ]
 )
 if test -z "${CCLIENTLOCATIONS}" ; then
  ifelse([$2], , :, [$2])
 else

  dnl
  dnl Looking for headers c-client headers
  dnl
   AC_MSG_CHECKING([for c-client.h])
  for p in `eval "echo {${CCLIENTLOCATIONS}}{,/c-client,/lib/c-client,/include/c-client,/include,/include/imap}"` ; do
   if test -r "${p}/c-client.h" -a -r "${p}/linkage.h" -a -r "${p}/linkage.c" ; then
    CCLIENT_INCLUDES="-I${p}"
    CCLIENT_LINKAGE_H="${p}/linkage.h"
    CCLIENT_LINKAGE_C="${p}/linkage.c"
    AC_MSG_RESULT([found in ${p}])
    break
   fi
  done
  if test -z "${CCLIENT_INCLUDES}" ; then
   AC_MSG_RESULT([not found])
   ifelse([$2], , :, [$2])
  else
   SOP=".so,.a"
   test "${PREFER_SO}" = "no" && SOP=".a,.so"
   
   dnl
   dnl Looking for c-client libraries
   dnl
   AC_MSG_CHECKING([for c-client library])
   for l in `eval "echo {${CCLIENTLOCATIONS}}{,/c-client,/lib/c-client,/lib,/lib64}/{libc-client,libc-client4,c-client}{${SOP}}"` ; do
    if test -r "$l" ; then
     AC_MSG_RESULT([found ${l}])
     CCLIENT_LIBS="$l"
     break
    fi
   done
   if test -z "${CCLIENT_LIBS}" ; then
    AC_MSG_RESULT([not found])
    ifelse([$2], , :, [$2])
   else

    dnl
    dnl Checking whether c-client was built with kerberos gssapi support
    dnl
    AC_MSG_CHECKING([whether c-client built with kerberos gssapi support])
    AC_EGREP_HEADER(auth_gss,${CCLIENT_LINKAGE_H},
     [
      need_krb=yes
      AC_MSG_RESULT([yes])
     ],[
      need_krb=no
      AC_MSG_RESULT([no])
     ]
    )
    dnl
    dnl Checking if kerberos is required and available for linking against c-client
    dnl
    AC_MSG_CHECKING([if kerberos is required and available for linking against c-client])
    if test "${need_krb}" = "yes" -a "${HAVE_KRB5_GSSAPI}" != "yes"; then
     AC_MSG_RESULT([no])
     ifelse([$2], , :, [$2])
    else
     if test "${need_krb}" = "yes" ; then
      AC_MSG_RESULT([yes])
      CCLIENT_LIBS="${CCLIENT_LIBS} ${KRB5_GSSAPI_LIBS}"
     else
      AC_MSG_RESULT([not required])
     fi
     
     dnl
     dnl Checking whether c-client was built with ssl support
     dnl
     AC_MSG_CHECKING([whether c-client requires ssl linkage])
     AC_EGREP_HEADER(ssl_,${CCLIENT_LINKAGE_C},
      [
       need_ssl=yes
      ],[
       need_ssl=no
      ]
     )
     AC_MSG_RESULT([${need_ssl}])
     if test "${need_ssl}" = "yes" -a "${HAVE_OPENSSL}" != "yes" ; then
      ifelse([$2], , :, [$2])
     else
      CCLIENT_LIBS="${CCLIENT_LIBS} ${OPENSSL_LDFLAGS} ${OPENSSL_LIBS}"
      AC_MSG_CHECKING([if simple c-client program compiles without pam support])
      AC_LANG_PUSH(C)
       xCPPFLAGS="${CPPFLAGS}"
       xLIBS="${LIBS}"
       CPPFLAGS="${CPPFLAGS} ${CCLIENT_INCLUDES}"
       LIBS="${LIBS} ${CCLIENT_LIBS}"
       AC_LINK_IFELSE(
	AC_LANG_SOURCE([
	 #include <stdio.h>
	 #include "c-client.h"
	 #include "linkage.h"
	 main(int argc,char **argv) {
	  #include "linkage.c"
	 }
	 void mm_log(char*a,long b){}
	 void mm_dlog(char*a){}
	 void mm_flags(MAILSTREAM*a,unsigned long b){}
	 void mm_fatal(char*a){}
	 void mm_critical(MAILSTREAM*a){}
	 void mm_nocritical(MAILSTREAM*a){}
	 void mm_notify(MAILSTREAM*a,char*b,long c){}
	 void mm_login(NETMBX*a,char*b,char*c,long d){}
	 long mm_diskerror(MAILSTREAM*a,long b,long c){}
	 void mm_status(MAILSTREAM*a,char*b,MAILSTATUS*c){}
	 void mm_lsub(MAILSTREAM*a,int b,char*c,long d){}
	 void mm_list(MAILSTREAM*a,int b,char*c,long d){}
	 void mm_exists(MAILSTREAM*a,unsigned long b){}
	 void mm_searched(MAILSTREAM*a,unsigned long b){}
	 void mm_expunged(MAILSTREAM*a,unsigned long b){}
	]),[
	 AC_MSG_RESULT([yes])
	 will_do_without_pam="yes"
	],[
	 AC_MSG_RESULT([no])
	 will_do_without_pam="no"
	]
       )
       if test "${will_do_without_pam}" = "no" ; then
	AC_MSG_CHECKING([if we've seen pam somewhere around])
        if test "${HAVE_PAM}" != "yes" ; then
	 AC_MSG_RESULT([no])
        else
         AC_MSG_RESULT([yes])
	 AC_MSG_CHECKING([if adding pam helps])
	 CCLIENT_LIBS="${CCLIENT_LIBS} ${PAM_LDFLAGS} -lpam"
	 LIBS="${xLIBS} ${CCLIENT_LIBS}"
	 AC_LINK_IFELSE(
	  AC_LANG_SOURCE([
	   #include <stdio.h>
	   #include "c-client.h"
	   #include "linkage.h"
	   main(int argc,char **argv) {
	    #include "linkage.c"
	   }
	   void mm_log(char*a,long b){}
	   void mm_dlog(char*a){}
	   void mm_flags(MAILSTREAM*a,unsigned long b){}
	   void mm_fatal(char*a){}
	   void mm_critical(MAILSTREAM*a){}
	   void mm_nocritical(MAILSTREAM*a){}
	   void mm_notify(MAILSTREAM*a,char*b,long c){}
	   void mm_login(NETMBX*a,char*b,char*c,long d){}
	   long mm_diskerror(MAILSTREAM*a,long b,long c){}
	   void mm_status(MAILSTREAM*a,char*b,MAILSTATUS*c){}
	   void mm_lsub(MAILSTREAM*a,int b,char*c,long d){}
	   void mm_list(MAILSTREAM*a,int b,char*c,long d){}
	   void mm_exists(MAILSTREAM*a,unsigned long b){}
	   void mm_searched(MAILSTREAM*a,unsigned long b){}
	   void mm_expunged(MAILSTREAM*a,unsigned long b){}
	  ]),[
	   AC_MSG_RESULT([yes])
	   will_do_with_pam="yes"
	  ],[
	   AC_MSG_RESULT([no])
	   will_do_with_pam="no"
	  ]
	 )
        fi
       fi
       CPPFLAGS="${xCPPFLAGS}"
       LIBS="${xLIBS}"
      AC_LANG_POP(C)
      if test "${will_do_without_pam}" = "no" -a "${will_do_with_pam}" = "no" ; then
       ifelse([$2], , :, [$2])
      else
       AC_LANG_PUSH(C++)
	xCPPFLAGS="${CPPFLAGS}"
	xCXXFLAGS="${CXXFLAGS}"
	CPPFLAGS="${CPPFLAGS} ${CCLIENT_INCLUDES}"
	opernames_resolved="no"
	AC_MSG_CHECKING([if c-client works without -fno-operator-names in c++])
	AC_COMPILE_IFELSE(
	 AC_LANG_SOURCE([
	  #include <stdio.h>
	  #include "c-client.h"
	 ]),[
	  AC_MSG_RESULT([yes])
	  opernames_resolved="yes"
	 ],[
	  AC_MSG_RESULT([no])
	  AC_MSG_CHECKING([if adding -fno-operator-names helps])
	  CXXFLAGS="${CXXFLAGS} -fno-operator-names"
	  AC_COMPILE_IFELSE(
	   AC_LANG_SOURCE([
	    #include <stdio.h>
	    #include "c-client.h"
	   ]),[
	    AC_MSG_RESULT([yes])
	    CCLIENT_CXXFLAGS=-fno-operator-names
	    opernames_resolved="yes"
	   ],[
	    AC_MSG_RESULT([no])
	   ]
	  )
	 ]
	)
	CXXFLAGS="${xCXXFLAGS}"
	CPPFLAGS="${xCPPFLAGS}"
       AC_LANG_POP(C++)
       if test "${opernames_resolved}" = "no" ; then
        ifelse([$2], , :, [$2])
       else
	HAVE_CCLIENT=yes
	AC_SUBST(CCLIENT_CXXFLAGS)
	AC_SUBST(CCLIENT_INCLUDES)
	AC_SUBST(CCLIENT_LIBS)
	ifelse([$1], , :, [$1])
       fi
      fi
     fi
    fi
   fi
  fi
 fi
 AM_CONDITIONAL(HAVE_CCLIENT,[test "${HAVE_CCLIENT}" = "yes"])
])
