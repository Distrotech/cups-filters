/*
 * "$Id: lpstat.c,v 1.25 2000/09/18 16:24:10 mike Exp $"
 *
 *   "lpstat" command for the Common UNIX Printing System (CUPS).
 *
 *   Copyright 1997-2000 by Easy Software Products.
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
 * Contents:
 *
 *   main()           - Parse options and show status information.
 *   show_accepting() - Show acceptance status.
 *   show_classes()   - Show printer classes.
 *   show_default()   - Show default destination.
 *   show_devices()   - Show printer devices.
 *   show_jobs()      - Show active print jobs.
 *   show_printers()  - Show printers.
 *   show_scheduler() - Show scheduler status.
 */

/*
 * Include necessary headers...
 */

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <cups/cups.h>
#include <cups/language.h>
#include <cups/debug.h>
#include <cups/string.h>


/*
 * Local functions...
 */

static void	show_accepting(http_t *, const char *, int, cups_dest_t *);
static void	show_classes(http_t *, const char *);
static void	show_default(int, cups_dest_t *);
static void	show_devices(http_t *, const char *, int, cups_dest_t *);
static void	show_jobs(http_t *, const char *, const char *, int, int);
static void	show_printers(http_t *, const char *, int, cups_dest_t *, int);
static void	show_scheduler(http_t *);


/*
 * 'main()' - Parse options and show status information.
 */

int
main(int  argc,			/* I - Number of command-line arguments */
     char *argv[])		/* I - Command-line arguments */
{
  int		i;		/* Looping var */
  http_t	*http;		/* Connection to server */
  char		server[1024];	/* CUPS_SERVER environment variable */
  int		num_dests;	/* Number of user destinations */
  cups_dest_t	*dests;		/* User destinations */
  int		long_status;	/* Long status report? */
  int		ranking;	/* Show job ranking? */


  http        = NULL;
  num_dests   = 0;
  dests       = NULL;
  long_status = 0;
  ranking     = 0;

  for (i = 1; i < argc; i ++)
    if (argv[i][0] == '-')
      switch (argv[i][1])
      {
        case 'D' : /* Show description */
	    long_status = 1;
	    break;

        case 'P' : /* Show paper types */
	    break;
	    
        case 'R' : /* Show ranking */
	    ranking = 1;
	    break;
	    
        case 'S' : /* Show charsets */
	    if (!argv[i][2])
	      i ++;

        case 'a' : /* Show acceptance status */
	    if (!http)
	    {
              http = httpConnect(cupsServer(), ippPort());

	      if (http == NULL)
	      {
		perror("lpstat: Unable to connect to server");
		return (1);
	      }
            }

            if (num_dests == 0)
	      num_dests = cupsGetDests(&dests);

	    if (argv[i][2] != '\0')
	      show_accepting(http, argv[i] + 2, num_dests, dests);
	    else if ((i + 1) < argc && argv[i + 1][0] != '-')
	    {
	      i ++;
	      show_accepting(http, argv[i], num_dests, dests);
	    }
	    else
	      show_accepting(http, NULL, num_dests, dests);
	    break;

        case 'c' : /* Show classes and members */
	    if (!http)
	    {
              http = httpConnect(cupsServer(), ippPort());

	      if (http == NULL)
	      {
		perror("lpstat: Unable to connect to server");
		return (1);
	      }
            }

	    if (argv[i][2] != '\0')
	      show_classes(http, argv[i] + 2);
	    else if ((i + 1) < argc && argv[i + 1][0] != '-')
	    {
	      i ++;
	      show_classes(http, argv[i]);
	    }
	    else
	      show_classes(http, NULL);
	    break;

        case 'd' : /* Show default destination */
            if (num_dests == 0)
	      num_dests = cupsGetDests(&dests);

            show_default(num_dests, dests);
	    break;

        case 'f' : /* Show forms */
	    if (!argv[i][2])
	      i ++;
	    break;
	    
        case 'h' : /* Connect to host */
	    if (http)
	      httpClose(http);

	    if (argv[i][2] != '\0')
	    {
	      http = httpConnect(argv[i] + 2, ippPort());
	      snprintf(server, sizeof(server), "CUPS_SERVER=%s", argv[i] + 2);
	    }
	    else
	    {
	      i ++;

	      if (i >= argc)
	      {
	        fputs("Error: need hostname after \'-h\' option!\n", stderr);
		return (1);
              }

	      http = httpConnect(argv[i], ippPort());
	      snprintf(server, sizeof(server), "CUPS_SERVER=%s", argv[i] + 2);
	    }

            putenv(server);
	    if (http == NULL)
	    {
	      perror("lpstat: Unable to connect to server");
	      return (1);
	    }
	    break;

        case 'l' : /* Long status */
	    long_status = 2;
	    break;

        case 'o' : /* Show jobs by destination */
	    if (!http)
	    {
              http = httpConnect(cupsServer(), ippPort());

	      if (http == NULL)
	      {
		perror("lpstat: Unable to connect to server");
		return (1);
	      }
            }

	    if (argv[i][2] != '\0')
	      show_jobs(http, argv[i] + 2, NULL, long_status, ranking);
	    else if ((i + 1) < argc && argv[i + 1][0] != '-')
	    {
	      i ++;
	      show_jobs(http, argv[i], NULL, long_status, ranking);
	    }
	    else
	      show_jobs(http, NULL, NULL, long_status, ranking);
	    break;

        case 'p' : /* Show printers */
	    if (!http)
	    {
              http = httpConnect(cupsServer(), ippPort());

	      if (http == NULL)
	      {
		perror("lpstat: Unable to connect to server");
		return (1);
	      }
            }

            if (num_dests == 0)
	      num_dests = cupsGetDests(&dests);

	    if (argv[i][2] != '\0')
	      show_printers(http, argv[i] + 2, num_dests, dests, long_status);
	    else if ((i + 1) < argc && argv[i + 1][0] != '-')
	    {
	      i ++;
	      show_printers(http, argv[i], num_dests, dests, long_status);
	    }
	    else
	      show_printers(http, NULL, num_dests, dests, long_status);
	    break;

        case 'r' : /* Show scheduler status */
	    if (!http)
	    {
              http = httpConnect(cupsServer(), ippPort());

	      if (http == NULL)
	      {
		perror("lpstat: Unable to connect to server");
		return (1);
	      }
            }

	    show_scheduler(http);
	    break;

        case 's' : /* Show summary */
	    if (!http)
	    {
              http = httpConnect(cupsServer(), ippPort());

	      if (http == NULL)
	      {
		perror("lpstat: Unable to connect to server");
		return (1);
	      }
            }

            if (num_dests == 0)
	      num_dests = cupsGetDests(&dests);

	    show_default(num_dests, dests);
	    show_classes(http, NULL);
	    show_devices(http, NULL, num_dests, dests);
	    break;

        case 't' : /* Show all info */
	    if (!http)
	    {
              http = httpConnect(cupsServer(), ippPort());

	      if (http == NULL)
	      {
		perror("lpstat: Unable to connect to server");
		return (1);
	      }
            }

            if (num_dests == 0)
	      num_dests = cupsGetDests(&dests);

	    show_scheduler(http);
	    show_default(num_dests, dests);
	    show_classes(http, NULL);
	    show_devices(http, NULL, num_dests, dests);
	    show_accepting(http, NULL, num_dests, dests);
	    show_printers(http, NULL, num_dests, dests, long_status);
	    show_jobs(http, NULL, NULL, long_status, ranking);
	    break;

        case 'u' : /* Show jobs by user */
	    if (!http)
	    {
              http = httpConnect(cupsServer(), ippPort());

	      if (http == NULL)
	      {
		perror("lpstat: Unable to connect to server");
		return (1);
	      }
            }

	    if (argv[i][2] != '\0')
	      show_jobs(http, NULL, argv[i] + 2, long_status, ranking);
	    else if ((i + 1) < argc && argv[i + 1][0] != '-')
	    {
	      i ++;
	      show_jobs(http, NULL, argv[i], long_status, ranking);
	    }
	    else
	      show_jobs(http, NULL, NULL, long_status, ranking);
	    break;

        case 'v' : /* Show printer devices */
	    if (!http)
	    {
              http = httpConnect(cupsServer(), ippPort());

	      if (http == NULL)
	      {
		perror("lpstat: Unable to connect to server");
		return (1);
	      }
            }

            if (num_dests == 0)
	      num_dests = cupsGetDests(&dests);

	    if (argv[i][2] != '\0')
	      show_devices(http, argv[i] + 2, num_dests, dests);
	    else if ((i + 1) < argc && argv[i + 1][0] != '-')
	    {
	      i ++;
	      show_devices(http, argv[i], num_dests, dests);
	    }
	    else
	      show_devices(http, NULL, num_dests, dests);
	    break;


	default :
	    fprintf(stderr, "lpstat: Unknown option \'%c\'!\n", argv[i][1]);
	    return (1);
      }
    else
    {
      fprintf(stderr, "lpstat: Unknown argument \'%s\'!\n", argv[i]);
      return (1);
    }

  if (argc == 1)
  {
    if (!http)
    {
      http = httpConnect(cupsServer(), ippPort());

      if (http == NULL)
      {
	perror("lpstat: Unable to connect to server");
	return (1);
      }
    }

    show_jobs(http, NULL, cupsUser(), long_status, ranking);
  }

  return (0);
}


