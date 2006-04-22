dnl
dnl "$Id$"
dnl
dnl   Default cupsd configuration settings for the Common UNIX Printing System
dnl   (CUPS).
dnl
dnl   Copyright 2006 by Easy Software Products, all rights reserved.
dnl
dnl   These coded instructions, statements, and computer programs are the
dnl   property of Easy Software Products and are protected by Federal
dnl   copyright law.  Distribution and use rights are outlined in the file
dnl   "LICENSE.txt" which should have been included with this file.  If this
dnl   file is missing or damaged please contact Easy Software Products
dnl   at:
dnl
dnl       Attn: CUPS Licensing Information
dnl       Easy Software Products
dnl       44141 Airport View Drive, Suite 204
dnl       Hollywood, Maryland 20636 USA
dnl
dnl       Voice: (301) 373-9600
dnl       EMail: cups-info@cups.org
dnl         WWW: http://www.cups.org
dnl

dnl Default langugages...
AC_ARG_WITH(languages, [  --with-languages        set installed languages, default="es ja" ],
	LANGUAGES="$withval",
	LANGUAGES="es ja pl sv")
AC_SUBST(LANGUAGES)

dnl Default ConfigFilePerm
AC_ARG_WITH(config_perm, [  --with-config-file-perm set default ConfigFilePerm value, default=0640],
	CUPS_CONFIG_FILE_PERM="$withval",
	if test "x$uname" = xDarwin; then
		CUPS_CONFIG_FILE_PERM="644"
	else
		CUPS_CONFIG_FILE_PERM="640"
	fi)
AC_SUBST(CUPS_CONFIG_FILE_PERM)
AC_DEFINE_UNQUOTED(CUPS_DEFAULT_CONFIG_FILE_PERM, 0$CUPS_CONFIG_FILE_PERM)

dnl Default LogFilePerm
AC_ARG_WITH(log_perm, [  --with-log-file-perm    set default LogFilePerm value, default=0644],
	CUPS_LOG_FILE_PERM="$withval",
	CUPS_LOG_FILE_PERM="644")
AC_SUBST(CUPS_LOG_FILE_PERM)
AC_DEFINE_UNQUOTED(CUPS_DEFAULT_LOG_FILE_PERM, 0$CUPS_LOG_FILE_PERM)

dnl Default Browsing
AC_ARG_ENABLE(browsing, [  --enable-browsing       enable Browsing by default, default=yes])
if test "x$enable_browsing" = xno; then
	CUPS_BROWSING="No"
	AC_DEFINE_UNQUOTED(CUPS_DEFAULT_BROWSING, 0)
else
	CUPS_BROWSING="Yes"
	AC_DEFINE_UNQUOTED(CUPS_DEFAULT_BROWSING, 1)
fi
AC_SUBST(CUPS_BROWSING)

dnl Default BrowseLocalProtocols
AC_ARG_WITH(browse_local, [  --with-local-protocols  set default BrowseLocalProtocols, default="CUPS"],
	CUPS_BROWSE_LOCAL_PROTOCOLS="$withval",
	CUPS_BROWSE_LOCAL_PROTOCOLS="CUPS")
AC_SUBST(CUPS_BROWSE_LOCAL_PROTOCOLS)
AC_DEFINE_UNQUOTED(CUPS_DEFAULT_BROWSE_LOCAL_PROTOCOLS,
	"$CUPS_BROWSE_LOCAL_PROTOCOLS")

dnl Default BrowseRemoteProtocols
AC_ARG_WITH(browse_remote, [  --with-remote-protocols set default BrowseRemoteProtocols, default="CUPS"],
	CUPS_BROWSE_REMOTE_PROTOCOLS="$withval",
	CUPS_BROWSE_REMOTE_PROTOCOLS="CUPS")
