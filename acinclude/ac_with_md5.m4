dnl AC_WITH_MD5([ACTION-IF-FOUND[,ACTION-IF-NOT-FOUND])
dnl Output:
dnl AC_DEFINE: HAVE_MD5
AC_DEFUN(AC_WITH_MD5,[
 AC_MSG_CHECKING([if c-client includes md5 support])
 AC_LANG_PUSH(C)
  xCFLAGS="${CFLAGS}"
  xLIBS="${LIBS}"
  CFLAGS="${CFLAGS} ${CCLIENT_INCLUDES}"
  LIBS="${LIBS} ${CCLIENT_LIBS}"
  AC_LINK_IFELSE(
   AC_LANG_SOURCE([
    #include <stdio.h>
    #include "c-client.h"
    #include "linkage.h"

    #define MD5BLKLEN 64           /* MD5 block length */

    typedef struct {
      unsigned long chigh;         /* high 32bits of byte count */
      unsigned long clow;          /* low 32bits of byte count */
      unsigned long state[4];      /* state (ABCD) */
      unsigned char buf[MD5BLKLEN];        /* input buffer */
      unsigned char *ptr;          /* buffer position */
    } MD5CONTEXT;

    void md5_init (MD5CONTEXT *ctx);

    main(int argc,char **argv) {
     #include "linkage.c"
     md5_init(NULL);
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
    AC_DEFINE([HAVE_MD5], [], [Does c-client include md5 support?])
   ],[
    AC_MSG_RESULT([no])
   ]
  )
  CFLAGS="${xCFLAGS}"
  LIBS="${xLIBS}"
])
