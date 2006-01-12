/*
 * "$Id$"
 *
 *   CGI <-> IPP variable routines for the Common UNIX Printing System (CUPS).
 *
 *   Copyright 1997-2006 by Easy Software Products.
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
 *   cgiGetAttributes()    - Get the list of attributes that are needed
 *                           by the template file.
 *   cupsGetIPPObjects()   - Get the objects in an IPP response.
 *   cgiRewriteURL()       - Rewrite a printer URI into a web browser URL...
 *   cgiSetIPPObjectVars() - Set CGI variables from an IPP object.
 *   cgiSetIPPVars()       - Set CGI variables from an IPP response.
 */

/*
 * Include necessary headers...
 */

#include "cgi-private.h"


/*
 * 'cgiGetAttributes()' - Get the list of attributes that are needed
 *                        by the template file.
 */

void
cgiGetAttributes(ipp_t      *request,	/* I - IPP request */
                 const char *tmpl)	/* I - Base filename */
{
  int		num_attrs;		/* Number of attributes */
  char		*attrs[1000];		/* Attributes */
  int		i;			/* Looping var */
  char		filename[1024],		/* Filename */
		locale[16];		/* Locale name */
  const char	*directory,		/* Directory */
		*lang;			/* Language */
  FILE		*in;			/* Input file */
  int		ch;			/* Character from file */
  char		name[255],		/* Name of variable */
		*nameptr;		/* Pointer into name */


 /*
  * Convert the language to a locale name...
  */

  if ((lang = getenv("LANG")) != NULL)
  {
    for (i = 0; lang[i] && i < 15; i ++)
      if (isalnum(lang[i] & 255))
        locale[i] = tolower(lang[i]);
      else
        locale[i] = '_';

    locale[i] = '\0';
  }
  else
    locale[0] = '\0';

 /*
  * See if we have a template file for this language...
  */

  directory = cgiGetTemplateDir();

  snprintf(filename, sizeof(filename), "%s/%s/%s", directory, locale, tmpl);
  if (access(filename, 0))
  {
    locale[2] = '\0';

    snprintf(filename, sizeof(filename), "%s/%s/%s", directory, locale, tmpl);
    if (access(filename, 0))
      snprintf(filename, sizeof(filename), "%s/%s", directory, tmpl);
  }

 /*
  * Open the template file...
  */

  if ((in = fopen(filename, "r")) == NULL)
    return;

 /*
  * Loop through the file adding attribute names as needed...
  */

  num_attrs = 0;

  while ((ch = getc(in)) != EOF)
    if (ch == '\\')
      getc(in);
    else if (ch == '{' && num_attrs < (sizeof(attrs) / sizeof(attrs[0])))
    {
     /*
      * Grab the name...
      */

      for (nameptr = name; (ch = getc(in)) != EOF;)
        if (strchr("}]<>=! \t\n", ch))
          break;
        else if (nameptr > name && ch == '?')
	  break;
	else if (nameptr < (name + sizeof(name) - 1))
	{
	  if (ch == '_')
	    *nameptr++ = '-';
	  else
            *nameptr++ = ch;
	}

      *nameptr = '\0';

      if (!strncmp(name, "printer_state_history", 21))
        strcpy(name, "printer_state_history");

     /*
      * Possibly add it to the list of attributes...
      */

      for (i = 0; i < num_attrs; i ++)
        if (!strcmp(attrs[i], name))
	  break;

      if (i >= num_attrs)
      {
	attrs[num_attrs] = strdup(name);
	num_attrs ++;
      }
    }

 /*
  * If we have attributes, add a requested-attributes attribute to the
  * request...
  */

  if (num_attrs > 0)
  {
    ippAddStrings(request, IPP_TAG_OPERATION, IPP_TAG_KEYWORD,
                  "requested-attributes", num_attrs, NULL, (const char **)attrs);

    for (i = 0; i < num_attrs; i ++)
      free(attrs[i]);
  }
}


/*
 * 'cupsGetIPPObjects()' - Get the objects in an IPP response.
 */

