/*
 * "$Id: classes.c,v 1.3 1999/01/24 14:25:11 mike Exp $"
 *
 *   for the Common UNIX Printing System (CUPS).
 *
 *   Copyright 1997-1999 by Easy Software Products, all rights reserved.
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
 *       44145 Airport View Drive, Suite 204
 *       Hollywood, Maryland 20636-3111 USA
 *
 *       Voice: (301) 373-9603
 *       EMail: cups-info@cups.org
 *         WWW: http://www.cups.org
 *
 * Contents:
 *
 *
 */

/*
 * Include necessary headers...
 */

#include "cupsd.h"


class_t *
AddClass(char *name)
{
  return (NULL);
}


void
AddPrinterToClass(class_t   *c,
                  printer_t *p)
{
}


void
DeleteAllClasses(void)
{
}


void
DeleteClass(class_t *c)
{
}


printer_t *
FindAvailablePrinter(char *name)
{
  return (NULL);
}


class_t *
FindClass(char *name)
{
  return (NULL);
}


void
LoadAllClasses(void)
{
}


void
SaveAllClasses(void)
{
}


/*
 * End of "$Id: classes.c,v 1.3 1999/01/24 14:25:11 mike Exp $".
 */
