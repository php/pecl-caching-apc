dnl
dnl $Id$
dnl

PHP_ARG_ENABLE(apc, for APC support,
[  --enable-apc           include APC support.])


if test "$PHP_APC" != "no"; then
  PHP_EXTENSION(apc, $ext_shared)
  AC_DEFINE(HAVE_APC, 1, [ ])
 AC_CACHE_CHECK(for union semun,php_cv_semun,
   AC_TRY_COMPILE([
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/sem.h>
   ],
   [union semun x;],
   [
     php_cv_semun=yes
   ],[
     php_cv_semun=no
   ])
 )
 if test "$php_cv_semun" = "yes"; then
   AC_DEFINE(HAVE_SEMUN, 1, [ ])
 else
   AC_DEFINE(HAVE_SEMUN, 0, [ ])
 fi
fi

