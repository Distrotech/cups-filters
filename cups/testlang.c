/*
 * "$Id: testlang.c,v 1.1 2002/05/23 20:07:34 mike Exp $"
 *
 *   HTTP test program for the Common UNIX Printing System (CUPS).
 *
 *   Copyright 1997-2002 by Easy Software Products.
 *
 *   These coded instructions, statements, and computer programs are the
 *   property of Easy Software Products and are protected by Federal
 *   copyright law.  Distribution and use rights are outlined in the file
 *   "LICENSE.txt" which should have been included with this file.  If this
 *   file is missing or damaged please contact Easy Software Products
 *   at:
 *
 *       Attn: CUPS Licensing Information
 *       Easy Software Products
 *       44141 Airport View Drive, Suite 204
 *       Hollywood, Maryland 20636-3111 USA
 *
 *       Voice: (301) 373-9603
 *       EMail: cups-info@cups.org
 *         WWW: http://www.cups.org
 *
 *   This file is subject to the Apple OS-Developed Software exception.
 *
 * Contents:
 *
 *   main() - Load the specified language and show the strings for yes and no.
 */

/*
 * Include necessary headers...
 */

#include <stdio.h>
#include "language.h"


/*
 * 'main()' - Load the specified language and show the strings for yes and no.
 */

int					/* O - Exit status */
main(int  argc,				/* I - Number of command-line arguments */
     char *argv[])			/* I - Command-line arguments */
{
  cups_lang_t		*language;	/* Message catalog */
  static const char	*charsets[] =	/* Character sets */
			{
			  "us-ascii",
			  "iso-8859-1",
			  "iso-8859-2",
			  "iso-8859-3",
			  "iso-8859-4",
			  "iso-8859-5",
			  "iso-8859-6",
			  "iso-8859-7",
			  "iso-8859-8",
			  "iso-8859-9",
			  "iso-8859-10",
			  "utf-8",
			  "iso8859-13",
			  "iso8859-14",
			  "iso8859-15",
			  "windows-874",
			  "windows-1250",
			  "windows-1251",
			  "windows-1252",
			  "windows-1253",
			  "windows-1254",
			  "windows-1255",
			  "windows-1256",
			  "windows-1257",
			  "windows-1258",
			  "koi8-r",
			  "koi8-u"
			};


  if (argc == 1)
    language = cupsLangDefault();
  else
    language = cupsLangGet(argv[1]);

  printf("Language = \"%s\"\n", language->language);
  printf("Encoding = \"%s\"\n", charsets[language->encoding]);
  printf("No       = \"%s\"\n", cupsLangString(language, CUPS_MSG_NO));
  printf("Yes      = \"%s\"\n", cupsLangString(language, CUPS_MSG_YES));

  return (0);
}


/*
 * End of "$Id: testlang.c,v 1.1 2002/05/23 20:07:34 mike Exp $".
 */
