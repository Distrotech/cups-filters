/*
 * "$Id: util.c,v 1.44 2000/01/04 13:45:37 mike Exp $"
 *
 *   Printing utilities for the Common UNIX Printing System (CUPS).
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
 *   cupsCancelJob()     - Cancel a print job.
 *   cupsDoFileRequest() - Do an IPP request...
 *   cupsGetClasses()    - Get a list of printer classes.
 *   cupsGetDefault()    - Get the default printer or class.
 *   cupsGetPPD()        - Get the PPD file for a printer.
 *   cupsGetPrinters()   - Get a list of printers.
 *   cupsPrintFile()     - Print a file to a printer or class.
 *   cups_connect()      - Connect to the specified host...
 *   cups_local_auth()   - Get the local authorization certificate if
 *                         available/applicable...
 */

/*
 * Include necessary headers...
 */

#include "cups.h"
#include "ipp.h"
#include "language.h"
#include "string.h"
#include "debug.h"
#include <stdlib.h>
#include <ctype.h>
#include <errno.h>
#include <sys/stat.h>
#if defined(WIN32) || defined(__EMX__)
#  include <io.h>
#else
#  include <unistd.h>
#endif /* WIN32 || __EMX__ */


/*
 * Local globals...
 */

static http_t		*cups_server = NULL;	/* Current server connection */
static ipp_status_t	last_error = IPP_OK;	/* Last IPP error */
static char		authstring[255] = "";	/* Authorization string */


/*
 * Local functions...
 */

static char	*cups_connect(const char *name, char *printer, char *hostname);
static int	cups_local_auth(http_t *http);


/*
 * 'cupsCancelJob()' - Cancel a print job.
 */

int				/* O - 1 on success, 0 on failure */
cupsCancelJob(const char *name,	/* I - Name of printer or class */
              int        job)	/* I - Job ID */
{
  char		printer[HTTP_MAX_URI],	/* Printer name */
		hostname[HTTP_MAX_URI],	/* Hostname */
		uri[HTTP_MAX_URI];	/* Printer URI */
  ipp_t		*request,		/* IPP request */
		*response;		/* IPP response */
  cups_lang_t	*language;		/* Language info */


 /*
  * See if we can connect to the server...
  */

  if (!cups_connect(name, printer, hostname))
    return (0);

 /*
  * Build an IPP_CANCEL_JOB request, which requires the following
  * attributes:
  *
  *    attributes-charset
  *    attributes-natural-language
  *    printer-uri
  *    job-id
  *    [requesting-user-name]
  */

  request = ippNew();

  request->request.op.operation_id = IPP_CANCEL_JOB;
  request->request.op.request_id   = 1;

  language = cupsLangDefault();

  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_CHARSET,
               "attributes-charset", NULL, cupsLangEncoding(language));

  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_LANGUAGE,
               "attributes-natural-language", NULL,
               language != NULL ? language->language : "C");

  snprintf(uri, sizeof(uri), "ipp://%s:%d/printers/%s", hostname, ippPort(), printer);
  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_URI, "printer-uri",
               NULL, uri);

  ippAddInteger(request, IPP_TAG_OPERATION, IPP_TAG_INTEGER, "job-id", job);

  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_NAME, "requesting-user-name",
               NULL, cupsUser());

 /*
  * Do the request...
  */

  if ((response = cupsDoRequest(cups_server, request, "/jobs/")) == NULL)
  {
    last_error = IPP_BAD_REQUEST;
    return (0);
  }
  else
  {
    last_error = response->request.status.status_code;
    ippDelete(response);

    return (1);
  }
}


/*
 * 'cupsDoFileRequest()' - Do an IPP request...
 */

