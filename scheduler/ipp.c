/*
 * "$Id: ipp.c,v 1.7 1999/04/19 21:17:09 mike Exp $"
 *
 *   IPP routines for the Common UNIX Printing System (CUPS) scheduler.
 *
 *   Copyright 1997-1999 by Easy Software Products, all rights reserved.
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
 *   ProcessIPPRequest() - Process an incoming IPP request...
 *   add_class()         - Add a class to the system.
 *   add_printer()       - Add a printer to the system.
 *   cancel_job()        - Cancel a print job.
 *   copy_attrs()        - Copy attributes from one request to another.
 *   delete_class()      - Remove a class from the system.
 *   delete_printer()    - Remove a printer from the system.
 *   get_classes()       - Get a list of classes.
 *   get_default()       - Get the default destination.
 *   get_jobs()          - Get a list of jobs for the specified printer.
 *   get_job_attrs()     - Get job attributes.
 *   get_printers()      - Get a list of printers.
 *   get_printer_attrs() - Get printer attributes.
 *   print_job()         - Print a file to a printer or class.
 *   send_ipp_error()    - Send an error status back to the IPP client.
 *   validate_dest()     - Validate a printer class destination.
 *   validate_job()      - Validate printer options and destination.
 */

/*
 * Include necessary headers...
 */

#include "cupsd.h"


/*
 * Local functions...
 */

static void	add_class(client_t *con);
static void	add_printer(client_t *con);
static void	cancel_job(client_t *con, ipp_attribute_t *uri);
static void	copy_attrs(ipp_t *to, ipp_t *from);
static void	delete_class(client_t *con);
static void	delete_printer(client_t *con);
static void	get_classes(client_t *con);
static void	get_default(client_t *con);
static void	get_jobs(client_t *con, ipp_attribute_t *uri);
static void	get_job_attrs(client_t *con, ipp_attribute_t *uri);
static void	get_printers(client_t *con);
static void	get_printer_attrs(client_t *con, ipp_attribute_t *uri);
static void	print_job(client_t *con, ipp_attribute_t *uri);
static void	send_ipp_error(client_t *con, ipp_status_t status);
static char	*validate_dest(char *resource, cups_ptype_t *dtype);
static void	validate_job(client_t *con, ipp_attribute_t *uri);


/*
 * 'ProcessIPPRequest()' - Process an incoming IPP request...
 */

void
ProcessIPPRequest(client_t *con)	/* I - Client connection */
{
  ipp_tag_t		group;		/* Current group tag */
  ipp_attribute_t	*attr;		/* Current attribute */
  ipp_attribute_t	*charset;	/* Character set attribute */
  ipp_attribute_t	*language;	/* Language attribute */
  ipp_attribute_t	*uri;		/* Printer URI attribute */


  DEBUG_printf(("ProcessIPPRequest(%08x)\n", con));
  DEBUG_printf(("ProcessIPPRequest: operation_id = %04x\n",
                con->request->request.op.operation_id));

 /*
  * First build an empty response message for this request...
  */

  con->response = ippNew();

  con->response->request.status.version[0] = 1;
  con->response->request.status.version[1] = 0;
  con->response->request.status.request_id = con->request->request.op.request_id;

 /*
  * Then validate the request header and required attributes...
  */
  
  if (con->request->request.any.version[0] != 1)
  {
   /*
    * Return an error, since we only support IPP 1.x.
    */

    send_ipp_error(con, IPP_VERSION_NOT_SUPPORTED);

    return;
  }  

 /*
  * Make sure that the attributes are provided in the correct order and
  * don't repeat groups...
  */

  for (attr = con->request->attrs, group = attr->group_tag;
       attr != NULL;
       attr = attr->next)
    if (attr->group_tag < group)
    {
     /*
      * Out of order; return an error...
      */

      DEBUG_puts("ProcessIPPRequest: attribute groups are out of order!");
      send_ipp_error(con, IPP_BAD_REQUEST);
      return;
    }
    else
      group = attr->group_tag;

 /*
  * Then make sure that the first three attributes are:
  *
  *     attributes-charset
  *     attributes-natural-language
  *     printer-uri
  */

  attr = con->request->attrs;
  if (attr != NULL && strcmp(attr->name, "attributes-charset") == 0 &&
      attr->value_tag == IPP_TAG_CHARSET)
    charset = attr;
  else
    charset = NULL;

  attr = attr->next;
  if (attr != NULL && strcmp(attr->name, "attributes-natural-language") == 0 &&
      attr->value_tag == IPP_TAG_LANGUAGE)
    language = attr;
  else
    language = NULL;

  attr = attr->next;
  if (attr != NULL && strcmp(attr->name, "printer-uri") == 0 &&
      attr->value_tag == IPP_TAG_URI)
    uri = attr;
  else if (attr != NULL && strcmp(attr->name, "job-uri") == 0 &&
           attr->value_tag == IPP_TAG_URI)
    uri = attr;
  else
    uri = NULL;

  if (charset == NULL || language == NULL ||
      (uri == NULL && con->request->request.op.operation_id < IPP_PRIVATE))
  {
   /*
    * Return an error, since attributes-charset,
    * attributes-natural-language, and printer-uri/job-uri are required
    * for all operations.
    */

    DEBUG_printf(("ProcessIPPRequest: missing attributes (%08x, %08x, %08x)!\n",
                  charset, language, uri));
    send_ipp_error(con, IPP_BAD_REQUEST);
    return;
  }

  attr = ippAddString(con->response, IPP_TAG_OPERATION, IPP_TAG_CHARSET,
                      "attributes-charset", NULL,
		      charset->values[0].string.text);

  attr = ippAddString(con->response, IPP_TAG_OPERATION, IPP_TAG_LANGUAGE,
                      "attributes-natural-language", NULL,
		      language->values[0].string.text);

 /*
  * OK, all the checks pass so far; try processing the operation...
  */

  switch (con->request->request.op.operation_id)
  {
    case IPP_PRINT_JOB :
        print_job(con, uri);
        break;

    case IPP_VALIDATE_JOB :
        validate_job(con, uri);
        break;

    case IPP_CANCEL_JOB :
        cancel_job(con, uri);
        break;

    case IPP_GET_JOB_ATTRIBUTES :
        get_job_attrs(con, uri);
        break;

    case IPP_GET_JOBS :
        get_jobs(con, uri);
        break;

    case IPP_GET_PRINTER_ATTRIBUTES :
        get_printer_attrs(con, uri);
        break;

    case CUPS_GET_DEFAULT :
        get_default(con);
        break;

    case CUPS_GET_PRINTERS :
        get_printers(con);
        break;

#if 0
    case CUPS_ADD_PRINTER :
        add_printer(con);
        break;

    case CUPS_DELETE_PRINTER :
        delete_printer(con);
        break;

    case CUPS_GET_CLASSES :
        get_classes(con);
        break;

    case CUPS_ADD_CLASS :
        add_class(con);
        break;

    case CUPS_DELETE_CLASS :
        delete_class(con);
        break;
#endif /* 0 */

    default :
        send_ipp_error(con, IPP_OPERATION_NOT_SUPPORTED);
	return;
  }

  SendHeader(con, HTTP_OK, "application/ipp");
  httpPrintf(HTTP(con), "Content-Length: %d\r\n\r\n", ippLength(con->response));

  FD_SET(con->http.fd, &OutputSet);
}


