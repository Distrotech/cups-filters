/*
 * "$Id$"
 *
 *   Line Printer Daemon interface for the Common UNIX Printing System (CUPS).
 *
 *   Copyright 1997-2005 by Easy Software Products, all rights reserved.
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
 *   main()             - Process an incoming LPD request...
 *   print_file()       - Print a file to a printer or class.
 *   recv_print_job()   - Receive a print job from the client.
 *   remove_jobs()      - Cancel one or more jobs.
 *   send_state()       - Send the queue state.
 *   smart_gets()       - Get a line of text, removing the trailing CR
 *                        and/or LF.
 */

/*
 * Include necessary headers...
 */

#include <cups/cups.h>
#include <cups/string.h>
#include <cups/language.h>
#include <stdlib.h>
#include <errno.h>
#include <syslog.h>
#include <ctype.h>
#include <unistd.h>
#include <fcntl.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>


/*
 * LPD "mini-daemon" for CUPS.  This program must be used in conjunction
 * with inetd or another similar program that monitors ports and starts
 * daemons for each client connection.  A typical configuration is:
 *
 *    printer stream tcp nowait lp /usr/lib/cups/daemon/cups-lpd cups-lpd
 *
 * This daemon implements most of RFC 1179 (the unofficial LPD specification)
 * except for:
 *
 *     - This daemon does not check to make sure that the source port is
 *       between 721 and 731, since it isn't necessary for proper
 *       functioning and port-based security is no security at all!
 *
 *     - The "Print any waiting jobs" command is a no-op.
 *
 * The LPD-to-IPP mapping is as defined in RFC 2569.  The report formats
 * currently match the Solaris LPD mini-daemon.
 */

/*
 * Prototypes...
 */

int	print_file(const char *name, const char *file,
	           const char *title, const char *docname,
	           const char *user, int num_options,
		   cups_option_t *options);
int	recv_print_job(const char *dest, int num_defaults, cups_option_t *defaults);
int	remove_jobs(const char *dest, const char *agent, const char *list);
int	send_state(const char *dest, const char *list, int longstatus);
char	*smart_gets(char *s, int len, FILE *fp);


/*
 * 'main()' - Process an incoming LPD request...
 */

