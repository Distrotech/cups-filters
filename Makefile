#
# "$Id: Makefile,v 1.13 2000/02/08 20:38:40 mike Exp $"
#
#   Top-level Makefile for the Common UNIX Printing System (CUPS).
#
#   Copyright 1997-1999 by Easy Software Products, all rights reserved.
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

DIRS	=	cups backend berkeley cgi-bin filter man pstoraster \
		scheduler systemv

#
# Make all targets...
#

all:
	for dir in $(DIRS); do\
		echo Making all in $$dir... ;\
		(cd $$dir; $(MAKE)) || break;\
	done

#
# Remove object and target files...
#

clean:
	for dir in $(DIRS); do\
		echo Cleaning in $$dir... ;\
		(cd $$dir; $(MAKE) clean) || break;\
	done

#
# Install object and target files...
#

install:
	for dir in $(DIRS); do\
		echo Installing in $$dir... ;\
		(cd $$dir; $(MAKE) install) || break;\
	done
	echo Installing in conf...
	(cd conf; $(MAKE) install)
	echo Installing in data...
	(cd data; $(MAKE) install)
	echo Installing in doc...
	(cd doc; $(MAKE) install)
	echo Installing in fonts...
	(cd fonts; $(MAKE) install)
	echo Installing in ppd...
	(cd ppd; $(MAKE) install)
	echo Installing in templates...
	(cd templates; $(MAKE) install)

#
# Make a software distribution...
#

epm:
	epm -v cups

rpm:
	epm -v -f rpm cups

deb:
	epm -v -f deb cups

tardist:
	epm -v -f tardist cups

#
# End of "$Id: Makefile,v 1.13 2000/02/08 20:38:40 mike Exp $".
#
