dnl
dnl $Id$
dnl

AC_MSG_CHECKING(whether apc needs to get compiler flags from apxs)
AC_ARG_WITH(apxs,
[  --with-apxs[=FILE]      Get compiler flags from apxs -q.  Provide the
                          pathname to the Apache apxs tool; defaults to "apxs".],[
  if test "$withval" != "no"; then
    if test "$withval" = "yes"; then
      APXS=apxs
      $APXS -q CFLAGS >/dev/null 2>&1
      if test "$?" != "0" && test -x /usr/sbin/apxs; then #SUSE 6.x
        APXS=/usr/sbin/apxs
      fi
    else
      PHP_EXPAND_PATH($withval, APXS)
    fi

    $APXS -q CFLAGS >/dev/null 2>&1
    if test "$?" != "0"; then
      AC_MSG_RESULT()
      AC_MSG_RESULT()
      AC_MSG_RESULT([Sorry, I was not able to successfully run APXS.  Possible reasons:])
      AC_MSG_RESULT()
      AC_MSG_RESULT([1.  Perl is not installed;])
      AC_MSG_RESULT([2.  Apache was not compiled with DSO support (--enable-module=so);])
      AC_MSG_RESULT([3.  'apxs' is not in your path.  Try to use --with-apxs=/path/to/apxs])
      AC_MSG_RESULT([The output of $APXS follows])
      $APXS -q CFLAGS
      AC_MSG_ERROR([Aborting])
    fi

    APC_CFLAGS=`$APXS -q CFLAGS`
    AC_MSG_RESULT(yes)
  else
    AC_MSG_RESULT(no)
  fi
],[
  AC_MSG_RESULT(no)
])

PHP_ARG_ENABLE(apc, whether to enable APC support,
[  --enable-apc           Enable APC support])

AC_MSG_CHECKING(Checking whether we should use mmap)
AC_ARG_ENABLE(apc-mmap,
[  --disable-apc-mmap
                          Disable mmap support and use IPC shm instead],
[
  PHP_APC_MMAP=$enableval
  AC_MSG_RESULT($enableval)
], [
  PHP_APC_MMAP=yes
  AC_MSG_RESULT(yes)
])

AC_MSG_CHECKING(Checking whether we should use semaphore locking instead of fcntl)
AC_ARG_ENABLE(apc-sem,
[  --enable-apc-sem
                          Enable semaphore locks instead of fcntl],
[
  PHP_APC_SEM=$enableval
  AC_MSG_RESULT($enableval)
], [
  PHP_APC_SEM=no
  AC_MSG_RESULT(no)
])

if test "$PHP_APC" != "no"; then
  test "$PHP_APC_MMAP" != "no" && AC_DEFINE(APC_MMAP, 1, [ ])
  test "$PHP_APC_SEM"  != "no" && AC_DEFINE(APC_SEM_LOCKS, 1, [ ])

  AC_CACHE_CHECK(for union semun, php_cv_semun,
  [
    AC_TRY_COMPILE([
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/sem.h>
    ], [union semun x;], [
      php_cv_semun=yes
    ],[
      php_cv_semun=no
    ])
  ])
  if test "$php_cv_semun" = "yes"; then
    AC_DEFINE(HAVE_SEMUN, 1, [ ])
  else
    AC_DEFINE(HAVE_SEMUN, 0, [ ])
  fi

  apc_sources="apc.c php_apc.c \
               apc_cache.c \
               apc_compile.c \
               apc_debug.c \
               apc_fcntl.c \
               apc_main.c \
               apc_mmap.c \
               apc_optimizer.c \
               apc_pair.c \
               apc_sem.c \
               apc_shm.c \
               apc_sma.c \
               apc_stack.c \
               apc_zend.c"

  PHP_CHECK_LIBRARY(rt, shm_open, [PHP_ADD_LIBRARY(rt,,APC_SHARED_LIBADD)])
  PHP_NEW_EXTENSION(apc, $apc_sources, $ext_shared,, \\$(APC_CFLAGS))
  PHP_SUBST(APC_SHARED_LIBADD)
  PHP_SUBST(APC_CFLAGS)
  AC_DEFINE(HAVE_APC, 1, [ ])
fi

