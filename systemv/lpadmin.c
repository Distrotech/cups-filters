/*
 * "$Id: lpadmin.c,v 1.6 1999/06/23 14:12:24 mike Exp $"
 *
 *   "lpadmin" command for the Common UNIX Printing System (CUPS).
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
 *       44141 Airport View Drive, Suite 204
 *       Hollywood, Maryland 20636-3111 USA
 *
 *       Voice: (301) 373-9603
 *       EMail: cups-info@cups.org
 *         WWW: http://www.cups.org
 *
 * Contents:
 *
 *   main()                      - Parse options and configure the scheduler.
 *   add_printer_to_class()      - Add a printer to a class.
 *   default_printer()           - Set the default printing destination.
 *   delete_printer()            - Delete a printer from the system...
 *   delete_printer_from_class() - Delete a printer from a class.
 *   enable_printer()            - Enable a printer...
 *   set_printer_device()        - Set the device-uri attribute.
 *   set_printer_file()          - Set the interface script or PPD file.
 *   set_printer_info()          - Set the printer description string.
 *   set_printer_location()      - Set the printer location string.
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
#include <config.h>
#ifdef HAVE_LIBZ
#  include <zlib.h>
#endif /* HAVE_LIBZ */


/*
 * Local functions...
 */

static void	add_printer_to_class(http_t *, char *, char *);
static void	default_printer(http_t *, char *);
static void	delete_printer(http_t *, char *);
static void	delete_printer_from_class(http_t *, char *, char *);
static void	enable_printer(http_t *, char *);
static void	set_printer_device(http_t *, char *, char *);
static void	set_printer_file(http_t *, char *, char *);
static void	set_printer_info(http_t *, char *, char *);
static void	set_printer_location(http_t *, char *, char *);


/*
 * 'main()' - Parse options and configure the scheduler.
 */