ipp_t *					/* O - Response data */
cupsDoFileRequest(http_t     *http,	/* I - HTTP connection to server */
                  ipp_t      *request,	/* I - IPP request */
                  const char *resource,	/* I - HTTP resource for POST */
		  const char *filename)	/* I - File to send or NULL */
{
  ipp_t		*response;	/* IPP response data */
  char		length[255];	/* Content-Length field */
  http_status_t	status;		/* Status of HTTP request */
  FILE		*file;		/* File to send */
  struct stat	fileinfo;	/* File information */
  int		bytes;		/* Number of bytes read/written */
  char		buffer[8192];	/* Output buffer */
  const char	*password;	/* Password string */
  char		plain[255],	/* Plaintext username:password */
		encode[255];	/* Encoded username:password */


  DEBUG_printf(("cupsDoFileRequest(%08x, %08s, \'%s\', \'%s\')\n",
                http, request, resource, filename ? filename : "(null)"));

 /*
  * See if we have a file to send...
  */

  if (filename != NULL)
  {
    if (stat(filename, &fileinfo))
    {
     /*
      * Can't get file information!
      */

      ippDelete(request);
      return (NULL);
    }

    if ((file = fopen(filename, "rb")) == NULL)
    {
     /*
      * Can't open file!
      */

      ippDelete(request);
      return (NULL);
    }
  }

 /*
  * Loop until we can send the request without authorization problems.
  */

  response = NULL;

  while (response == NULL)
  {
    DEBUG_puts("cupsDoFileRequest: setup...");

   /*
    * Setup the HTTP variables needed...
    */

    if (filename != NULL)
      sprintf(length, "%u", ippLength(request) + (size_t)fileinfo.st_size);
    else
      sprintf(length, "%u", ippLength(request));

    httpClearFields(http);
    httpSetField(http, HTTP_FIELD_CONTENT_LENGTH, length);
    httpSetField(http, HTTP_FIELD_CONTENT_TYPE, "application/ipp");
    httpSetField(http, HTTP_FIELD_AUTHORIZATION, authstring);

   /*
    * Try the request...
    */

    DEBUG_puts("cupsDoFileRequest: post...");

    if (httpPost(http, resource))
      continue;

   /*
    * Send the IPP data and wait for the response...
    */

    DEBUG_puts("cupsDoFileRequest: ipp write...");

    request->state = IPP_IDLE;
    if (ippWrite(http, request) != IPP_ERROR)
      if (filename != NULL)
      {
        DEBUG_puts("cupsDoFileRequest: file write...");

       /*
        * Send the file...
        */

        rewind(file);

        while ((bytes = fread(buffer, 1, sizeof(buffer), file)) > 0)
  	  if (httpWrite(http, buffer, bytes) < bytes)
            break;
      }

   /*
    * Get the server's return status...
    */

    DEBUG_puts("cupsDoFileRequest: update...");

    while ((status = httpUpdate(http)) == HTTP_CONTINUE);

    if (status == HTTP_UNAUTHORIZED)
    {
      DEBUG_puts("cupsDoFileRequest: unauthorized...");

     /*
      * Flush any error message...
      */

      httpFlush(http);

     /*
      * See if we can do local authentication...
      */

      if (cups_local_auth(http))
        continue;

     /*
      * Nope - get a password from the user...
      */

      printf("Authentication required for %s on %s...\n", cupsUser(),
             http->hostname);

      if ((password = cupsGetPassword("UNIX Password: ")) != NULL)
      {
       /*
	* Got a password; send it to the server...
	*/

        if (!password[0])
          break;
	snprintf(plain, sizeof(plain), "%s:%s", cupsUser(), password);
	httpEncode64(encode, plain);
	snprintf(authstring, sizeof(authstring), "Basic %s", encode);

        continue;
      }
      else
        break;
    }
    else if (status == HTTP_ERROR)
    {
#if defined(WIN32) || defined(__EMX__)
      if (http->error != WSAENETDOWN && http->error != WSAENETUNREACH)
#else
      if (http->error != ENETDOWN && http->error != ENETUNREACH)
#endif /* WIN32 || __EMX__ */
        continue;
      else
        break;
    }
    else if (status != HTTP_OK)
    {
      DEBUG_printf(("cupsDoFileRequest: error %d...\n", status));

     /*
      * Flush any error message...
      */

      httpFlush(http);
      break;
    }
    else
    {
     /*
      * Read the response...
      */

      DEBUG_puts("cupsDoFileRequest: response...");

      response = ippNew();

      if (ippRead(http, response) == IPP_ERROR)
      {
       /*
        * Delete the response...
	*/

	ippDelete(response);
	response = NULL;

       /*
	* Flush any remaining data...
	*/

	httpFlush(http);
	break;
      }
    }
  }

 /*
  * Close the file if needed...
  */

  if (filename != NULL)
    fclose(file);

 /*
  * Delete the original request and return the response...
  */
  
  ippDelete(request);

  return (response);
}


