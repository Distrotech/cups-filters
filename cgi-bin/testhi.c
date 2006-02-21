/*
 * "$Id$"
 *
 *   Help index test program for the Common UNIX Printing System (CUPS).
 *
 *   Copyright 1997-2005 by Easy Software Products.
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
 *   main()       - Test the help index code.
 *   list_nodes() - List nodes in an array...
 */

/*
 * Include necessary headers...
 */

#include "cgi.h"


/*
 * Local functions...
 */

static void	list_nodes(const char *title, cups_array_t *nodes);


/*
 * 'main()' - Test the help index code.
 */

int					/* O - Exit status */
main(int  argc,				/* I - Number of command-line arguments */
     char *argv[])			/* I - Command-line arguments */
{
  help_index_t	*hi,			/* Help index */
		*search;		/* Search index */


 /*
  * Load the help index...
  */

  hi = helpLoadIndex("testhi.index", ".");

  list_nodes("nodes", hi->nodes);
  list_nodes("sorted", hi->sorted);

 /*
  * Do any searches...
  */

  if (argc > 1)
  {
    search = helpSearchIndex(hi, argv[1], NULL, argv[2]);

    if (search)
    {
      list_nodes(argv[1], search->sorted);
      helpDeleteIndex(search);
    }
    else
      printf("%s (0 nodes)\n", argv[1]);
  }

  helpDeleteIndex(hi);

 /*
  * Return with no errors...
  */

  return (0);
}


/*
 * 'list_nodes()' - List nodes in an array...
 */

static void
list_nodes(const char   *title,		/* I - Title string */
	   cups_array_t *nodes)		/* I - Nodes */
{
  int		i;			/* Looping var */
  help_node_t	*node;			/* Current node */


  printf("%s (%d nodes):\n", title, cupsArrayCount(nodes));
  for (i = 1, node = (help_node_t *)cupsArrayFirst(nodes);
       node;
       i ++, node = (help_node_t *)cupsArrayNext(nodes))
    if (node->anchor)
      printf("    %d: %s#%s \"%s\"\n", i, node->filename, node->anchor,
             node->text);
    else
      printf("    %d: %s \"%s\"\n", i, node->filename, node->text);
}


/*
 * End of "$Id$".
 */