int					/* O - Exit status */
main(int  argc,				/* I - Number of command-line arguments */
     char *argv[])			/* I - Command-line arguments */
{
  int		i;			/* Looping var */
  int		num_defaults;		/* Number of default options */
  cups_option_t	*defaults;		/* Default options */
  char		line[256],		/* Command string */
		command,		/* Command code */
		*dest,			/* Pointer to destination */
		*list,			/* Pointer to list */
		*agent,			/* Pointer to user */
		status;			/* Status for client */
  int		hostlen;		/* Size of client address */
  http_addr_t	hostaddr;		/* Address of client */
  char		hostname[256],		/* Name of client */
		hostip[256],		/* IP address */
		*hostfamily;		/* Address family */


 /*
  * Don't buffer the output...
  */

  setbuf(stdout, NULL);

 /*
  * Log things using the "cups-lpd" name...
  */

  openlog("cups-lpd", LOG_PID, LOG_LPR);

 /*
  * Get the address of the client...
  */

  hostlen = sizeof(hostaddr);

  if (getpeername(0, (struct sockaddr *)&hostaddr, &hostlen))
  {
    syslog(LOG_WARNING, "Unable to get client address - %s", strerror(errno));
    strcpy(hostname, "unknown");
  }
  else
  {
    httpAddrLookup(&hostaddr, hostname, sizeof(hostname));
    httpAddrString(&hostaddr, hostip, sizeof(hostip));

#ifdef AF_INET6
    if (hostaddr.addr.sa_family == AF_INET6)
      hostfamily = "IPv6";
    else
#endif /* AF_INET6 */
    hostfamily = "IPv4";

    syslog(LOG_INFO, "Connection from %s (%s %s)", hostname, hostfamily, hostip);
  }

 /*
  * Scan the command-line for options...
  */

  num_defaults = 0;
  defaults     = NULL;

  num_defaults = cupsAddOption("job-originating-host-name", hostname,
                               num_defaults, &defaults);

  for (i = 1; i < argc; i ++)
    if (argv[i][0] == '-')
    {
      switch (argv[i][1])
      {
	case 'o' : /* Option */
	    if (argv[i][2])
	      num_defaults = cupsParseOptions(argv[i] + 2, num_defaults,
	                                      &defaults);
	    else
	    {
	      i ++;
	      if (i < argc)
		num_defaults = cupsParseOptions(argv[i], num_defaults, &defaults);
              else
        	syslog(LOG_WARNING, "Expected option string after -o option!");
            }
	    break;
	default :
	    syslog(LOG_WARNING, "Unknown option \"%c\" ignored!", argv[i][1]);
	    break;
      }
    }
    else
      syslog(LOG_WARNING, "Unknown command-line option \"%s\" ignored!", argv[i]);

 /*
  * RFC1179 specifies that only 1 daemon command can be received for
  * every connection.
  */

  if (smart_gets(line, sizeof(line), stdin) == NULL)
  {
   /*
    * Unable to get command from client!  Send an error status and return.
    */

    syslog(LOG_ERR, "Unable to get command line from client!");
    putchar(1);
    return (1);
  }

 /*
  * The first byte is the command byte.  After that will be the queue name,
  * resource list, and/or user name.
  */

  command = line[0];
  dest    = line + 1;

  for (list = dest + 1; *list && !isspace(*list & 255); list ++);

  while (isspace(*list & 255))
    *list++ = '\0';

 /*
  * Do the command...
  */

  switch (command)
  {
    default : /* Unknown command */
        syslog(LOG_ERR, "Unknown LPD command 0x%02X!", command);
        syslog(LOG_ERR, "Command line = %s", line + 1);
	putchar(1);

        status = 1;
	break;

    case 0x01 : /* Print any waiting jobs */
        syslog(LOG_INFO, "Print waiting jobs (no-op)");
	putchar(0);

        status = 0;
	break;

    case 0x02 : /* Receive a printer job */
        syslog(LOG_INFO, "Receive print job for %s", dest);
        /* recv_print_job() sends initial status byte */

        status = recv_print_job(dest, num_defaults, defaults);
	break;

    case 0x03 : /* Send queue state (short) */
        syslog(LOG_INFO, "Send queue state (short) for %s %s", dest, list);
	/* no status byte for this command */

        status = send_state(dest, list, 0);
	break;

    case 0x04 : /* Send queue state (long) */
        syslog(LOG_INFO, "Send queue state (long) for %s %s", dest, list);
	/* no status byte for this command */

        status = send_state(dest, list, 1);
	break;

    case 0x05 : /* Remove jobs */
       /*
        * Grab the agent and skip to the list of users and/or jobs.
	*/

        agent = list;

	for (; *list && !isspace(*list & 255); list ++);
	while (isspace(*list & 255))
	  *list++ = '\0';

        syslog(LOG_INFO, "Remove jobs %s on %s by %s", list, dest, agent);

        status = remove_jobs(dest, agent, list);

	putchar(status);
	break;
  }

  syslog(LOG_INFO, "Closing connection");
  closelog();

  return (status);
}


/*
 * 'check_printer()' - Check that a printer exists and is accepting jobs.
 */

int					/* O - Job ID */
check_printer(const char *name)		/* I - Printer or class name */
{
  http_t	*http;			/* Connection to server */
  ipp_t		*request;		/* IPP request */
  ipp_t		*response;		/* IPP response */
  ipp_attribute_t *attr;		/* IPP job-id attribute */
  char		uri[HTTP_MAX_URI];	/* Printer URI */
  cups_lang_t	*language;		/* Language to use */
  int		accepting;		/* printer-is-accepting-jobs value */


 /*
  * Setup a connection and request data...
  */

  if ((http = httpConnectEncrypt(cupsServer(), ippPort(),
                                 cupsEncryption())) == NULL)
  {
    syslog(LOG_ERR, "Unable to connect to server %s: %s", cupsServer(),
           strerror(errno));
    return (0);
  }

 /*
  * Build a standard CUPS URI for the printer and fill the standard IPP
  * attributes...
  */

  if ((request = ippNew()) == NULL)
  {
    syslog(LOG_ERR, "Unable to create request: %s", strerror(errno));
    httpClose(http);
    return (0);
  }

  request->request.op.operation_id = IPP_GET_PRINTER_ATTRIBUTES;
  request->request.op.request_id   = 1;

  snprintf(uri, sizeof(uri), "ipp://localhost/printers/%s", name);

  language = cupsLangDefault();

  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_CHARSET,
               "attributes-charset", NULL, cupsLangEncoding(language));

  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_LANGUAGE,
               "attributes-natural-language", NULL,
               language != NULL ? language->language : "C");

  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_URI, "printer-uri",
               NULL, uri);

  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_NAME, "requested-attributes",
               NULL, "printer-is-accepting-jobs");

 /*
  * Do the request...
  */

  response = cupsDoRequest(http, request, "/");

  if (response == NULL)
  {
    syslog(LOG_ERR, "Unable to check printer status - %s",
           ippErrorString(cupsLastError()));
    accepting = 0;
  }
  else if (response->request.status.status_code > IPP_OK_CONFLICT)
  {
    syslog(LOG_ERR, "Unable to check printer status - %s",
           ippErrorString(response->request.status.status_code));
    accepting = 0;
  }
  else if ((attr = ippFindAttribute(response, "printer-is-accepting-jobs",
                                    IPP_TAG_BOOLEAN)) == NULL)
  {
    syslog(LOG_ERR, "No printer-is-accepting-jobs attribute found in response from server!");
    accepting = 0;
  }
  else
    accepting = attr->values[0].boolean;

  if (response != NULL)
    ippDelete(response);

  httpClose(http);
  cupsLangFree(language);

  return (accepting);
}


