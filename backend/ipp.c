/*
 * "$Id: ipp.c,v 1.38.2.8 2002/03/01 19:55:08 mike Exp $"
 *
 *   IPP backend for the Common UNIX Printing System (CUPS).
 *
 *   Copyright 1997-2002 by Easy Software Products, all rights reserved.
 *
 *   These coded instructions, statements, and computer programs are the
 *   property of Easy Software Products and are protected by Federal
 *   copyright law.  Distribution and use rights are outlined in the file
 *   "LICENSE" which should have been included with this file.  If this
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
 *   main()                 - Send a file to the printer or server.
 *   password_cb()          - Disable the password prompt for
 *                            cupsDoFileRequest().
 *   report_printer_state() - Report the printer state.
 */

/*
 * Include necessary headers.
 */

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <cups/cups.h>
#include <cups/language.h>
#include <cups/string.h>
#include <signal.h>


/*
 * Local functions...
 */

const char	*password_cb(const char *);
int		report_printer_state(ipp_t *ipp);


/*
 * Local globals...
 */

char	*password = NULL;


/*
 * 'main()' - Send a file to the printer or server.
 *
 * Usage:
 *
 *    printer-uri job-id user title copies options [file]
 */

int			/* O - Exit status */
main(int  argc,		/* I - Number of command-line arguments (6 or 7) */
     char *argv[])	/* I - Command-line arguments */
{
  int		i;		/* Looping var */
  int		num_options;	/* Number of printer options */
  cups_option_t	*options;	/* Printer options */
  char		method[255],	/* Method in URI */
		hostname[1024],	/* Hostname */
		username[255],	/* Username info */
		resource[1024],	/* Resource info (printer name) */
		filename[1024];	/* File to print */
  int		port;		/* Port number (not used) */
  char		uri[HTTP_MAX_URI];/* Updated URI without user/pass */
  ipp_status_t	ipp_status;	/* Status of IPP request */
  http_t	*http;		/* HTTP connection */
  ipp_t		*request,	/* IPP request */
		*response,	/* IPP response */
		*supported;	/* get-printer-attributes response */
  ipp_attribute_t *job_id_attr;	/* job-id attribute */
  int		job_id;		/* job-id value */
  ipp_attribute_t *job_state;	/* job-state attribute */
  ipp_attribute_t *copies_sup;	/* copies-supported attribute */
  ipp_attribute_t *charset_sup;	/* charset-supported attribute */
  ipp_attribute_t *format_sup;	/* document-format-supported attribute */
  const char	*charset;	/* Character set to use */
  cups_lang_t	*language;	/* Default language */
  int		copies;		/* Number of copies remaining */
  const char	*content_type;	/* CONTENT_TYPE environment variable */
#if defined(HAVE_SIGACTION) && !defined(HAVE_SIGSET)
  struct sigaction action;	/* Actions for POSIX signals */
#endif /* HAVE_SIGACTION && !HAVE_SIGSET */
  int		version;	/* IPP version */
  int		reasons;	/* Number of printer-state-reasons shown */


 /*
  * Make sure status messages are not buffered...
  */

  setbuf(stderr, NULL);

 /*
  * Check command-line...
  */

  if (argc == 1)
  {
    char *s;

    if ((s = strrchr(argv[0], '/')) != NULL)
      s ++;
    else
      s = argv[0];

    printf("network %s \"Unknown\" \"Internet Printing Protocol (%s)\"\n", s, s);
    return (0);
  }
  else if (argc < 6 || argc > 7)
  {
    fprintf(stderr, "Usage: %s job-id user title copies options [file]\n",
            argv[0]);
    return (1);
  }

 /*
  * If we have 7 arguments, print the file named on the command-line.
  * Otherwise, copy stdin to a temporary file and print the temporary
  * file.
  */

  if (argc == 6)
  {
   /*
    * Copy stdin to a temporary file...
    */

    int  fd;		/* Temporary file */
    char buffer[8192];	/* Buffer for copying */
    int  bytes;		/* Number of bytes read */


    if ((fd = cupsTempFd(filename, sizeof(filename))) < 0)
    {
      perror("ERROR: unable to create temporary file");
      return (1);
    }

    while ((bytes = fread(buffer, 1, sizeof(buffer), stdin)) > 0)
      if (write(fd, buffer, bytes) < bytes)
      {
        perror("ERROR: unable to write to temporary file");
	close(fd);
	unlink(filename);
	return (1);
      }

    close(fd);
  }
  else
  {
    strncpy(filename, argv[6], sizeof(filename) - 1);
    filename[sizeof(filename) - 1] = '\0';
  }

 /*
  * Extract the hostname and printer name from the URI...
  */

  httpSeparate(argv[0], method, username, hostname, &port, resource);

 /*
  * Set the authentication info, if any...
  */

  cupsSetPasswordCB(password_cb);

  if (username[0])
  {
    if ((password = strchr(username, ':')) != NULL)
      *password++ = '\0';

    cupsSetUser(username);
  }

 /*
  * Try connecting to the remote server...
  */

  do
  {
    fprintf(stderr, "INFO: Connecting to %s...\n", hostname);

    if ((http = httpConnect(hostname, port)) == NULL)
    {
      if (errno == ECONNREFUSED)
      {
	fprintf(stderr, "INFO: Network host \'%s\' is busy; will retry in 30 seconds...",
                hostname);
	sleep(30);
      }
      else
      {
	perror("ERROR: Unable to connect to IPP host");
	sleep(30);
      }
    }
  }
  while (http == NULL);

 /*
  * Build a URI for the printer and fill the standard IPP attributes for
  * an IPP_PRINT_FILE request.  We can't use the URI in argv[0] because it
  * might contain username:password information...
  */

  snprintf(uri, sizeof(uri), "%s://%s:%d%s", method, hostname, port, resource);

 /*
  * First validate the destination and see if the device supports multiple
  * copies.  We have to do this because some IPP servers (e.g. HP JetDirect)
  * don't support the copies attribute...
  */

  language    = cupsLangDefault();
  charset_sup = NULL;
  copies_sup  = NULL;
  format_sup  = NULL;
  version     = 1;
  supported   = NULL;

  do
  {
   /*
    * Build the IPP request...
    */

    request = ippNew();
    request->request.op.version[1]   = version;
    request->request.op.operation_id = IPP_GET_PRINTER_ATTRIBUTES;
    request->request.op.request_id   = 1;

    ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_CHARSET,
        	 "attributes-charset", NULL, "utf-8");

    ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_LANGUAGE,
        	 "attributes-natural-language", NULL,
        	 language != NULL ? language->language : "en");

    ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_URI, "printer-uri",
        	 NULL, uri);

   /*
    * Do the request...
    */

    if ((supported = cupsDoRequest(http, request, resource)) == NULL)
      ipp_status = cupsLastError();
    else
      ipp_status = supported->request.status.status_code;

    if (ipp_status > IPP_OK_CONFLICT)
    {
      if (ipp_status == IPP_PRINTER_BUSY ||
	  ipp_status == IPP_SERVICE_UNAVAILABLE)
      {
	fputs("INFO: Printer busy; will retry in 10 seconds...\n", stderr);
        report_printer_state(supported);
	sleep(10);
      }
      else if ((ipp_status == IPP_BAD_REQUEST ||
	        ipp_status == IPP_VERSION_NOT_SUPPORTED) && version == 1)
      {
       /*
	* Switch to IPP/1.0...
	*/

	fputs("INFO: Printer does not support IPP/1.1, trying IPP/1.0...\n", stderr);
	version = 0;
      }
      else
	fprintf(stderr, "ERROR: Unable to get printer status (%s)!\n",
	        ippErrorString(ipp_status));

      if (supported)
        ippDelete(supported);

      continue;
    }
    else if ((copies_sup = ippFindAttribute(supported, "copies-supported",
	                                    IPP_TAG_RANGE)) != NULL)
    {
     /*
      * Has the "copies-supported" attribute - does it have an upper
      * bound > 1?
      */

      if (copies_sup->values[0].range.upper <= 1)
	copies_sup = NULL; /* No */
    }

    charset_sup = ippFindAttribute(supported, "charset-supported",
	                           IPP_TAG_CHARSET);
    format_sup  = ippFindAttribute(supported, "document-format-supported",
	                           IPP_TAG_MIMETYPE);

    if (format_sup)
    {
      fprintf(stderr, "DEBUG: document-format-supported (%d values)\n",
	      format_sup->num_values);
      for (i = 0; i < format_sup->num_values; i ++)
	fprintf(stderr, "DEBUG: [%d] = \"%s\"\n", i,
	        format_sup->values[i].string.text);
    }

    report_printer_state(supported);
  }
  while (ipp_status > IPP_OK_CONFLICT);

 /*
  * Now that we are "connected" to the port, ignore SIGTERM so that we
  * can finish out any page data the driver sends (e.g. to eject the
  * current page...  Only ignore SIGTERM if we are printing data from
  * stdin (otherwise you can't cancel raw jobs...)
  */

  if (argc < 7)
  {
#ifdef HAVE_SIGSET /* Use System V signals over POSIX to avoid bugs */
    sigset(SIGTERM, SIG_IGN);
#elif defined(HAVE_SIGACTION)
    memset(&action, 0, sizeof(action));

    sigemptyset(&action.sa_mask);
    action.sa_handler = SIG_IGN;
    sigaction(SIGTERM, &action, NULL);
#else
    signal(SIGTERM, SIG_IGN);
#endif /* HAVE_SIGSET */
  }

 /*
  * See if the printer supports multiple copies...
  */

  if (copies_sup || argc < 7)
    copies = 1;
  else
    copies = atoi(argv[4]);

 /*
  * Figure out the character set to use...
  */

  charset = language ? cupsLangEncoding(language) : "us-ascii";

  if (charset_sup)
  {
   /*
    * See if IPP server supports the requested character set...
    */

    for (i = 0; i < charset_sup->num_values; i ++)
      if (strcasecmp(charset, charset_sup->values[i].string.text) == 0)
        break;

   /*
    * If not, choose us-ascii or utf-8...
    */

    if (i >= charset_sup->num_values)
    {
     /*
      * See if us-ascii is supported...
      */

      for (i = 0; i < charset_sup->num_values; i ++)
        if (strcasecmp("us-ascii", charset_sup->values[i].string.text) == 0)
          break;

      if (i < charset_sup->num_values)
        charset = "us-ascii";
      else
        charset = "utf-8";
    }
  }

 /*
  * Then issue the print-job request...
  */

  reasons = 0;

  while (copies > 0)
  {
   /*
    * Build the IPP request...
    */

    request = ippNew();
    request->request.op.version[1]   = version;
    request->request.op.operation_id = IPP_PRINT_JOB;
    request->request.op.request_id   = 1;

    ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_CHARSET,
        	 "attributes-charset", NULL, charset);

    ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_LANGUAGE,
        	 "attributes-natural-language", NULL,
        	 language != NULL ? language->language : "en");

    ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_URI, "printer-uri",
        	 NULL, uri);

    fprintf(stderr, "DEBUG: printer-uri = \"%s\"\n", uri);

    if (argv[2][0])
      ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_NAME, "requesting-user-name",
        	   NULL, argv[2]);

    fprintf(stderr, "DEBUG: requesting-user-name = \"%s\"\n", argv[2]);

    if (argv[3][0])
      ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_NAME, "job-name", NULL,
        	   argv[3]);

    fprintf(stderr, "DEBUG: job-name = \"%s\"\n", argv[3]);

   /*
    * Handle options on the command-line...
    */

    options     = NULL;
    num_options = cupsParseOptions(argv[5], 0, &options);

    if (argc > 6)
      content_type = getenv("CONTENT_TYPE");
    else
      content_type = "application/vnd.cups-raw";

    if (content_type != NULL && format_sup != NULL)
    {
      for (i = 0; i < format_sup->num_values; i ++)
        if (strcasecmp(content_type, format_sup->values[i].string.text) == 0)
          break;

      if (i < format_sup->num_values)
        num_options = cupsAddOption("document-format", content_type,
	                            num_options, &options);
    }

    if (copies_sup)
    {
     /*
      * Only send options if the destination printer supports the copies
      * attribute.  This is a hack for the HP JetDirect implementation of
      * IPP, which does not accept extension attributes and incorrectly
      * reports a client-error-bad-request error instead of the
      * successful-ok-unsupported-attributes status.  In short, at least
      * some HP implementations of IPP are non-compliant.
      */

      cupsEncodeOptions(request, num_options, options);
      ippAddInteger(request, IPP_TAG_JOB, IPP_TAG_INTEGER, "copies",
                    atoi(argv[4]));
    }

    cupsFreeOptions(num_options, options);

   /*
    * Do the request...
    */

    if ((response = cupsDoFileRequest(http, request, resource, filename)) == NULL)
      ipp_status = cupsLastError();
    else
      ipp_status = response->request.status.status_code;

    if (ipp_status > IPP_OK_CONFLICT)
    {
      job_id = 0;

      if (ipp_status == IPP_SERVICE_UNAVAILABLE ||
	  ipp_status == IPP_PRINTER_BUSY)
      {
	fputs("INFO: Printer is busy; retrying print job...\n", stderr);
	sleep(10);
      }
      else
        fprintf(stderr, "ERROR: Print file was not accepted (%s)!\n",
	        ippErrorString(ipp_status));
    }
    else if ((job_id_attr = ippFindAttribute(response, "job-id",
                                             IPP_TAG_INTEGER)) == NULL)
    {
      fputs("INFO: Print file accepted - job ID unknown.\n", stderr);
      job_id = 0;
    }
    else
    {
      job_id = job_id_attr->values[0].integer;
      fprintf(stderr, "INFO: Print file accepted - job ID %d.\n", job_id);
    }

    if (response)
      ippDelete(response);

    if (ipp_status <= IPP_OK_CONFLICT && argc > 6)
    {
      fprintf(stderr, "PAGE: 1 %d\n", copies_sup ? atoi(argv[4]) : 1);
      copies --;
    }
    else if (ipp_status != IPP_SERVICE_UNAVAILABLE &&
	     ipp_status != IPP_PRINTER_BUSY)
      break;

   /*
    * Wait for the job to complete...
    */

    if (!job_id)
      continue;

    fputs("INFO: Waiting for job to complete...\n", stderr);

    for (;;)
    {
     /*
      * Build an IPP_GET_JOB_ATTRIBUTES request...
      */

      request = ippNew();
      request->request.op.version[1]   = version;
      request->request.op.operation_id = IPP_GET_JOB_ATTRIBUTES;
      request->request.op.request_id   = 1;

      ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_CHARSET,
        	   "attributes-charset", NULL, charset);

      ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_LANGUAGE,
        	   "attributes-natural-language", NULL,
        	   language != NULL ? language->language : "en");

      ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_URI, "printer-uri",
        	   NULL, uri);

      ippAddInteger(request, IPP_TAG_OPERATION, IPP_TAG_INTEGER, "job-id",
        	    job_id);

      if (argv[2][0])
	ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_NAME, "requesting-user-name",
        	     NULL, argv[2]);

      ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_KEYWORD,
                   "requested-attributes", NULL, "job-state");

     /*
      * Do the request...
      */

      if ((response = cupsDoRequest(http, request, resource)) == NULL)
	ipp_status = cupsLastError();
      else
	ipp_status = response->request.status.status_code;

      if (ipp_status == IPP_NOT_FOUND)
      {
       /*
        * Job has gone away and/or the server has no job history...
	*/

        ippDelete(response);

	ipp_status = IPP_OK;
        break;
      }

      if (ipp_status > IPP_OK_CONFLICT)
      {
	if (ipp_status != IPP_SERVICE_UNAVAILABLE &&
	    ipp_status != IPP_PRINTER_BUSY)
	{
	  if (response)
	    ippDelete(response);

          fprintf(stderr, "ERROR: Unable to get job %d attributes (%s)!\n",
	          job_id, ippErrorString(ipp_status));
          break;
	}
      }
      else if ((job_state = ippFindAttribute(response, "job-state", IPP_TAG_ENUM)) != NULL)
      {
       /*
        * Stop polling if the job is finished or pending-held...
	*/

        if (job_state->values[0].integer > IPP_JOB_PROCESSING ||
	    job_state->values[0].integer == IPP_JOB_HELD)
	{
	  ippDelete(response);
	  break;
	}
      }

      if (response)
	ippDelete(response);


     /*
      * Now check on the printer state...
      */

      request = ippNew();
      request->request.op.version[1]   = version;
      request->request.op.operation_id = IPP_GET_PRINTER_ATTRIBUTES;
      request->request.op.request_id   = 1;

      ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_CHARSET,
        	   "attributes-charset", NULL, charset);

      ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_LANGUAGE,
        	   "attributes-natural-language", NULL,
        	   language != NULL ? language->language : "en");

      ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_URI, "printer-uri",
        	   NULL, uri);

      if (argv[2][0])
	ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_NAME, "requesting-user-name",
        	     NULL, argv[2]);

      ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_KEYWORD,
                   "requested-attributes", NULL, "printer-state-reasons");

     /*
      * Do the request...
      */

      if ((response = cupsDoRequest(http, request, resource)) != NULL)
      {
        reasons = report_printer_state(response);
	ippDelete(response);
      }

     /*
      * Wait 10 seconds before polling again...
      */

      sleep(10);
    }
  }

 /*
  * Free memory...
  */

  httpClose(http);

  if (supported)
    ippDelete(supported);

 /*
  * Close and remove the temporary file if necessary...
  */

  if (argc < 7)
    unlink(filename);

 /*
  * Return the queue status...
  */

  if (ipp_status <= IPP_OK_CONFLICT && reasons == 0)
    fputs("INFO: Ready to print.\n", stderr);

  return (ipp_status > IPP_OK_CONFLICT);
}


