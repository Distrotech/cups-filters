/*
 * "$Id: lpmove.c,v 1.4 2001/01/22 15:04:02 mike Exp $"
 *
 *   "lpmove" command for the Common UNIX Printing System (CUPS).
 *
 *   Copyright 1997-2001 by Easy Software Products.
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
 *   main()     - Parse options and move jobs.
 *   move_job() - Move a job.
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

static void	move_job(http_t *, int, const char *);


/*
 * 'main()' - Parse options and show status information.
 */

int
main(int  argc,			/* I - Number of command-line arguments */
     char *argv[])		/* I - Command-line arguments */
{
  int		i;		/* Looping var */
  http_t	*http;		/* Connection to server */
  const char	*job;		/* Job name */
  const char	*dest;		/* New destination */


  http = NULL;
  job  = NULL;
  dest = NULL;

  for (i = 1; i < argc; i ++)
    if (argv[i][0] == '-')
      switch (argv[i][1])
      {
        case 'h' : /* Connect to host */
	    if (http)
	      httpClose(http);

	    if (argv[i][2] != '\0')
	      http = httpConnect(argv[i] + 2, ippPort());
	    else
	    {
	      i ++;

	      if (i >= argc)
	      {
	        fputs("Error: need hostname after \'-h\' option!\n", stderr);
		return (1);
              }

	      http = httpConnect(argv[i], ippPort());
	    }

	    if (http == NULL)
	    {
	      perror("lpmove: Unable to connect to server");
	      return (1);
	    }
	    break;

	default :
	    fprintf(stderr, "lpmove: Unknown option \'%c\'!\n", argv[i][1]);
	    return (1);
      }
    else if (job == NULL)
    {
      if ((job = strrchr(argv[i], '-')) != NULL)
        job ++;
      else
        job = argv[i];
    }
    else if (dest == NULL)
      dest = argv[i];
    else
    {
      fprintf(stderr, "lpmove: Unknown argument \'%s\'!\n", argv[i]);
      return (1);
    }

  if (job == NULL || dest == NULL)
  {
    puts("Usage: lpmove job dest");
    return (1);
  }

  if (!http)
  {
    http = httpConnect(cupsServer(), ippPort());

    if (http == NULL)
    {
      perror("lpmove: Unable to connect to server");
      return (1);
    }
  }

  move_job(http, atoi(job), dest);

  return (0);
}


/*
 * 'move_job()' - Move a job.
 */

static void
move_job(http_t     *http,	/* I - HTTP connection to server */
         int        jobid,	/* I - Job ID */
	 const char *dest)	/* I - Destination */
{
  ipp_t		*request,	/* IPP Request */
		*response;	/* IPP Response */
  cups_lang_t	*language;	/* Default language */
  char		job_uri[HTTP_MAX_URI],
				/* job-uri */
		printer_uri[HTTP_MAX_URI];
				/* job-printer-uri */


  if (http == NULL)
    return;

 /*
  * Build a CUPS_MOVE_JOB request, which requires the following
  * attributes:
  *
  *    attributes-charset
  *    attributes-natural-language
  *    job-uri
  *    job-printer-uri
  */

  request = ippNew();

  request->request.op.operation_id = CUPS_MOVE_JOB;
  request->request.op.request_id   = 1;

  language = cupsLangDefault();

  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_CHARSET,
               "attributes-charset", NULL, cupsLangEncoding(language));

  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_LANGUAGE,
               "attributes-natural-language", NULL, language->language);

  snprintf(job_uri, sizeof(job_uri), "ipp://localhost/jobs/%d", jobid);
  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_URI, "job-uri", NULL, job_uri);

  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_NAME, "requesting-user-name",
               NULL, cupsUser());

  snprintf(printer_uri, sizeof(printer_uri), "ipp://localhost/printers/%s", dest);
  ippAddString(request, IPP_TAG_JOB, IPP_TAG_URI, "job-printer-uri",
               NULL, printer_uri);

 /*
  * Do the request and get back a response...
  */

  if ((response = cupsDoRequest(http, request, "/jobs")) != NULL)
  {
    if (response->request.status.status_code > IPP_OK_CONFLICT)
    {
      fprintf(stderr, "lpmove: move-job failed: %s\n",
              ippErrorString(response->request.status.status_code));
      ippDelete(response);
      return;
    }

    ippDelete(response);
  }
  else
    fprintf(stderr, "lpmove: move-job failed: %s\n",
            ippErrorString(cupsLastError()));
}


/*
 * End of "$Id: lpmove.c,v 1.4 2001/01/22 15:04:02 mike Exp $".
 */
