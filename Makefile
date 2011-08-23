#
# "$Id$"
#
#   Top-level Makefile for CUPS Legacy.
#
#   Copyright 2007-2011 by Apple Inc.
#   Copyright 1997-2007 by Easy Software Products, all rights reserved.
#
#   These coded instructions, statements, and computer programs are the
#   property of Apple Inc. and are protected by Federal copyright
#   law.  Distribution and use rights are outlined in the file "LICENSE.txt"
#   which should have been included with this file.  If this file is
#   file is missing or damaged, see the license at "http://www.cups.org/".
#

include Makedefs


#
# Directories to make...
#

DIRS	=	cupslegacy backend filter $(PHPDIR) conf data fonts


#
# Make all targets...
#

all:
	chmod +x cups-config
	echo Using ALL_CFLAGS="$(ALL_CFLAGS)"
	echo Using CC="$(CC)"
	echo Using DSOFLAGS="$(DSOFLAGS)"
	echo Using LDFLAGS="$(LDFLAGS)"
	echo Using LIBS="$(LIBS)"
	for dir in $(DIRS); do\
		echo Making all in $$dir... ;\
		(cd $$dir ; $(MAKE) $(MFLAGS) all) || exit 1;\
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
	-$(RM) -r autom4te*.cache clang filter/test


#
# Make dependencies
#

depend:
	for dir in $(DIRS); do\
		echo Making dependencies in $$dir... ;\
		(cd $$dir; $(MAKE) $(MFLAGS) depend) || exit 1;\
	done


#
# Run the clang.llvm.org static code analysis tool on the C sources.
# (at least checker-231 is required for scan-build to work this way)
#

.PHONY: clang clang-changes
clang:
	$(RM) -r clang
	scan-build -V -k -o `pwd`/clang $(MAKE) $(MFLAGS) clean all
clang-changes:
	scan-build -V -k -o `pwd`/clang $(MAKE) $(MFLAGS) all


#
# Generate a ctags file...
#

ctags:
	ctags -R .


#
# Install everything...
#

install:	install-data install-headers install-libs install-exec


#
# Install data files...
#

install-data:
	for dir in $(DIRS); do\
		echo Installing data files in $$dir... ;\
		(cd $$dir; $(MAKE) $(MFLAGS) install-data) || exit 1;\
	done


#
# Install header files...
#

install-headers:


#
# Install programs...
#

install-exec:	all
	for dir in $(DIRS); do\
		echo Installing programs in $$dir... ;\
		(cd $$dir; $(MAKE) $(MFLAGS) install-exec) || exit 1;\
	done


#
# Install libraries...
#

install-libs:	libs
	for dir in $(DIRS); do\
		echo Installing libraries in $$dir... ;\
		(cd $$dir; $(MAKE) $(MFLAGS) install-libs) || exit 1;\
	done


#
# Uninstall object and target files...
#

uninstall:
	for dir in $(DIRS); do\
		echo Uninstalling in $$dir... ;\
		(cd $$dir; $(MAKE) $(MFLAGS) uninstall) || exit 1;\
	done


#
# Don't run top-level build targets in parallel...
#

.NOTPARALLEL:


#
# End of "$Id$".
#