cups_array_t *				/* O - Array of objects */
cgiGetIPPObjects(ipp_t *response,	/* I - IPP response */
                 void  *search)		/* I - Search filter */
{
  int			i;		/* Looping var */
  cups_array_t		*objs;		/* Array of objects */
  ipp_attribute_t	*attr,		/* Current attribute */
			*first;		/* First attribute for object */
  ipp_tag_t		group;		/* Current group tag */
  int			add;		/* Add this object to the array? */


  if (!response)
    return (0);

  for (add = 0, first = NULL, objs = cupsArrayNew(NULL, NULL),
           group = IPP_TAG_ZERO, attr = response->attrs;
       attr;
       attr = attr->next)
  {
    if (attr->group_tag != group)
    {
      group = attr->group_tag;

      if (group != IPP_TAG_ZERO && group != IPP_TAG_OPERATION)
      {
        first = attr;
	add   = 0;
      }
      else if (add && first)
      {
        cupsArrayAdd(objs, first);

	add   = 0;
	first = NULL;
      }
    }

    if (attr->name && attr->group_tag != IPP_TAG_OPERATION && !add)
    {
      if (!search)
      {
       /*
        * Add all objects if there is no search...
	*/

        add = 1;
      }
      else
      {
       /*
        * Check the search string against the string and integer values.
	*/

        switch (attr->value_tag)
	{
	  case IPP_TAG_TEXTLANG :
	  case IPP_TAG_NAMELANG :
	  case IPP_TAG_TEXT :
	  case IPP_TAG_NAME :
	  case IPP_TAG_KEYWORD :
	  case IPP_TAG_URI :
	  case IPP_TAG_MIMETYPE :
	      for (i = 0; !add && i < attr->num_values; i ++)
		if (cgiDoSearch(search, attr->values[i].string.text))
		  add = 1;
	      break;

          case IPP_TAG_INTEGER :
	      for (i = 0; !add && i < attr->num_values; i ++)
	      {
	        char	buf[255];	/* Number buffer */


                sprintf(buf, "%d", attr->values[i].integer);

		if (cgiDoSearch(search, buf))
		  add = 1;
	      }
	      break;

          default :
	      break;
	}
      }
    }
  }

  if (add && first)
    cupsArrayAdd(objs, first);

  return (objs);
}


/*
 * 'cgiRewriteURL()' - Rewrite a printer URI into a web browser URL...
 */