/*
 * 'password_cb()' - Disable the password prompt for cupsDoFileRequest().
 */

const char *			/* O - Password  */
password_cb(const char *prompt)	/* I - Prompt (not used) */
{
  (void)prompt;

  return (password);
}


/*
 * 'report_printer_state()' - Report the printer state.
 */

int					/* O - Number of reasons shown */
report_printer_state(ipp_t *ipp)	/* I - IPP response */
{
  int			i;		/* Looping var */
  int			count;		/* Count of reasons shown... */
  ipp_attribute_t	*reasons;	/* printer-state-reasons */
  const char		*message;	/* Message to show */
  char			unknown[1024];	/* Unknown message string */


  if ((reasons = ippFindAttribute(ipp, "printer-state-reasons",
                                  IPP_TAG_KEYWORD)) == NULL)
    return (0);

  for (i = 0, count = 0; i < reasons->num_values; i ++)
  {
    message = NULL;

    if (strncmp(reasons->values[i].string.text, "media-needed", 12) == 0)
      message = "Media tray needs to be filled.";
    else if (strncmp(reasons->values[i].string.text, "media-jam", 9) == 0)
      message = "Media jam!";
    else if (strncmp(reasons->values[i].string.text, "moving-to-paused", 16) == 0 ||
             strncmp(reasons->values[i].string.text, "paused", 6) == 0 ||
	     strncmp(reasons->values[i].string.text, "shutdown", 8) == 0)
      message = "Printer off-line.";
    else if (strncmp(reasons->values[i].string.text, "toner-low", 9) == 0)
      message = "Toner low.";
    else if (strncmp(reasons->values[i].string.text, "toner-empty", 11) == 0)
      message = "Out of toner!";
    else if (strncmp(reasons->values[i].string.text, "cover-open", 10) == 0)
      message = "Cover open.";
    else if (strncmp(reasons->values[i].string.text, "interlock-open", 14) == 0)
      message = "Interlock open.";
    else if (strncmp(reasons->values[i].string.text, "door-open", 9) == 0)
      message = "Door open.";
    else if (strncmp(reasons->values[i].string.text, "input-tray-missing", 18) == 0)
      message = "Media tray missing!";
    else if (strncmp(reasons->values[i].string.text, "media-low", 9) == 0)
      message = "Media tray almost empty.";
    else if (strncmp(reasons->values[i].string.text, "media-empty", 11) == 0)
      message = "Media tray empty!";
    else if (strncmp(reasons->values[i].string.text, "output-tray-missing", 19) == 0)
      message = "Output tray missing!";
    else if (strncmp(reasons->values[i].string.text, "output-area-almost-full", 23) == 0)
      message = "Output bin almost full.";
    else if (strncmp(reasons->values[i].string.text, "output-area-full", 16) == 0)
      message = "Output bin full!";
    else if (strncmp(reasons->values[i].string.text, "marker-supply-low", 17) == 0)
      message = "Ink/toner almost empty.";
    else if (strncmp(reasons->values[i].string.text, "marker-supply-empty", 19) == 0)
      message = "Ink/toner empty!";
    else if (strncmp(reasons->values[i].string.text, "marker-waste-almost-full", 24) == 0)
      message = "Ink/toner waste bin almost full.";
    else if (strncmp(reasons->values[i].string.text, "marker-waste-full", 17) == 0)
      message = "Ink/toner waste bin full!";
    else if (strncmp(reasons->values[i].string.text, "fuser-over-temp", 15) == 0)
      message = "Fuser temperature high!";
    else if (strncmp(reasons->values[i].string.text, "fuser-under-temp", 16) == 0)
      message = "Fuser temperature low!";
    else if (strncmp(reasons->values[i].string.text, "opc-near-eol", 12) == 0)
      message = "OPC almost at end-of-life.";
    else if (strncmp(reasons->values[i].string.text, "opc-life-over", 13) == 0)
      message = "OPC at end-of-life!";
    else if (strncmp(reasons->values[i].string.text, "developer-low", 13) == 0)
      message = "Developer almost empty.";
    else if (strncmp(reasons->values[i].string.text, "developer-empty", 15) == 0)
      message = "Developer empty!";
    else if (strstr(reasons->values[i].string.text, "error") != NULL)
    {
      message = unknown;

      snprintf(unknown, sizeof(unknown), "Unknown printer error (%s)!",
               reasons->values[i].string.text);
    }

    if (message)
    {
      count ++;
      if (strstr(reasons->values[i].string.text, "error"))
        fprintf(stderr, "ERROR: %s\n", message);
      else if (strstr(reasons->values[i].string.text, "warning"))
        fprintf(stderr, "WARNING: %s\n", message);
      else
        fprintf(stderr, "INFO: %s\n", message);
    }
  }

  return (count);
}



/*
 * End of "$Id: ipp.c,v 1.38.2.8 2002/03/01 19:55:08 mike Exp $".
 */
