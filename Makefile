#
# "$Id$"
#
#   Top-level Makefile for the Common UNIX Printing System (CUPS).
#
#   Copyright 1997-2006 by Easy Software Products, all rights reserved.
#
#   These coded instructions, statements, and computer programs are the
#   property of Easy Software Products and are protected by Federal
#   copyright law.  Distribution and use rights are outlined in the file
#   "LICENSE.txt" which should have been included with this file.  If this
#   file is missing or damaged please contact Easy Software Products
#   at:
#
#       Attn: CUPS Licensing Information
#       Easy Software Products
#       44141 Airport View Drive, Suite 204
#       Hollywood, Maryland 20636-3142 USA
#
#       Voice: (301) 373-9600
#       EMail: cups-info@cups.org
#         WWW: http://www.cups.org
#

include Makedefs

#
# Directories to make...
#

DIRS	=	cups backend berkeley cgi-bin filter locale man monitor \
		notifier $(PDFTOPS) scheduler systemv test \
		$(PHPDIR) \
		conf data doc fonts ppd templates


#
# Make all targets...
#

all:
	chmod +x cups-config
	for dir in $(DIRS); do\
		echo Making all in $$dir... ;\
		(cd $$dir ; $(MAKE) $(MFLAGS)) || exit 1;\
	done


#
# Remove object and target files...
#

clean:
	for dir in $(DIRS); do\
		echo Cleaning in $$dir... ;\
		(cd $$dir; $(MAKE) $(MFLAGS) clean) || exit 1;\
	done


#
# Remove all non-distribution files...
#