char *					/* O - New URL */
cgiRewriteURL(const char *uri,		/* I - Current URI */
              char       *url,		/* O - New URL */
	      int        urlsize,	/* I - Size of URL buffer */
	      const char *newresource)	/* I - Replacement resource */
{
  char			method[HTTP_MAX_URI],
			userpass[HTTP_MAX_URI],
			hostname[HTTP_MAX_URI],
			rawresource[HTTP_MAX_URI],
			resource[HTTP_MAX_URI],
					/* URI components... */
			*rawptr,	/* Pointer into rawresource */
			*resptr;	/* Pointer into resource */
  int			port;		/* Port number */
  static int		ishttps = -1;	/* Using encryption? */
  static const char	*server;	/* Name of server */
  static char		servername[1024];
					/* Local server name */
  static const char	hexchars[] = "0123456789ABCDEF";
					/* Hexadecimal conversion characters */


 /*
  * Check if we have been called before...
  */

  if (ishttps < 0)
  {
   /*
    * No, initialize static vars for the conversion...
    *
    * First get the server name associated with the client interface as
    * well as the locally configured hostname.  We'll check *both* of
    * these to see if the printer URL is local...
    */

    if ((server = getenv("SERVER_NAME")) == NULL)
      server = "";

    httpGetHostname(servername, sizeof(servername));

   /*
    * Then flag whether we are using SSL on this connection...
    */

    ishttps = getenv("HTTPS") != NULL;
  }

 /*
  * Convert the URI to a URL...
  */

  httpSeparateURI(uri, method, sizeof(method), userpass, sizeof(userpass),
                  hostname, sizeof(hostname), &port,
		  rawresource, sizeof(rawresource));

  if (!strcmp(method, "ipp") ||
      !strcmp(method, "http") ||
      !strcmp(method, "https"))
  {
    if (newresource)
    {
     /*
      * Force the specified resource name instead of the one in the URL...
      */

      strlcpy(resource, newresource, sizeof(resource));
    }
    else
    {
     /*
      * Rewrite the resource string so it doesn't contain any
      * illegal chars...
      */

      for (rawptr = rawresource, resptr = resource; *rawptr; rawptr ++)
	if ((*rawptr & 128) || *rawptr == '%' || *rawptr == ' ' ||
	    *rawptr == '#' || *rawptr == '?' ||
	    *rawptr == '.') /* For MSIE */
	{
	  if (resptr < (resource + sizeof(resource) - 3))
	  {
	    *resptr++ = '%';
	    *resptr++ = hexchars[(*rawptr >> 4) & 15];
	    *resptr++ = hexchars[*rawptr & 15];
	  }
	}
	else if (resptr < (resource + sizeof(resource) - 1))
	  *resptr++ = *rawptr;

      *resptr = '\0';
    }

   /*
    * Map local access to a local URI...
    */

    if (!strcasecmp(hostname, "localhost") ||
	!strncasecmp(hostname, "localhost.", 10) ||
	!strcasecmp(hostname, server) ||
	!strcasecmp(hostname, servername))
    {
     /*
      * Make URI relative to the current server...
      */

      strlcpy(url, resource, urlsize);
    }
    else
    {
     /*
      * Rewrite URI with HTTP/HTTPS scheme...
      */

      if (userpass[0])
	snprintf(url, urlsize, "%s://%s@%s:%d%s",
		 ishttps ? "https" : "http",
		 userpass, hostname, port, resource);
      else
	snprintf(url, urlsize, "%s://%s:%d%s", 
		 ishttps ? "https" : "http",
		 hostname, port, resource);
    }
  }
  else
    strlcpy(url, uri, urlsize);

  return (url);
}


/*
 * 'cgiSetIPPObjectVars()' - Set CGI variables from an IPP object.
 */

