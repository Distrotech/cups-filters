/*
 * "$Id$"
 *
 *   HTTP support routines for the Common UNIX Printing System (CUPS) scheduler.
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
 *   This file is subject to the Apple OS-Developed Software exception.
 *
 * Contents:
 *
 *   httpSeparate()     - Separate a Universal Resource Identifier into its
 *                        components.
 *   httpSeparate2()    - Separate a Universal Resource Identifier into its
 *                        components.
 *   httpStatus()       - Return a short string describing a HTTP status code.
 *   _cups_hstrerror()  - hstrerror() emulation function for Solaris and others...
 *   http_copy_decode() - Copy and decode a URI.
 */

/*
 * Include necessary headers...
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <ctype.h>
#include "string.h"

#include "http.h"
#include "ipp.h"


/*
 * Local functions...
 */

static const char	*http_copy_decode(char *dst, const char *src,
			                  int dstsize, const char *term);


/*
 * 'httpSeparate()' - Separate a Universal Resource Identifier into its
 *                    components.
 */

void
httpSeparate(const char *uri,		/* I - Universal Resource Identifier */
             char       *scheme,	/* O - Scheme [32] (http, https, etc.) */
	     char       *username,	/* O - Username [1024] */
	     char       *host,		/* O - Hostname [1024] */
	     int        *port,		/* O - Port number to use */
             char       *resource)	/* O - Resource/filename [1024] */
{
  httpSeparate2(uri, scheme, 32, username, HTTP_MAX_URI, host, HTTP_MAX_URI,
                port, resource, HTTP_MAX_URI);
}


/*
 * 'httpSeparate2()' - Separate a Universal Resource Identifier into its
 *                     components.
 */

void
httpSeparate2(const char *uri,		/* I - Universal Resource Identifier */
              char       *scheme,	/* O - Scheme (http, https, etc.) */
	      int        schemelen,	/* I - Size of scheme buffer */
	      char       *username,	/* O - Username */
	      int        usernamelen,	/* I - Size of username buffer */
	      char       *host,		/* O - Hostname */
	      int        hostlen,	/* I - Size of hostname buffer */
	      int        *port,		/* O - Port number to use */
              char       *resource,	/* O - Resource/filename */
	      int        resourcelen)	/* I - Size of resource buffer */
{
  char		*ptr;			/* Pointer into string... */
  const char	*atsign,		/* @ sign */
		*slash;			/* Separator */


 /*
  * Range check input...
  */

  if (uri == NULL || scheme == NULL || username == NULL || host == NULL ||
      port == NULL || resource == NULL)
    return;

 /*
  * Grab the scheme portion of the URI...
  */

  if (!strncmp(uri, "//", 2))
  {
   /*
    * Workaround for HP IPP client bug...
    */

    strlcpy(scheme, "ipp", schemelen);
  }
  else
  {
   /*
    * Standard URI with scheme...
    */

    uri = http_copy_decode(host, uri, hostlen, ":");

    if (*uri == ':')
      uri ++;

   /*
    * If the scheme contains a period or slash, then it's probably
    * hostname/filename...
    */

    if (strchr(host, '.') || strchr(host, '/') || !*uri)
    {
      if ((ptr = strchr(host, '/')) != NULL)
      {
	strlcpy(resource, ptr, resourcelen);
	*ptr = '\0';
      }
      else
	resource[0] = '\0';

      if (isdigit(*uri & 255))
      {
       /*
	* OK, we have "hostname:port[/resource]"...
	*/

	*port = strtol(uri, (char **)&uri, 10);

	if (*uri == '/')
	  http_copy_decode(resource, uri, resourcelen, "");
      }
      else
	*port = 631;

      strlcpy(scheme, "http", schemelen);
      username[0] = '\0';
      return;
    }
    else
    {
     /*
      * Copy scheme over...
      */

      strlcpy(scheme, host, schemelen);
    }
  }

 /*
  * If the scheme starts with less than 2 slashes then it is a local resource...
  */

  if (strncmp(uri, "//", 2))
  {
   /*
    * File-based URI...
    */

    http_copy_decode(resource, uri, resourcelen, "");

    username[0] = '\0';
    host[0]     = '\0';
    *port       = 0;
    return;
  }

 /*
  * Grab the username, if any...
  */

  uri += 2;

  if ((slash = strchr(uri, '/')) == NULL)
    slash = uri + strlen(uri);

  if ((atsign = strchr(uri, '@')) != NULL && atsign < slash)
  {
   /*
    * Get a username:password combo...
    */

    uri = http_copy_decode(username, uri, usernamelen, "@") + 1;
  }
  else
  {
   /*
    * No username:password combo...
    */

    username[0] = '\0';
  }

 /*
  * Grab the hostname...
  */

  if (uri[0] == '[')
  {
   /*
    * Get IPv6 address...
    */

    uri = http_copy_decode(host, uri, hostlen, "]");

    if (*uri == ']')
      uri ++;
  }
  else
  {
   /*
    * Get IPv4 address or hostname...
    */

    uri = http_copy_decode(host, uri, hostlen, ":/");
  }

  if (*uri != ':')
  {
   /*
    * Use a standard port number for the given scheme.
    */

    if (!strcmp(scheme, "http"))
      *port = 80;
    else if (!strcmp(scheme, "https"))
      *port = 443;
    else if (!strcmp(scheme, "ipp"))
      *port = 631;
    else if (!strcasecmp(scheme, "lpd"))
      *port = 515;
    else if (!strcmp(scheme, "socket"))	/* Not yet registered... */
      *port = 9100;
    else
      *port = 0;
  }
  else
  {
   /*
    * Parse port number...
    */

    *port = strtol(uri + 1, (char **)&uri, 10);
  }

  if (!*uri)
  {
   /*
    * No resource path...
    */

    resource[0] = '/';
    resource[1] = '\0';
    return;
  }

 /*
  * The remaining portion is the resource string...
  */

  http_copy_decode(resource, uri, resourcelen, "");
}