/*
 * 'print_file()' - Print a file to a printer or class.
 */

int					/* O - Job ID */
print_file(const char    *name,		/* I - Printer or class name */
           const char    *file,		/* I - File to print */
           const char    *title,	/* I - Title of job */
           const char    *docname,	/* I - Name of job file */
           const char    *user,		/* I - Owner of job */
           int           num_options,	/* I - Number of options */
	   cups_option_t *options)	/* I - Options */
{
  http_t	*http;			/* Connection to server */
  ipp_t		*request;		/* IPP request */
  ipp_t		*response;		/* IPP response */
  ipp_attribute_t *attr;		/* IPP job-id attribute */
  char		uri[HTTP_MAX_URI];	/* Printer URI */
  cups_lang_t	*language;		/* Language to use */
  int		jobid;			/* New job ID */


 /*
  * Setup a connection and request data...
  */

  if ((http = httpConnectEncrypt(cupsServer(), ippPort(),
                                 cupsEncryption())) == NULL)
  {
    syslog(LOG_ERR, "Unable to connect to server %s: %s", cupsServer(),
           strerror(errno));
    return (0);
  }

 /*
  * Build a standard CUPS URI for the printer and fill the standard IPP
  * attributes...
  */

  if ((request = ippNew()) == NULL)
  {
    syslog(LOG_ERR, "Unable to create request: %s", strerror(errno));
    httpClose(http);
    return (0);
  }

  request->request.op.operation_id = IPP_PRINT_JOB;
  request->request.op.request_id   = 1;

  snprintf(uri, sizeof(uri), "ipp://localhost/printers/%s", name);

  language = cupsLangDefault();

  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_CHARSET,
               "attributes-charset", NULL, cupsLangEncoding(language));

  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_LANGUAGE,
               "attributes-natural-language", NULL,
               language != NULL ? language->language : "C");

  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_URI, "printer-uri",
               NULL, uri);

  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_NAME, "requesting-user-name",
               NULL, user);

  if (title)
    ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_NAME, "job-name", NULL, title);
  if (docname)
    ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_NAME, "document-name", NULL, docname);

 /*
  * Then add all options on the command-line...
  */

  cupsEncodeOptions(request, num_options, options);

 /*
  * Do the request...
  */

  snprintf(uri, sizeof(uri), "/printers/%s", name);

  response = cupsDoFileRequest(http, request, uri, file);

  if (response == NULL)
  {
    syslog(LOG_ERR, "Unable to print file - %s",
           ippErrorString(cupsLastError()));
    jobid = 0;
  }
  else if (response->request.status.status_code > IPP_OK_CONFLICT)
  {
    syslog(LOG_ERR, "Unable to print file - %s",
           ippErrorString(response->request.status.status_code));
    jobid = 0;
  }
  else if ((attr = ippFindAttribute(response, "job-id", IPP_TAG_INTEGER)) == NULL)
  {
    syslog(LOG_ERR, "No job-id attribute found in response from server!");
    jobid = 0;
  }
  else
  {
    jobid = attr->values[0].integer;

    syslog(LOG_INFO, "Print file - job ID = %d", jobid);
  }

  if (response != NULL)
    ippDelete(response);

  httpClose(http);
  cupsLangFree(language);

  return (jobid);
}


/*
 * 'recv_print_job()' - Receive a print job from the client.
 */