AC_SUBST(CUPS_BROWSE_REMOTE_PROTOCOLS)
AC_DEFINE_UNQUOTED(CUPS_DEFAULT_BROWSE_REMOTE_PROTOCOLS,
	"$CUPS_BROWSE_REMOTE_PROTOCOLS")

dnl Default BrowseShortNames
AC_ARG_ENABLE(browse_short, [  --enable-browse-short-names
                          enable BrowseShortNames by default, default=yes])
if test "x$enable_browse_short" = xno; then
	CUPS_BROWSE_SHORT_NAMES="No"
	AC_DEFINE_UNQUOTED(CUPS_DEFAULT_BROWSE_SHORT_NAMES, 0)
else
	CUPS_BROWSE_SHORT_NAMES="Yes"
	AC_DEFINE_UNQUOTED(CUPS_DEFAULT_BROWSE_SHORT_NAMES, 1)
fi
AC_SUBST(CUPS_BROWSE_SHORT_NAMES)

dnl Default DefaultShared
AC_ARG_ENABLE(default_shared, [  --enable-default-shared enable DefaultShared by default, default=yes])
if test "x$enable_default_shared" = xno; then
	CUPS_DEFAULT_SHARED="No"
	AC_DEFINE_UNQUOTED(CUPS_DEFAULT_DEFAULT_SHARED, 0)
else
	CUPS_DEFAULT_SHARED="Yes"
	AC_DEFINE_UNQUOTED(CUPS_DEFAULT_DEFAULT_SHARED, 1)
fi
AC_SUBST(CUPS_DEFAULT_SHARED)

dnl Default ImplicitClasses
AC_ARG_ENABLE(implicit, [  --enable-implicit-classes
                          enable ImplicitClasses by default, default=yes])
if test "x$enable_implicit" = xno; then
	CUPS_IMPLICIT_CLASSES="No"
	AC_DEFINE_UNQUOTED(CUPS_DEFAULT_IMPLICIT_CLASSES, 0)
else
	CUPS_IMPLICIT_CLASSES="Yes"
	AC_DEFINE_UNQUOTED(CUPS_DEFAULT_IMPLICIT_CLASSES, 1)
fi
AC_SUBST(CUPS_IMPLICIT_CLASSES)

dnl Default UseNetworkDefault
AC_ARG_ENABLE(network_default, [  --enable-use-network-default
                          enable UseNetworkDefault by default, default=auto])
if test "x$enable_network_default" != xno; then
	AC_MSG_CHECKING(whether to use network default printers)
	if test "x$enable_network_default" = xyes -o $uname != Darwin; then
		CUPS_USE_NETWORK_DEFAULT="Yes"
		AC_DEFINE_UNQUOTED(CUPS_DEFAULT_USE_NETWORK_DEFAULT, 1)
		AC_MSG_RESULT(yes)
	else
		CUPS_USE_NETWORK_DEFAULT="No"
		AC_DEFINE_UNQUOTED(CUPS_DEFAULT_USE_NETWORK_DEFAULT, 0)
		AC_MSG_RESULT(no)
	fi
else
	CUPS_USE_NETWORK_DEFAULT="No"
	AC_DEFINE_UNQUOTED(CUPS_DEFAULT_USE_NETWORK_DEFAULT, 0)
fi
AC_SUBST(CUPS_USE_NETWORK_DEFAULT)

dnl Determine the correct username and group for this OS...
AC_ARG_WITH(cups-user, [  --with-cups-user        set default user for CUPS],
	CUPS_USER="$withval",
	AC_MSG_CHECKING(for default print user)
	if test -f /etc/passwd; then
		CUPS_USER=""
		for user in lp lpd guest daemon nobody; do
			if test "`grep \^${user}: /etc/passwd`" != ""; then
				CUPS_USER="$user"
				AC_MSG_RESULT($user)
				break;
			fi
		done

		if test x$CUPS_USER = x; then
			CUPS_USER="nobody"
			AC_MSG_RESULT(not found, using "$CUPS_USER")
		fi
	else
		CUPS_USER="nobody"
		AC_MSG_RESULT(no password file, using "$CUPS_USER")
	fi)