/*
 * 'httpStatus()' - Return a short string describing a HTTP status code.
 */

const char *				/* O - String or NULL */
httpStatus(http_status_t status)	/* I - HTTP status code */
{
  switch (status)
  {
    case HTTP_CONTINUE :
        return ("Continue");
    case HTTP_SWITCHING_PROTOCOLS :
        return ("Switching Protocols");
    case HTTP_OK :
        return ("OK");
    case HTTP_CREATED :
        return ("Created");
    case HTTP_ACCEPTED :
        return ("Accepted");
    case HTTP_NO_CONTENT :
        return ("No Content");
    case HTTP_NOT_MODIFIED :
        return ("Not Modified");
    case HTTP_BAD_REQUEST :
        return ("Bad Request");
    case HTTP_UNAUTHORIZED :
        return ("Unauthorized");
    case HTTP_FORBIDDEN :
        return ("Forbidden");
    case HTTP_NOT_FOUND :
        return ("Not Found");
    case HTTP_REQUEST_TOO_LARGE :
        return ("Request Entity Too Large");
    case HTTP_URI_TOO_LONG :
        return ("URI Too Long");
    case HTTP_UPGRADE_REQUIRED :
        return ("Upgrade Required");
    case HTTP_NOT_IMPLEMENTED :
        return ("Not Implemented");
    case HTTP_NOT_SUPPORTED :
        return ("Not Supported");
    default :
        return ("Unknown");
  }
}


#ifndef HAVE_HSTRERROR
/*
 * '_cups_hstrerror()' - hstrerror() emulation function for Solaris and others...
 */

const char *				/* O - Error string */
_cups_hstrerror(int error)		/* I - Error number */
{
  static const char * const errors[] =	/* Error strings */
		{
		  "OK",
		  "Host not found.",
		  "Try again.",
		  "Unrecoverable lookup error.",
		  "No data associated with name."
		};


  if (error < 0 || error > 4)
    return ("Unknown hostname lookup error.");
  else
    return (errors[error]);
}
#endif /* !HAVE_HSTRERROR */


/*
 * 'http_copy_decode()' - Copy and decode a URI.
 */

static const char *			/* O - New source pointer */
http_copy_decode(char       *dst,	/* O - Destination buffer */ 
                 const char *src,	/* I - Source pointer */
		 int        dstsize,	/* I - Destination size */
		 const char *term)	/* I - Terminating characters */
{
  char	*ptr,				/* Pointer into buffer */
	*end;				/* End of buffer */
  int	quoted;				/* Quoted character */


 /*
  * Copy the src to the destination until we hit a terminating character
  * or the end of the string.
  */

  for (ptr = dst, end = dst + dstsize - 1; *src && !strchr(term, *src); src ++)
    if (ptr < end)
    {
      if (*src == '%' && isxdigit(src[1] & 255) && isxdigit(src[2] & 255))
      {
       /*
	* Grab a hex-encoded character...
	*/

        src ++;
	if (isalpha(*src))
	  quoted = (tolower(*src) - 'a' + 10) << 4;
	else
	  quoted = (*src - '0') << 4;

        src ++;
	if (isalpha(*src))
	  quoted |= tolower(*src) - 'a' + 10;
	else
	  quoted |= *src - '0';

        *ptr++ = quoted;
      }
      else
	*ptr++ = *src;
    }

  *ptr = '\0';

  return (src);
}


/*
 * End of "$Id$".
 */