int					/* O - Command status */
recv_print_job(const char    *dest,	/* I - Destination */
               int           num_defaults,
					/* I - Number of default options */
	       cups_option_t *defaults)	/* I - Default options */
{
  int		i;			/* Looping var */
  int		status;			/* Command status */
  int		fd;			/* Temporary file */
  FILE		*fp;			/* File pointer */
  char		filename[1024];		/* Temporary filename */
  int		bytes;			/* Bytes received */
  char		line[256],		/* Line from file/stdin */
		command,		/* Command from line */
		*count,			/* Number of bytes */
		*name;			/* Name of file */
  const char	*job_sheets;		/* Job sheets */
  int		num_data;		/* Number of data files */
  char		control[1024],		/* Control filename */
		data[32][256],		/* Data files */
		temp[32][1024];		/* Temporary files */
  char		user[1024],		/* User name */
		title[1024],		/* Job title */
		docname[1024],		/* Document name */
		queue[256],		/* Printer/class queue */
		*instance;		/* Printer/class instance */
  int		num_dests;		/* Number of destinations */
  cups_dest_t	*dests,			/* Destinations */
		*destptr;		/* Current destination */
  int		num_options;		/* Number of options */
  cups_option_t	*options;		/* Options */
  int		banner;			/* Print banner? */


  status   = 0;
  num_data = 0;
  fd       = -1;

  control[0] = '\0';

  strlcpy(queue, dest, sizeof(queue));

  if ((instance = strrchr(queue, '/')) != NULL)
    *instance++ = '\0';

  num_dests = cupsGetDests(&dests);
  if ((destptr = cupsGetDest(queue, instance, num_dests, dests)) == NULL)
  {
   /*
    * If the queue name is blank or "lp" then use the default queue.
    */

    if (!queue[0] || !strcmp(queue, "lp"))
      if ((destptr = cupsGetDest(NULL, NULL, num_dests, dests)) != NULL)
	strlcpy(queue, destptr->name, sizeof(queue));

    if (destptr == NULL)
    {
      if (instance)
	syslog(LOG_ERR, "Unknown destination %s/%s!", queue, instance);
      else
	syslog(LOG_ERR, "Unknown destination %s!", queue);

      cupsFreeDests(num_dests, dests);

      putchar(1);

      return (1);
    }
  }

  if (!check_printer(queue))
  {
    cupsFreeDests(num_dests, dests);

    putchar(1);

    return (1);
  }

  putchar(0);

  while (smart_gets(line, sizeof(line), stdin) != NULL)
  {
    if (strlen(line) < 2)
    {
      status = 1;
      break;
    }

    command = line[0];
    count   = line + 1;

    for (name = count + 1; *name && !isspace(*name & 255); name ++);
    while (isspace(*name & 255))
      *name++ = '\0';

    switch (command)
    {
      default :
      case 0x01 : /* Abort */
          status = 1;
	  break;

      case 0x02 : /* Receive control file */
          if (strlen(name) < 2)
	  {
	    syslog(LOG_ERR, "Bad control file name \"%s\"", name);
	    putchar(1);
	    status = 1;
	    break;
	  }

          if (control[0])
	  {
	   /*
	    * Append to the existing control file - the LPD spec is
	    * not entirely clear, but at least the OS/2 LPD code sends
	    * multiple control files per connection...
	    */

	    if ((fd = open(control, O_WRONLY)) < 0)
	    {
	      syslog(LOG_ERR,
	             "Unable to append to temporary control file \"%s\" - %s",
        	     control, strerror(errno));
	      putchar(1);
	      status = 1;
	      break;
	    }

	    lseek(fd, 0, SEEK_END);
          }
	  else
	  {
	    if ((fd = cupsTempFd(control, sizeof(control))) < 0)
	    {
	      syslog(LOG_ERR, "Unable to open temporary control file \"%s\" - %s",
        	     control, strerror(errno));
	      putchar(1);
	      status = 1;
	      break;
	    }

	    strcpy(filename, control);
	  }
	  break;

      case 0x03 : /* Receive data file */
          if (strlen(name) < 2)
	  {
	    syslog(LOG_ERR, "Bad data file name \"%s\"", name);
	    putchar(1);
	    status = 1;
	    break;
	  }

          if (num_data >= (sizeof(data) / sizeof(data[0])))
	  {
	   /*
	    * Too many data files...
	    */

	    syslog(LOG_ERR, "Too many data files (%d)", num_data);
	    putchar(1);
	    status = 1;
	    break;
	  }

	  strlcpy(data[num_data], name, sizeof(data[0]));

          if ((fd = cupsTempFd(temp[num_data], sizeof(temp[0]))) < 0)
	  {
	    syslog(LOG_ERR, "Unable to open temporary data file \"%s\" - %s",
        	   temp[num_data], strerror(errno));
	    putchar(1);
	    status = 1;
	    break;
	  }

	  strcpy(filename, temp[num_data]);

          num_data ++;
	  break;
    }

    putchar(status);

    if (status)
      break;

   /*
    * Copy the data or control file from the client...
    */

    for (i = atoi(count); i > 0; i -= bytes)
    {
      if (i > sizeof(line))
        bytes = sizeof(line);
      else
        bytes = i;

      if ((bytes = fread(line, 1, bytes, stdin)) > 0)
        bytes = write(fd, line, bytes);

      if (bytes < 1)
      {
	syslog(LOG_ERR, "Error while reading file - %s",
               strerror(errno));
        status = 1;
	break;
      }
    }

   /*
    * Read trailing nul...
    */

    if (!status)
    {
      if (fread(line, 1, 1, stdin) < 1)
      {
        status = 1;
	syslog(LOG_ERR, "Error while reading trailing nul - %s",
               strerror(errno));
      }
      else if (line[0])
      {
        status = 1;
	syslog(LOG_ERR, "Trailing character after file is not nul (%02X)!",
	       line[0]);
      }
    }

   /*
    * Close the file and send an acknowledgement...
    */

    close(fd);

    putchar(status);

    if (status)
      break;
  }

  if (!status)
  {
   /*
    * Process the control file and print stuff...
    */

    if ((fp = fopen(control, "rb")) == NULL)
      status = 1;
    else
    {
     /*
      * Grab the job information first...
      */

      title[0]   = '\0';
      user[0]    = '\0';
      docname[0] = '\0';
      banner     = 0;

      while (smart_gets(line, sizeof(line), fp) != NULL)
      {
       /*
        * Process control lines...
	*/

	switch (line[0])
	{
	  case 'J' : /* Job name */
	      strlcpy(title, line + 1, sizeof(title));
	      break;

	  case 'N' : /* Document name */
	      strlcpy(docname, line + 1, sizeof(docname));
	      break;

	  case 'P' : /* User identification */
	      strlcpy(user, line + 1, sizeof(user));
	      break;

	  case 'L' : /* Print banner page */
	      banner = 1;
	      break;
	}

	if (status)
	  break;
      }

     /*
      * Then print the jobs...
      */

      rewind(fp);

      while (smart_gets(line, sizeof(line), fp) != NULL)
      {
       /*
        * Process control lines...
	*/

	switch (line[0])
	{
	  case 'c' : /* Plot CIF file */
	  case 'd' : /* Print DVI file */
	  case 'f' : /* Print formatted file */
	  case 'g' : /* Plot file */
	  case 'l' : /* Print file leaving control characters (raw) */
	  case 'n' : /* Print ditroff output file */
	  case 'o' : /* Print PostScript output file */
	  case 'p' : /* Print file with 'pr' format (prettyprint) */
	  case 'r' : /* File to print with FORTRAN carriage control */
	  case 't' : /* Print troff output file */
	  case 'v' : /* Print raster file */
	     /*
	      * Check that we have a username...
	      */

	      if (!user[0])
	      {
		syslog(LOG_WARNING, "No username specified by client! "
		                    "Using \"anonymous\"...");
		strcpy(user, "anonymous");
	      }

             /*
	      * Copy the default options...
	      */

              num_options = 0;
	      options     = NULL;

	      for (i = 0; i < destptr->num_options; i ++)
	        num_options = cupsAddOption(destptr->options[i].name,
		                            destptr->options[i].value,
		                            num_options, &options);
	      for (i = 0; i < num_defaults; i ++)
	        num_options = cupsAddOption(defaults[i].name,
		                            defaults[i].value,
		                            num_options, &options);

             /*
	      * If a banner was requested and it's not overridden by a
	      * command line option and the destination's default is none
	      * then add the standard banner...
	      */

              if (banner &&
	          cupsGetOption("job-sheets", num_defaults, defaults) == NULL &&
                  ((job_sheets = cupsGetOption("job-sheets",
		                               destptr->num_options,
					       destptr->options)) == NULL ||
                   !strcmp(job_sheets, "none,none")))
	      {
	        num_options = cupsAddOption("job-sheets", "standard",
		                            num_options, &options);
	      }

             /*
	      * Add additional options as needed...
	      */

	      if (line[0] == 'l')
	        num_options = cupsAddOption("raw", "", num_options, &options);

              if (line[0] == 'p')
	        num_options = cupsAddOption("prettyprint", "", num_options,
		                            &options);

             /*
	      * Figure out which file we are printing...
	      */

	      for (i = 0; i < num_data; i ++)
	        if (strcmp(data[i], line + 1) == 0)
		  break;

              if (i >= num_data)
	      {
	        status = 1;
		break;
	      }

             /*
	      * Send the print request...
	      */

              if (print_file(queue, temp[i], title, docname, user, num_options,
	                     options) == 0)
                status = 1;
	      else
	        status = 0;

              cupsFreeOptions(num_options, options);
	      break;
	}

	if (status)
	  break;
      }

      fclose(fp);
    }
  }

 /*
  * Clean up all temporary files and return...
  */

  unlink(control);

  for (i = 0; i < num_data; i ++)
    unlink(temp[i]);

  cupsFreeDests(num_dests, dests);

  return (status);
}