/*
 * 'show_accepting()' - Show acceptance status.
 */

static void
show_accepting(http_t      *http,	/* I - HTTP connection to server */
               const char  *printers,	/* I - Destinations */
               int         num_dests,	/* I - Number of user-defined dests */
	       cups_dest_t *dests)	/* I - User-defined destinations */
{
  int		i;		/* Looping var */
  ipp_t		*request,	/* IPP Request */
		*response;	/* IPP Response */
  ipp_attribute_t *attr;	/* Current attribute */
  cups_lang_t	*language;	/* Default language */
  const char	*printer,	/* Printer name */
		*message;	/* Printer device URI */
  int		accepting;	/* Accepting requests? */
  const char	*dptr,		/* Pointer into destination list */
		*ptr;		/* Pointer into printer name */
  int		match;		/* Non-zero if this job matches */


  DEBUG_printf(("show_accepting(%08x, %08x)\n", http, printers));

  if (http == NULL)
    return;

  if (printers != NULL && strcmp(printers, "all") == 0)
    printers = NULL;

 /*
  * Build a CUPS_GET_PRINTERS request, which requires the following
  * attributes:
  *
  *    attributes-charset
  *    attributes-natural-language
  */

  request = ippNew();

  request->request.op.operation_id = CUPS_GET_PRINTERS;
  request->request.op.request_id   = 1;

  language = cupsLangDefault();

  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_CHARSET,
               "attributes-charset", NULL, cupsLangEncoding(language));

  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_LANGUAGE,
               "attributes-natural-language", NULL, language->language);

 /*
  * Do the request and get back a response...
  */

  if ((response = cupsDoRequest(http, request, "/")) != NULL)
  {
    DEBUG_puts("show_accepting: request succeeded...");

    if (response->request.status.status_code > IPP_OK_CONFLICT)
    {
      fprintf(stderr, "lpstat: get-printers failed: %s\n",
              ippErrorString(response->request.status.status_code));
      ippDelete(response);
      return;
    }

   /*
    * Loop through the printers returned in the list and display
    * their devices...
    */

    for (attr = response->attrs; attr != NULL; attr = attr->next)
    {
     /*
      * Skip leading attributes until we hit a printer...
      */

      while (attr != NULL && attr->group_tag != IPP_TAG_PRINTER)
        attr = attr->next;

      if (attr == NULL)
        break;

     /*
      * Pull the needed attributes from this printer...
      */

      printer   = NULL;
      message   = NULL;
      accepting = 1;

      while (attr != NULL && attr->group_tag == IPP_TAG_PRINTER)
      {
        if (strcmp(attr->name, "printer-name") == 0 &&
	    attr->value_tag == IPP_TAG_NAME)
	  printer = attr->values[0].string.text;

        if (strcmp(attr->name, "printer-state-message") == 0 &&
	    attr->value_tag == IPP_TAG_TEXT)
	  message = attr->values[0].string.text;

        if (strcmp(attr->name, "printer-is-accepting-jobs") == 0 &&
	    attr->value_tag == IPP_TAG_BOOLEAN)
	  accepting = attr->values[0].boolean;

        attr = attr->next;
      }

     /*
      * See if we have everything needed...
      */

      if (printer == NULL)
      {
        if (attr == NULL)
	  break;
	else
          continue;
      }

     /*
      * See if this is a printer we're interested in...
      */

      match = printers == NULL;

      if (printers != NULL)
      {
        for (dptr = printers; *dptr != '\0';)
	{
	 /*
	  * Skip leading whitespace and commas...
	  */

	  while (isspace(*dptr) || *dptr == ',')
	    dptr ++;

	  if (*dptr == '\0')
	    break;

         /*
	  * Compare names...
	  */

	  for (ptr = printer;
	       *ptr != '\0' && *dptr != '\0' && *ptr == *dptr;
	       ptr ++, dptr ++);

          if (*ptr == '\0' && (*dptr == '\0' || *dptr == ',' || isspace(*dptr)))
	  {
	    match = 1;
	    break;
	  }

         /*
	  * Skip trailing junk...
	  */

          while (!isspace(*dptr) && *dptr != '\0')
	    dptr ++;
	  while (isspace(*dptr) || *dptr == ',')
	    dptr ++;

	  if (*dptr == '\0')
	    break;
        }
      }

     /*
      * Display the printer entry if needed...
      */

      if (match)
      {
        if (accepting)
	  printf("%s accepting requests\n", printer);
	else
	  printf("%s not accepting requests -\n\t%s\n", printer,
	         message == NULL ? "reason unknown" : message);

        for (i = 0; i < num_dests; i ++)
	  if (strcasecmp(dests[i].name, printer) == 0 && dests[i].instance)
	  {
            if (accepting)
	      printf("%s/%s accepting requests\n", printer, dests[i].instance);
	    else
	      printf("%s/%s not accepting requests -\n\t%s\n", printer,
	             dests[i].instance,
	             message == NULL ? "reason unknown" : message);
	  }
      }

      if (attr == NULL)
        break;
    }

    ippDelete(response);
  }
  else
    fprintf(stderr, "lpstat: get-printers failed: %s\n",
            ippErrorString(cupsLastError()));
}