/*
 * 'add_class()' - Add a class to the system.
 */

static void
add_class(client_t *con)		/* I - Client connection */
{
  send_ipp_error(con, IPP_OPERATION_NOT_SUPPORTED);
}


/*
 * 'add_printer()' - Add a printer to the system.
 */

static void
add_printer(client_t *con)		/* I - Client connection */
{
  send_ipp_error(con, IPP_OPERATION_NOT_SUPPORTED);
}


/*
 * 'cancel_job()' - Cancel a print job.
 */

static void
cancel_job(client_t        *con,	/* I - Client connection */
	   ipp_attribute_t *uri)	/* I - Job or Printer URI */
{
  ipp_attribute_t	*attr;		/* Current attribute */
  int			jobid;		/* Job ID */
  char			method[HTTP_MAX_URI],
					/* Method portion of URI */
			username[HTTP_MAX_URI],
					/* Username portion of URI */
			host[HTTP_MAX_URI],
					/* Host portion of URI */
			resource[HTTP_MAX_URI];
					/* Resource portion of URI */
  int			port;		/* Port portion of URI */


  DEBUG_printf(("cancel_job(%08x, %08x)\n", con, uri));

 /*
  * See if we have a job URI or a printer URI...
  */

  if (strcmp(uri->name, "printer-uri") == 0)
  {
   /*
    * Got a printer URI; see if we also have a job-id attribute...
    */

    if ((attr = ippFindAttribute(con->request, "job-id", IPP_TAG_INTEGER)) == NULL)
    {
      DEBUG_puts("cancel_job: got a printer-uri attribute but no job-id!");
      send_ipp_error(con, IPP_BAD_REQUEST);
      return;
    }

    jobid = attr->values[0].integer;
  }
  else
  {
   /*
    * Got a job URI; parse it to get the job ID...
    */

    httpSeparate(uri->values[0].string.text, method, username, host, &port, resource);
 
    if (strncmp(resource, "/jobs/", 6) != 0)
    {
     /*
      * Not a valid URI!
      */

      DEBUG_printf(("cancel_job: bad job-uri attribute \'%s\'!\n",
                    uri->values[0].string.text));
      send_ipp_error(con, IPP_BAD_REQUEST);
      return;
    }

    jobid = atoi(resource + 6);
  }

 /*
  * See if the job exists...
  */

  if (FindJob(jobid) == NULL)
  {
   /*
    * Nope - return a "not found" error...
    */

    DEBUG_printf(("cancel_job: job #%d doesn't exist!\n", jobid));
    send_ipp_error(con, IPP_NOT_FOUND);
    return;
  }

 /*
  * Cancel the job and return...
  */

  CancelJob(jobid);
  CheckJobs();

  con->response->request.status.status_code = IPP_OK;
}