/*
 * 'remove_jobs()' - Cancel one or more jobs.
 */

int					/* O - Command status */
remove_jobs(const char *dest,		/* I - Destination */
            const char *agent,		/* I - User agent */
	    const char *list)		/* I - List of jobs or users */
{
  int		id;			/* Job ID */
  http_t	*http;			/* HTTP server connection */
  ipp_t		*request,		/* IPP Request */
		*response;		/* IPP Response */
  cups_lang_t	*language;		/* Default language */
  char		uri[HTTP_MAX_URI];	/* Job URI */


  (void)dest;	/* Suppress compiler warnings... */

 /*
  * Try connecting to the local server...
  */

  if ((http = httpConnectEncrypt(cupsServer(), ippPort(),
                                 cupsEncryption())) == NULL)
  {
    syslog(LOG_ERR, "Unable to connect to server %s: %s", cupsServer(),
           strerror(errno));
    return (1);
  }

  language = cupsLangDefault();

 /*
  * Loop for each job...
  */

  while ((id = atoi(list)) > 0)
  {
   /*
    * Skip job ID in list...
    */

    while (isdigit(*list & 255))
      list ++;
    while (isspace(*list & 255))
      list ++;

   /*
    * Build an IPP_CANCEL_JOB request, which requires the following
    * attributes:
    *
    *    attributes-charset
    *    attributes-natural-language
    *    job-uri
    *    requesting-user-name
    */

    request = ippNew();

    request->request.op.operation_id = IPP_CANCEL_JOB;
    request->request.op.request_id   = 1;

    ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_CHARSET,
        	 "attributes-charset", NULL, cupsLangEncoding(language));

    ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_LANGUAGE,
        	 "attributes-natural-language", NULL, language->language);

    sprintf(uri, "ipp://localhost/jobs/%d", id);
    ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_URI, "job-uri", NULL, uri);

    ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_NAME,
                 "requesting-user-name", NULL, agent);

   /*
    * Do the request and get back a response...
    */

    if ((response = cupsDoRequest(http, request, "/jobs")) != NULL)
    {
      if (response->request.status.status_code > IPP_OK_CONFLICT)
      {
	syslog(LOG_WARNING, "Cancel of job ID %d failed: %s\n", id,
               ippErrorString(response->request.status.status_code));
	ippDelete(response);
	cupsLangFree(language);
	httpClose(http);
	return (1);
      }
      else
        syslog(LOG_INFO, "Job ID %d cancelled", id);

      ippDelete(response);
    }
    else
    {
      syslog(LOG_WARNING, "Cancel of job ID %d failed: %s\n", id,
             ippErrorString(cupsLastError()));
      cupsLangFree(language);
      httpClose(http);
      return (1);
    }
  }

  cupsLangFree(language);
  httpClose(http);

  return (0);
}