ipp_attribute_t *			/* O - Next object */
cgiSetIPPObjectVars(
    ipp_attribute_t *obj,		/* I - Response data to be copied... */
    const char      *prefix,		/* I - Prefix for name or NULL */
    int             element)		/* I - Parent element number */
{
  ipp_attribute_t	*attr;		/* Attribute in response... */
  int			i;		/* Looping var */
  char			name[1024],	/* Name of attribute */
			*nameptr,	/* Pointer into name */
			value[16384],	/* Value(s) */
			*valptr;	/* Pointer into value */
  struct tm		*date;		/* Date information */


  fprintf(stderr, "DEBUG2: cgiSetIPPObjectVars(obj=%p, prefix=\"%s\", "
                  "element=%d)\n",
          obj, prefix, element);

 /*
  * Set common CGI template variables...
  */

  if (!prefix)
    cgiSetServerVersion();

 /*
  * Loop through the attributes and set them for the template...
  */

  for (attr = obj; attr && attr->group_tag != IPP_TAG_ZERO; attr = attr->next)
  {
   /*
    * Copy the attribute name, substituting "_" for "-"...
    */

    if (!attr->name)
      continue;

    if (prefix)
    {
      snprintf(name, sizeof(name), "%s.", prefix);
      nameptr = name + strlen(name);
    }
    else
      nameptr = name;

    for (i = 0; attr->name[i] && nameptr < (name + sizeof(name) - 1); i ++)
      if (attr->name[i] == '-')
	*nameptr++ = '_';
      else
        *nameptr++ = attr->name[i];

    *nameptr = '\0';

   /*
    * Add "job_printer_name" variable if we have a "job_printer_uri"
    * attribute...
    */

    if (!strcmp(name, "job_printer_uri"))
    {
      if ((valptr = strrchr(attr->values[0].string.text, '/')) == NULL)
	valptr = "unknown";
      else
	valptr ++;

      cgiSetArray("job_printer_name", element, valptr);
    }

   /*
    * Add "admin_uri" variable if we have a "printer_uri_supported"
    * attribute...
    */

    if (!strcmp(name, "printer_uri_supported"))
    {
      cgiRewriteURL(attr->values[0].string.text, value, sizeof(value),
	            "/admin/");

      cgiSetArray("admin_uri", element, value);
    }

   /*
    * Copy values...
    */

    value[0] = '\0';	/* Initially an empty string */
    valptr   = value; /* Start at the beginning */

    for (i = 0; i < attr->num_values; i ++)
    {
      if (i)
	strlcat(valptr, ",", sizeof(value) - (valptr - value));

      valptr += strlen(valptr);

      switch (attr->value_tag)
      {
	case IPP_TAG_INTEGER :
	case IPP_TAG_ENUM :
	    if (strncmp(name, "time_at_", 8) == 0)
	    {
	      time_t	t;		/* Temporary time value */

              t    = (time_t)attr->values[i].integer;
	      date = localtime(&t);

	      strftime(valptr, sizeof(value) - (valptr - value), "%c", date);
	    }
	    else
	      snprintf(valptr, sizeof(value) - (valptr - value),
		       "%d", attr->values[i].integer);
	    break;

	case IPP_TAG_BOOLEAN :
	    snprintf(valptr, sizeof(value) - (valptr - value),
	             "%d", attr->values[i].boolean);
	    break;

	case IPP_TAG_NOVALUE :
	    strlcat(valptr, "novalue", sizeof(value) - (valptr - value));
	    break;

	case IPP_TAG_RANGE :
	    snprintf(valptr, sizeof(value) - (valptr - value),
	             "%d-%d", attr->values[i].range.lower,
		     attr->values[i].range.upper);
	    break;

	case IPP_TAG_RESOLUTION :
	    snprintf(valptr, sizeof(value) - (valptr - value),
	             "%dx%d%s", attr->values[i].resolution.xres,
		     attr->values[i].resolution.yres,
		     attr->values[i].resolution.units == IPP_RES_PER_INCH ?
			 "dpi" : "dpc");
	    break;

	case IPP_TAG_URI :
	    if (strchr(attr->values[i].string.text, ':') != NULL)
	    {
	     /*
	      * Rewrite URIs...
	      */

              if (!strcmp(name, "member_uris"))
	      {
		char	url[1024];	/* URL for class member... */


		cgiRewriteURL(attr->values[i].string.text, url,
		              sizeof(url), NULL);

                snprintf(valptr, sizeof(value) - (valptr - value),
		         "<A HREF=\"%s\">%s</A>", url,
			 strrchr(url, '/') + 1);
	      }
	      else
		cgiRewriteURL(attr->values[i].string.text, valptr,
		              sizeof(value) - (valptr - value), NULL);
              break;
            }

        case IPP_TAG_STRING :
	case IPP_TAG_TEXT :
	case IPP_TAG_NAME :
	case IPP_TAG_KEYWORD :
	case IPP_TAG_CHARSET :
	case IPP_TAG_LANGUAGE :
	case IPP_TAG_MIMETYPE :
	    strlcat(valptr, attr->values[i].string.text,
	            sizeof(value) - (valptr - value));
	    break;

        case IPP_TAG_BEGIN_COLLECTION :
	    snprintf(value, sizeof(value), "%s%d", name, i + 1);
            cgiSetIPPVars(attr->values[i].collection, NULL, NULL, value,
	                  element);
            break;

        default :
	    break; /* anti-compiler-warning-code */
      }
    }

   /*
    * Add the element...
    */

    if (attr->value_tag != IPP_TAG_BEGIN_COLLECTION)
    {
      cgiSetArray(name, element, value);

      fprintf(stderr, "DEBUG2: %s[%d]=\"%s\"\n", name, element, value);
    }
  }

  return (attr ? attr->next : NULL);
}


/*
 * 'cgiSetIPPVars()' - Set CGI variables from an IPP response.
 */