/*
 * 'copy_attrs()' - Copy attributes from one request to another.
 */

static void
copy_attrs(ipp_t *to,		/* I - Destination request */
           ipp_t *from)		/* I - Source request */
{
  int			i;		/* Looping var */
  ipp_attribute_t	*toattr,	/* Destination attribute */
			*fromattr;	/* Source attribute */


  DEBUG_printf(("copy_attrs(%08x, %08x)\n", to, from));

  if (to == NULL || from == NULL)
    return;

  for (fromattr = from->attrs; fromattr != NULL; fromattr = fromattr->next)
  {
    DEBUG_printf(("copy_attrs: copying attribute \'%s\'...\n", fromattr->name));

    switch (fromattr->value_tag)
    {
      case IPP_TAG_INTEGER :
      case IPP_TAG_ENUM :
          toattr = ippAddIntegers(to, fromattr->group_tag, fromattr->value_tag,
	                          fromattr->name, fromattr->num_values, NULL);

          for (i = 0; i < fromattr->num_values; i ++)
	    toattr->values[i].integer = fromattr->values[i].integer;
          break;

      case IPP_TAG_BOOLEAN :
          toattr = ippAddBooleans(to, fromattr->group_tag, fromattr->name,
	                          fromattr->num_values, NULL);

          for (i = 0; i < fromattr->num_values; i ++)
	    toattr->values[i].boolean = fromattr->values[i].boolean;
          break;

      case IPP_TAG_STRING :
      case IPP_TAG_TEXT :
      case IPP_TAG_NAME :
      case IPP_TAG_KEYWORD :
      case IPP_TAG_URI :
      case IPP_TAG_URISCHEME :
      case IPP_TAG_CHARSET :
      case IPP_TAG_LANGUAGE :
      case IPP_TAG_MIMETYPE :
          toattr = ippAddStrings(to, fromattr->group_tag, fromattr->value_tag,
	                         fromattr->name, fromattr->num_values, NULL,
				 NULL);

          for (i = 0; i < fromattr->num_values; i ++)
	    toattr->values[i].string.text = strdup(fromattr->values[i].string.text);
          break;

      case IPP_TAG_DATE :
          toattr = ippAddDate(to, fromattr->group_tag, fromattr->name,
	                      fromattr->values[0].date);
          break;

      case IPP_TAG_RESOLUTION :
          toattr = ippAddResolutions(to, fromattr->group_tag, fromattr->name,
	                             fromattr->num_values, IPP_RES_PER_INCH,
				     NULL, NULL);

          for (i = 0; i < fromattr->num_values; i ++)
	  {
	    toattr->values[i].resolution.xres  = fromattr->values[i].resolution.xres;
	    toattr->values[i].resolution.yres  = fromattr->values[i].resolution.yres;
	    toattr->values[i].resolution.units = fromattr->values[i].resolution.units;
	  }
          break;

      case IPP_TAG_RANGE :
          toattr = ippAddRanges(to, fromattr->group_tag, fromattr->name,
	                        fromattr->num_values, NULL, NULL);

          for (i = 0; i < fromattr->num_values; i ++)
	  {
	    toattr->values[i].range.lower = fromattr->values[i].range.lower;
	    toattr->values[i].range.upper = fromattr->values[i].range.upper;
	  }
          break;

      case IPP_TAG_TEXTLANG :
      case IPP_TAG_NAMELANG :
          toattr = ippAddStrings(to, fromattr->group_tag, fromattr->value_tag,
	                         fromattr->name, fromattr->num_values, NULL, NULL);

          for (i = 0; i < fromattr->num_values; i ++)
	  {
	    if (i == 0)
	      toattr->values[0].string.charset =
	          strdup(fromattr->values[0].string.charset);
	    else
	      toattr->values[i].string.charset =
	          toattr->values[0].string.charset;

	    toattr->values[i].string.text =
	        strdup(fromattr->values[i].string.text);
          }
          break;
    }
  }
}


/*
 * 'delete_class()' - Remove a class from the system.
 */

static void
delete_class(client_t *con)		/* I - Client connection */
{
  send_ipp_error(con, IPP_OPERATION_NOT_SUPPORTED);
}


/*
 * 'delete_printer()' - Remove a printer from the system.
 */

static void
delete_printer(client_t *con)		/* I - Client connection */
{
  send_ipp_error(con, IPP_OPERATION_NOT_SUPPORTED);
}


/*
 * 'get_classes()' - Get a list of classes.
 */

static void
get_classes(client_t *con)		/* I - Client connection */
{
  send_ipp_error(con, IPP_OPERATION_NOT_SUPPORTED);
}


/*
 * 'get_default()' - Get the default destination.
 */