/*
 * 'cupsGetClasses()' - Get a list of printer classes.
 */

int				/* O - Number of classes */
cupsGetClasses(char ***classes)	/* O - Classes */
{
  int		n;		/* Number of classes */
  ipp_t		*request,	/* IPP Request */
		*response;	/* IPP Response */
  ipp_attribute_t *attr;	/* Current attribute */
  cups_lang_t	*language;	/* Default language */
  char		**temp;		/* Temporary pointer */


 /*
  * Try to connect to the server...
  */

  if (!cups_connect("default", NULL, NULL))
    return (0);

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

  n        = 0;
  *classes = NULL;

  if ((response = cupsDoRequest(cups_server, request, "/")) != NULL)
  {
    last_error = response->request.status.status_code;

    for (attr = response->attrs; attr != NULL; attr = attr->next)
      if (attr->name != NULL &&
          strcasecmp(attr->name, "printer-name") == 0 &&
          attr->value_tag == IPP_TAG_NAME)
      {
        if (n == 0)
	  temp = malloc(sizeof(char *));
	else
	  temp = realloc(*classes, sizeof(char *) * (n + 1));

	if (temp == NULL)
	{
	 /*
	  * Ran out of memory!
	  */

          while (n > 0)
	  {
	    n --;
	    free((*classes)[n]);
	  }

	  free(*classes);
	  ippDelete(response);
	  return (0);
	}

        *classes = temp;
        temp[n]  = strdup(attr->values[0].string.text);
	n ++;
      }

    ippDelete(response);
  }
  else
    last_error = IPP_BAD_REQUEST;

  return (n);
}


/*
 * 'cupsGetDefault()' - Get the default printer or class.
 */

const char *			/* O - Default printer or NULL */
cupsGetDefault(void)
{
  ipp_t		*request,	/* IPP Request */
		*response;	/* IPP Response */
  ipp_attribute_t *attr;	/* Current attribute */
  cups_lang_t	*language;	/* Default language */
  static char	def_printer[64];/* Default printer */


 /*
  * First see if the LPDEST or PRINTER environment variables are
  * set...
  */

  if (getenv("LPDEST") != NULL)
    return (getenv("LPDEST"));
  else if (getenv("PRINTER") != NULL)
    return (getenv("PRINTER"));

 /*
  * Try to connect to the server...
  */

  if (!cups_connect("default", NULL, NULL))
    return (NULL);

 /*
  * Build a CUPS_GET_DEFAULT request, which requires the following
  * attributes:
  *
  *    attributes-charset
  *    attributes-natural-language
  */

  request = ippNew();

  request->request.op.operation_id = CUPS_GET_DEFAULT;
  request->request.op.request_id   = 1;

  language = cupsLangDefault();

  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_CHARSET,
               "attributes-charset", NULL, cupsLangEncoding(language));

  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_LANGUAGE,
               "attributes-natural-language", NULL, language->language);

 /*
  * Do the request and get back a response...
  */

  if ((response = cupsDoRequest(cups_server, request, "/")) != NULL)
  {
    last_error = response->request.status.status_code;

    if ((attr = ippFindAttribute(response, "printer-name", IPP_TAG_NAME)) != NULL)
    {
      strncpy(def_printer, attr->values[0].string.text, sizeof(def_printer) - 1);
      def_printer[sizeof(def_printer) - 1] = '\0';
      ippDelete(response);
      return (def_printer);
    }

    ippDelete(response);
  }
  else
    last_error = IPP_BAD_REQUEST;

  return (NULL);
}


/*
 * 'cupsGetPPD()' - Get the PPD file for a printer.
 */