int
main(int  argc,		/* I - Number of command-line arguments */
     char *argv[])	/* I - Command-line arguments */
{
  int	i;		/* Looping var */
  http_t *http;		/* Connection to server */
  char	*printer,	/* Destination printer */
	*host,		/* Pointer to hostname */
	filename[1024];	/* Model filename */


  if ((http = httpConnect(cupsServer(), ippPort())) == NULL)
  {
    fputs("lpadmin: Unable to contact server!\n", stderr);
    return (1);
  }

  printer = NULL;

  for (i = 1; i < argc; i ++)
    if (argv[i][0] == '-')
      switch (argv[i][1])
      {
        case 'c' : /* Add printer to class */
	    if (printer == NULL)
	    {
	      fputs("lpadmin: Unable to add a printer to the class:\n", stderr);
	      fputs("         You must specify a printer name first!\n", stderr);
	      return (1);
	    }

	    if (argv[i][2])
	      add_printer_to_class(http, printer, argv[i] + 2);
	    else
	    {
	      i ++;
	      add_printer_to_class(http, printer, argv[i]);
	    }
	    break;

        case 'd' : /* Set as default destination */
	    if (argv[i][2])
	      printer = argv[i] + 2;
	    else
	    {
	      i ++;
	      printer = argv[i];
	    }

            default_printer(http, printer);
	    i = argc;
	    break;

        case 'h' : /* Connect to host */
	    httpClose(http);

	    if (argv[i][2] != '\0')
	      http = httpConnect(argv[i] + 2, ippPort());
	    else
	    {
	      i ++;
	      http = httpConnect(argv[i], ippPort());
	    }

	    if (http == NULL)
	    {
	      perror("lpadmin: Unable to connect to server");
	      return (1);
	    }
	    break;

        case 'i' : /* Use the specified interface script */
	    if (printer == NULL)
	    {
	      fputs("lpadmin: Unable to set the interface script:\n", stderr);
	      fputs("         You must specify a printer name first!\n", stderr);
	      return (1);
	    }

	    if (argv[i][2])
	      set_printer_file(http, printer, argv[i] + 2);
	    else
	    {
	      i ++;
	      set_printer_file(http, printer, argv[i]);
	    }
	    break;

        case 'E' : /* Enable the printer */
	    if (printer == NULL)
	    {
              if (argv[i][2])
                printer = argv[i] + 2;
              else
              {
                i ++;
                printer = argv[i];
              }
	    }

            enable_printer(http, printer);
            break;

        case 'm' : /* Use the specified standard script/PPD file */
	    if (printer == NULL)
	    {
	      fputs("lpadmin: Unable to set the interface script or PPD file:\n", stderr);
	      fputs("         You must specify a printer name first!\n", stderr);
	      return (1);
	    }

	    if (argv[i][2])
	      sprintf(filename, CUPS_DATADIR "/model/%s", argv[i] + 2);
	    else
	    {
	      i ++;
	      sprintf(filename, CUPS_DATADIR "/model/%s", argv[i]);
	    }

            set_printer_file(http, printer, filename);
	    break;

        case 'p' : /* Add/modify a printer */
	    if (argv[i][2])
	      printer = argv[i] + 2;
	    else
	    {
	      i ++;
	      printer = argv[i];
	    }

	    if ((host = strchr(printer, '@')) != NULL)
	    {
	     /*
	      * printer@host - reconnect to the named host...
	      */

	      httpClose(http);

              *host++ = '\0';
              if ((http = httpConnect(host, ippPort())) == NULL)
	      {
		perror("lpadmin: Unable to connect to server");
		return (1);
	      }
	    }
	    break;

        case 'r' : /* Remove printer from class */
	    if (printer == NULL)
	    {
	      fputs("lpadmin: Unable to remove a printer from the class:\n", stderr);
	      fputs("         You must specify a printer name first!\n", stderr);
	      return (1);
	    }

	    if (argv[i][2])
	      delete_printer_from_class(http, printer, argv[i] + 2);
	    else
	    {
	      i ++;
	      delete_printer_from_class(http, printer, argv[i]);
	    }
	    break;

        case 'v' : /* Set the device-uri attribute */
	    if (printer == NULL)
	    {
	      fputs("lpadmin: Unable to set the device URI:\n", stderr);
	      fputs("         You must specify a printer name first!\n", stderr);
	      return (1);
	    }

	    if (argv[i][2])
	      set_printer_device(http, printer, argv[i] + 2);
	    else
	    {
	      i ++;
	      set_printer_device(http, printer, argv[i]);
	    }
	    break;

        case 'x' : /* Delete a printer */
	    if (argv[i][2])
	      printer = argv[i] + 2;
	    else
	    {
	      i ++;
	      printer = argv[i];
	    }

	    if ((host = strchr(printer, '@')) != NULL)
	    {
	     /*
	      * printer@host - reconnect to the named host...
	      */

	      httpClose(http);

              *host++ = '\0';
              if ((http = httpConnect(host, ippPort())) == NULL)
	      {
		perror("lpadmin: Unable to connect to server");
		return (1);
	      }
	    }

            delete_printer(http, printer);
	    i = argc;
	    break;

        case 'D' : /* Set the printer-info attribute */
	    if (printer == NULL)
	    {
	      fputs("lpadmin: Unable to set the printer description:\n", stderr);
	      fputs("         You must specify a printer name first!\n", stderr);
	      return (1);
	    }

	    if (argv[i][2])
	      set_printer_info(http, printer, argv[i] + 2);
	    else
	    {
	      i ++;
	      set_printer_info(http, printer, argv[i]);
	    }
	    break;

        case 'L' : /* Set the printer-location attribute */
	    if (printer == NULL)
	    {
	      fputs("lpadmin: Unable to set the printer location:\n", stderr);
	      fputs("         You must specify a printer name first!\n", stderr);
	      return (1);
	    }

	    if (argv[i][2])
	      set_printer_location(http, printer, argv[i] + 2);
	    else
	    {
	      i ++;
	      set_printer_location(http, printer, argv[i]);
	    }
	    break;

        case 'P' : /* Use the specified PPD file */
	    if (printer == NULL)
	    {
	      fputs("lpadmin: Unable to set the PPD file:\n", stderr);
	      fputs("         You must specify a printer name first!\n", stderr);
	      return (1);
	    }

	    if (argv[i][2])
	      set_printer_file(http, printer, argv[i] + 2);
	    else
	    {
	      i ++;
	      set_printer_file(http, printer, argv[i]);
	    }
	    break;

	default :
	    fprintf(stderr, "lpadmin: Unknown option \'%c\'!\n", argv[i][1]);
	    return (1);
      }
    else
    {
      fprintf(stderr, "lpadmin: Unknown argument \'%s\'!\n", argv[i]);
      return (1);
    }

  if (printer == NULL)
  {
    puts("Usage:");
    puts("");
    puts("    lpadmin [-h server] -d destination");
    puts("    lpadmin [-h server] -x destination");
    puts("    lpadmin [-h server] -p printer [-c add-class] [-i interface] [-m model]");
    puts("                       [-r remove-class] [-v device] [-D description]");
    puts("                       [-P ppd-file]");
    puts("");
  }

  httpClose(http);

  return (0);
}