static void
get_default(client_t *con)		/* I - Client connection */
{
  printer_t	*printer;		/* Printer information */


  DEBUG_printf(("get_default(%08x)\n", con));

  if ((printer = FindPrinter(DefaultPrinter)) == NULL)
    printer = Printers;

  copy_attrs(con->response, printer->attrs);

  if (ippFindAttribute(con->request, "requested-attributes", IPP_TAG_KEYWORD) != NULL)
    con->response->request.status.status_code = IPP_OK_SUBST;
  else
    con->response->request.status.status_code = IPP_OK;
}


/*
 * 'get_jobs()' - Get a list of jobs for the specified printer.
 */

static void
get_jobs(client_t        *con,		/* I - Client connection */
	 ipp_attribute_t *uri)		/* I - Printer URI */
{
  ipp_attribute_t	*attr;		/* Current attribute */
  char			*dest;		/* Destination */
  cups_ptype_t		dtype;		/* Destination type (printer or class) */
  char			method[HTTP_MAX_URI],
					/* Method portion of URI */
			username[HTTP_MAX_URI],
					/* Username portion of URI */
			host[HTTP_MAX_URI],
					/* Host portion of URI */
			resource[HTTP_MAX_URI];
					/* Resource portion of URI */
  int			port;		/* Port portion of URI */
  int			limit;		/* Maximum number of jobs to return */
  int			count;		/* Number of jobs that match */
  job_t			*job;		/* Current job pointer */
  char			job_uri[HTTP_MAX_URI],
					/* Job URI... */
			printer_uri[HTTP_MAX_URI];
					/* Printer URI... */
  struct stat		filestats;	/* Print file information */


  DEBUG_printf(("get_jobs(%08x, %08x)\n", con, uri));

 /*
  * Is the destination valid?
  */

  httpSeparate(uri->values[0].string.text, method, username, host, &port, resource);

  if ((strncmp(resource, "/jobs", 5) == 0 && strlen(resource) <= 6) ||
      (strncmp(resource, "/printers", 9) == 0 && strlen(resource) <= 10))
  {
    dest  = NULL;
    dtype = (cups_ptype_t)0;
  }
  else if (strncmp(resource, "/classes", 8) == 0 && strlen(resource) <= 9)
  {
    dest  = NULL;
    dtype = CUPS_PRINTER_CLASS;
  }
  else if ((dest = validate_dest(resource, &dtype)) == NULL)
  {
   /*
    * Bad URI...
    */

    DEBUG_printf(("get_jobs: resource name \'%s\' no good!\n",
	          resource));
    send_ipp_error(con, IPP_NOT_FOUND);
    return;
  }

 /*
  * See if the "which-jobs" attribute have been specified; if so, return
  * right away if they specify "completed" - we don't keep old job records...
  */

  if ((attr = ippFindAttribute(con->request, "which-jobs", IPP_TAG_KEYWORD)) != NULL &&
      strcmp(attr->values[0].string.text, "completed") == 0)
  {
    con->response->request.status.status_code = IPP_OK;
    return;
  }

 /*
  * See if they want to limit the number of jobs reported; if not, limit
  * the report to 1000 jobs to prevent swamping of the server...
  */

  if ((attr = ippFindAttribute(con->request, "limit", IPP_TAG_INTEGER)) != NULL)
    limit = attr->values[0].integer;
  else
    limit = 1000;

 /*
  * See if we only want to see jobs for a specific user...
  */

  if ((attr = ippFindAttribute(con->request, "my-jobs", IPP_TAG_BOOLEAN)) != NULL &&
      attr->values[0].boolean)
  {
    strcpy(username, con->username);

    if ((attr = ippFindAttribute(con->request, "requesting-user-name", IPP_TAG_NAME)) != NULL)
    {
      strncpy(username, attr->values[0].string.text, sizeof(username) - 1);
      username[sizeof(username) - 1] = '\0';
    }
  }
  else
    username[0] = '\0';

 /*
  * OK, build a list of jobs for this printer...
  */

  for (count = 0, job = Jobs; count < limit && job != NULL; job = job->next)
  {
   /*
    * Filter out jobs that don't match...
    */

    DEBUG_printf(("get_jobs: job->id = %d\n", job->id));

    if ((dest != NULL && strcmp(job->dest, dest) != 0))
      continue;
    if (job->dtype != dtype &&
        (username[0] == '\0' || strncmp(resource, "/jobs", 5) != 0))
      continue;
    if (username[0] != '\0' && strcmp(username, job->username) != 0)
      continue;

    count ++;

    DEBUG_printf(("get_jobs: count = %d\n", count));

   /*
    * Send the following attributes for each job:
    *
    *    job-id
    *    job-k-octets
    *    job-more-info
    *    job-originating-user-name
    *    job-printer-uri
    *    job-priority
    *    job-state
    *    job-uri
    *
    * Note that we are supposed to look at the "requested-attributes"
    * attribute to determine what we send, however the IPP/1.0 spec also
    * doesn't state that the server must limit the attributes to those
    * requested.  In other words, the server can either implement the
    * filtering or not, and if not it needs to send all attributes that
    * it has...
    */

    sprintf(job_uri, "http://%s:%d/jobs/%d", ServerName,
	    ntohs(con->http.hostaddr.sin_port), job->id);

    if (job->dtype == CUPS_PRINTER_CLASS)
      sprintf(printer_uri, "http://%s:%d/classes/%s", ServerName,
	      ntohs(con->http.hostaddr.sin_port), job->dest);
    else
      sprintf(printer_uri, "http://%s:%d/printers/%s", ServerName,
	      ntohs(con->http.hostaddr.sin_port), job->dest);

    attr = ippAddInteger(con->response, IPP_TAG_JOB, IPP_TAG_INTEGER,
                         "job-id", job->id);

    stat(job->filename, &filestats);
    attr = ippAddInteger(con->response, IPP_TAG_JOB, IPP_TAG_INTEGER,
                         "job-k-octets", (filestats.st_size + 1023) / 1024);

    attr = ippAddString(con->response, IPP_TAG_JOB, IPP_TAG_URI,
                        "job-more-info", NULL, job_uri);

    attr = ippAddString(con->response, IPP_TAG_JOB, IPP_TAG_NAME,
                        "job-originating-user-name", NULL, job->username);

    attr = ippAddString(con->response, IPP_TAG_JOB, IPP_TAG_URI,
                        "job-printer-uri", NULL, printer_uri);

    attr = ippAddInteger(con->response, IPP_TAG_JOB, IPP_TAG_ENUM,
                         "job-state", job->state);

    attr = ippAddString(con->response, IPP_TAG_JOB, IPP_TAG_URI,
                        "job-uri", NULL, job_uri);

    ippAddSeparator(con->response);
  }

  if (ippFindAttribute(con->request, "requested-attributes", IPP_TAG_KEYWORD) != NULL)
    con->response->request.status.status_code = IPP_OK_SUBST;
  else
    con->response->request.status.status_code = IPP_OK;
}