const char *			/* O - Filename for PPD file */
cupsGetPPD(const char *name)	/* I - Printer name */
{
  FILE		*fp;			/* PPD file */
  int		bytes;			/* Number of bytes read */
  char		buffer[8192];		/* Buffer for file */
  char		printer[HTTP_MAX_URI],	/* Printer name */
		hostname[HTTP_MAX_URI],	/* Hostname */
		resource[HTTP_MAX_URI];	/* Resource name */
  char		*tempdir;		/* Temporary file directory */
  const char	*password;		/* Password string */
  char		plain[255],		/* Plaintext username:password */
		encode[255];		/* Encoded username:password */
  http_status_t	status;			/* HTTP status from server */
  static char	filename[HTTP_MAX_URI];	/* Local filename */


 /*
  * See if we can connect to the server...
  */

  if (!cups_connect(name, printer, hostname))
    return (NULL);

 /*
  * Then check for the cache file...
  */

#if defined(WIN32) || defined(__EMX__)
  tempdir = "C:/WINDOWS/TEMP";

  snprintf(filename, sizeof(filename), "%s/%s.ppd", tempdir, printer);
#else
  if ((tempdir = getenv("TMPDIR")) == NULL)
    tempdir = "/tmp";

  snprintf(filename, sizeof(filename), "%s/%d.%s.ppd", tempdir, getuid(), printer);
#endif /* WIN32 || __EMX__ */

 /*
  * And send a request to the HTTP server...
  */

  snprintf(resource, sizeof(resource), "/printers/%s.ppd", printer);

  do
  {
    httpClearFields(cups_server);
    httpSetField(cups_server, HTTP_FIELD_HOST, hostname);
    httpSetField(cups_server, HTTP_FIELD_AUTHORIZATION, authstring);

    if (httpGet(cups_server, resource))
    {
      status = HTTP_UNAUTHORIZED;
      continue;
    }

    while ((status = httpUpdate(cups_server)) == HTTP_CONTINUE);

    if (status == HTTP_UNAUTHORIZED)
    {
      DEBUG_puts("cupsGetPPD: unauthorized...");

     /*
      * Flush any error message...
      */

      httpFlush(cups_server);

     /*
      * See if we can do local authentication...
      */

      if (cups_local_auth(cups_server))
        continue;

     /*
      * Nope, get a password from the user...
      */

      printf("Authentication required for %s on %s...\n", cupsUser(),
             cups_server->hostname);

      if ((password = cupsGetPassword("UNIX Password: ")) != NULL)
      {
       /*
	* Got a password; send it to the server...
	*/

        if (!password[0])
          break;
	snprintf(plain, sizeof(plain), "%s:%s", cupsUser(), password);
	httpEncode64(encode, plain);
	snprintf(authstring, sizeof(authstring), "Basic %s", encode);

        continue;
      }
      else
        break;
    }
  }
  while (status == HTTP_UNAUTHORIZED);

 /*
  * OK, we need to copy the file; open the file and copy it...
  */

  unlink(filename);
  if ((fp = fopen(filename, "w")) == NULL)
  {
   /*
    * Can't open file; close the server connection and return NULL...
    */

    httpClose(cups_server);
    cups_server = NULL;
    return (NULL);
  }

  while ((bytes = httpRead(cups_server, buffer, sizeof(buffer))) > 0)
    fwrite(buffer, bytes, 1, fp);

  fclose(fp);

  return (filename);
}


/*
 * 'cupsGetPrinters()' - Get a list of printers.
 */