/*
 * 'show_classes()' - Show printer classes.
 */

static void
show_classes(http_t     *http,	/* I - HTTP connection to server */
             const char *dests)	/* I - Destinations */
{
  int		i;		/* Looping var */
  ipp_t		*request,	/* IPP Request */
		*response;	/* IPP Response */
  ipp_attribute_t *attr;	/* Current attribute */
  cups_lang_t	*language;	/* Default language */
  const char	*printer;	/* Printer class name */
  ipp_attribute_t *members;	/* Printer members */
  const char	*dptr,		/* Pointer into destination list */
		*ptr;		/* Pointer into printer name */
  int		match;		/* Non-zero if this job matches */


  DEBUG_printf(("show_classes(%08x, %08x)\n", http, dests));

  if (http == NULL)
    return;

  if (dests != NULL && strcmp(dests, "all") == 0)
    dests = NULL;

 /*
  * Build a CUPS_GET_CLASSES request, which requires the following
  * attributes:
  *
  *    attributes-charset
  *    attributes-natural-language
  */

  request = ippNew();

  request->request.op.operation_id = CUPS_GET_CLASSES;
  request->request.op.request_id   = 1;

  language = cupsLangDefault();

  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_CHARSET,
               "attributes-charset", NULL, cupsLangEncoding(language));

  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_LANGUAGE,
               "attributes-natural-language", NULL, language->language);

 /*
  * Do the request and get back a response...
  */

  if ((response = cupsDoRequest(http, request, "/")) != NULL)
  {
    DEBUG_puts("show_classes: request succeeded...");

    if (response->request.status.status_code > IPP_OK_CONFLICT)
    {
      fprintf(stderr, "lpstat: get-classes failed: %s\n",
              ippErrorString(response->request.status.status_code));
      ippDelete(response);
      return;
    }

   /*
    * Loop through the printers returned in the list and display
    * their devices...
    */

    for (attr = response->attrs; attr != NULL; attr = attr->next)
    {
     /*
      * Skip leading attributes until we hit a job...
      */

      while (attr != NULL && attr->group_tag != IPP_TAG_PRINTER)
        attr = attr->next;

      if (attr == NULL)
        break;

     /*
      * Pull the needed attributes from this job...
      */

      printer = NULL;
      members = NULL;

      while (attr != NULL && attr->group_tag == IPP_TAG_PRINTER)
      {
        if (strcmp(attr->name, "printer-name") == 0 &&
	    attr->value_tag == IPP_TAG_NAME)
	  printer = attr->values[0].string.text;

        if (strcmp(attr->name, "member-names") == 0 &&
	    attr->value_tag == IPP_TAG_NAME)
	  members = attr;

        attr = attr->next;
      }

     /*
      * See if we have everything needed...
      */

      if (printer == NULL || members == NULL)
      {
        if (attr == NULL)
	  break;
	else
          continue;
      }

     /*
      * See if this is a printer we're interested in...
      */

      match = dests == NULL;

      if (dests != NULL)
      {
        for (dptr = dests; *dptr != '\0';)
	{
	 /*
	  * Skip leading whitespace and commas...
	  */

	  while (isspace(*dptr) || *dptr == ',')
	    dptr ++;

	  if (*dptr == '\0')
	    break;

         /*
	  * Compare names...
	  */

	  for (ptr = printer;
	       *ptr != '\0' && *dptr != '\0' && *ptr == *dptr;
	       ptr ++, dptr ++);

          if (*ptr == '\0' && (*dptr == '\0' || *dptr == ',' || isspace(*dptr)))
	  {
	    match = 1;
	    break;
	  }

         /*
	  * Skip trailing junk...
	  */

          while (!isspace(*dptr) && *dptr != '\0')
	    dptr ++;
	  while (isspace(*dptr) || *dptr == ',')
	    dptr ++;

	  if (*dptr == '\0')
	    break;
        }
      }

     /*
      * Display the printer entry if needed...
      */

      if (match)
      {
        printf("members of class %s:\n", printer);
	for (i = 0; i < members->num_values; i ++)
	  printf("\t%s\n", members->values[i].string.text);
      }

      if (attr == NULL)
        break;
    }

    ippDelete(response);
  }
  else
    fprintf(stderr, "lpstat: get-classes failed: %s\n",
            ippErrorString(cupsLastError()));
}