/*
 * 'add_printer_to_class()' - Add a printer to a class.
 */

static void
add_printer_to_class(http_t *http,	/* I - Server connection */
                     char   *printer,	/* I - Printer to add */
		     char   *pclass)	/* I - Class to add to */
{
  int		i;			/* Looping var */
  ipp_t		*request,		/* IPP Request */
		*response;		/* IPP Response */
  ipp_attribute_t *attr,		/* Current attribute */
		*members;		/* Members in class */
  cups_lang_t	*language;		/* Default language */
  char		uri[HTTP_MAX_URI];	/* URI for printer/class */


  DEBUG_printf(("add_printer_to_class(%08x, \"%s\", \"%s\")\n", http,
                printer, pclass));

 /*
  * Build an IPP_GET_PRINTER_ATTRIBUTES request, which requires the following
  * attributes:
  *
  *    attributes-charset
  *    attributes-natural-language
  *    printer-uri
  */

  sprintf(uri, "ipp://localhost/classes/%s", pclass);

  request = ippNew();

  request->request.op.operation_id = IPP_GET_PRINTER_ATTRIBUTES;
  request->request.op.request_id   = 1;

  language = cupsLangDefault();

  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_CHARSET,
               "attributes-charset", NULL, cupsLangEncoding(language));

  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_LANGUAGE,
               "attributes-natural-language", NULL, language->language);

  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_URI,
               "printer-uri", NULL, uri);

 /*
  * Do the request and get back a response...
  */

  response = cupsDoRequest(http, request, "/classes/");

 /*
  * Build a CUPS_ADD_CLASS request, which requires the following
  * attributes:
  *
  *    attributes-charset
  *    attributes-natural-language
  *    printer-uri
  *    member-uris
  */

  request = ippNew();

  request->request.op.operation_id = CUPS_ADD_CLASS;
  request->request.op.request_id   = 1;

  language = cupsLangDefault();

  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_CHARSET,
               "attributes-charset", NULL, cupsLangEncoding(language));

  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_LANGUAGE,
               "attributes-natural-language", NULL, language->language);

  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_URI,
               "printer-uri", NULL, uri);

 /*
  * See if the printer is already in the class...
  */

  if (response != NULL &&
      (members = ippFindAttribute(response, "member-names", IPP_TAG_NAME)) != NULL)
    for (i = 0; i < members->num_values; i ++)
      if (strcasecmp(printer, members->values[i].string.text) == 0)
      {
        fprintf(stderr, "lpadmin: Printer %s is already a member of class %s.\n",
	        printer, pclass);
        ippDelete(request);
	ippDelete(response);
	return;
      }

 /*
  * OK, the printer isn't part of the class, so add it...
  */

  sprintf(uri, "ipp://localhost/printers/%s", printer);

  if (response != NULL &&
      (members = ippFindAttribute(response, "member-uris", IPP_TAG_URI)) != NULL)
  {
   /*
    * Add the printer to the existing list...
    */

    attr = ippAddStrings(request, IPP_TAG_PRINTER, IPP_TAG_URI,
                         "member-uris", members->num_values + 1, NULL, NULL);
    for (i = 0; i < members->num_values; i ++)
      attr->values[i].string.text = strdup(members->values[i].string.text);

    attr->values[i].string.text = strdup(uri);
  }
  else
    ippAddString(request, IPP_TAG_PRINTER, IPP_TAG_URI, "member-uris", NULL, uri);

 /*
  * Then send the request...
  */

  ippDelete(response);

  if ((response = cupsDoRequest(http, request, "/admin/")) == NULL)
    fputs("lpadmin: Unable to add printer to class!\n", stderr);
  else
    ippDelete(response);
}


/*
 * 'default_printer()' - Set the default printing destination.
 */