int					/* O - Number of printers */
cupsGetPrinters(char ***printers)	/* O - Printers */
{
  int		n;		/* Number of printers */
  ipp_t		*request,	/* IPP Request */
		*response;	/* IPP Response */
  ipp_attribute_t *attr;	/* Current attribute */
  cups_lang_t	*language;	/* Default language */
  char		**temp;		/* Temporary pointer */


 /*
  * Try to connect to the server...
  */

  if (!cups_connect("default", NULL, NULL))
    return (0);

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

  n         = 0;
  *printers = NULL;

  if ((response = cupsDoRequest(cups_server, request, "/")) != NULL)
  {
    last_error = response->request.status.status_code;

    for (attr = response->attrs; attr != NULL; attr = attr->next)
      if (attr->name != NULL &&
          strcasecmp(attr->name, "printer-name") == 0 &&
          attr->value_tag == IPP_TAG_NAME)
      {
        if (n == 0)
	  temp = malloc(sizeof(char *));
	else
	  temp = realloc(*printers, sizeof(char *) * (n + 1));

	if (temp == NULL)
	{
	 /*
	  * Ran out of memory!
	  */

	  while (n > 0)
	  {
	    n --;
	    free((*printers)[n]);
	  }

	  free(*printers);
	  ippDelete(response);
	  return (0);
	}

        *printers = temp;
        temp[n]   = strdup(attr->values[0].string.text);
	n ++;
      }

    ippDelete(response);
  }
  else
    last_error = IPP_BAD_REQUEST;

  return (n);
}


/*
 * 'cupsLastError()' - Return the last IPP error that occurred.
 */

ipp_status_t		/* O - IPP error code */
cupsLastError(void)
{
  return (last_error);
}


/*
 * 'cupsPrintFile()' - Print a file to a printer or class.
 */

