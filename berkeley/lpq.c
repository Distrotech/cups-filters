/*
 * "$Id: lpq.c,v 1.17.2.6 2002/05/16 13:59:55 mike Exp $"
 *
 *   "lpq" command for the Common UNIX Printing System (CUPS).
 *
 *   Copyright 1997-2002 by Easy Software Products.
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
 *   main()         - Parse options and commands.
 *   show_jobs()    - Show jobs.
 *   show_printer() - Show printer status.
 */

/*
 * Include necessary headers...
 */

/*
 * Include necessary headers...
 */

#include <stdio.h>
#include <stdlib.h>
#include <cups/string.h>
#include <cups/cups.h>
#include <cups/language.h>
#include <cups/debug.h>


/*
 * Local functions...
 */

static int	show_jobs(http_t *, const char *, const char *, const int,
		          const int);
static void	show_printer(http_t *, const char *);


/*
 * 'main()' - Parse options and commands.
 */

int
main(int  argc,		/* I - Number of command-line arguments */
     char *argv[])	/* I - Command-line arguments */
{
  int		i;		/* Looping var */
  http_t	*http;		/* Connection to server */
  const char	*dest,		/* Desired printer */
		*user;		/* Desired user */
  char		*instance;	/* Printer instance */
  int		id,		/* Desired job ID */
		interval,	/* Reporting interval */
		longstatus;	/* Show file details */
  int		num_dests;	/* Number of destinations */
  cups_dest_t	*dests;		/* Destinations */
#ifdef HAVE_LIBSSL
  http_encryption_t encryption;	/* Encryption? */
#endif /* HAVE_LIBSSL */


 /*
  * Connect to the scheduler...
  */

  if ((http = httpConnectEncrypt(cupsServer(), ippPort(),
                                 cupsEncryption())) == NULL)
  {
    fputs("lpq: Unable to contact server!\n", stderr);
    return (1);
  }

 /*
  * Check for command-line options...
  */

  dest       = NULL;
  user       = NULL;
  id         = 0;
  interval   = 0;
  longstatus = 0;

  num_dests = cupsGetDests(&dests);

  for (i = 0; i < num_dests; i ++)
    if (dests[i].is_default)
      dest = dests[i].name;

  for (i = 1; i < argc; i ++)
    if (argv[i][0] == '+')
      interval = atoi(argv[i] + 1);
    else if (argv[i][0] == '-')
    {
      switch (argv[i][1])
      {
        case 'E' : /* Encrypt */
#ifdef HAVE_LIBSSL
	    encryption = HTTP_ENCRYPT_REQUIRED;

	    if (http)
	      httpEncryption(http, encryption);
#else
            fprintf(stderr, "%s: Sorry, no encryption support compiled in!\n",
	            argv[0]);
#endif /* HAVE_LIBSSL */
	    break;

        case 'P' : /* Printer */
	    if (argv[i][2])
	      dest = argv[i] + 2;
	    else
	    {
	      i ++;
	      dest = argv[i];
	    }

	    if ((instance = strchr(dest, '/')) != NULL)
	      *instance = '\0';
	    break;

	case 'a' : /* All printers */
	    dest = NULL;
	    break;

	case 'l' : /* Long status */
	    longstatus = 1;
	    break;

	default :
	    fputs("Usage: lpq [-P dest] [-l] [+interval]\n", stderr);
	    httpClose(http);
            cupsFreeDests(num_dests, dests);
	    return (1);
      }
    }
    else if (isdigit(argv[i][0]))
      id = atoi(argv[i]);
    else
      user = argv[i];

 /*
  * Show the status in a loop...
  */

  for (;;)
  {
    if (dest)
      show_printer(http, dest);

    i = show_jobs(http, dest, user, id, longstatus);

    if (i && interval)
    {
      fflush(stdout);
      sleep(interval);
    }
    else
      break;
  }

 /*
  * Close the connection to the server and return...
  */

  cupsFreeDests(num_dests, dests);
  httpClose(http);

  return (0);
}


/*
 * 'show_jobs()' - Show jobs.
 */