static void
default_printer(http_t *http,		/* I - Server connection */
                char   *printer)	/* I - Printer name */
{
  ipp_t		*request,		/* IPP Request */
		*response;		/* IPP Response */
  cups_lang_t	*language;		/* Default language */
  char		uri[HTTP_MAX_URI];	/* URI for printer/class */


  DEBUG_printf(("default_printer(%08x, \"%s\")\n", http, printer));

 /*
  * Build a CUPS_SET_DEFAULT request, which requires the following
  * attributes:
  *
  *    attributes-charset
  *    attributes-natural-language
  *    printer-uri
  */

  sprintf(uri, "ipp://localhost/printers/%s", printer);

  request = ippNew();

  request->request.op.operation_id = CUPS_SET_DEFAULT;
  request->request.op.request_id   = 1;

  language = cupsLangDefault();

  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_CHARSET,
               "attributes-charset", NULL, cupsLangEncoding(language));

  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_LANGUAGE,
               "attributes-natural-language", NULL, language->language);

  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_URI,
               "printer-uri", NULL, uri);

 /*
  * Do the request and get back a response...
  */

  if ((response = cupsDoRequest(http, request, "/admin/")) == NULL)
    fputs("lpadmin: Unable to set default destination!\n", stderr);
  else
  {
    if (response->request.status.status_code == IPP_NOT_FOUND)
      fprintf(stderr, "lpadmin: Destination %s does not exist!\n", printer);

    ippDelete(response);
  }
}


/*
 * 'delete_printer()' - Delete a printer from the system...
 */

static void
delete_printer(http_t *http,		/* I - Server connection */
               char   *printer)		/* I - Printer to delete */
{
  ipp_t		*request,		/* IPP Request */
		*response;		/* IPP Response */
  cups_lang_t	*language;		/* Default language */
  char		uri[HTTP_MAX_URI];	/* URI for printer/class */


  DEBUG_printf(("delete_printer(%08x, \"%s\")\n", http, printer));

 /*
  * Build a CUPS_DELETE_PRINTER request, which requires the following
  * attributes:
  *
  *    attributes-charset
  *    attributes-natural-language
  *    printer-uri
  */

  sprintf(uri, "ipp://localhost/printers/%s", printer);

  request = ippNew();

  request->request.op.operation_id = CUPS_DELETE_PRINTER;
  request->request.op.request_id   = 1;

  language = cupsLangDefault();

  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_CHARSET,
               "attributes-charset", NULL, cupsLangEncoding(language));

  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_LANGUAGE,
               "attributes-natural-language", NULL, language->language);

  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_URI,
               "printer-uri", NULL, uri);

 /*
  * Do the request and get back a response...
  */

  if ((response = cupsDoRequest(http, request, "/admin/")) == NULL)
    fputs("lpadmin: Unable to delete printer!\n", stderr);
  else
  {
    if (response->request.status.status_code == IPP_NOT_FOUND)
      fprintf(stderr, "lpadmin: Destination %s does not exist!\n", printer);

    ippDelete(response);
  }
}


/*
 * 'delete_printer_from_class()' - Delete a printer from a class.
 */