/*
 * 'show_default()' - Show default destination.
 */

static void
show_default(int         num_dests,	/* I - Number of user-defined dests */
	     cups_dest_t *dests)	/* I - User-defined destinations */
{
  int	i;				/* Looping var */


  for (i = 0; i < num_dests; i ++)
    if (dests[i].is_default)
      break;

  if (i < num_dests)
  {
    if (dests[i].instance)
      printf("system default destination: %s/%s\n", dests[i].name,
             dests[i].instance);
    else
      printf("system default destination: %s\n", dests[i].name);
  }
  else
    puts("no system default destination");
}


/*
 * 'show_devices()' - Show printer devices.
 */

static void
show_devices(http_t      *http,		/* I - HTTP connection to server */
             const char  *printers,	/* I - Destinations */
             int         num_dests,	/* I - Number of user-defined dests */
	     cups_dest_t *dests)	/* I - User-defined destinations */
{
  int		i;		/* Looping var */
  ipp_t		*request,	/* IPP Request */
		*response;	/* IPP Response */
  ipp_attribute_t *attr;	/* Current attribute */
  cups_lang_t	*language;	/* Default language */
  const char	*printer,	/* Printer name */
		*uri,		/* Printer URI */
		*device,	/* Printer device URI */
		*dptr,		/* Pointer into destination list */
		*ptr;		/* Pointer into printer name */
  int		match;		/* Non-zero if this job matches */


  DEBUG_printf(("show_devices(%08x, %08x)\n", http, dests));

  if (http == NULL)
    return;

  if (printers != NULL && strcmp(printers, "all") == 0)
    printers = NULL;

 /*
  * Build a CUPS_GET_PRINTERS request, which requires the following
  * attributes:
  *
  *    attributes-charset
  *    attributes-natural-language
  */

  request = ippNew();

  request->request.op.operation_id = CUPS_GET_PRINTERS;
  request->request.op.request_id   = 1;

  language = cupsLangDefault();

  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_CHARSET,
               "attributes-charset", NULL, cupsLangEncoding(language));

  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_LANGUAGE,
               "attributes-natural-language", NULL, language->language);

 /*
  * Do the request and get back a response...
  */

  if ((response = cupsDoRequest(http, request, "/")) != NULL)
  {
    DEBUG_puts("show_devices: request succeeded...");

    if (response->request.status.status_code > IPP_OK_CONFLICT)
    {
      fprintf(stderr, "lpstat: get-printers failed: %s\n",
              ippErrorString(response->request.status.status_code));
      ippDelete(response);
      return;
    }

   /*
    * Loop through the printers returned in the list and display
    * their devices...
    */

    for (attr = response->attrs; attr != NULL; attr = attr->next)
    {
     /*
      * Skip leading attributes until we hit a job...
      */

      while (attr != NULL && attr->group_tag != IPP_TAG_PRINTER)
        attr = attr->next;

      if (attr == NULL)
        break;

     /*
      * Pull the needed attributes from this job...
      */

      printer = NULL;
      device  = NULL;
      uri     = NULL;

      while (attr != NULL && attr->group_tag == IPP_TAG_PRINTER)
      {
        if (strcmp(attr->name, "printer-name") == 0 &&
	    attr->value_tag == IPP_TAG_NAME)
	  printer = attr->values[0].string.text;

        if (strcmp(attr->name, "printer-uri-supported") == 0 &&
	    attr->value_tag == IPP_TAG_URI)
	  printer = attr->values[0].string.text;

        if (strcmp(attr->name, "device-uri") == 0 &&
	    attr->value_tag == IPP_TAG_URI)
	  device = attr->values[0].string.text;

        attr = attr->next;
      }

     /*
      * See if we have everything needed...
      */

      if (printer == NULL)
      {
        if (attr == NULL)
	  break;
	else
          continue;
      }

     /*
      * See if this is a printer we're interested in...
      */

      match = printers == NULL;

      if (printers != NULL)
      {
        for (dptr = printers; *dptr != '\0';)
	{
	 /*
	  * Skip leading whitespace and commas...
	  */

	  while (isspace(*dptr) || *dptr == ',')
	    dptr ++;

	  if (*dptr == '\0')
	    break;

         /*
	  * Compare names...
	  */

	  for (ptr = printer;
	       *ptr != '\0' && *dptr != '\0' && *ptr == *dptr;
	       ptr ++, dptr ++);

          if (*ptr == '\0' && (*dptr == '\0' || *dptr == ',' || isspace(*dptr)))
	  {
	    match = 1;
	    break;
	  }

         /*
	  * Skip trailing junk...
	  */

          while (!isspace(*dptr) && *dptr != '\0')
	    dptr ++;
	  while (isspace(*dptr) || *dptr == ',')
	    dptr ++;

	  if (*dptr == '\0')
	    break;
        }
      }

     /*
      * Display the printer entry if needed...
      */

      if (match)
      {
#ifdef __osf__ /* Compaq/Digital like to do it their own way... */
        char	method[HTTP_MAX_URI],	/* Components of printer URI */
		username[HTTP_MAX_URI],
		hostname[HTTP_MAX_URI],
		resource[HTTP_MAX_URI];
	int	port;


        if (device == NULL)
	{
	  httpSeparate(uri, method, username, hostname, &port, resource);
          printf("Output for printer %s is sent to remote printer %s on %s\n",
	         printer, strrchr(resource, '/') + 1, hostname);
        }
        else if (strncmp(device, "file:", 5) == 0)
          printf("Output for printer %s is sent to %s\n", printer, device + 5);
        else
          printf("Output for printer %s is sent to %s\n", printer, device);

        for (i = 0; i < num_dests; i ++)
	  if (strcasecmp(printer, dests[i].name) == 0 && dests[i].instance)
	  {
            if (device == NULL)
              printf("Output for printer %s/%s is sent to remote printer %s on %s\n",
	             printer, dests[i].instance, strrchr(resource, '/') + 1,
		     hostname);
            else if (strncmp(device, "file:", 5) == 0)
              printf("Output for printer %s/%s is sent to %s\n", printer, dests[i].instance, device + 5);
            else
              printf("Output for printer %s/%s is sent to %s\n", printer, dests[i].instance, device);
	  }
#else
        if (device == NULL)
          printf("device for %s: /dev/null\n", printer);
        else if (strncmp(device, "file:", 5) == 0)
          printf("device for %s: %s\n", printer, device + 5);
        else
          printf("device for %s: %s\n", printer, device);

        for (i = 0; i < num_dests; i ++)
	  if (strcasecmp(printer, dests[i].name) == 0 && dests[i].instance)
	  {
            if (device == NULL)
              printf("device for %s/%s: /dev/null\n", printer, dests[i].instance);
            else if (strncmp(device, "file:", 5) == 0)
              printf("device for %s/%s: %s\n", printer, dests[i].instance, device + 5);
            else
              printf("device for %s/%s: %s\n", printer, dests[i].instance, device);
	  }
#endif /* __osf__ */
      }

      if (attr == NULL)
        break;
    }

    ippDelete(response);
  }
  else
    fprintf(stderr, "lpstat: get-printers failed: %s\n",
            ippErrorString(cupsLastError()));
}


