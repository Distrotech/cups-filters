dnl
dnl "$Id$"
dnl
dnl   DNS Service Discovery (aka Bonjour) stuff for the Common UNIX Printing System (CUPS).
dnl
dnl   http://www.dns-sd.org
dnl   http://www.multicastdns.org/
dnl   http://developer.apple.com/networking/bonjour/
dnl
dnl   Copyright ...
dnl
dnl

AC_ARG_ENABLE(dnssd, [  --enable-dnssd            turn on DNS Service Discovery support, default=yes])
AC_ARG_WITH(dnssd-libs, [  --with-dnssd-libs        set directory for DNS Service Discovery library],
	LDFLAGS="-L$withval $LDFLAGS"
	DSOFLAGS="-L$withval $DSOFLAGS",)
AC_ARG_WITH(dnssd-includes, [  --with-dnssd-includes    set directory for DNS Service Discovery includes],
	CFLAGS="-I$withval $CFLAGS"
	CXXFLAGS="-I$withval $CXXFLAGS"
	CPPFLAGS="-I$withval $CPPFLAGS",)

DNSSDLIBS=""

if test x$enable_dnssd != xno; then
	AC_CHECK_HEADER(dns_sd.h, [
		AC_DEFINE(HAVE_DNSSD)
		case "$uname" in
			Darwin*)
				# Darwin and MacOS X...
				DNSSDLIBS="-framework CoreFoundation -framework SystemConfiguration"
				AC_DEFINE(HAVE_COREFOUNDATION)
				AC_DEFINE(HAVE_SYSTEMCONFIGURATION)
				;;
			*)
				# All others...
				DNSSDLIBS="???"
				;;
		esac
	])
fi

AC_SUBST(DNSSDLIBS)

dnl
dnl End of "$Id$".
dnl