/*
 * 'get_job_attrs()' - Get job attributes.
 */

static void
get_job_attrs(client_t        *con,		/* I - Client connection */
	      ipp_attribute_t *uri)		/* I - Job URI */
{
  ipp_attribute_t	*attr;		/* Current attribute */
  int			jobid;		/* Job ID */
  job_t			*job;		/* Current job */
  char			method[HTTP_MAX_URI],
					/* Method portion of URI */
			username[HTTP_MAX_URI],
					/* Username portion of URI */
			host[HTTP_MAX_URI],
					/* Host portion of URI */
			resource[HTTP_MAX_URI];
					/* Resource portion of URI */
  int			port;		/* Port portion of URI */


  DEBUG_printf(("get_job_attrs(%08x, %08x)\n", con, uri));

 /*
  * See if we have a job URI or a printer URI...
  */

  if (strcmp(uri->name, "printer-uri") == 0)
  {
   /*
    * Got a printer URI; see if we also have a job-id attribute...
    */

    if ((attr = ippFindAttribute(con->request, "job-id", IPP_TAG_INTEGER)) == NULL)
    {
      DEBUG_puts("get_job_attrs: got a printer-uri attribute but no job-id!");
      send_ipp_error(con, IPP_BAD_REQUEST);
      return;
    }

    jobid = attr->values[0].integer;
  }
  else
  {
   /*
    * Got a job URI; parse it to get the job ID...
    */

    httpSeparate(uri->values[0].string.text, method, username, host, &port, resource);
 
    if (strncmp(resource, "/jobs/", 6) != 0)
    {
     /*
      * Not a valid URI!
      */

      DEBUG_printf(("get_job_attrs: bad job-uri attribute \'%s\'!\n",
                    uri->values[0].string.text));
      send_ipp_error(con, IPP_BAD_REQUEST);
      return;
    }

    jobid = atoi(resource + 6);
  }

 /*
  * See if the job exists...
  */

  if ((job = FindJob(jobid)) == NULL)
  {
   /*
    * Nope - return a "not found" error...
    */

    DEBUG_printf(("get_job_attrs: job #%d doesn't exist!\n", jobid));
    send_ipp_error(con, IPP_NOT_FOUND);
    return;
  }

 /*
  * Copy the job attributes to the response, ignoring the requested-attributes
  * attribute that may be provided by the client.
  */

  copy_attrs(con->response, job->attrs);

  if (ippFindAttribute(con->request, "requested-attributes", IPP_TAG_KEYWORD) != NULL)
    con->response->request.status.status_code = IPP_OK_SUBST;
  else
    con->response->request.status.status_code = IPP_OK;
}


/*
 * 'get_printers()' - Get a list of printers.
 */

