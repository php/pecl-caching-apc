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