static void
delete_printer_from_class(http_t *http,		/* I - Server connection */
                          char   *printer,	/* I - Printer to remove */
			  char   *pclass)	/* I - Class to remove from */
{
  int		i, j, k;		/* Looping vars */
  ipp_t		*request,		/* IPP Request */
		*response;		/* IPP Response */
  ipp_attribute_t *attr,		/* Current attribute */
		*members;		/* Members in class */
  cups_lang_t	*language;		/* Default language */
  char		uri[HTTP_MAX_URI];	/* URI for printer/class */


  DEBUG_printf(("delete_printer_from_class(%08x, \"%s\", \"%s\")\n", http,
                printer, pclass));

 /*
  * Build an IPP_GET_PRINTER_ATTRIBUTES request, which requires the following
  * attributes:
  *
  *    attributes-charset
  *    attributes-natural-language
  *    printer-uri
  */

  sprintf(uri, "ipp://localhost/classes/%s", pclass);

  request = ippNew();

  request->request.op.operation_id = IPP_GET_PRINTER_ATTRIBUTES;
  request->request.op.request_id   = 1;

  language = cupsLangDefault();

  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_CHARSET,
               "attributes-charset", NULL, cupsLangEncoding(language));

  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_LANGUAGE,
               "attributes-natural-language", NULL, language->language);

  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_URI,
               "printer-uri", NULL, uri);

 /*
  * Do the request and get back a response...
  */

  if ((response = cupsDoRequest(http, request, "/classes/")) == NULL ||
      response->request.status.status_code == IPP_NOT_FOUND)
  {
    ippDelete(response);
    fprintf(stderr, "lpadmin: Class %s does not exist!\n", pclass);
    return;
  }

 /*
  * See if the printer is already in the class...
  */

  if ((members = ippFindAttribute(response, "member-names", IPP_TAG_NAME)) == NULL)
  {
    ippDelete(response);
    fputs("lpadmin: No member names were seen!\n", stderr);
    return;
  }

  for (i = 0; i < members->num_values; i ++)
    if (strcasecmp(printer, members->values[i].string.text) == 0)
      break;

  if (i >= members->num_values)
  {
    fprintf(stderr, "lpadmin: Printer %s is not a member of class %s.\n",
	    printer, pclass);
    ippDelete(response);
    return;
  }

  if (members->num_values == 1)
  {
   /*
    * Build a CUPS_DELETE_CLASS request, which requires the following
    * attributes:
    *
    *    attributes-charset
    *    attributes-natural-language
    *    printer-uri
    */

    request = ippNew();

    request->request.op.operation_id = CUPS_DELETE_CLASS;
    request->request.op.request_id   = 1;

    language = cupsLangDefault();

    ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_CHARSET,
        	 "attributes-charset", NULL, cupsLangEncoding(language));

    ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_LANGUAGE,
        	 "attributes-natural-language", NULL, language->language);

    ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_URI,
        	 "printer-uri", NULL, uri);
  }
  else
  {
   /*
    * Build a CUPS_ADD_CLASS request, which requires the following
    * attributes:
    *
    *    attributes-charset
    *    attributes-natural-language
    *    printer-uri
    *    member-uris
    */

    request = ippNew();

    request->request.op.operation_id = CUPS_ADD_CLASS;
    request->request.op.request_id   = 1;

    language = cupsLangDefault();

    ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_CHARSET,
        	 "attributes-charset", NULL, cupsLangEncoding(language));

    ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_LANGUAGE,
        	 "attributes-natural-language", NULL, language->language);

    ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_URI,
        	 "printer-uri", NULL, uri);

   /*
    * Delete the printer from the class...
    */

    members = ippFindAttribute(response, "member-uris", IPP_TAG_URI);
    attr = ippAddStrings(request, IPP_TAG_PRINTER, IPP_TAG_URI,
                         "member-uris", members->num_values - 1, NULL, NULL);

    for (j = 0, k = 0; j < members->num_values; j ++)
      if (j != i)
        attr->values[k ++].string.text = strdup(members->values[j].string.text);
  }

 /*
  * Then send the request...
  */

  ippDelete(response);

  if ((response = cupsDoRequest(http, request, "/admin/")) == NULL)
    fputs("lpadmin: Unable to remove printer from class!\n", stderr);
  else
    ippDelete(response);
}


/*
 * 'enable_printer()' - Enable a printer...
 */

static void
enable_printer(http_t *http,		/* I - Server connection */
               char   *printer)		/* I - Printer to enable */
{
  ipp_t		*request,		/* IPP Request */
		*response;		/* IPP Response */
  cups_lang_t	*language;		/* Default language */
  char		uri[HTTP_MAX_URI];	/* URI for printer/class */


  DEBUG_printf(("enable_printer(%08x, \"%s\")\n", http, printer));

 /*
  * Build a CUPS_ADD_PRINTER request, which requires the following
  * attributes:
  *
  *    attributes-charset
  *    attributes-natural-language
  *    printer-uri
  *    printer-state
  *    printer-is-accepting-jobs
  */

  sprintf(uri, "ipp://localhost/printers/%s", printer);

  request = ippNew();

  request->request.op.operation_id = CUPS_ADD_PRINTER;
  request->request.op.request_id   = 1;

  language = cupsLangDefault();

  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_CHARSET,
               "attributes-charset", NULL, cupsLangEncoding(language));

  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_LANGUAGE,
               "attributes-natural-language", NULL, language->language);

  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_URI,
               "printer-uri", NULL, uri);

  ippAddInteger(request, IPP_TAG_PRINTER, IPP_TAG_ENUM, "printer-state",
                IPP_PRINTER_IDLE);

  ippAddBoolean(request, IPP_TAG_PRINTER, "printer-is-accepting-jobs", 1);

 /*
  * Do the request and get back a response...
  */

  if ((response = cupsDoRequest(http, request, "/admin/")) == NULL)
    fputs("lpadmin: Unable to delete printer!\n", stderr);
  else
  {
    if (response->request.status.status_code == IPP_NOT_FOUND)
      fprintf(stderr, "lpadmin: Destination %s does not exist!\n", printer);

    ippDelete(response);
  }
}