/*
 * 'show_jobs()' - Show active print jobs.
 */

static void
show_jobs(http_t     *http,	/* I - HTTP connection to server */
          const char *dests,	/* I - Destinations */
          const char *users,	/* I - Users */
          int        long_status,/* I - Show long status? */
          int        ranking)	/* I - Show job ranking? */
{
  ipp_t		*request,	/* IPP Request */
		*response;	/* IPP Response */
  ipp_attribute_t *attr;	/* Current attribute */
  cups_lang_t	*language;	/* Default language */
  const char	*dest,		/* Pointer into job-printer-uri */
		*username;	/* Pointer to job-originating-user-name */
  int		rank,		/* Rank in queue */
		jobid,		/* job-id */
		size;		/* job-k-octets */
  time_t	jobtime;	/* time-at-creation */
  struct tm	*jobdate;	/* Date & time */
  const char	*dptr,		/* Pointer into destination list */
		*ptr;		/* Pointer into printer name */
  int		match;		/* Non-zero if this job matches */
  char		temp[22],	/* Temporary buffer */
		date[32];	/* Date buffer */


  DEBUG_printf(("show_jobs(%08x, %08x, %08x)\n", http, dests, users));

  if (http == NULL)
    return;

  if (dests != NULL && strcmp(dests, "all") == 0)
    dests = NULL;

 /*
  * Build a IPP_GET_JOBS request, which requires the following
  * attributes:
  *
  *    attributes-charset
  *    attributes-natural-language
  *    job-uri
  */

  request = ippNew();

  request->request.op.operation_id = IPP_GET_JOBS;
  request->request.op.request_id   = 1;

  language = cupsLangDefault();

  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_CHARSET,
               "attributes-charset", NULL, cupsLangEncoding(language));

  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_LANGUAGE,
               "attributes-natural-language", NULL, language->language);

  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_URI, "job-uri",
               NULL, "ipp://localhost/jobs/");

 /*
  * Do the request and get back a response...
  */

  if ((response = cupsDoRequest(http, request, "/")) != NULL)
  {
   /*
    * Loop through the job list and display them...
    */

    if (response->request.status.status_code > IPP_OK_CONFLICT)
    {
      fprintf(stderr, "lpstat: get-jobs failed: %s\n",
              ippErrorString(response->request.status.status_code));
      ippDelete(response);
      return;
    }

    rank = -1;

    for (attr = response->attrs; attr != NULL; attr = attr->next)
    {
     /*
      * Skip leading attributes until we hit a job...
      */

      while (attr != NULL && attr->group_tag != IPP_TAG_JOB)
        attr = attr->next;

      if (attr == NULL)
        break;

     /*
      * Pull the needed attributes from this job...
      */

      jobid    = 0;
      size     = 0;
      username = NULL;
      dest     = NULL;
      jobtime  = 0;

      while (attr != NULL && attr->group_tag == IPP_TAG_JOB)
      {
        if (strcmp(attr->name, "job-id") == 0 &&
	    attr->value_tag == IPP_TAG_INTEGER)
	  jobid = attr->values[0].integer;

        if (strcmp(attr->name, "job-k-octets") == 0 &&
	    attr->value_tag == IPP_TAG_INTEGER)
	  size = attr->values[0].integer * 1024;

        if (strcmp(attr->name, "time-at-creation") == 0 &&
	    attr->value_tag == IPP_TAG_INTEGER)
	  jobtime = attr->values[0].integer;

        if (strcmp(attr->name, "job-printer-uri") == 0 &&
	    attr->value_tag == IPP_TAG_URI)
	  if ((dest = strrchr(attr->values[0].string.text, '/')) != NULL)
	    dest ++;

        if (strcmp(attr->name, "job-originating-user-name") == 0 &&
	    attr->value_tag == IPP_TAG_NAME)
	  username = attr->values[0].string.text;

        attr = attr->next;
      }

     /*
      * See if we have everything needed...
      */

      if (dest == NULL || jobid == 0)
      {
        if (attr == NULL)
	  break;
	else
          continue;
      }

     /*
      * See if this is a job we're interested in...
      */

      match = (dests == NULL && users == NULL);
      rank ++;

      if (dests != NULL)
      {
        for (dptr = dests; *dptr != '\0';)
	{
	 /*
	  * Skip leading whitespace and commas...
	  */

	  while (isspace(*dptr) || *dptr == ',')
	    dptr ++;

	  if (*dptr == '\0')
	    break;

         /*
	  * Compare names...
	  */

	  for (ptr = dest;
	       *ptr != '\0' && *dptr != '\0' && *ptr == *dptr;
	       ptr ++, dptr ++);

          if (*ptr == '\0' && (*dptr == '\0' || *dptr == ',' || isspace(*dptr)))
	  {
	    match = 1;
	    break;
	  }

         /*
	  * Skip trailing junk...
	  */

          while (!isspace(*dptr) && *dptr != '\0')
	    dptr ++;
	  while (isspace(*dptr) || *dptr == ',')
	    dptr ++;

	  if (*dptr == '\0')
	    break;
        }
      }

      if (users != NULL && username != NULL)
      {
        for (dptr = users; *dptr != '\0';)
	{
	 /*
	  * Skip leading whitespace and commas...
	  */

	  while (isspace(*dptr) || *dptr == ',')
	    dptr ++;

	  if (*dptr == '\0')
	    break;

         /*
	  * Compare names...
	  */

	  for (ptr = username;
	       *ptr != '\0' && *dptr != '\0' && *ptr == *dptr;
	       ptr ++, dptr ++);

          if (*ptr == '\0' && (*dptr == '\0' || *dptr == ',' || isspace(*dptr)))
	  {
	    match = 1;
	    break;
	  }

         /*
	  * Skip trailing junk...
	  */

          while (!isspace(*dptr) && *dptr != '\0')
	    dptr ++;
	  while (isspace(*dptr) || *dptr == ',')
	    dptr ++;

	  if (*dptr == '\0')
	    break;
        }
      }

     /*
      * Display the job...
      */

      if (match)
      {
        jobdate = localtime(&jobtime);
	strftime(date, sizeof(date), "%c", jobdate);
        snprintf(temp, sizeof(temp), "%s-%d", dest, jobid);
        
        if (ranking)
	  printf("%3d %-21s %-13s %8d %s\n", rank, temp,
	         username ? username : "unknown", size, date);
        else
	  printf("%-23s %-13s %8d   %s\n", temp,
	         username ? username : "unknown", size, date);
        if (long_status)
	  printf("\tqueued for %s\n", dest);
      }

      if (attr == NULL)
        break;
    }

    ippDelete(response);
  }
  else
    fprintf(stderr, "lpstat: get-jobs failed: %s\n",
            ippErrorString(cupsLastError()));
}


