dnl $Id$
dnl config.m4 for extension apc
dnl don't forget to call PHP_EXTENSION(apc)

dnl Comments in this file start with the string 'dnl'.
dnl Remove where necessary. This file will not work
dnl without editing.

dnl If your extension references something external, use with:

dnl PHP_ARG_WITH(apc, for apc support,
dnl Make sure that the comment is aligned:
dnl [  --with-apc             Include apc support])

dnl Otherwise use enable:

PHP_ARG_ENABLE(apc, whether to enable apc support,
     [  --enable-apc            Enable apc support])

if test "$PHP_APC" != "no"; then
  dnl If you will not be testing anything external, like existence of
  dnl headers, libraries or functions in them, just uncomment the 
  dnl following line and you are ready to go.
  AC_DEFINE(HAVE_APC, 1, [ ])

  dnl Write more examples of tests here...
  PHP_EXTENSION(apc, $ext_shared)
fi

dnl Check for system type.
AC_DEFUN(AC_SYSTEM,[
	AC_MSG_CHECKING([system type])
	SYSTEM="`uname -s | tr a-z A-Z`"
  AC_MSG_RESULT($SYSTEM)
])                                              
AC_SYSTEM()
case $SYSTEM in
	*LINUX*)
		AC_DEFINE(__LINUX__, 1, [ ])
		;;
	*BSD*)
		AC_DEFINE(__BSD__, 1, [ ])
		;;
	*SUNOS*)
		AC_DEFINE(__SUNOS__, 1, [ ])
		;;
	dnl else define nothing
esac