static void
get_printers(client_t *con)		/* I - Client connection */
{
  ipp_attribute_t	*attr;		/* Current attribute */
  int			limit;		/* Maximum number of printers to return */
  int			count;		/* Number of printers that match */
  printer_t		*printer;	/* Current printer pointer */


  DEBUG_printf(("get_printers(%08x)\n", con));

 /*
  * See if they want to limit the number of printers reported; if not, limit
  * the report to 1000 printers to prevent swamping of the server...
  */

  if ((attr = ippFindAttribute(con->request, "limit", IPP_TAG_INTEGER)) != NULL)
    limit = attr->values[0].integer;
  else
    limit = 1000;

 /*
  * OK, build a list of printers for this printer...
  */

  for (count = 0, printer = Printers;
       count < limit && printer != NULL;
       printer = printer->next)
  {
   /*
    * Send the following attributes for each printer:
    *
    *    printer-state
    *    printer-state-message
    *    printer-is-accepting-jobs
    *    printer-device-uri
    *    + all printer attributes
    */

    ippAddInteger(con->response, IPP_TAG_PRINTER, IPP_TAG_ENUM,
                  "printer-state", printer->state);

    if (printer->state_message[0])
      ippAddString(con->response, IPP_TAG_PRINTER, IPP_TAG_TEXT,
                   "printer-state-message", NULL, printer->state_message);

    ippAddBoolean(con->response, IPP_TAG_PRINTER, "printer-is-accepting-jobs", 1);

    ippAddString(con->response, IPP_TAG_PRINTER, IPP_TAG_URI,
                 "printer-device-uri", NULL, printer->device_uri);

    copy_attrs(con->response, printer->attrs);

    ippAddSeparator(con->response);
  }

  if (ippFindAttribute(con->request, "requested-attributes", IPP_TAG_KEYWORD) != NULL)
    con->response->request.status.status_code = IPP_OK_SUBST;
  else
    con->response->request.status.status_code = IPP_OK;
}


/*
 * 'get_printer_attrs()' - Get printer attributes.
 */

static void
get_printer_attrs(client_t        *con,	/* I - Client connection */
		  ipp_attribute_t *uri)	/* I - Printer URI */
{
  char			*dest;		/* Destination */
  cups_ptype_t		dtype;		/* Destination type (printer or class) */
  char			method[HTTP_MAX_URI],
					/* Method portion of URI */
			username[HTTP_MAX_URI],
					/* Username portion of URI */
			host[HTTP_MAX_URI],
					/* Host portion of URI */
			resource[HTTP_MAX_URI];
					/* Resource portion of URI */
  int			port;		/* Port portion of URI */
  printer_t		*printer;	/* Printer */
  class_t		*pclass;	/* Printer class */


  DEBUG_printf(("get_printer_attrs(%08x, %08x)\n", con, uri));

 /*
  * Is the destination valid?
  */

  httpSeparate(uri->values[0].string.text, method, username, host, &port, resource);

  if ((dest = validate_dest(resource, &dtype)) == NULL)
  {
   /*
    * Bad URI...
    */

    DEBUG_printf(("get_printer_attrs: resource name \'%s\' no good!\n",
	          resource));
    send_ipp_error(con, IPP_NOT_FOUND);
    return;
  }

 /*
  * Copy the printer attributes to the response, ignoring requested-attributes
  * and document-format attributes that may be provided by the client.
  */

  if (dtype == CUPS_PRINTER_CLASS)
  {
   /*
    * For classes, return the attributes supported by the first printer
    * in the class.
    */

    pclass  = FindClass(dest);
    printer = pclass->printers[0];
  }
  else
    printer = FindPrinter(dest);

  copy_attrs(con->response, printer->attrs);

  ippAddInteger(con->response, IPP_TAG_PRINTER, IPP_TAG_ENUM, "printer-state",
                printer->state);

  if (printer->state_message[0])
    ippAddString(con->response, IPP_TAG_PRINTER, IPP_TAG_TEXT,
                 "printer-state-message", NULL, printer->state_message);

  if (ippFindAttribute(con->request, "requested-attributes", IPP_TAG_KEYWORD) != NULL)
    con->response->request.status.status_code = IPP_OK_SUBST;
  else
    con->response->request.status.status_code = IPP_OK;
}


/*
 * 'print_job()' - Print a file to a printer or class.
 */