/*
 * 'show_printers()' - Show printers.
 */

static void
show_printers(http_t      *http,	/* I - HTTP connection to server */
              const char  *printers,	/* I - Destinations */
              int         num_dests,	/* I - Number of user-defined dests */
	      cups_dest_t *dests,	/* I - User-defined destinations */
              int         long_status)	/* I - Show long status? */
{
  int		i;		/* Looping var */
  ipp_t		*request,	/* IPP Request */
		*response,	/* IPP Response */
		*jobs;		/* IPP Get Jobs response */
  ipp_attribute_t *attr;	/* Current attribute */
  ipp_attribute_t *jobattr;	/* Job ID attribute */
  cups_lang_t	*language;	/* Default language */
  const char	*printer,	/* Printer name */
		*message,	/* Printer state message */
		*description;	/* Description of printer */
  ipp_pstate_t	pstate;		/* Printer state */
  cups_ptype_t	ptype;		/* Printer type */
  int		jobid;		/* Job ID of current job */
  const char	*dptr,		/* Pointer into destination list */
		*ptr;		/* Pointer into printer name */
  int		match;		/* Non-zero if this job matches */
  char		printer_uri[HTTP_MAX_URI];
				/* Printer URI */
  const char	*root;		/* Server root directory... */


  DEBUG_printf(("show_printers(%08x, %08x)\n", http, dests));

  if (http == NULL)
    return;

  if ((root = getenv("CUPS_SERVERROOT")) == NULL)
    root = CUPS_SERVERROOT;

  if (printers != NULL && strcmp(printers, "all") == 0)
    printers = NULL;

 /*
  * Build a CUPS_GET_PRINTERS request, which requires the following
  * attributes:
  *
  *    attributes-charset
  *    attributes-natural-language
  */

  request = ippNew();

  request->request.op.operation_id = CUPS_GET_PRINTERS;
  request->request.op.request_id   = 1;

  language = cupsLangDefault();

  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_CHARSET,
               "attributes-charset", NULL, cupsLangEncoding(language));

  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_LANGUAGE,
               "attributes-natural-language", NULL, language->language);

 /*
  * Do the request and get back a response...
  */

  if ((response = cupsDoRequest(http, request, "/")) != NULL)
  {
    DEBUG_puts("show_printers: request succeeded...");

    if (response->request.status.status_code > IPP_OK_CONFLICT)
    {
      fprintf(stderr, "lpstat: get-printers failed: %s\n",
              ippErrorString(response->request.status.status_code));
      ippDelete(response);
      return;
    }

   /*
    * Loop through the printers returned in the list and display
    * their status...
    */

    for (attr = response->attrs; attr != NULL; attr = attr->next)
    {
     /*
      * Skip leading attributes until we hit a job...
      */

      while (attr != NULL && attr->group_tag != IPP_TAG_PRINTER)
        attr = attr->next;

      if (attr == NULL)
        break;

     /*
      * Pull the needed attributes from this job...
      */

      printer     = NULL;
      ptype       = CUPS_PRINTER_LOCAL;
      pstate      = IPP_PRINTER_IDLE;
      message     = NULL;
      description = NULL;
      jobid       = 0;

      while (attr != NULL && attr->group_tag == IPP_TAG_PRINTER)
      {
        if (strcmp(attr->name, "printer-name") == 0 &&
	    attr->value_tag == IPP_TAG_NAME)
	  printer = attr->values[0].string.text;

        if (strcmp(attr->name, "printer-state") == 0 &&
	    attr->value_tag == IPP_TAG_ENUM)
	  pstate = (ipp_pstate_t)attr->values[0].integer;

        if (strcmp(attr->name, "printer-type") == 0 &&
	    attr->value_tag == IPP_TAG_ENUM)
	  ptype = (cups_ptype_t)attr->values[0].integer;

        if (strcmp(attr->name, "printer-state") == 0 &&
	    attr->value_tag == IPP_TAG_ENUM)
	  pstate = (ipp_pstate_t)attr->values[0].integer;

        if (strcmp(attr->name, "printer-state-message") == 0 &&
	    attr->value_tag == IPP_TAG_TEXT)
	  message = attr->values[0].string.text;

        if (strcmp(attr->name, "printer-info") == 0 &&
	    attr->value_tag == IPP_TAG_TEXT)
	  description = attr->values[0].string.text;

        attr = attr->next;
      }

     /*
      * See if we have everything needed...
      */

      if (printer == NULL)
      {
        if (attr == NULL)
	  break;
	else
          continue;
      }

     /*
      * See if this is a printer we're interested in...
      */

      match = printers == NULL;

      if (printers != NULL)
      {
        for (dptr = printers; *dptr != '\0';)
	{
	 /*
	  * Skip leading whitespace and commas...
	  */

	  while (isspace(*dptr) || *dptr == ',')
	    dptr ++;

	  if (*dptr == '\0')
	    break;

         /*
	  * Compare names...
	  */

	  for (ptr = printer;
	       *ptr != '\0' && *dptr != '\0' && *ptr == *dptr;
	       ptr ++, dptr ++);

          if (*ptr == '\0' && (*dptr == '\0' || *dptr == ',' || isspace(*dptr)))
	  {
	    match = 1;
	    break;
	  }

         /*
	  * Skip trailing junk...
	  */

          while (!isspace(*dptr) && *dptr != '\0')
	    dptr ++;
	  while (isspace(*dptr) || *dptr == ',')
	    dptr ++;

	  if (*dptr == '\0')
	    break;
        }
      }

     /*
      * Display the printer entry if needed...
      */

      if (match)
      {
       /*
        * If the printer state is "IPP_PRINTER_PROCESSING", then grab the
	* current job for the printer.
	*/

        if (pstate == IPP_PRINTER_PROCESSING)
	{
	 /*
	  * Build an IPP_GET_JOBS request, which requires the following
	  * attributes:
	  *
	  *    attributes-charset
	  *    attributes-natural-language
	  *    printer-uri
	  *    limit
	  */

	  request = ippNew();

	  request->request.op.operation_id = IPP_GET_JOBS;
	  request->request.op.request_id   = 1;

	  language = cupsLangDefault();

	  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_CHARSET,
            	       "attributes-charset", NULL,
		       cupsLangEncoding(language));

	  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_LANGUAGE,
                       "attributes-natural-language", NULL,
		       language->language);

          sprintf(printer_uri, "ipp://%s/printers/%s", http->hostname, printer);
	  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_URI,
	               "printer-uri", NULL, printer_uri);

	  ippAddInteger(request, IPP_TAG_OPERATION, IPP_TAG_INTEGER,
	        	"limit", 1);

          if ((jobs = cupsDoRequest(http, request, "/jobs/")) != NULL)
	  {
	    if ((jobattr = ippFindAttribute(jobs, "job-id", IPP_TAG_INTEGER)) != NULL)
              jobid = jobattr->values[0].integer;

            ippDelete(jobs);
	  }
        }

       /*
        * Display it...
	*/

        switch (pstate)
	{
	  case IPP_PRINTER_IDLE :
	      printf("printer %s is idle.\n", printer);
	      break;
	  case IPP_PRINTER_PROCESSING :
	      printf("printer %s now printing %s-%d.\n", printer, printer, jobid);
	      break;
	  case IPP_PRINTER_STOPPED :
	      printf("printer %s disabled -\n\t%s\n", printer,
	             message == NULL ? "reason unknown" : message);
	      break;
	}

        if (long_status > 1)
	{
	  puts("\tForm mounted:");
	  puts("\tContent types: any");
	  puts("\tPrinter types: unknown");
	}
        if (long_status)
	  printf("\tDescription: %s\n", description ? description : "");
        if (long_status > 1)
	{
	  printf("\tConnection: %s\n",
	         (ptype & CUPS_PRINTER_REMOTE) ? "remote" : "direct");
	  if (!(ptype & CUPS_PRINTER_REMOTE))
	    printf("\tInterface: %s/ppd/%s.ppd\n", root, printer);
	  puts("\tOn fault: no alert");
	  puts("\tAfter fault: continue");
	  puts("\tUsers allowed:");
	  puts("\t\t(all)");
	  puts("\tForms allowed:");
	  puts("\t\t(none)");
	  puts("\tBanner required");
	  puts("\tCharset sets:");
	  puts("\t\t(none)");
	  puts("\tDefault pitch:");
	  puts("\tDefault page size:");
	  puts("\tDefault port settings:");
	}

        for (i = 0; i < num_dests; i ++)
	  if (strcasecmp(printer, dests[i].name) == 0 && dests[i].instance)
	  {
            switch (pstate)
	    {
	      case IPP_PRINTER_IDLE :
		  printf("printer %s/%s is idle.\n", printer, dests[i].instance);
		  break;
	      case IPP_PRINTER_PROCESSING :
		  printf("printer %s/%s now printing %s-%d.\n", printer,
		         dests[i].instance, printer, jobid);
		  break;
	      case IPP_PRINTER_STOPPED :
		  printf("printer %s/%s disabled -\n\t%s\n", printer,
		         dests[i].instance,
			 message == NULL ? "reason unknown" : message);
		  break;
	    }

            if (long_status > 1)
	    {
	      puts("\tForm mounted:");
	      puts("\tContent types: any");
	      puts("\tPrinter types: unknown");
	    }
            if (long_status)
	      printf("\tDescription: %s\n", description ? description : "");
            if (long_status > 1)
	    {
	      printf("\tConnection: %s\n",
	             (ptype & CUPS_PRINTER_REMOTE) ? "remote" : "direct");
	      if (!(ptype & CUPS_PRINTER_REMOTE))
		printf("\tInterface: %s/ppd/%s.ppd\n", root, printer);
	      puts("\tOn fault: no alert");
	      puts("\tAfter fault: continue");
	      puts("\tUsers allowed:");
	      puts("\t\t(all)");
	      puts("\tForms allowed:");
	      puts("\t\t(none)");
	      puts("\tBanner required");
	      puts("\tCharset sets:");
	      puts("\t\t(none)");
	      puts("\tDefault pitch:");
	      puts("\tDefault page size:");
	      puts("\tDefault port settings:");
	    }
	  }
      }

      if (attr == NULL)
        break;
    }

    ippDelete(response);
  }
  else
    fprintf(stderr, "lpstat: get-printers failed: %s\n",
            ippErrorString(cupsLastError()));
}


/*
 * 'show_scheduler()' - Show scheduler status.
 */

static void
show_scheduler(http_t *http)	/* I - HTTP connection to server */
{
  printf("scheduler is %srunning\n", http == NULL ? "not " : "");
}


/*
 * End of "$Id: lpstat.c,v 1.25 2000/09/18 16:24:10 mike Exp $".
 */
