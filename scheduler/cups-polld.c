/*
 * "$Id: cups-polld.c,v 1.5 2001/01/22 15:03:59 mike Exp $"
 *
 *   Polling daemon for the Common UNIX Printing System (CUPS).
 *
 *   Copyright 1997-2001 by Easy Software Products, all rights reserved.
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
 */

/*
 * Include necessary headers...
 */

#include <cups/cups.h>
#include <stdlib.h>
#include <cups/language.h>
#include <cups/string.h>


/*
 * Local functions...
 */

int	poll_server(http_t *http, cups_lang_t *language, ipp_op_t op,
	            int sock, int port);


/*
 * 'main()' - Open socks and poll until we are killed...
 */

int					/* O - Exit status */
main(int  argc,				/* I - Number of command-line arguments */
     char *argv[])			/* I - Command-line arguments */
{
  http_t		*http;		/* HTTP connection */
  cups_lang_t		*language;	/* Language info */
  int			interval;	/* Polling interval */
  int			sock;		/* Browser sock */
  int			port;		/* Browser port */
  int			val;		/* Socket option value */


 /*
  * The command-line must contain the following:
  *
  *    cups-polld server server-port interval port
  */

  if (argc != 5)
  {
    fputs("Usage: cups-polld server server-port interval port\n", stderr);
    return (1);
  }

  interval = atoi(argv[3]);
  port     = atoi(argv[4]);

 /*
  * Open a connection to the server...
  */

  if ((http = httpConnect(argv[1], atoi(argv[2]))) == NULL)
  {
    perror("cups-polld");
    return (1);
  }

 /*
  * Open a broadcast sock...
  */

  if ((sock = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
  {
    perror("cups-polld");
    httpClose(http);
    return (1);
  }

 /*
  * Set the "broadcast" flag...
  */

  val = 1;
  if (setsockopt(sock, SOL_SOCKET, SO_BROADCAST, &val, sizeof(val)))
  {
    perror("cups-polld");

    close(sock);
    httpClose(http);
    return (1);
  }

 /*
  * Loop forever, asking for available printers and classes...
  */

  language = cupsLangDefault();

  for (;;)
  {
    if (poll_server(http, language, CUPS_GET_PRINTERS, sock, port))
      continue;

    if (poll_server(http, language, CUPS_GET_CLASSES, sock, port))
      continue;

    sleep(interval);
  }
}


/*
 * 'poll_server()' - Poll the server for the given set of printers or classes.
 */

int					/* O - 0 for success, -1 on error */
poll_server(http_t      *http,		/* I - HTTP connection */
            cups_lang_t *language,	/* I - Language */
	    ipp_op_t    op,		/* I - Operation code */
	    int         sock,		/* I - Broadcast sock */
	    int         port)		/* I - Broadcast port */
{
  ipp_t			*request,	/* Request data */
			*response;	/* Response data */
  ipp_attribute_t	*attr;		/* Current attribute */
  const char		*uri,		/* printer-uri */
			*info,		/* printer-info */
			*location,	/* printer-location */
			*make_model;	/* printer-make-and-model */
  cups_ptype_t		type;		/* printer-type */
  ipp_pstate_t		state;		/* printer-state */
  struct sockaddr_in	addr;		/* Broadcast address */
  char			packet[1540];	/* Data packet */


 /*
  * Broadcast to 127.0.0.1 (localhost)
  */

  memset(&addr, 0, sizeof(addr));
  addr.sin_addr.s_addr = htonl(0x7f000001);
  addr.sin_family      = AF_INET;
  addr.sin_port        = htons(port);

 /*
  * Build a CUPS_GET_PRINTERS or CUPS_GET_CLASSES request, which requires
  * only the attributes-charset and attributes-natural-language attributes.
  */

  request = ippNew();

  request->request.op.operation_id = op;
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
    if (response->request.status.status_code > IPP_OK_CONFLICT)
    {
      fprintf(stderr, "cups-polld: get-%s failed: %s\n",
              op == CUPS_GET_PRINTERS ? "printers" : "classes",
              ippErrorString(response->request.status.status_code));
      ippDelete(response);
      return (-1);
    }

   /*
    * Loop through the printers or classes returned in the list...
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

      uri        = NULL;
      info       = "";
      location   = "";
      make_model = "";
      type       = CUPS_PRINTER_REMOTE;
      state      = IPP_PRINTER_IDLE;

      while (attr != NULL && attr->group_tag == IPP_TAG_PRINTER)
      {
        if (strcmp(attr->name, "printer-uri-supported") == 0 &&
	    attr->value_tag == IPP_TAG_URI)
	  uri = attr->values[0].string.text;

        if (strcmp(attr->name, "printer-info") == 0 &&
	    attr->value_tag == IPP_TAG_TEXT)
	  info = attr->values[0].string.text;

        if (strcmp(attr->name, "printer-location") == 0 &&
	    attr->value_tag == IPP_TAG_TEXT)
	  location = attr->values[0].string.text;

        if (strcmp(attr->name, "printer-make-and-model") == 0 &&
	    attr->value_tag == IPP_TAG_TEXT)
	  make_model = attr->values[0].string.text;

        if (strcmp(attr->name, "printer-state") == 0 &&
	    attr->value_tag == IPP_TAG_ENUM)
	  state = (ipp_pstate_t)attr->values[0].integer;

        if (strcmp(attr->name, "printer-type") == 0 &&
	    attr->value_tag == IPP_TAG_ENUM)
	  type = (cups_ptype_t)attr->values[0].integer;

        attr = attr->next;
      }

     /*
      * See if we have everything needed...
      */

      if (uri == NULL)
      {
        if (attr == NULL)
	  break;
	else
          continue;
      }

     /*
      * See if this is a local printer or class...
      */

      if (!(type & CUPS_PRINTER_REMOTE))
      {
       /*
	* Send the printer information...
	*/

	snprintf(packet, sizeof(packet), "%x %x %s \"%s\" \"%s\" \"%s\"\n",
        	 type | CUPS_PRINTER_REMOTE, state, uri,
		 location, info, make_model);
        puts(packet);

	if (sendto(sock, packet, strlen(packet), 0,
	           (struct sockaddr *)&addr, sizeof(addr)) <= 0)
	{
	  perror("cups-polld");
	  return (-1);
	}
      }

      if (attr == NULL)
        break;
    }

    ippDelete(response);
  }
  else
  {
    fprintf(stderr, "cups-polld: get-%s failed: %s\n",
            op == CUPS_GET_PRINTERS ? "printers" : "classes",
            ippErrorString(cupsLastError()));
    return (-1);
  }

  return (0);
}


/*
 * End of "$Id: cups-polld.c,v 1.5 2001/01/22 15:03:59 mike Exp $".
 */