/*
 * 'set_printer_device()' - Set the device-uri attribute.
 */

static void
set_printer_device(http_t *http,	/* I - Server connection */
                   char   *printer,	/* I - Printer */
		   char   *device)	/* I - New device URI */
{
  ipp_t		*request,		/* IPP Request */
		*response;		/* IPP Response */
  cups_lang_t	*language;		/* Default language */
  char		uri[HTTP_MAX_URI];	/* URI for printer/class */


  DEBUG_printf(("set_printer_device(%08x, \"%s\", \"%s\")\n", http, printer,
                device));

 /*
  * Build a CUPS_ADD_PRINTER request, which requires the following
  * attributes:
  *
  *    attributes-charset
  *    attributes-natural-language
  *    printer-uri
  */

  sprintf(uri, "ipp://localhost/printers/%s", printer);

  request = ippNew();

  request->request.op.operation_id = CUPS_ADD_PRINTER;
  request->request.op.request_id   = 1;

  language = cupsLangDefault();

  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_CHARSET,
               "attributes-charset", NULL, cupsLangEncoding(language));

  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_LANGUAGE,
               "attributes-natural-language", NULL, language->language);

  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_URI,
               "printer-uri", NULL, uri);

 /*
  * Add the device URI...
  */

  if (device[0] == '/')
  {
   /*
    * Convert filename to URI...
    */

    sprintf(uri, "file:%s", device);
    ippAddString(request, IPP_TAG_PRINTER, IPP_TAG_URI, "device-uri", NULL,
                 uri);
  }
  else
    ippAddString(request, IPP_TAG_PRINTER, IPP_TAG_URI, "device-uri", NULL,
                 device);

 /*
  * Do the request and get back a response...
  */

  if ((response = cupsDoRequest(http, request, "/admin/")) == NULL)
    fputs("lpadmin: Unable to set device-uri attribute!\n", stderr);
  else
    ippDelete(response);
}


/*
 * 'set_printer_file()' - Set the interface script or PPD file.
 */

static void
set_printer_file(http_t *http,		/* I - Server connection */
                 char   *printer,	/* I - Printer */
		 char   *file)		/* I - PPD file or interface script */
{
  ipp_t		*request,		/* IPP Request */
		*response;		/* IPP Response */
  cups_lang_t	*language;		/* Default language */
  char		uri[HTTP_MAX_URI];	/* URI for printer/class */
#ifdef HAVE_LIBZ
  char		tempfile[1024];		/* Temporary filename */
  FILE		*fp;			/* Temporary file */
  gzFile	*gz;			/* GZIP'd file */
  char		buffer[8192];		/* Copy buffer */
  int		bytes;			/* Bytes in buffer */


  DEBUG_printf(("set_printer_file(%08x, \"%s\", \"%s\")\n", http, printer,
                file));

 /*
  * See if the file is gzip'd; if so, unzip it to a temporary file and
  * send the uncompressed file.
  */

  if (strcmp(file + strlen(file) - 3, ".gz") == 0)
  {
   /*
    * Yes, the file is compressed; uncompress to a temp file...
    */

    if ((fp = fopen(tmpnam(tempfile), "wb")) == NULL)
    {
      perror("lpadmin: Unable to create temporary file");
      return;
    }

    if ((gz = gzopen(file, "rb")) == NULL)
    {
      perror("lpadmin: Unable to open file");
      fclose(fp);
      unlink(tempfile);
      return;
    }

    while ((bytes = gzread(gz, buffer, sizeof(buffer))) > 0)
      fwrite(buffer, bytes, 1, fp);

    fclose(fp);
    gzclose(gz);

    file = tempfile;
  }
#endif /* HAVE_LIBZ */

 /*
  * Build a CUPS_ADD_PRINTER request, which requires the following
  * attributes:
  *
  *    attributes-charset
  *    attributes-natural-language
  *    printer-uri
  */

  sprintf(uri, "ipp://localhost/printers/%s", printer);

  request = ippNew();

  request->request.op.operation_id = CUPS_ADD_PRINTER;
  request->request.op.request_id   = 1;

  language = cupsLangDefault();

  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_CHARSET,
               "attributes-charset", NULL, cupsLangEncoding(language));

  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_LANGUAGE,
               "attributes-natural-language", NULL, language->language);

  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_URI,
               "printer-uri", NULL, uri);

 /*
  * Do the request and get back a response...
  */

  if ((response = cupsDoFileRequest(http, request, "/admin/", file)) == NULL)
    fputs("lpadmin: Unable to set interface script or PPD file!\n", stderr);
  else
    ippDelete(response);

