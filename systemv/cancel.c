/*
 * "$Id: cancel.c,v 1.8 1999/07/30 15:36:00 mike Exp $"
 *
 *   "cancel" command for the Common UNIX Printing System (CUPS).
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
 *   main() - Parse options and cancel jobs.
 */

/*
 * Include necessary headers...
 */

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>

#include <cups/cups.h>
#include <cups/language.h>


/*
 * 'main()' - Parse options and cancel jobs.
 */

int				/* O - Exit status */
main(int  argc,			/* I - Number of command-line arguments */
     char *argv[])		/* I - Command-line arguments */
{
  http_t	*http;		/* HTTP connection to server */
  int		i;		/* Looping var */
  int		job_id;		/* Job ID */
  const char	*dest,		/* Destination printer */
		*host;		/* Host name */
  char		name[255];	/* Printer name */
  char		uri[1024];	/* Printer or job URI */
  ipp_t		*request;	/* IPP request */
  ipp_t		*response;	/* IPP response */
  ipp_op_t	op;		/* Operation */
  cups_lang_t	*language;	/* Language */


 /*
  * Setup to cancel individual print jobs...
  */

  op     = IPP_CANCEL_JOB;
  job_id = 0;
  dest   = NULL;

 /*
  * Open a connection to the server...
  */

  if ((http = httpConnect(cupsServer(), ippPort())) == NULL)
  {
    fputs("cancel: Unable to contact server!\n", stderr);
    return (1);
  }

 /*
  * Process command-line arguments...
  */

  for (i = 1; i < argc; i ++)
    if (argv[i][0] == '-')
      switch (argv[i][1])
      {
        case 'a' : /* Cancel all jobs */
	    op = IPP_PURGE_JOBS;
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
	      perror("cancel: Unable to connect to server");
	      return (1);
	    }
	    break;

	default :
	    fprintf(stderr, "cancel: Unknown option \'%c\'!\n", argv[i][1]);
	    return (1);
      }
    else
    {
     /*
      * Cancel a job or printer...
      */

      if (isdigit(argv[i][0]))
      {
        dest   = NULL;
	op     = IPP_CANCEL_JOB;
        job_id = atoi(argv[i]);
      }
      else
      {
	dest   = name;
        job_id = 0;
	sscanf(argv[i], "%[^-]-%d", name, &job_id);
	if (job_id)
	  op = IPP_CANCEL_JOB;

        if ((host = strchr(name, '@')) != NULL)
	{
	 /*
	  * Reconnect to the named host...
	  */

          httpClose(http);

	  *host++ = '\0';

	  if ((http = httpConnect(host, ippPort())) == NULL)
	  {
	    perror("cancel: Unable to connect to server");
	    return (1);
	  }
	}
      }

     /*
      * Build an IPP request, which requires the following
      * attributes:
      *
      *    attributes-charset
      *    attributes-natural-language
      *    printer-uri + job-id *or* job-uri
      */

      request = ippNew();

      request->request.op.operation_id = op;
      request->request.op.request_id   = 1;

      language = cupsLangDefault();

      ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_CHARSET,
              	   "attributes-charset", NULL, cupsLangEncoding(language));

      ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_LANGUAGE,
                   "attributes-natural-language", NULL, language->language);

      if (dest)
      {
        sprintf(uri, "ipp://localhost/printers/%s", dest);
	ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_URI,
	             "printer-uri", NULL, uri);
	ippAddInteger(request, IPP_TAG_OPERATION, IPP_TAG_INTEGER, "job-id",
	              job_id);
      }
      else
      {
        sprintf(uri, "ipp://localhost/jobs/%d", job_id);
	ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_URI, "job-uri", NULL,
	             uri);
      }

     /*
      * Do the request and get back a response...
      */

      if (op == IPP_PURGE_JOBS)
        response = cupsDoRequest(http, request, "/admin/");
      else
        response = cupsDoRequest(http, request, "/jobs/");

      if (response != NULL)
      {
        if (response->request.status.status_code == IPP_NOT_FOUND)
          fputs("cancel: Job or printer not found!\n", stderr);
        else if (response->request.status.status_code > IPP_OK_CONFLICT)
          fputs("cancel: Unable to cancel job(s)!\n", stderr);

        ippDelete(response);
      }
      else
      {
        fputs("cancel: Unable to cancel job(s)!\n", stderr);
	return (1);
      }
    }

  return (0);
}


/*
 * End of "$Id: cancel.c,v 1.8 1999/07/30 15:36:00 mike Exp $".
 */