/*
 * 'send_state()' - Send the queue state.
 */

int					/* O - Command status */
send_state(const char *dest,		/* I - Destination */
           const char *list,		/* I - Job or user */
	   int        longstatus)	/* I - List of jobs or users */
{
  int		id;			/* Job ID from list */
  http_t	*http;			/* HTTP server connection */
  ipp_t		*request,		/* IPP Request */
		*response;		/* IPP Response */
  ipp_attribute_t *attr;		/* Current attribute */
  cups_lang_t	*language;		/* Default language */
  ipp_pstate_t	state;			/* Printer state */
  const char	*jobdest,		/* Pointer into job-printer-uri */
		*jobuser,		/* Pointer to job-originating-user-name */
		*jobname;		/* Pointer to job-name */
  ipp_jstate_t	jobstate;		/* job-state */
  int		jobid,			/* job-id */
		jobsize,		/* job-k-octets */
		jobcount,		/* Number of jobs */
		jobcopies,		/* Number of copies */
		rank;			/* Rank of job */
  char		rankstr[255];		/* Rank string */
  char		namestr[1024];		/* Job name string */
  char		uri[HTTP_MAX_URI];	/* Printer URI */
  char		queue[256],		/* Printer/class queue */
		*instance;		/* Printer/class instance */
  static const char * const ranks[10] =	/* Ranking strings */
		{
		  "th",
		  "st",
		  "nd",
		  "rd",
		  "th",
		  "th",
		  "th",
		  "th",
		  "th",
		  "th"
		};
  static const char * const requested[] =
		{			/* Requested attributes */
		  "job-id",
		  "job-k-octets",
		  "job-state",
		  "job-printer-uri",
		  "job-originating-user-name",
		  "job-name",
		  "copies"
		};


 /*
  * Remove instance from destination, if any...
  */

  strlcpy(queue, dest, sizeof(queue));

  if ((instance = strrchr(queue, '/')) != NULL)
    *instance++ = '\0';

 /*
  * Try connecting to the local server...
  */

  if ((http = httpConnectEncrypt(cupsServer(), ippPort(),
                                 cupsEncryption())) == NULL)
  {
    syslog(LOG_ERR, "Unable to connect to server %s: %s", cupsServer(),
           strerror(errno));
    printf("Unable to connect to server %s: %s", cupsServer(), strerror(errno));
    return (1);
  }

 /*
  * Build an IPP_GET_PRINTER_ATTRIBUTES request, which requires the following
  * attributes:
  *
  *    attributes-charset
  *    attributes-natural-language
  *    printer-uri
  */

  request = ippNew();

  request->request.op.operation_id = IPP_GET_PRINTER_ATTRIBUTES;
  request->request.op.request_id   = 1;

  language = cupsLangDefault();

  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_CHARSET,
               "attributes-charset", NULL, cupsLangEncoding(language));

  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_LANGUAGE,
               "attributes-natural-language", NULL, language->language);

  snprintf(uri, sizeof(uri), "ipp://localhost/printers/%s", queue);
  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_URI,
               "printer-uri", NULL, uri);

  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_KEYWORD,
               "requested-attributes", NULL, "printer-state");

 /*
  * Do the request and get back a response...
  */

  if ((response = cupsDoRequest(http, request, "/")) != NULL)
  {
    if (response->request.status.status_code > IPP_OK_CONFLICT)
    {
      syslog(LOG_WARNING, "Unable to get printer list: %s\n",
             ippErrorString(response->request.status.status_code));
      printf("Unable to get printer list: %s\n",
             ippErrorString(response->request.status.status_code));
      ippDelete(response);
      return (1);
    }

    if ((attr = ippFindAttribute(response, "printer-state", IPP_TAG_ENUM)) != NULL)
      state = (ipp_pstate_t)attr->values[0].integer;
    else
      state = IPP_PRINTER_STOPPED;

    switch (state)
    {
      case IPP_PRINTER_IDLE :
          printf("%s is ready\n", dest);
	  break;
      case IPP_PRINTER_PROCESSING :
          printf("%s is ready and printing\n", dest);
	  break;
      case IPP_PRINTER_STOPPED :
          printf("%s is not ready\n", dest);
	  break;
    }

    ippDelete(response);
  }
  else
  {
    syslog(LOG_WARNING, "Unable to get printer list: %s\n",
           ippErrorString(cupsLastError()));
    printf("Unable to get printer list: %s\n",
           ippErrorString(cupsLastError()));
    return (1);
  }

 /*
  * Build an IPP_GET_JOBS or IPP_GET_JOB_ATTRIBUTES request, which requires
  * the following attributes:
  *
  *    attributes-charset
  *    attributes-natural-language
  *    job-uri or printer-uri
  */

  id = atoi(list);

  request = ippNew();

  request->request.op.operation_id = id ? IPP_GET_JOB_ATTRIBUTES : IPP_GET_JOBS;
  request->request.op.request_id   = 1;

  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_CHARSET,
               "attributes-charset", NULL, cupsLangEncoding(language));

  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_LANGUAGE,
               "attributes-natural-language", NULL, language->language);

  snprintf(uri, sizeof(uri), "ipp://localhost/printers/%s", queue);

  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_URI, "printer-uri",
               NULL, uri);

  if (id)
    ippAddInteger(request, IPP_TAG_OPERATION, IPP_TAG_INTEGER, "job-id", id);
  else
  {
    ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_NAME,
                 "requesting-user-name", NULL, list);
    ippAddBoolean(request, IPP_TAG_OPERATION, "my-jobs", 1);
  }

  ippAddStrings(request, IPP_TAG_OPERATION, IPP_TAG_KEYWORD,
                "requested-attributes", sizeof(requested) / sizeof(requested[0]),
		NULL, requested);

 /*
  * Do the request and get back a response...
  */

  jobcount = 0;

  if ((response = cupsDoRequest(http, request, "/")) != NULL)
  {
    if (response->request.status.status_code > IPP_OK_CONFLICT)
    {
      printf("get-jobs failed: %s\n",
             ippErrorString(response->request.status.status_code));
      ippDelete(response);
      return (1);
    }

    rank = 1;

   /*
    * Loop through the job list and display them...
    */

    for (attr = response->attrs; attr != NULL; attr = attr->next)
    {
     /*
      * Skip leading attributes until we hit a job...
      */

      while (attr != NULL &&
             (attr->group_tag != IPP_TAG_JOB || attr->name == NULL))
        attr = attr->next;

      if (attr == NULL)
        break;

     /*
      * Pull the needed attributes from this job...
      */

      jobid       = 0;
      jobsize     = 0;
      jobstate    = IPP_JOB_PENDING;
      jobname     = "untitled";
      jobuser     = NULL;
      jobdest     = NULL;
      jobcopies   = 1;

      while (attr != NULL && attr->group_tag == IPP_TAG_JOB)
      {
        if (strcmp(attr->name, "job-id") == 0 &&
	    attr->value_tag == IPP_TAG_INTEGER)
	  jobid = attr->values[0].integer;

        if (strcmp(attr->name, "job-k-octets") == 0 &&
	    attr->value_tag == IPP_TAG_INTEGER)
	  jobsize = attr->values[0].integer * 1024;

        if (strcmp(attr->name, "job-state") == 0 &&
	    attr->value_tag == IPP_TAG_ENUM)
	  jobstate = (ipp_jstate_t)attr->values[0].integer;

        if (strcmp(attr->name, "job-printer-uri") == 0 &&
	    attr->value_tag == IPP_TAG_URI)
	  if ((jobdest = strrchr(attr->values[0].string.text, '/')) != NULL)
	    jobdest ++;

        if (strcmp(attr->name, "job-originating-user-name") == 0 &&
	    attr->value_tag == IPP_TAG_NAME)
	  jobuser = attr->values[0].string.text;

        if (strcmp(attr->name, "job-name") == 0 &&
	    attr->value_tag == IPP_TAG_NAME)
	  jobname = attr->values[0].string.text;

        if (strcmp(attr->name, "copies") == 0 &&
	    attr->value_tag == IPP_TAG_INTEGER)
	  jobcopies = attr->values[0].integer;

        attr = attr->next;
      }

     /*
      * See if we have everything needed...
      */

      if (jobdest == NULL || jobid == 0)
      {
        if (attr == NULL)
	  break;
	else
          continue;
      }

      if (!longstatus && jobcount == 0)
	puts("Rank    Owner   Job     File(s)                         Total Size");

      jobcount ++;

     /*
      * Display the job...
      */

      if (jobstate == IPP_JOB_PROCESSING)
	strcpy(rankstr, "active");
      else
      {
	snprintf(rankstr, sizeof(rankstr), "%d%s", rank, ranks[rank % 10]);
	rank ++;
      }

      if (longstatus)
      {
        puts("");

        if (jobcopies > 1)
	  snprintf(namestr, sizeof(namestr), "%d copies of %s", jobcopies,
	           jobname);
	else
	  strlcpy(namestr, jobname, sizeof(namestr));

        printf("%s: %-33.33s [job %d localhost]\n", jobuser, rankstr, jobid);
        printf("        %-39.39s %d bytes\n", namestr, jobsize);
      }
      else
        printf("%-7s %-7.7s %-7d %-31.31s %d bytes\n", rankstr, jobuser,
	       jobid, jobname, jobsize);

      if (attr == NULL)
        break;
    }

    ippDelete(response);
  }
  else
  {
    printf("get-jobs failed: %s\n", ippErrorString(cupsLastError()));
    return (1);
  }

  if (jobcount == 0)
    puts("no entries");

  cupsLangFree(language);
  httpClose(http);

  return (0);
}


/*
 * 'smart_gets()' - Get a line of text, removing the trailing CR and/or LF.
 */

char *					/* O - Line read or NULL */
smart_gets(char *s,			/* I - Pointer to line buffer */
           int  len,			/* I - Size of line buffer */
	   FILE *fp)			/* I - File to read from */
{
  char	*ptr,				/* Pointer into line */
	*end;				/* End of line */
  int	ch;				/* Character from file */


 /*
  * Read the line; unlike fgets(), we read the entire line but dump
  * characters that go past the end of the buffer.  Also, we accept
  * CR, LF, or CR LF for the line endings to be "safe", although
  * RFC 1179 specifically says "just use LF".
  */

  ptr = s;
  end = s + len - 1;

  while ((ch = getc(fp)) != EOF)
  {
    if (ch == '\n')
      break;
    else if (ch == '\r')
    {
     /*
      * See if a LF follows...
      */

      ch = getc(fp);

      if (ch != '\n')
        ungetc(ch, fp);

      break;
    }
    else if (ptr < end)
      *ptr++ = ch;
  }

  *ptr = '\0';

  if (ch == EOF && ptr == s)
    return (NULL);
  else
    return (s);
}


/*
 * End of "$Id$".
 */