#ifdef HAVE_LIBZ
 /*
  * Remove the temporary file as needed...
  */

  if (file == tempfile)
    unlink(tempfile);
#endif /* HAVE_LIBZ */
}


/*
 * 'set_printer_info()' - Set the printer description string.
 */

static void
set_printer_info(http_t *http,		/* I - Server connection */
                 char   *printer,	/* I - Printer */
		 char   *info)		/* I - New description string */
{
  ipp_t		*request,		/* IPP Request */
		*response;		/* IPP Response */
  cups_lang_t	*language;		/* Default language */
  char		uri[HTTP_MAX_URI];	/* URI for printer/class */


  DEBUG_printf(("set_printer_info(%08x, \"%s\", \"%s\")\n", http, printer,
                info));

 /*
  * Build a CUPS_ADD_PRINTER request, which requires the following
  * attributes:
  *
  *    attributes-charset
  *    attributes-natural-language
  *    printer-uri
  */

  sprintf(uri, "ipp://localhost/printers/%s", printer);

  request = ippNew();

  request->request.op.operation_id = CUPS_ADD_PRINTER;
  request->request.op.request_id   = 1;

  language = cupsLangDefault();

  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_CHARSET,
               "attributes-charset", NULL, cupsLangEncoding(language));

  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_LANGUAGE,
               "attributes-natural-language", NULL, language->language);

  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_URI,
               "printer-uri", NULL, uri);

 /*
  * Add the info string...
  */

  ippAddString(request, IPP_TAG_PRINTER, IPP_TAG_TEXT, "printer-info", NULL,
               info);

 /*
  * Do the request and get back a response...
  */

  if ((response = cupsDoRequest(http, request, "/admin/")) == NULL)
    fputs("lpadmin: Unable to set printer-info attribute!\n", stderr);
  else
    ippDelete(response);
}


/*
 * 'set_printer_location()' - Set the printer location string.
 */

static void
set_printer_location(http_t *http,	/* I - Server connection */
                     char   *printer,	/* I - Printer */
		     char   *location)	/* I - New location string */
{
  ipp_t		*request,		/* IPP Request */
		*response;		/* IPP Response */
  cups_lang_t	*language;		/* Default language */
  char		uri[HTTP_MAX_URI];	/* URI for printer/class */


  DEBUG_printf(("set_printer_location(%08x, \"%s\", \"%s\")\n", http, printer,
                location));

 /*
  * Build a CUPS_ADD_PRINTER request, which requires the following
  * attributes:
  *
  *    attributes-charset
  *    attributes-natural-language
  *    printer-uri
  */

  sprintf(uri, "ipp://localhost/printers/%s", printer);

  request = ippNew();

  request->request.op.operation_id = CUPS_ADD_PRINTER;
  request->request.op.request_id   = 1;

  language = cupsLangDefault();

  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_CHARSET,
               "attributes-charset", NULL, cupsLangEncoding(language));

  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_LANGUAGE,
               "attributes-natural-language", NULL, language->language);

  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_URI,
               "printer-uri", NULL, uri);

 /*
  * Add the location string...
  */

  ippAddString(request, IPP_TAG_PRINTER, IPP_TAG_TEXT, "printer-location", NULL,
               location);

 /*
  * Do the request and get back a response...
  */

  if ((response = cupsDoRequest(http, request, "/admin/")) == NULL)
    fputs("lpadmin: Unable to set printer-location attribute!\n", stderr);
  else
    ippDelete(response);
}


/*
 * End of "$Id: lpadmin.c,v 1.6 1999/06/23 14:12:24 mike Exp $".
 */
