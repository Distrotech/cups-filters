/*
 * "$Id: classes.c,v 1.14 2000/03/30 05:19:19 mike Exp $"
 *
 *   Class status CGI for the Common UNIX Printing System (CUPS).
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
 *   main() - Main entry for CGI.
 */

/*
 * Include necessary headers...
 */

#include "ipp-var.h"


/*
 * 'main()' - Main entry for CGI.
 */

int				/* O - Exit status */
main(int  argc,			/* I - Number of command-line arguments */
     char *argv[])		/* I - Command-line arguments */
{
  cups_lang_t	*language;	/* Language information */
  char		*pclass;	/* Printer class name */
  http_t	*http;		/* Connection to the server */
  ipp_t		*request,	/* IPP request */
		*response;	/* IPP response */
  char		uri[HTTP_MAX_URI];
				/* Printer URI */
  const char	*which_jobs;	/* Which jobs to show */
 

 /*
  * Get any form variables...
  */

  cgiInitialize();

 /*
  * Get the request language...
  */

  language = cupsLangDefault();

 /*
  * Connect to the HTTP server...
  */

  http = httpConnect("localhost", ippPort());

 /*
  * Tell the client to expect HTML...
  */

  printf("Content-Type: text/html;charset=%s\n\n", cupsLangEncoding(language));

 /*
  * See if we need to show a list of printers or the status of a
  * single printer...
  */

  pclass = argv[0];
  if (strcmp(pclass, "/") == 0 || strcmp(pclass, "classes.cgi") == 0)
  {
    pclass = NULL;
    cgiSetVariable("TITLE", "Classes");
  }
  else
    cgiSetVariable("TITLE", pclass);

 /*
  * Get the printer info...
  */

  request = ippNew();

  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_CHARSET,
               "attributes-charset", NULL, cupsLangEncoding(language));

  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_LANGUAGE,
               "attributes-natural-language", NULL, language->language);

  if (pclass == NULL)
  {
   /*
    * Build a CUPS_GET_CLASSES request, which requires the following
    * attributes:
    *
    *    attributes-charset
    *    attributes-natural-language
    */

    request->request.op.operation_id = CUPS_GET_CLASSES;
    request->request.op.request_id   = 1;
  }
  else
  {
   /*
    * Build an IPP_GET_PRINTER_ATTRIBUTES request, which requires the following
    * attributes:
    *
    *    attributes-charset
    *    attributes-natural-language
    *    printer-uri
    */

    request->request.op.operation_id = IPP_GET_PRINTER_ATTRIBUTES;
    request->request.op.request_id   = 1;

    snprintf(uri, sizeof(uri), "ipp://%s/classes/%s", getenv("SERVER_NAME"),
             pclass);
    ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_URI, "printer-uri", NULL,
                 uri);
  }

 /*
  * Do the request and get back a response...
  */

  if ((response = cupsDoRequest(http, request, "/")) != NULL)
  {
    ippSetCGIVars(response);
    ippDelete(response);
  }

 /*
  * Write the report...
  */

  cgiCopyTemplateLang(stdout, TEMPLATES, "header.tmpl", getenv("LANG"));
  cgiCopyTemplateLang(stdout, TEMPLATES, "classes.tmpl", getenv("LANG"));

 /*
  * Get jobs for the specified class if a class has been chosen...
  */

  if (pclass != NULL)
  {
   /*
    * Build an IPP_GET_JOBS request, which requires the following
    * attributes:
    *
    *    attributes-charset
    *    attributes-natural-language
    *    printer-uri
    */

    request = ippNew();

    ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_CHARSET,
        	 "attributes-charset", NULL, cupsLangEncoding(language));

    ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_LANGUAGE,
        	 "attributes-natural-language", NULL, language->language);

    request->request.op.operation_id = IPP_GET_JOBS;
    request->request.op.request_id   = 1;

    snprintf(uri, sizeof(uri), "ipp://%s/classes/%s", getenv("SERVER_NAME"),
             pclass);
    ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_URI, "printer-uri", NULL,
                 uri);

    if ((which_jobs = cgiGetVariable("which_jobs")) != NULL)
      ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_KEYWORD, "which-jobs",
                   NULL, which_jobs);

   /*
    * Do the request and get back a response...
    */

    if ((response = cupsDoRequest(http, request, "/")) != NULL)
    {
      ippSetCGIVars(response);
      ippDelete(response);

      cgiCopyTemplateLang(stdout, TEMPLATES, "jobs.tmpl", getenv("LANG"));
    }
  }

  cgiCopyTemplateLang(stdout, TEMPLATES, "trailer.tmpl", getenv("LANG"));

 /*
  * Close the HTTP server connection...
  */

  httpClose(http);
  cupsLangFree(language);

 /*
  * Return with no errors...
  */

  return (0);
}


/*
 * End of "$Id: classes.c,v 1.14 2000/03/30 05:19:19 mike Exp $".
 */