static void
print_job(client_t        *con,		/* I - Client connection */
	  ipp_attribute_t *uri)		/* I - Printer URI */
{
  ipp_attribute_t	*attr;		/* Current attribute */
  ipp_attribute_t	*format;	/* Document-format attribute */
  char			*dest;		/* Destination */
  cups_ptype_t		dtype;		/* Destination type (printer or class) */
  int			priority;	/* Job priority */
  job_t			*job;		/* Current job */
  char			job_uri[HTTP_MAX_URI],
					/* Job URI */
			method[HTTP_MAX_URI],
					/* Method portion of URI */
			username[HTTP_MAX_URI],
					/* Username portion of URI */
			host[HTTP_MAX_URI],
					/* Host portion of URI */
			resource[HTTP_MAX_URI];
					/* Resource portion of URI */
  int			port;		/* Port portion of URI */
  mime_type_t		*filetype;	/* Type of file */
  char			super[MIME_MAX_SUPER],
					/* Supertype of file */
			type[MIME_MAX_TYPE];
					/* Subtype of file */


  DEBUG_printf(("print_job(%08x, %08x)\n", con, uri));

 /*
  * OK, see if the client is sending the document compressed - CUPS
  * doesn't support compression yet...
  */

  if ((attr = ippFindAttribute(con->request, "compression", IPP_TAG_KEYWORD)) != NULL)
  {
    DEBUG_puts("print_job: Unsupported compression attribute!");
    send_ipp_error(con, IPP_ATTRIBUTES);
    attr = ippAddString(con->response, IPP_TAG_UNSUPPORTED, IPP_TAG_KEYWORD,
	                "compression", NULL, attr->values[0].string.text);
    return;
  }

 /*
  * Do we have a file to print?
  */

  if (con->filename[0] == '\0')
  {
    DEBUG_puts("print_job: No filename!?!");
    send_ipp_error(con, IPP_BAD_REQUEST);
    return;
  }

 /*
  * Is it a format we support?
  */

  if ((format = ippFindAttribute(con->request, "document-format", IPP_TAG_MIMETYPE)) == NULL)
  {
    DEBUG_puts("print_job: missing document-format attribute!");
    send_ipp_error(con, IPP_BAD_REQUEST);
    return;
  }

  if (sscanf(format->values[0].string.text, "%15[^/]/%31[^;]", super, type) != 2)
  {
    DEBUG_printf(("print_job: could not scan type \'%s\'!\n",
	          format->values[0].string.text));
    send_ipp_error(con, IPP_BAD_REQUEST);
    return;
  }

  if (strcmp(super, "application") == 0 &&
      strcmp(type, "octet-stream") == 0)
  {
    DEBUG_puts("print_job: auto-typing request using magic rules.");
    filetype = mimeFileType(MimeDatabase, con->filename);
  }
  else
    filetype = mimeType(MimeDatabase, super, type);

  if (filetype == NULL)
  {
    DEBUG_printf(("print_job: Unsupported format \'%s\'!\n",
	          format->values[0].string.text));
    send_ipp_error(con, IPP_DOCUMENT_FORMAT);
    attr = ippAddString(con->response, IPP_TAG_UNSUPPORTED, IPP_TAG_MIMETYPE,
                        "document-format", NULL, format->values[0].string.text);
    return;
  }

  DEBUG_printf(("print_job: request file type is %s/%s.\n",
	        filetype->super, filetype->type));

 /*
  * Is the destination valid?
  */

  httpSeparate(uri->values[0].string.text, method, username, host, &port, resource);

  if ((dest = validate_dest(resource, &dtype)) == NULL)
  {
   /*
    * Bad URI...
    */

    DEBUG_printf(("print_job: resource name \'%s\' no good!\n",
	          resource));
    send_ipp_error(con, IPP_NOT_FOUND);
    return;
  }

 /*
  * Create the job and set things up...
  */

  if ((attr = ippFindAttribute(con->request, "job-priority", IPP_TAG_INTEGER)) != NULL)
    priority = attr->values[0].integer;
  else
    priority = 50;

  if ((job = AddJob(priority, dest)) == NULL)
  {
    send_ipp_error(con, IPP_INTERNAL_ERROR);
    return;
  }

  job->dtype    = dtype;
  job->state    = IPP_JOB_PENDING;
  job->filetype = filetype;
  job->attrs    = con->request;
  con->request  = NULL;

  strcpy(job->filename, con->filename);
  con->filename[0] = '\0';

  strcpy(job->username, con->username);
  if ((attr = ippFindAttribute(job->attrs, "requesting-user-name", IPP_TAG_NAME)) != NULL)
  {
    DEBUG_printf(("print_job: requesting-user-name = \'%s\'\n",
                  attr->values[0].string.text));

    strncpy(job->username, attr->values[0].string.text, sizeof(job->username) - 1);
    job->username[sizeof(job->username) - 1] = '\0';
  }

  if (job->username[0] == '\0')
    strcpy(job->username, "guest");

  DEBUG_printf(("print_job: job->username = \'%s\', attr = %08x\n",
                job->username, attr));

 /*
  * Start the job if possible...
  */

  CheckJobs();

 /*
  * Fill in the response info...
  */

  sprintf(job_uri, "http://%s:%d/jobs/%d", ServerName,
	  ntohs(con->http.hostaddr.sin_port), job->id);
  attr = ippAddString(con->response, IPP_TAG_JOB, IPP_TAG_URI, "job-uri",
                      NULL, job_uri);

  attr = ippAddInteger(con->response, IPP_TAG_JOB, IPP_TAG_INTEGER,
                       "job-id", job->id);

  attr = ippAddInteger(con->response, IPP_TAG_JOB, IPP_TAG_ENUM,
                       "job-state", job->state);

  con->response->request.status.status_code = IPP_OK;
}


/*
 * 'send_ipp_error()' - Send an error status back to the IPP client.
 */

