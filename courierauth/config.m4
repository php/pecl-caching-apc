dnl $Id$
dnl config.m4 for extension courierauth

PHP_ARG_WITH(courierauth, for courierauth support,
	[  --with-courierauth-config
                          Path to courierauthconfig script])
PHP_ARG_WITH(courierauth-security-risk, whether to enable passwd security risk,
	[  --with-courierauth-security-risk
                          Enable passwd security risk], no, no)

if test "$PHP_COURIERAUTH" != "no"; then
	AC_MSG_CHECKING(for courierauthconfig)
	COURIERAUTHCONFIG=
	for i in "$PHP_COURIERAUTH_CONFIG" /usr/local/bin/courierauthconfig /usr/bin/courierauthconfig "`which courierauthconfig`"; do
		if test -x "$i"; then
			COURIERAUTHCONFIG="$i"
			break
		fi
	done
	if test -z "$COURIERAUTHCONFIG"; then
		AC_MSG_ERROR(not found)
	else
		AC_MSG_RESULT($COURIERAUTHCONFIG)
	fi
	
	PHP_EVAL_LIBLINE("`$COURIERAUTHCONFIG --ldflags` -lcourierauth", COURIERAUTH_SHARED_LIBADD)
	PHP_EVAL_INCLINE(`$COURIERAUTHCONFIG --cppflags`)
	
	if test "$PHP_COURIERAUTH_SECURITY_RISK" = "yes"; then
		AC_DEFINE(PHP_COURIERAUTH_SECURITY_RISK, 1, [passwd security risk])
	else
		AC_DEFINE(PHP_COURIERAUTH_SECURITY_RISK, 0, [passwd security risk])
	fi
	
	PHP_SUBST(COURIERAUTH_SHARED_LIBADD)
	
	PHP_NEW_EXTENSION(courierauth, courierauth.c, $ext_shared)
fi