AC_ARG_WITH(cups-group, [  --with-cups-group       set default group for CUPS],
	CUPS_GROUP="$withval",
	AC_MSG_CHECKING(for default print group)
	if test -f /etc/group; then
		GROUP_LIST="lp nobody"
		CUPS_GROUP=""
		for group in $GROUP_LIST; do
			if test "`grep \^${group}: /etc/group`" != ""; then
				CUPS_GROUP="$group"
				AC_MSG_RESULT($group)
				break;
			fi
		done

		if test x$CUPS_GROUP = x; then
			CUPS_GROUP="nobody"
			AC_MSG_RESULT(not found, using "$CUPS_GROUP")
		fi
	else
		CUPS_GROUP="nobody"
		AC_MSG_RESULT(no group file, using "$CUPS_GROUP")
	fi)

AC_ARG_WITH(system-groups, [  --with-system-groups    set default system groups for CUPS],
	CUPS_SYSTEM_GROUPS="$withval",
	if test x$uname = xDarwin; then
		GROUP_LIST="admin"
	else
		GROUP_LIST="lpadmin sys system root"
	fi

	AC_MSG_CHECKING(for default system groups)
	if test -f /etc/group; then
		CUPS_SYSTEM_GROUPS=""
		for group in $GROUP_LIST; do
			if test "`grep \^${group}: /etc/group`" != ""; then
				if test "x$CUPS_SYSTEM_GROUPS" = x; then
					CUPS_SYSTEM_GROUPS="$group"
				else
					CUPS_SYSTEM_GROUPS="$CUPS_SYSTEM_GROUPS $group"
				fi
			fi
		done

		if test "x$CUPS_SYSTEM_GROUPS" = x; then
			CUPS_SYSTEM_GROUPS="$GROUP_LIST"
			AC_MSG_RESULT(no groups found, using "$CUPS_SYSTEM_GROUPS")
		else
			AC_MSG_RESULT("$CUPS_SYSTEM_GROUPS")
		fi
	else
		CUPS_SYSTEM_GROUPS="$GROUP_LIST"
		AC_MSG_RESULT(no group file, using "$CUPS_SYSTEM_GROUPS")
	fi)

CUPS_PRIMARY_SYSTEM_GROUP="`echo $CUPS_SYSTEM_GROUPS | awk '{print $1}'`"

AC_SUBST(CUPS_USER)
AC_SUBST(CUPS_GROUP)
AC_SUBST(CUPS_SYSTEM_GROUPS)
AC_SUBST(CUPS_PRIMARY_SYSTEM_GROUP)

AC_DEFINE_UNQUOTED(CUPS_DEFAULT_USER, "$CUPS_USER")
AC_DEFINE_UNQUOTED(CUPS_DEFAULT_GROUP, "$CUPS_GROUP")
AC_DEFINE_UNQUOTED(CUPS_DEFAULT_SYSTEM_GROUPS, "$CUPS_SYSTEM_GROUPS")

dnl Default printcap file...
AC_ARG_WITH(printcap, [  --with-printcap     set default printcap file],
	default_printcap="$withval",
	default_printcap="/etc/printcap")

if test x$enable_printcap != xno -a x$default_printcap != xno; then
	if test "x$default_printcap" = "x/etc/printcap" -a "$uname" = "Darwin" -a $uversion -ge 90; then
		CUPS_DEFAULT_PRINTCAP=""
	else
		CUPS_DEFAULT_PRINTCAP="$default_printcap"
	fi
else
	CUPS_DEFAULT_PRINTCAP=""
fi

AC_DEFINE_UNQUOTED(CUPS_DEFAULT_PRINTCAP, "$CUPS_DEFAULT_PRINTCAP")

dnl
dnl End of "$Id$".
dnl