static int			/* O - Number of jobs in queue */
show_jobs(http_t     *http,	/* I - HTTP connection to server */
          const char *dest,	/* I - Destination */
	  const char *user,	/* I - User */
	  const int  id,	/* I - Job ID */
	  const int  longstatus)/* I - 1 if long report desired */
{
  ipp_t		*request,	/* IPP Request */
		*response;	/* IPP Response */
  ipp_attribute_t *attr;	/* Current attribute */
  cups_lang_t	*language;	/* Default language */
  const char	*jobdest,	/* Pointer into job-printer-uri */
		*jobuser,	/* Pointer to job-originating-user-name */
		*jobname;	/* Pointer to job-name */
  ipp_jstate_t	jobstate;	/* job-state */
  int		jobid,		/* job-id */
		jobsize,	/* job-k-octets */
#ifdef __osf__
		jobpriority,	/* job-priority */
#endif /* __osf__ */
		jobcount,	/* Number of jobs */
		jobcopies,	/* Number of copies */
		rank;		/* Rank of job */
  char		resource[1024];	/* Resource string */
  char		rankstr[255];	/* Rank string */
  char		namestr[1024];	/* Job name string */
  static const char *ranks[10] =/* Ranking strings */
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


  DEBUG_printf(("show_jobs(%08x, %08x, %08x, %d, %d)\n", http, dest, user, id,
                longstatus));

  if (http == NULL)
    return (0);

 /*
  * Build an IPP_GET_JOBS or IPP_GET_JOB_ATTRIBUTES request, which requires
  * the following attributes:
  *
  *    attributes-charset
  *    attributes-natural-language
  *    job-uri or printer-uri
  */

  request = ippNew();

  request->request.op.operation_id = id ? IPP_GET_JOB_ATTRIBUTES : IPP_GET_JOBS;
  request->request.op.request_id   = 1;

  language = cupsLangDefault();

  attr = ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_CHARSET,
                      "attributes-charset", NULL, cupsLangEncoding(language));

  attr = ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_LANGUAGE,
                      "attributes-natural-language", NULL, language->language);

  if (dest == NULL)
  {
    if (id)
      sprintf(resource, "ipp://localhost/jobs/%d", id);
    else
      strcpy(resource, "ipp://localhost/jobs");

    ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_URI, "job-uri",
                 NULL, resource);
  }
  else
  {
    snprintf(resource, sizeof(resource), "ipp://localhost/printers/%s", dest);

    ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_URI, "printer-uri",
                 NULL, resource);
  }

  if (user)
  {
    ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_NAME,
                 "requesting-user-name", NULL, user);
    ippAddBoolean(request, IPP_TAG_OPERATION, "my-jobs", 1);
  }

 /*
  * Do the request and get back a response...
  */

  jobcount = 0;

  if ((response = cupsDoRequest(http, request, "/")) != NULL)
  {
    if (response->request.status.status_code > IPP_OK_CONFLICT)
    {
      fprintf(stderr, "lpq: get-jobs failed: %s\n",
              ippErrorString(response->request.status.status_code));
      ippDelete(response);
      return (0);
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

      while (attr != NULL && attr->group_tag != IPP_TAG_JOB)
        attr = attr->next;

      if (attr == NULL)
        break;

     /*
      * Pull the needed attributes from this job...
      */

      jobid       = 0;
      jobsize     = 0;
#ifdef __osf__
      jobpriority = 50;
#endif /* __osf__ */
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

#ifdef __osf__
        if (strcmp(attr->name, "job-priority") == 0 &&
	    attr->value_tag == IPP_TAG_INTEGER)
	  jobpriority = attr->values[0].integer;
#endif /* __osf__ */

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
#ifdef __osf__
	puts("Rank   Owner      Pri  Job        Files                       Total Size");
#else
	puts("Rank    Owner   Job     File(s)                         Total Size");
#endif /* __osf__ */

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

        printf("%s: %-34.34s[job %d localhost]\n", jobuser, rankstr, jobid);
        printf("        %-40.40s%d bytes\n", namestr, jobsize);
      }
      else
#ifdef __osf__
        printf("%-6s %-10.10s %-4d %-10d %-27.27s %d bytes\n", rankstr, jobuser,
	       jobpriority, jobid, jobname, jobsize);
#else
        printf("%-7s %-8.8s%-8d%-32.32s%d bytes\n", rankstr, jobuser,
	       jobid, jobname, jobsize);
#endif /* __osf */

      if (attr == NULL)
        break;
    }

    ippDelete(response);
  }
  else
  {
    fprintf(stderr, "lpq: get-jobs failed: %s\n", ippErrorString(cupsLastError()));
    return (0);
  }

  if (jobcount == 0)
    puts("no entries");

  return (jobcount);
}


/*
 * 'show_printer()' - Show printer status.
 */

static void
show_printer(http_t     *http,	/* I - HTTP connection to server */
             const char *dest)	/* I - Destination */
{
  ipp_t		*request,	/* IPP Request */
		*response;	/* IPP Response */
  ipp_attribute_t *attr;	/* Current attribute */
  cups_lang_t	*language;	/* Default language */
  ipp_pstate_t	state;		/* Printer state */
  char		uri[HTTP_MAX_URI];
				/* Printer URI */


  if (http == NULL)
    return;

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

  snprintf(uri, sizeof(uri), "ipp://localhost/printers/%s", dest);
  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_URI,
               "printer-uri", NULL, uri);

 /*
  * Do the request and get back a response...
  */

  if ((response = cupsDoRequest(http, request, "/")) != NULL)
  {
    if (response->request.status.status_code > IPP_OK_CONFLICT)
    {
      fprintf(stderr, "lpq: get-printer-attributes failed: %s\n",
              ippErrorString(response->request.status.status_code));
      ippDelete(response);
      return;
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
    fprintf(stderr, "lpq: get-printer-attributes failed: %s\n",
            ippErrorString(cupsLastError()));
}


/*
 * End of "$Id: lpq.c,v 1.17.2.6 2002/05/16 13:59:55 mike Exp $".
 */
