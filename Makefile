#
# "$Id: Makefile,v 1.31.2.5 2002/01/24 18:54:45 mike Exp $"
#
#   Top-level Makefile for the Common UNIX Printing System (CUPS).
#
#   Copyright 1997-2002 by Easy Software Products, all rights reserved.
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
#       Hollywood, Maryland 20636-3111 USA
#
#       Voice: (301) 373-9603
#       EMail: cups-info@cups.org
#         WWW: http://www.cups.org
#

include Makedefs

#
# Directories to make...
#

DIRS	=	cups backend berkeley cgi-bin filter man pdftops \
		scheduler systemv

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
# Install object and target files...
#

install:
	for dir in $(DIRS); do\
		echo Installing in $$dir... ;\
		(cd $$dir; $(MAKE) $(MFLAGS) install) || exit 1;\
	done
	echo Installing in conf...
	(cd conf; $(MAKE) $(MFLAGS) install)
	echo Installing in data...
	(cd data; $(MAKE) $(MFLAGS) install)
	echo Installing in doc...
	(cd doc; $(MAKE) $(MFLAGS) install)
	echo Installing in locale...
	(cd locale; $(MAKE) $(MFLAGS) install)
	echo Installing in ppd...
	(cd ppd; $(MAKE) $(MFLAGS) install)
	echo Installing in templates...
	(cd templates; $(MAKE) $(MFLAGS) install)
	echo Installing cups-config script...
	$(INSTALL_DIR) $(BINDIR)
	$(INSTALL_SCRIPT) cups-config $(BINDIR)/cups-config
	echo Installing startup script...
	if test "x$(INITDIR)" != "x"; then \
		$(INSTALL_DIR) $(BUILDROOT)$(INITDIR)/init.d; \
		$(INSTALL_SCRIPT) cups.sh $(BUILDROOT)$(INITDIR)/init.d/cups; \
		$(INSTALL_DIR) $(BUILDROOT)$(INITDIR)/rc0.d; \
		$(INSTALL_SCRIPT) cups.sh  $(BUILDROOT)$(INITDIR)/rc0.d/K00cups; \
		$(INSTALL_DIR) $(BUILDROOT)$(INITDIR)/rc2.d; \
		$(INSTALL_SCRIPT) cups.sh $(BUILDROOT)$(INITDIR)/rc2.d/S99cups; \
		$(INSTALL_DIR) $(BUILDROOT)$(INITDIR)/rc3.d; \
		$(INSTALL_SCRIPT) cups.sh $(BUILDROOT)$(INITDIR)/rc3.d/S99cups; \
		$(INSTALL_DIR) $(BUILDROOT)$(INITDIR)/rc5.d; \
		$(INSTALL_SCRIPT) cups.sh $(BUILDROOT)$(INITDIR)/rc5.d/S99cups; \
	fi
	if test "x$(INITDIR)" = "x" -a "x$(INITDDIR)" != "x"; then \
		$(INSTALL_DIR) $(BUILDROOT)$(INITDDIR); \
		if test "$(INITDDIR)" = "/System/Library/StartupItems/CUPS"; then \
			$(INSTALL_SCRIPT) cups.sh $(BUILDROOT)$(INITDDIR)/CUPS; \
			$(INSTALL_DATA) cups.plist $(BUILDROOT)$(INITDDIR)/StartupParameters.plist; \
			$(INSTALL_DIR) $(BUILDROOT)$(INITDDIR)/Resources/English.lproj; \
			$(INSTALL_DATA) cups.strings $(BUILDROOT)$(INITDDIR)/Resources/English.lproj/Localizable.strings; \
		else \
			$(INSTALL_SCRIPT) cups.sh $(BUILDROOT)$(INITDDIR)/cups; \
		fi \
	fi


#
# Run the test suite...
#

test:	all
	echo Running CUPS test suite...
	cd test; ./run-stp-tests.sh


#
# Make software distributions using EPM (http://www.easysw.com/epm)...
#

EPMFLAGS	=	-v

aix:
	epm $(EPMFLAGS) -f aix cups

bsd:
	epm $(EPMFLAGS) -f bsd cups

epm:
	epm $(EPMFLAGS) cups

rpm:
	epm $(EPMFLAGS) -f rpm cups

deb:
	epm $(EPMFLAGS) -f deb cups

depot:
	epm $(EPMFLAGS) -f depot cups

pkg:
	epm $(EPMFLAGS) -f pkg cups

tardist:
	epm $(EPMFLAGS) -f tardist cups

#
# End of "$Id: Makefile,v 1.31.2.5 2002/01/24 18:54:45 mike Exp $".
#
