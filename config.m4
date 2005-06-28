dnl
dnl $Id$
dnl

PHP_ARG_ENABLE(apc, whether to enable APC support,
[  --enable-apc           Enable APC support])

PHP_ARG_ENABLE(apc-mmap, whether to enable mmap support instead of IPC shm,
[  --enable-apc-mmap        APC: Enable mmap support instead of IPC shm], no, no)

PHP_ARG_ENABLE(apc-sem, whether to prefer semaphore based locks,
[  --enable-apc-sem         APC: Enable IPC semamphore based locks
                                instead of standard locks], no, no)

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
  PHP_NEW_EXTENSION(apc, $apc_sources, $ext_shared)
  PHP_SUBST(APC_SHARED_LIBADD)
  AC_DEFINE(HAVE_APC, 1, [ ])
fi