int					/* O - Maximum number of elements */
cgiSetIPPVars(ipp_t      *response,	/* I - Response data to be copied... */
              const char *filter_name,	/* I - Filter name */
	      const char *filter_value,	/* I - Filter value */
	      const char *prefix,	/* I - Prefix for name or NULL */
	      int        parent_el)	/* I - Parent element number */
{
  int			element;	/* Element in CGI array */
  ipp_attribute_t	*attr,		/* Attribute in response... */
			*filter;	/* Filtering attribute */


  fprintf(stderr, "DEBUG2: cgiSetIPPVars(response=%p, filter_name=\"%s\", "
                  "filter_value=\"%s\", prefix=\"%s\", parent_el=%d)\n",
          response, filter_name, filter_value, prefix, parent_el);

 /*
  * Set common CGI template variables...
  */

  if (!prefix)
    cgiSetServerVersion();

 /*
  * Loop through the attributes and set them for the template...
  */

  attr = response->attrs;

  if (!prefix)
    while (attr && attr->group_tag == IPP_TAG_OPERATION)
      attr = attr->next;

  for (element = parent_el; attr; element ++)
  {
   /*
    * Copy attributes to a separator...
    */

    while (attr && attr->group_tag == IPP_TAG_ZERO)
      attr= attr->next;

    if (!attr)
      break;

    if (filter_name)
    {
      for (filter = attr;
           filter != NULL && filter->group_tag != IPP_TAG_ZERO;
           filter = filter->next)
        if (filter->name && !strcmp(filter->name, filter_name) &&
	    (filter->value_tag == IPP_TAG_STRING ||
	     (filter->value_tag >= IPP_TAG_TEXTLANG &&
	      filter->value_tag <= IPP_TAG_MIMETYPE)) &&
	    filter->values[0].string.text != NULL &&
	    !strcasecmp(filter->values[0].string.text, filter_value))
	  break;

      if (!filter)
        return (element + 1);

      if (filter->group_tag == IPP_TAG_ZERO)
      {
        attr = filter;
	element --;
	continue;
      }
    }

    attr = cgiSetIPPObjectVars(attr, prefix, element);
  }

  fprintf(stderr, "DEBUG2: Returing %d from cgiSetIPPVars()...\n", element + 1);

  return (element + 1);
}


/*
 * 'cgiShowJobs()' - Show print jobs.
 */