int					/* O - Job ID */
cupsPrintFile(const char    *name,	/* I - Printer or class name */
              const char    *filename,	/* I - File to print */
	      const char    *title,	/* I - Title of job */
              int           num_options,/* I - Number of options */
	      cups_option_t *options)	/* I - Options */
{
  int		i;			/* Looping var */
  int		n, n2;			/* Attribute values */
  char		*option,		/* Name of option */
		*val,			/* Pointer to option value */
		*s;			/* Pointer into option value */
  ipp_t		*request;		/* IPP request */
  ipp_t		*response;		/* IPP response */
  ipp_attribute_t *attr;		/* IPP job-id attribute */
  char		hostname[HTTP_MAX_URI],	/* Hostname */
		printer[HTTP_MAX_URI],	/* Printer or class name */
		uri[HTTP_MAX_URI];	/* Printer URI */
  cups_lang_t	*language;		/* Language to use */
  int		jobid;			/* New job ID */


  DEBUG_printf(("cupsPrintFile(\'%s\', \'%s\', %d, %08x)\n",
                printer, filename, num_options, options));

  if (name == NULL || filename == NULL)
    return (0);

 /*
  * Setup a connection and request data...
  */

  if ((request = ippNew()) == NULL)
    return (0);

  if (!cups_connect(name, printer, hostname))
  {
    DEBUG_printf(("cupsPrintFile: Unable to open connection - %s.\n",
                  strerror(errno)));
    ippDelete(request);
    return (0);
  }

 /*
  * Build a standard CUPS URI for the printer and fill the standard IPP
  * attributes...
  */

  request->request.op.operation_id = IPP_PRINT_JOB;
  request->request.op.request_id   = 1;

  snprintf(uri, sizeof(uri), "ipp://%s:%d/printers/%s", hostname, ippPort(), printer);

  language = cupsLangDefault();

  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_CHARSET,
               "attributes-charset", NULL, cupsLangEncoding(language));

  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_LANGUAGE,
               "attributes-natural-language", NULL,
               language != NULL ? language->language : "C");

  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_URI, "printer-uri",
               NULL, uri);

 /*
  * Handle raw print files...
  */

  if (cupsGetOption("raw", num_options, options))
    ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_MIMETYPE, "document-format",
        	 NULL, "application/vnd.cups-raw");
  else
    ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_MIMETYPE, "document-format",
        	 NULL, "application/octet-stream");

  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_NAME, "requesting-user-name",
               NULL, cupsUser());

  if (title)
    ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_NAME, "job-name", NULL, title);

 /*
  * Then add all options on the command-line...
  */

  for (i = 0; i < num_options; i ++)
  {
   /*
    * Skip the "raw" option - handled above...
    */

    if (strcasecmp(options[i].name, "raw") == 0)
      continue;

   /*
    * See what the option value is; for compatibility with older interface
    * scripts, we have to support single-argument options as well as
    * option=value, option=low-high, and option=MxN.
    */

    option = options[i].name;
    val    = options[i].value;

    if (*val == '\0')
      val = NULL;

    if (val != NULL)
    {
      if (strcasecmp(val, "true") == 0 ||
          strcasecmp(val, "on") == 0 ||
	  strcasecmp(val, "yes") == 0)
      {
       /*
	* Boolean value - true...
	*/

	n   = 1;
	val = "";
      }
      else if (strcasecmp(val, "false") == 0 ||
               strcasecmp(val, "off") == 0 ||
	       strcasecmp(val, "no") == 0)
      {
       /*
	* Boolean value - false...
	*/

	n   = 0;
	val = "";
      }

      n = strtol(val, &s, 0);
    }
    else
    {
      if (strncasecmp(option, "no", 2) == 0)
      {
	option += 2;
	n      = 0;
      }
      else
        n = 1;

      s = "";
    }

    if (*s != '\0' && *s != '-' && (*s != 'x' || s == val))
    {
     /*
      * String value(s)...
      */

      DEBUG_printf(("cupsPrintFile: Adding string option \'%s\' with value \'%s\'...\n",
                    option, val));

      ippAddString(request, IPP_TAG_JOB, IPP_TAG_KEYWORD, option, NULL, val);
    }
    else if (val != NULL)
    {
     /*
      * Numeric value, range, or resolution...
      */

      if (*s == '-')
      {
        n2 = strtol(s + 1, NULL, 0);
        ippAddRange(request, IPP_TAG_JOB, option, n, n2);

	DEBUG_printf(("cupsPrintFile: Adding range option \'%s\' with value %d-%d...\n",
                      option, n, n2));
      }
      else if (*s == 'x')
      {
        n2 = strtol(s + 1, &s, 0);

	if (strcasecmp(s, "dpc") == 0)
          ippAddResolution(request, IPP_TAG_JOB, option, IPP_RES_PER_CM, n, n2);
        else if (strcasecmp(s, "dpi") == 0)
          ippAddResolution(request, IPP_TAG_JOB, option, IPP_RES_PER_INCH, n, n2);
        else
          ippAddString(request, IPP_TAG_JOB, IPP_TAG_KEYWORD, option, NULL, val);

	DEBUG_printf(("cupsPrintFile: Adding resolution option \'%s\' with value %s...\n",
                      option, val));
      }
      else
      {
        ippAddInteger(request, IPP_TAG_JOB, IPP_TAG_INTEGER, option, n);

	DEBUG_printf(("cupsPrintFile: Adding integer option \'%s\' with value %d...\n",
                      option, n));
      }
    }
    else
    {
     /*
      * Boolean value...
      */

      DEBUG_printf(("cupsPrintFile: Adding boolean option \'%s\' with value %d...\n",
                    option, n));
      ippAddBoolean(request, IPP_TAG_JOB, option, (char)n);
    }
  }

 /*
  * Try printing the file...
  */

  snprintf(uri, sizeof(uri), "/printers/%s", printer);

  if ((response = cupsDoFileRequest(cups_server, request, uri, filename)) == NULL)
    jobid = 0;
  else if (response->request.status.status_code > IPP_OK_CONFLICT)
  {
    DEBUG_printf(("IPP response code was 0x%x!\n",
                  response->request.status.status_code));
    jobid = 0;
  }
  else if ((attr = ippFindAttribute(response, "job-id", IPP_TAG_INTEGER)) == NULL)
  {
    DEBUG_puts("No job ID!");
    jobid = 0;
  }
  else
    jobid = attr->values[0].integer;

  if (response != NULL)
    ippDelete(response);

  return (jobid);
}


/*
 * 'cupsTempFile()' - Generate a temporary filename.
 */