distclean:	clean
	$(RM) Makedefs config.h config.log config.status
	$(RM) cups-config conf/cupsd.conf conf/pam.std
	$(RM) doc/help/standard.html doc/index.html
	$(RM) init/cups.sh init/cups-lpd
	$(RM) man/cups-deviced.man man/cups-driverd.man
	$(RM) man/cups-lpd.man man/cupsaddsmb.man man/cupsd.man
	$(RM) man/cupsd.conf.man man/lpoptions.man
	$(RM) packaging/cups templates/edit-config.tmpl templates/header.tmpl
	-$(RM) doc/*/index.html
	-$(RM) templates/*/edit-config.tmpl
	-$(RM) templates/*/header.tmpl
	-$(RM) -r autom4te*.cache


#
# Make dependencies
#

depend:
	for dir in $(DIRS); do\
		echo Making dependencies in $$dir... ;\
		(cd $$dir; $(MAKE) $(MFLAGS) depend) || exit 1;\
	done


#
# Install object and target files...
#

install:	installhdrs
	for dir in $(DIRS); do\
		echo Installing in $$dir... ;\
		(cd $$dir; $(MAKE) $(MFLAGS) install) || exit 1;\
	done
	echo Installing cups-config script...
	$(INSTALL_DIR) -m 755 $(BINDIR)
	$(INSTALL_SCRIPT) cups-config $(BINDIR)/cups-config
	if test "x$(INITDIR)" != x; then \
		echo Installing init scripts...; \
		$(INSTALL_DIR) -m 755 $(BUILDROOT)$(INITDIR)/init.d; \
		$(INSTALL_SCRIPT) init/cups.sh $(BUILDROOT)$(INITDIR)/init.d/cups; \
		$(INSTALL_DIR) -m 755 $(BUILDROOT)$(INITDIR)/rc0.d; \
		$(INSTALL_SCRIPT) init/cups.sh  $(BUILDROOT)$(INITDIR)/rc0.d/K00cups; \
		$(INSTALL_DIR) -m 755 $(BUILDROOT)$(INITDIR)/rc2.d; \
		$(INSTALL_SCRIPT) init/cups.sh $(BUILDROOT)$(INITDIR)/rc2.d/S99cups; \
		$(INSTALL_DIR) -m 755 $(BUILDROOT)$(INITDIR)/rc3.d; \
		$(INSTALL_SCRIPT) init/cups.sh $(BUILDROOT)$(INITDIR)/rc3.d/S99cups; \
		$(INSTALL_DIR) -m 755 $(BUILDROOT)$(INITDIR)/rc5.d; \
		$(INSTALL_SCRIPT) init/cups.sh $(BUILDROOT)$(INITDIR)/rc5.d/S99cups; \
	fi
	if test "x$(INITDIR)" = x -a "x$(INITDDIR)" != x; then \
		$(INSTALL_DIR) $(BUILDROOT)$(INITDDIR); \
		if test "$(INITDDIR)" = "/System/Library/StartupItems/PrintingServices"; then \
			echo Installing StartupItems files...; \
			$(INSTALL_SCRIPT) init/PrintingServices $(BUILDROOT)$(INITDDIR)/PrintingServices; \
			$(INSTALL_DATA) init/StartupParameters.plist $(BUILDROOT)$(INITDDIR)/StartupParameters.plist; \
			$(INSTALL_DIR) -m 755 $(BUILDROOT)$(INITDDIR)/Resources/English.lproj; \
			$(INSTALL_DATA) init/Localizable.strings $(BUILDROOT)$(INITDDIR)/Resources/English.lproj/Localizable.strings; \
		elif test "$(INITDDIR)" = "/System/Library/LaunchDaemons"; then \
			echo Installing LaunchDaemons configuration file...; \
			$(INSTALL_DATA) init/org.cups.cupsd.plist $(BUILDROOT)$(DEFAULT_LAUNCHD_CONF); \
		else \
			echo Installing RC script...; \
			$(INSTALL_SCRIPT) init/cups.sh $(BUILDROOT)$(INITDDIR)/cups; \
		fi \
	fi
	if test "x$(DBUSDIR)" != x; then \
		echo Installing cups.conf in $(DBUSDIR)...;\
		$(INSTALL_DIR) -m 755 $(BUILDROOT)$(DBUSDIR); \
		$(INSTALL_DATA) packaging/cups-dbus.conf $(BUILDROOT)$(DBUSDIR)/cups.conf; \
	fi
	if test "x$(XINETD)" != x; then \
		echo Installing xinetd configuration file for cups-lpd...; \
		$(INSTALL_DIR) -m 755 $(BUILDROOT)$(XINETD); \
		$(INSTALL_DATA) init/cups-lpd $(BUILDROOT)$(XINETD)/cups-lpd; \
	fi
	if test -d /usr/share/applications; then \
		echo Installing desktop icons...; \
		$(INSTALL_DIR) -m 755 $(BUILDROOT)/usr/share/applications; \
		$(INSTALL_DATA) desktop/cups.desktop $(BUILDROOT)/usr/share/applications; \
		$(INSTALL_DIR) -m 755 $(BUILDROOT)/usr/share/icons/hicolor/16x16/apps; \
		$(INSTALL_DATA) desktop/cups-16.png $(BUILDROOT)/usr/share/icons/hicolor/16x16/apps/cups.png; \
		$(INSTALL_DIR) -m 755 $(BUILDROOT)/usr/share/icons/hicolor/32x32/apps; \
		$(INSTALL_DATA) desktop/cups-32.png $(BUILDROOT)/usr/share/icons/hicolor/32x32/apps/cups.png; \
		$(INSTALL_DIR) -m 755 $(BUILDROOT)/usr/share/icons/hicolor/64x64/apps; \
		$(INSTALL_DATA) desktop/cups-64.png $(BUILDROOT)/usr/share/icons/hicolor/64x64/apps/cups.png; \
		$(INSTALL_DIR) -m 755 $(BUILDROOT)/usr/share/icons/hicolor/128x128/apps; \
		$(INSTALL_DATA) desktop/cups-128.png $(BUILDROOT)/usr/share/icons/hicolor/128x128/apps/cups.png; \
	fi


#
# Install source and header files...
#

installsrc:
	gnutar --dereference --exclude=.svn -cf - . | gnutar -C $(SRCROOT) -xf -

installhdrs:
	(cd cups ; $(MAKE) $(MFLAGS) installhdrs) || exit 1;\
	(cd filter ; $(MAKE) $(MFLAGS) installhdrs) || exit 1;


#
# Uninstall object and target files...
#

uninstall:
	for dir in $(DIRS); do\
		echo Uninstalling in $$dir... ;\
		(cd $$dir; $(MAKE) $(MFLAGS) uninstall) || exit 1;\
	done
	echo Uninstalling cups-config script...
	$(RM) $(BINDIR)/cups-config
	-$(RMDIR) $(BINDIR)
	echo Uninstalling startup script...
	if test "x$(INITDIR)" != x; then \
		$(RM) $(BUILDROOT)$(INITDIR)/init.d/cups; \
		$(RMDIR) $(BUILDROOT)$(INITDIR)/init.d; \
		$(RM)  $(BUILDROOT)$(INITDIR)/rc0.d/K00cups; \
		$(RMDIR) $(BUILDROOT)$(INITDIR)/rc0.d; \
		$(RM) $(BUILDROOT)$(INITDIR)/rc2.d/S99cups; \
		$(RMDIR) $(BUILDROOT)$(INITDIR)/rc2.d; \
		$(RM) $(BUILDROOT)$(INITDIR)/rc3.d/S99cups; \
		$(RMDIR) $(BUILDROOT)$(INITDIR)/rc3.d; \
		$(RM) $(BUILDROOT)$(INITDIR)/rc5.d/S99cups; \
		$(RMDIR) $(BUILDROOT)$(INITDIR)/rc5.d; \
	fi
	if test "x$(INITDIR)" = x -a "x$(INITDDIR)" != x; then \
		if test "$(INITDDIR)" = "/System/Library/StartupItems/PrintingServices"; then \
			$(RM) $(BUILDROOT)$(INITDDIR)/PrintingServices; \
			$(RM) $(BUILDROOT)$(INITDDIR)/StartupParameters.plist; \
			$(RM) $(BUILDROOT)$(INITDDIR)/Resources/English.lproj/Localizable.strings; \
			$(RMDIR) $(BUILDROOT)$(INITDDIR)/Resources/English.lproj; \
		elif test "$(INITDDIR)" = "/System/Library/LaunchDaemons"; then \
			$(RM) $(BUILDROOT)$(DEFAULT_LAUNCHD_CONF); \
		else \
			$(INSTALL_SCRIPT) init/cups.sh $(BUILDROOT)$(INITDDIR)/cups; \
		fi \
		$(RMDIR) $(BUILDROOT)$(INITDDIR); \
	fi
	if test "x$(DBUSDIR)" != x; then \
		echo Uninstalling cups.conf in $(DBUSDIR)...;\
		$(RM) $(BUILDROOT)$(DBUSDIR)/cups.conf; \
		$(RMDIR) $(BUILDROOT)$(DBUSDIR); \
	fi
	$(RM) $(BUILDROOT)/etc/xinetd.d/cups-lpd
	$(RM) $(BUILDROOT)/usr/share/applications/cups.desktop
	$(RM) $(BUILDROOT)/usr/share/icons/hicolor/16x16/apps/cups.png
	$(RM) $(BUILDROOT)/usr/share/icons/hicolor/32x32/apps/cups.png
	$(RM) $(BUILDROOT)/usr/share/icons/hicolor/64x64/apps/cups.png
	$(RM) $(BUILDROOT)/usr/share/icons/hicolor/128x128/apps/cups.png


#
# Run the test suite...
#

check test:	all
	echo Running CUPS test suite...
	cd test; ./run-stp-tests.sh


#
# Make software distributions using EPM (http://www.easysw.com/epm/)...
#

EPMFLAGS	=	-v

aix bsd deb depot inst osx pkg rpm setld slackware swinstall tardist:
	epm $(EPMFLAGS) -f $@ cups packaging/cups.list

epm:
	epm $(EPMFLAGS) cups packaging/cups.list


#
# End of "$Id$".
#