void
cgiShowJobs(http_t     *http,		/* I - Connection to server */
            const char *dest)		/* I - Destination name or NULL */
{
  int			i;		/* Looping var */
  const char		*which_jobs;	/* Which jobs to show */
  ipp_t			*request,	/* IPP request */
			*response;	/* IPP response */
  cups_array_t		*jobs;		/* Array of job objects */
  ipp_attribute_t	*job;		/* Job object */
  int			ascending,	/* Order of jobs (0 = descending) */
			first,		/* First job to show */
			count;		/* Number of jobs */
  const char		*var;		/* Form variable */
  void			*search;	/* Search data */
  char			url[1024],	/* URL for prev/next/this */
			*urlptr,	/* Position in URL */
			*urlend;	/* End of URL */
  cups_lang_t		*language;	/* Language information */


 /*
  * Build an IPP_GET_JOBS request, which requires the following
  * attributes:
  *
  *    attributes-charset
  *    attributes-natural-language
  *    printer-uri
  */

  request = ippNew();

  language = cupsLangDefault();

  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_CHARSET,
               "attributes-charset", NULL, cupsLangEncoding(language));

  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_LANGUAGE,
               "attributes-natural-language", NULL, language->language);

  request->request.op.operation_id = IPP_GET_JOBS;
  request->request.op.request_id   = 1;

  if (dest)
  {
    httpAssembleURIf(url, sizeof(url), "ipp", NULL, "localhost", ippPort(),
                     "/printers/%s", dest);
    ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_URI, "printer-uri",
                 NULL, url);
  }
  else
    ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_URI, "job-uri", NULL,
        	 "ipp://localhost/jobs");

  if ((which_jobs = cgiGetVariable("which_jobs")) != NULL)
    ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_KEYWORD, "which-jobs",
                 NULL, which_jobs);

  cgiGetAttributes(request, "jobs.tmpl");

 /*
  * Do the request and get back a response...
  */

  if ((response = cupsDoRequest(http, request, "/")) != NULL)
  {
   /*
    * Get a list of matching job objects.
    */

    if (!dest && (var = cgiGetVariable("QUERY")) != NULL)
      search = cgiCompileSearch(var);
    else
      search = NULL;

    jobs  = cgiGetIPPObjects(response, search);
    count = cupsArrayCount(jobs);

    if (search)
      cgiFreeSearch(search);

   /*
    * Figure out which jobs to display...
    */

    if ((var = cgiGetVariable("FIRST")) != NULL)
      first = atoi(var);
    else
      first = 0;

    if (first >= count)
      first = count - CUPS_PAGE_MAX;

    first = (first / CUPS_PAGE_MAX) * CUPS_PAGE_MAX;

    if (first < 0)
      first = 0;

    sprintf(url, "%d", count);
    cgiSetVariable("TOTAL", url);

    if ((var = cgiGetVariable("ORDER")) != NULL)
      ascending = !strcasecmp(var, "asc");
    else
      ascending = 1;

    if (ascending)
    {
      for (i = 0, job = (ipp_attribute_t *)cupsArrayIndex(jobs, first);
	   i < CUPS_PAGE_MAX && job;
	   i ++, job = (ipp_attribute_t *)cupsArrayNext(jobs))
        cgiSetIPPObjectVars(job, NULL, i);
    }
    else
    {
      for (i = 0, job = (ipp_attribute_t *)cupsArrayIndex(jobs, count - first - 1);
	   i < CUPS_PAGE_MAX && job;
	   i ++, job = (ipp_attribute_t *)cupsArrayPrev(jobs))
        cgiSetIPPObjectVars(job, NULL, i);
    }

   /*
    * Save navigation URLs...
    */

    urlend = url + sizeof(url);

    if (dest)
    {
      snprintf(url, sizeof(url), "/%s/%s?", cgiGetVariable("SECTION"), dest);
      urlptr = url + strlen(url);
    }
    else if ((var = cgiGetVariable("QUERY")) != NULL)
    {
      strlcpy(url, "/jobs/?QUERY=", sizeof(url));
      urlptr = url + strlen(url);

      cgiFormEncode(urlptr, var, urlend - urlptr);
      urlptr += strlen(urlptr);

      strlcpy(urlptr, "&", urlend - urlptr);
      urlptr += strlen(urlptr);
    }
    else
    {
      strlcpy(url, "/jobs/?", sizeof(url));
      urlptr = url + strlen(url);
    }

    if (which_jobs)
    {
      strlcpy(urlptr, "WHICH_JOBS=", urlend - urlptr);
      urlptr += strlen(urlptr);

      cgiFormEncode(urlptr, which_jobs, urlend - urlptr);
      urlptr += strlen(urlptr);

      strlcpy(urlptr, "&", urlend - urlptr);
      urlptr += strlen(urlptr);
    }

    snprintf(urlptr, urlend - urlptr, "FIRST=%d", first);
    cgiSetVariable("THISURL", url);

    if (first > 0)
    {
      snprintf(urlptr, urlend - urlptr, "FIRST=%d&ORDER=%s",
	       first - CUPS_PAGE_MAX, ascending ? "asc" : "dec");
      cgiSetVariable("PREVURL", url);
    }

    if ((first + CUPS_PAGE_MAX) < count)
    {
      snprintf(urlptr, urlend - urlptr, "FIRST=%d&ORDER=%s",
	       first + CUPS_PAGE_MAX, ascending ? "asc" : "dec");
      cgiSetVariable("NEXTURL", url);
    }

   /*
    * Then show everything...
    */

    if (!dest)
      cgiCopyTemplateLang("search.tmpl");

    cgiCopyTemplateLang("jobs-header.tmpl");

    if (count > CUPS_PAGE_MAX)
      cgiCopyTemplateLang("page.tmpl");

    cgiCopyTemplateLang("jobs.tmpl");

    if (count > CUPS_PAGE_MAX)
      cgiCopyTemplateLang("page.tmpl");

    ippDelete(response);
  }
}



/*
 * End of "$Id$".
 */