char *					/* O - Filename */
cupsTempFile(char *filename,		/* I - Pointer to buffer */
             int  len)			/* I - Size of buffer */
{
  char		*tmpdir;		/* TMPDIR environment var */
  static char	buf[1024] = "";		/* Buffer if you pass in NULL and 0 */


 /*
  * See if a filename was specified...
  */

  if (filename == NULL)
  {
    filename = buf;
    len      = sizeof(buf);
  }

 /*
  * See if TMPDIR is defined...
  */

  if ((tmpdir = getenv("TMPDIR")) == NULL)
    tmpdir = "/var/tmp";

  if ((int)(strlen(tmpdir) + 8) > len)
  {
   /*
    * The specified directory exceeds the size of the buffer; default it...
    */

    strcpy(buf, "/var/tmp/XXXXXX");
    return (mktemp(buf));
  }
  else
  {
   /*
    * Make the temporary name using the specified directory...
    */

    sprintf(filename, "%s/XXXXXX", tmpdir);
    return (mktemp(filename));
  }
}


/*
 * 'cups_connect()' - Connect to the specified host...
 */

static char *				/* I - Printer name or NULL */
cups_connect(const char *name,		/* I - Destination (printer[@host]) */
	     char       *printer,	/* O - Printer name [HTTP_MAX_URI] */
             char       *hostname)	/* O - Hostname [HTTP_MAX_URI] */
{
  char		hostbuf[HTTP_MAX_URI];
				/* Name of host */
  static char	printerbuf[HTTP_MAX_URI];
				/* Name of printer or class */


  if (name == NULL)
  {
    last_error = IPP_BAD_REQUEST;
    return (NULL);
  }

  if (sscanf(name, "%1023[^@]@%1023s", printerbuf, hostbuf) == 1)
  {
    strncpy(hostbuf, cupsServer(), sizeof(hostbuf) - 1);
    hostbuf[sizeof(hostbuf) - 1] = '\0';
  }

  if (hostname != NULL)
  {
    strncpy(hostname, hostbuf, HTTP_MAX_URI - 1);
    hostname[HTTP_MAX_URI - 1] = '\0';
  }
  else
    hostname = hostbuf;

  if (printer != NULL)
  {
    strncpy(printer, printerbuf, HTTP_MAX_URI - 1);
    printer[HTTP_MAX_URI - 1] = '\0';
  }
  else
    printer = printerbuf;

  if (cups_server != NULL)
  {
    if (strcasecmp(cups_server->hostname, hostname) == 0)
      return (printer);

    httpClose(cups_server);
  }

  if ((cups_server = httpConnect(hostname, ippPort())) == NULL)
  {
    last_error = IPP_SERVICE_UNAVAILABLE;
    return (NULL);
  }
  else
    return (printer);
}


/*
 * 'cups_local_auth()' - Get the local authorization certificate if
 *                       available/applicable...
 */

static int			/* O - 1 if available, 0 if not */
cups_local_auth(http_t *http)	/* I - Connection */
{
#if defined(WIN32) || defined(__EMX__)
 /*
  * Currently WIN32 and OS-2 do not support the CUPS server...
  */

  return (0);
#else
  int	pid;			/* Current process ID */
  FILE	*fp;			/* Certificate file */
  char	filename[1024],		/* Certificate filename */
	certificate[33];	/* Certificate string */


 /*
  * See if we are accessing localhost...
  */

  if (ntohl(http->hostaddr.sin_addr.s_addr) != 0x7f000001 &&
      strcasecmp(http->hostname, "localhost") != 0)
    return (0);

 /*
  * Try opening a certificate file for this PID.  If that fails,
  * try the root certificate...
  */

  pid = getpid();
  sprintf(filename, CUPS_SERVERROOT "/certs/%d", pid);
  if ((fp = fopen(filename, "r")) == NULL && pid > 0)
    fp = fopen(CUPS_SERVERROOT "/certs/0", "r");

  if (fp == NULL)
    return (0);

 /*
  * Read the certificate from the file...
  */

  fgets(certificate, sizeof(certificate), fp);
  fclose(fp);

 /*
  * Set the authorization string and return...
  */

  sprintf(authstring, "Local %s", certificate);

  return (1);
#endif /* WIN32 || __EMX__ */
}


/*
 * End of "$Id: util.c,v 1.44 2000/01/04 13:45:37 mike Exp $".
 */
