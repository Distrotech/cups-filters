/*
 * "$Id$"
 *
 *   PPD model-specific attribute routines for the Common UNIX Printing System
 *   (CUPS).
 *
 *   Copyright 1997-2006 by Easy Software Products.
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
 *       Hollywood, Maryland 20636 USA
 *
 *       Voice: (301) 373-9600
 *       EMail: cups-info@cups.org
 *         WWW: http://www.cups.org
 *
 * Contents:
 *
 *   ppdFindAttr()     - Find the first matching attribute...
 *   ppdFindNextAttr() - Find the next matching attribute...
 */

/*
 * Include necessary headers...
 */

#include "ppd.h"
#include "debug.h"
#include "string.h"
#include <stdlib.h>


/*
 * 'ppdFindAttr()' - Find the first matching attribute...
 *
 * @since CUPS 1.1.19@
 */

ppd_attr_t *			/* O - Attribute or NULL if not found */
ppdFindAttr(ppd_file_t *ppd,	/* I - PPD file data */
            const char *name,	/* I - Attribute name */
            const char *spec)	/* I - Specifier string or NULL */
{
  ppd_attr_t	key;		/* Search key */


 /*
  * Range check input...
  */

  if (!ppd || !name || ppd->num_attrs == 0)
    return (NULL);

 /*
  * Search for a matching attribute...
  */

  memset(&key, 0, sizeof(key));
  strlcpy(key.name, name, sizeof(key.name));
  if (spec)
    strlcpy(key.spec, spec, sizeof(key.spec));

 /*
  * Return the first matching attribute, if any...
  */

  return ((ppd_attr_t *)cupsArrayFind(ppd->sorted_attrs, &key));
}


/*
 * 'ppdFindNextAttr()' - Find the next matching attribute...
 *
 * @since CUPS 1.1.19@
 */

ppd_attr_t *				/* O - Attribute or NULL if not found */
ppdFindNextAttr(ppd_file_t *ppd,	/* I - PPD file data */
                const char *name,	/* I - Attribute name */
		const char *spec)	/* I - Specifier string or NULL */
{
  ppd_attr_t	*attr;			/* Current attribute */


 /*
  * Range check input...
  */

  if (!ppd || !name || ppd->num_attrs == 0 ||
      !cupsArrayCurrent(ppd->sorted_attrs))
    return (NULL);

 /*
  * See if there are more attributes to return...
  */

  if ((attr = (ppd_attr_t *)cupsArrayNext(ppd->sorted_attrs)) == NULL)
    return (NULL);

 /*
  * Check the next attribute to see if it is a match...
  */

  if (strcasecmp(attr->name, name) || (spec && strcasecmp(attr->spec, spec)))
  {
   /*
    * Nope, reset the current pointer to the end of the array...
    */

    cupsArrayIndex(ppd->sorted_attrs, cupsArrayCount(ppd->sorted_attrs));

    return (NULL);
  }
  
 /*
  * Return the next attribute's value...
  */

  return (attr);
}


/*
 * End of "$Id$".
 */
