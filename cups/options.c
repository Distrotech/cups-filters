/*
 * "$Id: options.c,v 1.1 1999/02/05 17:40:54 mike Exp $"
 *
 *   Option routines for the Common UNIX Printing System (CUPS).
 *
 *   Copyright 1997-1999 by Easy Software Products.
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
 *   cupsAddOption()    - Add an option to an option array.
 *   cupsFreeOptions()  - Free all memory used by options.
 *   cupsGetOption()    - Get an option value.
 *   cupsParseOptions() - Parse options from a command-line argument.
 */

/*
 * Include necessary headers...
 */

#include "cups.h"
#include <stdlib.h>
#include <ctype.h>
#include "string.h"


/*
 * 'cupsAddOption()' - Add an option to an option array.
 */

int						/* O - Number of options */
cupsAddOption(char          *name,		/* I - Name of option */
              char          *value,		/* I - Value of option */
	      int           num_options,	/* I - Number of options */
              cups_option_t **options)		/* IO - Pointer to options */
{
  cups_option_t	*temp;				/* Pointer to new option */


  if (name == NULL || value == NULL || options == NULL)
    return (0);

  if (num_options == 0)
    temp = (cups_option_t *)malloc(sizeof(cups_option_t));
  else
    temp = (cups_option_t *)realloc(*options, sizeof(cups_option_t) *
                                              (num_options + 1));

  if (temp == NULL)
    return (0);

  *options = temp;
  temp     += num_options;

  temp->name  = strdup(name);
  temp->value = strdup(value);

  return (num_options + 1);
}


/*
 * 'cupsFreeOptions()' - Free all memory used by options.
 */

void
cupsFreeOptions(int           num_options,	/* I - Number of options */
                cups_option_t *options)		/* I - Pointer to options */
{
  int	i;					/* Looping var */


  if (num_options == 0 || options == NULL)
    return;

  for (i = 0; i < num_options; i ++)
  {
    free(options[i].name);
    free(options[i].value);
  }

  free(options);
}


/*
 * 'cupsGetOption()' - Get an option value.
 */

char *					/* O - Option value or NULL */
cupsGetOption(char          *name,	/* I - Name of option */
              int           num_options,/* I - Number of options */
              cups_option_t *options)	/* I - Options */
{
  int	i;				/* Looping var */


  if (name == NULL || num_options == 0 || options == NULL)
    return (NULL);

  for (i = 0; i < num_options; i ++)
    if (strcmp(options[i].name, name) == 0)
      return (options[i].value);

  return (NULL);
}


/*
 * 'cupsParseOptions()' - Parse options from a command-line argument.
 */

int						/* O - Number of options found */
cupsParseOptions(char          *arg,		/* I - Argument to parse */
                 cups_option_t **options)	/* O - Options found */
{
  int	num_options;				/* Number of options */
  char	*copyarg,				/* Copy of input string */
	*ptr,					/* Pointer into string */
	*name,					/* Pointer to name */
	*value;					/* Pointer to value */


  if (arg == NULL || options == NULL)
    return (0);

 /*
  * Make a copy of the argument string and then divide it up...
  */

  copyarg     = strdup(arg);
  num_options = 0;
  ptr         = copyarg;

  while (*ptr != '\0')
  {
   /*
    * Get the name up to a SPACE, =, or end-of-string...
    */

    name = ptr;
    while (!isspace(*ptr) && *ptr != '=' && *ptr != '\0')
      ptr ++;

   /*
    * Skip trailing spaces...
    */

    while (isspace(*ptr))
      *ptr++ = '\0';

    if (*ptr != '=')
    {
     /*
      * Start of another option...
      */

      num_options = cupsAddOption(name, "", num_options, options);
      continue;
    }

   /*
    * Remove = and parse the value...
    */

    *ptr++ = '\0';

    if (*ptr == '\'')
    {
     /*
      * Quoted string constant...
      */

      ptr ++;
      value = ptr;

      while (*ptr != '\'' && *ptr != '\0')
        ptr ++;

      if (*ptr != '\0')
        *ptr++ = '\0';
    }
    else if (*ptr == '\"')
    {
     /*
      * Double-quoted string constant...
      */

      ptr ++;
      value = ptr;

      while (*ptr != '\"' && *ptr != '\0')
        ptr ++;

      if (*ptr != '\0')
        *ptr++ = '\0';
    }
    else
    {
     /*
      * Normal space-delimited string...
      */

      value = ptr;

      while (!isspace(*ptr) && *ptr != '\0')
	ptr ++;

      while (isspace(*ptr))
        *ptr++ = '\0';
    }

   /*
    * Add the string value...
    */

    num_options = cupsAddOption(name, value, num_options, options);
  }

 /*
  * Free the copy of the argument we made and return the number of options
  * found.
  */

  free(copyarg);

  return (num_options);
}


/*
 * End of "$Id: options.c,v 1.1 1999/02/05 17:40:54 mike Exp $".
 */