static void
send_ipp_error(client_t     *con,	/* I - Client connection */
               ipp_status_t status)	/* I - IPP status code */
{
  ipp_attribute_t	*attr;		/* Current attribute */


  DEBUG_printf(("send_ipp_error(%08x, %04x)\n", con, status));

  if (con->filename[0])
    unlink(con->filename);

  con->response->request.status.status_code = status;

  attr = ippAddString(con->response, IPP_TAG_OPERATION, IPP_TAG_CHARSET,
                      "attributes-charset", NULL, DefaultCharset);

  attr = ippAddString(con->response, IPP_TAG_OPERATION, IPP_TAG_LANGUAGE,
                      "attributes-natural-language", NULL, DefaultLanguage);

  SendHeader(con, HTTP_OK, "application/ipp");
  httpPrintf(HTTP(con), "Content-Length: %d\r\n\r\n", ippLength(con->response));

  FD_SET(con->http.fd, &OutputSet);
}


/*
 * 'validate_dest()' - Validate a printer class destination.
 */

static char *				/* O - Printer or class name */
validate_dest(char         *resource,	/* I - Resource name */
              cups_ptype_t *dtype)	/* O - Type (printer or class) */
{
  if (strncmp(resource, "/classes/", 9) == 0)
  {
   /*
    * Print to a class...
    */

    *dtype = CUPS_PRINTER_CLASS;

    if (FindClass(resource + 9) == NULL)
      return (NULL);
    else
      return (resource + 9);
  }
  else if (strncmp(resource, "/printers/", 10) == 0)
  {
   /*
    * Print to a specific printer...
    */

    *dtype = (cups_ptype_t)0;

    if (FindPrinter(resource + 10) == NULL)
    {
      *dtype = CUPS_PRINTER_CLASS;

      if (FindClass(resource + 10) == NULL)
        return (NULL);
    }

    return (resource + 10);
  }
  else
    return (NULL);
}


/*
 * 'validate_job()' - Validate printer options and destination.
 */

static void
validate_job(client_t        *con,	/* I - Client connection */
	     ipp_attribute_t *uri)	/* I - Printer URI */
{
  ipp_attribute_t	*attr;		/* Current attribute */
  ipp_attribute_t	*format;	/* Document-format attribute */
  cups_ptype_t		dtype;		/* Destination type (printer or class) */
  char			method[HTTP_MAX_URI],
					/* Method portion of URI */
			username[HTTP_MAX_URI],
					/* Username portion of URI */
			host[HTTP_MAX_URI],
					/* Host portion of URI */
			resource[HTTP_MAX_URI];
					/* Resource portion of URI */
  int			port;		/* Port portion of URI */
  char			super[MIME_MAX_SUPER],
					/* Supertype of file */
			type[MIME_MAX_TYPE];
					/* Subtype of file */


  DEBUG_printf(("validate_job(%08x, %08x)\n", con, uri));

 /*
  * OK, see if the client is sending the document compressed - CUPS
  * doesn't support compression yet...
  */

  if ((attr = ippFindAttribute(con->request, "compression", IPP_TAG_KEYWORD)) != NULL)
  {
    DEBUG_puts("validate_job: Unsupported compression attribute!");
    send_ipp_error(con, IPP_ATTRIBUTES);
    attr = ippAddString(con->response, IPP_TAG_UNSUPPORTED, IPP_TAG_KEYWORD,
	                "compression", NULL, attr->values[0].string.text);
    return;
  }

 /*
  * Is it a format we support?
  */

  if ((format = ippFindAttribute(con->request, "document-format", IPP_TAG_MIMETYPE)) == NULL)
  {
    DEBUG_puts("validate_job: missing document-format attribute!");
    send_ipp_error(con, IPP_BAD_REQUEST);
    return;
  }

  if (sscanf(format->values[0].string.text, "%15[^/]/%31[^;]", super, type) != 2)
  {
    DEBUG_printf(("validate_job: could not scan type \'%s\'!\n",
	          format->values[0].string.text));
    send_ipp_error(con, IPP_BAD_REQUEST);
    return;
  }

  if ((strcmp(super, "application") != 0 ||
       strcmp(type, "octet-stream") != 0) &&
      mimeType(MimeDatabase, super, type) == NULL)
  {
    DEBUG_printf(("validate_job: Unsupported format \'%s\'!\n",
	          format->values[0].string.text));
    send_ipp_error(con, IPP_DOCUMENT_FORMAT);
    attr = ippAddString(con->response, IPP_TAG_UNSUPPORTED, IPP_TAG_MIMETYPE,
                        "document-format", NULL, format->values[0].string.text);
    return;
  }

 /*
  * Is the destination valid?
  */

  httpSeparate(uri->values[0].string.text, method, username, host, &port, resource);

  if (validate_dest(resource, &dtype) == NULL)
  {
   /*
    * Bad URI...
    */

    DEBUG_printf(("validate_job: resource name \'%s\' no good!\n",
	          resource));
    send_ipp_error(con, IPP_NOT_FOUND);
    return;
  }

 /*
  * Everything was ok, so return OK status...
  */

  con->response->request.status.status_code = IPP_OK;
}


/*
 * End of "$Id: ipp.c,v 1.7 1999/04/19 21:17:09 mike Exp $".
 */
