/*
 * "$Id: http.c,v 1.82.2.14 2002/05/16 13:59:58 mike Exp $"
 *
 *   HTTP routines for the Common UNIX Printing System (CUPS).
 *
 *   Copyright 1997-2002 by Easy Software Products, all rights reserved.
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
 *   This file is subject to the Apple OS-Developed Software exception.
 *
 * Contents:
 *
 *   httpInitialize()     - Initialize the HTTP interface library and set the
 *                          default HTTP proxy (if any).
 *   httpCheck()          - Check to see if there is a pending response from
 *                          the server.
 *   httpClose()          - Close an HTTP connection...
 *   httpConnect()        - Connect to a HTTP server.
 *   httpConnectEncrypt() - Connect to a HTTP server using encryption.
 *   httpEncryption()     - Set the required encryption on the link.
 *   httpReconnect()      - Reconnect to a HTTP server...
 *   httpGetHostByName()  - Lookup a hostname or IP address, and return
 *                          address records for the specified name.
 *   httpSeparate()       - Separate a Universal Resource Identifier into its
 *                          components.
 *   httpSetField()       - Set the value of an HTTP header.
 *   httpDelete()         - Send a DELETE request to the server.
 *   httpGet()            - Send a GET request to the server.
 *   httpHead()           - Send a HEAD request to the server.
 *   httpOptions()        - Send an OPTIONS request to the server.
 *   httpPost()           - Send a POST request to the server.
 *   httpPut()            - Send a PUT request to the server.
 *   httpTrace()          - Send an TRACE request to the server.
 *   httpFlush()          - Flush data from a HTTP connection.
 *   httpRead()           - Read data from a HTTP connection.
 *   httpWrite()          - Write data to a HTTP connection.
 *   httpGets()           - Get a line of text from a HTTP connection.
 *   httpPrintf()         - Print a formatted string to a HTTP connection.
 *   httpStatus()         - Return a short string describing a HTTP status code.
 *   httpGetDateString()  - Get a formatted date/time string from a time value.
 *   httpGetDateTime()    - Get a time value from a formatted date/time string.
 *   httpUpdate()         - Update the current HTTP state for incoming data.
 *   httpDecode64()       - Base64-decode a string.
 *   httpEncode64()       - Base64-encode a string.
 *   httpGetLength()      - Get the amount of data remaining from the
 *                          content-length or transfer-encoding fields.
 *   http_field()         - Return the field index for a field name.
 *   http_send()          - Send a request with all fields and the trailing
 *                          blank line.
 *   http_upgrade()       - Force upgrade to TLS encryption.
 */

/*
 * Include necessary headers...
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <ctype.h>
#include "string.h"
#include <fcntl.h>
#include <errno.h>

#include "http.h"
#include "ipp.h"
#include "debug.h"

#ifndef WIN32
#  include <signal.h>
#endif /* !WIN32 */

#ifdef HAVE_LIBSSL
#  include <openssl/err.h>
#  include <openssl/rand.h>
#  include <openssl/ssl.h>
#endif /* HAVE_LIBSSL */


/*
 * Some operating systems have done away with the Fxxxx constants for
 * the fcntl() call; this works around that "feature"...
 */

#ifndef FNONBLK
#  define FNONBLK O_NONBLOCK
#endif /* !FNONBLK */


/*
 * Local functions...
 */

static http_field_t	http_field(const char *name);
static int		http_send(http_t *http, http_state_t request,
			          const char *uri);
#ifdef HAVE_LIBSSL
static int		http_upgrade(http_t *http);
#endif /* HAVE_LIBSSL */


/*
 * Local globals...
 */

static const char	*http_fields[] =
			{
			  "Accept-Language",
			  "Accept-Ranges",
			  "Authorization",
			  "Connection",
			  "Content-Encoding",
			  "Content-Language",
			  "Content-Length",
			  "Content-Location",
			  "Content-MD5",
			  "Content-Range",
			  "Content-Type",
			  "Content-Version",
			  "Date",
			  "Host",
			  "If-Modified-Since",
			  "If-Unmodified-since",
			  "Keep-Alive",
			  "Last-Modified",
			  "Link",
			  "Location",
			  "Range",
			  "Referer",
			  "Retry-After",
			  "Transfer-Encoding",
			  "Upgrade",
			  "User-Agent",
			  "WWW-Authenticate"
			};
static const char	*days[7] =
			{
			  "Sun",
			  "Mon",
			  "Tue",
			  "Wed",
			  "Thu",
			  "Fri",
			  "Sat"
			};
static const char	*months[12] =
			{
			  "Jan",
			  "Feb",
			  "Mar",
			  "Apr",
			  "May",
			  "Jun",
		          "Jul",
			  "Aug",
			  "Sep",
			  "Oct",
			  "Nov",
			  "Dec"
			};


/*
 * 'httpInitialize()' - Initialize the HTTP interface library and set the
 *                      default HTTP proxy (if any).
 */

void
httpInitialize(void)
{
#ifdef HAVE_LIBSSL
  struct timeval	curtime;	/* Current time in microseconds */
  int			i;		/* Looping var */
  unsigned char		data[1024];	/* Seed data */
#endif /* HAVE_LIBSSL */

#ifdef WIN32
  WSADATA	winsockdata;	/* WinSock data */
  static int	initialized = 0;/* Has WinSock been initialized? */


  if (!initialized)
    WSAStartup(MAKEWORD(1,1), &winsockdata);
#elif defined(HAVE_SIGSET)
  sigset(SIGPIPE, SIG_IGN);
#elif defined(HAVE_SIGACTION)
  struct sigaction	action;	/* POSIX sigaction data */


 /*
  * Ignore SIGPIPE signals...
  */

  memset(&action, 0, sizeof(action));
  action.sa_handler = SIG_IGN;
  sigaction(SIGPIPE, &action, NULL);
#else
  signal(SIGPIPE, SIG_IGN);
#endif /* WIN32 */

#ifdef HAVE_LIBSSL
  SSL_load_error_strings();
  SSL_library_init();

 /*
  * Using the current time is a dubious random seed, but on some systems
  * it is the best we can do (on others, this seed isn't even used...)
  */

  gettimeofday(&curtime, NULL);
  srand(curtime.tv_sec + curtime.tv_usec);

  for (i = 0; i < sizeof(data); i ++)
    data[i] = rand(); /* Yes, this is a poor source of random data... */

  RAND_seed(&data, sizeof(data));
#endif /* HAVE_LIBSSL */
}


/*
 * 'httpCheck()' - Check to see if there is a pending response from the server.
 */

int				/* O - 0 = no data, 1 = data available */
httpCheck(http_t *http)		/* I - HTTP connection */
{
  fd_set	input;		/* Input set for select() */
  struct timeval timeout;	/* Timeout */


 /*
  * First see if there is data in the buffer...
  */

  if (http == NULL)
    return (0);

  if (http->used)
    return (1);

 /*
  * Then try doing a select() to poll the socket...
  */

  FD_ZERO(&input);
  FD_SET(http->fd, &input);

  timeout.tv_sec  = 0;
  timeout.tv_usec = 0;

  return (select(http->fd + 1, &input, NULL, NULL, &timeout) > 0);
}


/*
 * 'httpClose()' - Close an HTTP connection...
 */

void
httpClose(http_t *http)		/* I - Connection to close */
{
#ifdef HAVE_LIBSSL
  SSL_CTX	*context;	/* Context for encryption */
  SSL		*conn;		/* Connection for encryption */
#endif /* HAVE_LIBSSL */


  if (!http)
    return;

#ifdef HAVE_LIBSSL
  if (http->tls)
  {
    conn    = (SSL *)(http->tls);
    context = SSL_get_SSL_CTX(conn);

    SSL_shutdown(conn);
    SSL_CTX_free(context);
    SSL_free(conn);

    http->tls = NULL;
  }
#endif /* HAVE_LIBSSL */

#ifdef WIN32
  closesocket(http->fd);
#else
  close(http->fd);
#endif /* WIN32 */

  free(http);
}


/*
 * 'httpConnect()' - Connect to a HTTP server.
 */

http_t *			/* O - New HTTP connection */
httpConnect(const char *host,	/* I - Host to connect to */
            int        port)	/* I - Port number */
{
  http_encryption_t	encrypt;/* Type of encryption to use */


 /*
  * Set the default encryption status...
  */

  if (port == 443)
    encrypt = HTTP_ENCRYPT_ALWAYS;
  else
    encrypt = HTTP_ENCRYPT_IF_REQUESTED;

  return (httpConnectEncrypt(host, port, encrypt));
}


/*
 * 'httpConnectEncrypt()' - Connect to a HTTP server using encryption.
 */

http_t *				/* O - New HTTP connection */
httpConnectEncrypt(const char *host,	/* I - Host to connect to */
                   int        port,	/* I - Port number */
		   http_encryption_t encrypt)
					/* I - Type of encryption to use */
{
  int			i;		/* Looping var */
  http_t		*http;		/* New HTTP connection */
  struct hostent	*hostaddr;	/* Host address data */


  if (host == NULL)
    return (NULL);

  httpInitialize();

 /*
  * Lookup the host...
  */

  if ((hostaddr = httpGetHostByName(host)) == NULL)
  {
   /*
    * This hack to make users that don't have a localhost entry in
    * their hosts file or DNS happy...
    */

    if (strcasecmp(host, "localhost") != 0)
      return (NULL);
    else if ((hostaddr = httpGetHostByName("127.0.0.1")) == NULL)
      return (NULL);
  }

 /*
  * Verify that it is an IPv4 address (IPv6 support will come in CUPS 1.2...)
  */

#ifdef AF_INET6
  if ((hostaddr->h_addrtype != AF_INET || hostaddr->h_length != 4) &&
      (hostaddr->h_addrtype != AF_INET6 || hostaddr->h_length != 16))
    return (NULL);
#else
  if (hostaddr->h_addrtype != AF_INET || hostaddr->h_length != 4)
    return (NULL);
#endif /* AF_INET6 */

 /*
  * Allocate memory for the structure...
  */

  http = calloc(sizeof(http_t), 1);
  if (http == NULL)
    return (NULL);

  http->version  = HTTP_1_1;
  http->blocking = 1;
  http->activity = time(NULL);
  http->fd       = -1;

 /*
  * Set the encryption status...
  */

  if (port == 443)	/* Always use encryption for https */
    http->encryption = HTTP_ENCRYPT_ALWAYS;
  else
    http->encryption = encrypt;

 /*
  * Loop through the addresses we have until one of them connects...
  */

  strlcpy(http->hostname, host, sizeof(http->hostname));

  for (i = 0; hostaddr->h_addr_list[i]; i ++)
  {
   /*
    * Load the address...
    */

    httpAddrLoad(hostaddr, port, i, &(http->hostaddr));

   /*
    * Connect to the remote system...
    */

    if (!httpReconnect(http))
      break;
  }

 /*
  * Return the new structure if we could connect; otherwise, bail out...
  */

  if (hostaddr->h_addr_list[i])
    return (http);
  else
  {
    free(http);
    return (NULL);
  }
}


/*
 * 'httpEncryption()' - Set the required encryption on the link.
 */

int					/* O - -1 on error, 0 on success */
httpEncryption(http_t            *http,	/* I - HTTP data */
               http_encryption_t e)	/* I - New encryption preference */
{
#ifdef HAVE_LIBSSL
  if (!http)
    return (0);

  http->encryption = e;

  if ((http->encryption == HTTP_ENCRYPT_ALWAYS && !http->tls) ||
      (http->encryption == HTTP_ENCRYPT_NEVER && http->tls))
    return (httpReconnect(http));
  else if (http->encryption == HTTP_ENCRYPT_REQUIRED && !http->tls)
    return (http_upgrade(http));
  else
    return (0);
#else
  if (e == HTTP_ENCRYPT_ALWAYS || e == HTTP_ENCRYPT_REQUIRED)
    return (-1);
  else
    return (0);
#endif /* HAVE_LIBSSL */
}


/*
 * 'httpReconnect()' - Reconnect to a HTTP server...
 */

int				/* O - 0 on success, non-zero on failure */
httpReconnect(http_t *http)	/* I - HTTP data */
{
  int		val;		/* Socket option value */
#ifdef HAVE_LIBSSL
  SSL_CTX	*context;	/* Context for encryption */
  SSL		*conn;		/* Connection for encryption */


  if (http->tls)
  {
    conn    = (SSL *)(http->tls);
    context = SSL_get_SSL_CTX(conn);

    SSL_shutdown(conn);
    SSL_CTX_free(context);
    SSL_free(conn);

    http->tls = NULL;
  }
#endif /* HAVE_LIBSSL */

 /*
  * Close any previously open socket...
  */

  if (http->fd >= 0)
#ifdef WIN32
    closesocket(http->fd);
#else
    close(http->fd);
#endif /* WIN32 */

 /*
  * Create the socket and set options to allow reuse.
  */

  if ((http->fd = socket(http->hostaddr.addr.sa_family, SOCK_STREAM, 0)) < 0)
  {
#ifdef WIN32
    http->error  = WSAGetLastError();
#else
    http->error  = errno;
#endif /* WIN32 */
    http->status = HTTP_ERROR;
    return (-1);
  }

#ifdef FD_CLOEXEC
  fcntl(http->fd, F_SETFD, FD_CLOEXEC);	/* Close this socket when starting *
					 * other processes...              */
#endif /* FD_CLOEXEC */

  val = 1;
  setsockopt(http->fd, SOL_SOCKET, SO_REUSEADDR, (char *)&val, sizeof(val));

#ifdef SO_REUSEPORT
  val = 1;
  setsockopt(http->fd, SOL_SOCKET, SO_REUSEPORT, &val, sizeof(val));
#endif /* SO_REUSEPORT */

 /*
  * Using TCP_NODELAY improves responsiveness, especially on systems
  * with a slow loopback interface...  Since we write large buffers
  * when sending print files and requests, there shouldn't be any
  * performance penalty for this...
  */

  val = 1;
  setsockopt(http->fd, IPPROTO_TCP, TCP_NODELAY, &val, sizeof(val)); 

 /*
  * Connect to the server...
  */

  if (connect(http->fd, (struct sockaddr *)&(http->hostaddr),
              sizeof(http->hostaddr)) < 0)
  {
#ifdef WIN32
    http->error  = WSAGetLastError();
#else
    http->error  = errno;
#endif /* WIN32 */
    http->status = HTTP_ERROR;

#ifdef WIN32
    closesocket(http->fd);
#else
    close(http->fd);
#endif

    http->fd = -1;

    return (-1);
  }

  http->error  = 0;
  http->status = HTTP_CONTINUE;

#ifdef HAVE_LIBSSL
  if (http->encryption == HTTP_ENCRYPT_ALWAYS)
  {
   /*
    * Always do encryption via SSL.
    */

    context = SSL_CTX_new(SSLv23_method());
    conn    = SSL_new(context);

    SSL_set_fd(conn, http->fd);
    if (SSL_connect(conn) != 1)
    {
      SSL_CTX_free(context);
      SSL_free(conn);

#ifdef WIN32
      http->error  = WSAGetLastError();
#else
      http->error  = errno;
#endif /* WIN32 */
      http->status = HTTP_ERROR;

#ifdef WIN32
      closesocket(http->fd);
#else
      close(http->fd);
#endif

      return (-1);
    }

    http->tls = conn;
  }
  else if (http->encryption == HTTP_ENCRYPT_REQUIRED)
    return (http_upgrade(http));
#endif /* HAVE_LIBSSL */

  return (0);
}


/*
 * 'httpGetHostByName()' - Lookup a hostname or IP address, and return
 *                         address records for the specified name.
 */

struct hostent *			/* O - Host entry */
httpGetHostByName(const char *name)	/* I - Hostname or IP address */
{
  unsigned		ip[4];		/* IP address components */
  static unsigned	packed_ip;	/* Packed IPv4 address */
  static char		*packed_ptr[2];	/* Pointer to packed address */
  static struct hostent	host_ip;	/* Host entry for IP address */

#if defined(__APPLE__)
  /* OS X hack to avoid it's ocassional long delay in lookupd */
  static char sLoopback[] = "127.0.0.1";
  if (strcmp(name, "localhost") == 0)
    name = sLoopback;
#endif /* __APPLE__ */

 /*
  * This function is needed because some operating systems have a
  * buggy implementation of httpGetHostByName() that does not support
  * IP addresses.  If the first character of the name string is a
  * number, then sscanf() is used to extract the IP components.
  * We then pack the components into an IPv4 address manually,
  * since the inet_aton() function is deprecated.  We use the
  * htonl() macro to get the right byte order for the address.
  */

  if (isdigit(name[0]))
  {
   /*
    * We have an IP address; break it up and provide the host entry
    * to the caller.  Currently only supports IPv4 addresses, although
    * it should be trivial to support IPv6 in CUPS 1.2.
    */

    if (sscanf(name, "%u.%u.%u.%u", ip, ip + 1, ip + 2, ip + 3) != 4)
      return (NULL); /* Must have 4 numbers */

    packed_ip = htonl(((((((ip[0] << 8) | ip[1]) << 8) | ip[2]) << 8) | ip[3]));

   /*
    * Fill in the host entry and return it...
    */

    host_ip.h_name      = (char *)name;
    host_ip.h_aliases   = NULL;
    host_ip.h_addrtype  = AF_INET;
    host_ip.h_length    = 4;
    host_ip.h_addr_list = packed_ptr;
    packed_ptr[0]       = (char *)(&packed_ip);
    packed_ptr[1]       = NULL;

    return (&host_ip);
  }
  else
  {
   /*
    * Use the gethostbyname() function to get the IP address for
    * the name...
    */

    return (gethostbyname(name));
  }
}


/*
 * 'httpSeparate()' - Separate a Universal Resource Identifier into its
 *                    components.
 */

void
httpSeparate(const char *uri,		/* I - Universal Resource Identifier */
             char       *method,	/* O - Method [32] (http, https, etc.) */
	     char       *username,	/* O - Username [32] */
	     char       *host,		/* O - Hostname [32] */
	     int        *port,		/* O - Port number to use */
             char       *resource)	/* O - Resource/filename [1024] */
{
  char		*ptr;				/* Pointer into string... */
  const char	*atsign,			/* @ sign */
		*slash;				/* Separator */
  char		safeuri[HTTP_MAX_URI];		/* "Safe" local copy of URI */


 /*
  * Range check input...
  */

  if (uri == NULL || method == NULL || username == NULL || host == NULL ||
      port == NULL || resource == NULL)
    return;

 /*
  * Copy the URL to a local string to make sure we don't have a URL
  * longer than HTTP_MAX_URI characters long...
  */

  strlcpy(safeuri, uri, sizeof(safeuri));

  uri = safeuri;

 /*
  * Grab the method portion of the URI...
  */

  if (strncmp(uri, "//", 2) == 0)
  {
   /*
    * Workaround for HP IPP client bug...
    */

    strcpy(method, "ipp");
  }
  else
  {
   /*
    * Standard URI with method...
    */

    for (ptr = host; *uri != ':' && *uri != '\0'; uri ++)
      if (ptr < (host + HTTP_MAX_URI - 1))
        *ptr++ = *uri;

    *ptr = '\0';
    if (*uri == ':')
      uri ++;

   /*
    * If the method contains a period or slash, then it's probably
    * hostname/filename...
    */

    if (strchr(host, '.') != NULL || strchr(host, '/') != NULL || *uri == '\0')
    {
      if ((ptr = strchr(host, '/')) != NULL)
      {
	strlcpy(resource, ptr, HTTP_MAX_URI);
	*ptr = '\0';
      }
      else
	resource[0] = '\0';

      if (isdigit(*uri))
      {
       /*
	* OK, we have "hostname:port[/resource]"...
	*/

	*port = strtol(uri, (char **)&uri, 10);

	if (*uri == '/')
          strlcpy(resource, uri, HTTP_MAX_URI);
      }
      else
	*port = 631;

      strcpy(method, "http");
      username[0] = '\0';
      return;
    }
    else
      strlcpy(method, host, 32);
  }

 /*
  * If the method starts with less than 2 slashes then it is a local resource...
  */

  if (strncmp(uri, "//", 2) != 0)
  {
    strlcpy(resource, uri, HTTP_MAX_URI);

    username[0] = '\0';
    host[0]     = '\0';
    *port       = 0;
    return;
  }

 /*
  * Grab the username, if any...
  */

  while (*uri == '/')
    uri ++;

  if ((slash = strchr(uri, '/')) == NULL)
    slash = uri + strlen(uri);

  if ((atsign = strchr(uri, '@')) != NULL && atsign < slash)
  {
   /*
    * Got a username:password combo...
    */

    for (ptr = username; uri < atsign; uri ++)
      if (ptr < (username + HTTP_MAX_URI - 1))
	*ptr++ = *uri;

    *ptr = '\0';

    uri = atsign + 1;
  }
  else
    username[0] = '\0';

 /*
  * Grab the hostname...
  */

  for (ptr = host; *uri != ':' && *uri != '/' && *uri != '\0'; uri ++)
    if (ptr < (host + HTTP_MAX_URI - 1))
      *ptr++ = *uri;

  *ptr = '\0';

  if (*uri != ':')
  {
    if (strcasecmp(method, "http") == 0)
      *port = 80;
    else if (strcasecmp(method, "https") == 0)
      *port = 443;
    else if (strcasecmp(method, "ipp") == 0)
      *port = ippPort();
    else if (strcasecmp(method, "socket") == 0)	/* Not registered yet... */
      *port = 9100;
    else
      *port = 0;
  }
  else
  {
   /*
    * Parse port number...
    */

    *port = 0;
    uri ++;
    while (isdigit(*uri))
    {
      *port = (*port * 10) + *uri - '0';
      uri ++;
    }
  }

  if (*uri == '\0')
  {
   /*
    * Hostname but no port or path...
    */

    resource[0] = '/';
    resource[1] = '\0';
    return;
  }

 /*
  * The remaining portion is the resource string...
  */

  strlcpy(resource, uri, HTTP_MAX_URI);
}


/*
 * 'httpGetSubField()' - Get a sub-field value.
 */

char *					/* O - Value or NULL */
httpGetSubField(http_t       *http,	/* I - HTTP data */
                http_field_t field,	/* I - Field index */
                const char   *name,	/* I - Name of sub-field */
		char         *value)	/* O - Value string */
{
  const char	*fptr;			/* Pointer into field */
  char		temp[HTTP_MAX_VALUE],	/* Temporary buffer for name */
		*ptr;			/* Pointer into string buffer */


  if (http == NULL ||
      field < HTTP_FIELD_ACCEPT_LANGUAGE ||
      field > HTTP_FIELD_WWW_AUTHENTICATE ||
      name == NULL || value == NULL)
    return (NULL);

  for (fptr = http->fields[field]; *fptr;)
  {
   /*
    * Skip leading whitespace...
    */

    while (isspace(*fptr))
      fptr ++;

    if (*fptr == ',')
    {
      fptr ++;
      continue;
    }

   /*
    * Get the sub-field name...
    */

    for (ptr = temp;
         *fptr && *fptr != '=' && !isspace(*fptr) && ptr < (temp + sizeof(temp) - 1);
         *ptr++ = *fptr++);

    *ptr = '\0';

   /*
    * Skip trailing chars up to the '='...
    */

    while (*fptr && *fptr != '=')
      fptr ++;

    if (!*fptr)
      break;

   /*
    * Skip = and leading whitespace...
    */

    fptr ++;

    while (isspace(*fptr))
      fptr ++;

    if (*fptr == '\"')
    {
     /*
      * Read quoted string...
      */

      for (ptr = value, fptr ++;
           *fptr && *fptr != '\"' && ptr < (value + HTTP_MAX_VALUE - 1);
	   *ptr++ = *fptr++);

      *ptr = '\0';

      while (*fptr && *fptr != '\"')
        fptr ++;

      if (*fptr)
        fptr ++;
    }
    else
    {
     /*
      * Read unquoted string...
      */

      for (ptr = value;
           *fptr && !isspace(*fptr) && *fptr != ',' && ptr < (value + HTTP_MAX_VALUE - 1);
	   *ptr++ = *fptr++);

      *ptr = '\0';

      while (*fptr && !isspace(*fptr) && *fptr != ',')
        fptr ++;
    }

   /*
    * See if this is the one...
    */

    if (strcmp(name, temp) == 0)
      return (value);
  }

  value[0] = '\0';

  return (NULL);
}


/*
 * 'httpSetField()' - Set the value of an HTTP header.
 */

void
httpSetField(http_t       *http,	/* I - HTTP data */
             http_field_t field,	/* I - Field index */
	     const char   *value)	/* I - Value */
{
  if (http == NULL ||
      field < HTTP_FIELD_ACCEPT_LANGUAGE ||
      field > HTTP_FIELD_WWW_AUTHENTICATE ||
      value == NULL)
    return;

  strlcpy(http->fields[field], value, HTTP_MAX_VALUE);
}


/*
 * 'httpDelete()' - Send a DELETE request to the server.
 */

int					/* O - Status of call (0 = success) */
httpDelete(http_t     *http,		/* I - HTTP data */
           const char *uri)		/* I - URI to delete */
{
  return (http_send(http, HTTP_DELETE, uri));
}


/*
 * 'httpGet()' - Send a GET request to the server.
 */

int					/* O - Status of call (0 = success) */
httpGet(http_t     *http,		/* I - HTTP data */
        const char *uri)		/* I - URI to get */
{
  return (http_send(http, HTTP_GET, uri));
}


/*
 * 'httpHead()' - Send a HEAD request to the server.
 */

int					/* O - Status of call (0 = success) */
httpHead(http_t     *http,		/* I - HTTP data */
         const char *uri)		/* I - URI for head */
{
  return (http_send(http, HTTP_HEAD, uri));
}


/*
 * 'httpOptions()' - Send an OPTIONS request to the server.
 */

int					/* O - Status of call (0 = success) */
httpOptions(http_t     *http,		/* I - HTTP data */
            const char *uri)		/* I - URI for options */
{
  return (http_send(http, HTTP_OPTIONS, uri));
}


/*
 * 'httpPost()' - Send a POST request to the server.
 */

int					/* O - Status of call (0 = success) */
httpPost(http_t     *http,		/* I - HTTP data */
         const char *uri)		/* I - URI for post */
{
  httpGetLength(http);

  return (http_send(http, HTTP_POST, uri));
}


/*
 * 'httpPut()' - Send a PUT request to the server.
 */

int					/* O - Status of call (0 = success) */
httpPut(http_t     *http,		/* I - HTTP data */
        const char *uri)		/* I - URI to put */
{
  httpGetLength(http);

  return (http_send(http, HTTP_PUT, uri));
}


/*
 * 'httpTrace()' - Send an TRACE request to the server.
 */

int					/* O - Status of call (0 = success) */
httpTrace(http_t     *http,		/* I - HTTP data */
          const char *uri)		/* I - URI for trace */
{
  return (http_send(http, HTTP_TRACE, uri));
}


/*
 * 'httpFlush()' - Flush data from a HTTP connection.
 */

void
httpFlush(http_t *http)	/* I - HTTP data */
{
  char	buffer[8192];	/* Junk buffer */


  while (httpRead(http, buffer, sizeof(buffer)) > 0);
}


/*
 * 'httpRead()' - Read data from a HTTP connection.
 */

int					/* O - Number of bytes read */
httpRead(http_t *http,			/* I - HTTP data */
         char   *buffer,		/* I - Buffer for data */
	 int    length)			/* I - Maximum number of bytes */
{
  int		bytes;			/* Bytes read */
  char		len[32];		/* Length string */


  DEBUG_printf(("httpRead(%p, %p, %d)\n", http, buffer, length));

  if (http == NULL || buffer == NULL)
    return (-1);

  http->activity = time(NULL);

  if (length <= 0)
    return (0);

  if (http->data_encoding == HTTP_ENCODE_CHUNKED &&
      http->data_remaining <= 0)
  {
    DEBUG_puts("httpRead: Getting chunk length...");

    if (httpGets(len, sizeof(len), http) == NULL)
    {
      DEBUG_puts("httpRead: Could not get length!");
      return (0);
    }

    http->data_remaining = strtol(len, NULL, 16);
  }

  DEBUG_printf(("httpRead: data_remaining = %d\n", http->data_remaining));

  if (http->data_remaining == 0)
  {
   /*
    * A zero-length chunk ends a transfer; unless we are reading POST
    * data, go idle...
    */

    if (http->data_encoding == HTTP_ENCODE_CHUNKED)
      httpGets(len, sizeof(len), http);

    if (http->state == HTTP_POST_RECV)
      http->state ++;
    else
      http->state = HTTP_WAITING;

    return (0);
  }
  else if (length > http->data_remaining)
    length = http->data_remaining;

  if (http->used == 0 && length <= 256)
  {
   /*
    * Buffer small reads for better performance...
    */

    if (http->data_remaining > sizeof(http->buffer))
      bytes = sizeof(http->buffer);
    else
      bytes = http->data_remaining;

#ifdef HAVE_LIBSSL
    if (http->tls)
      bytes = SSL_read((SSL *)(http->tls), http->buffer, bytes);
    else
#endif /* HAVE_LIBSSL */
    {
      DEBUG_printf(("httpRead: reading %d bytes from socket into buffer...\n",
                    bytes));

      bytes = recv(http->fd, http->buffer, bytes, 0);

      DEBUG_printf(("httpRead: read %d bytes from socket into buffer...\n",
                    bytes));
    }

    if (bytes > 0)
      http->used = bytes;
    else if (bytes < 0)
    {
#ifdef WIN32
      http->error = WSAGetLastError();
#else
      http->error = errno;
#endif /* WIN32 */
      return (-1);
    }
    else
      return (0);
  }

  if (http->used > 0)
  {
    if (length > http->used)
      length = http->used;

    bytes = length;

    DEBUG_printf(("httpRead: grabbing %d bytes from input buffer...\n", bytes));

    memcpy(buffer, http->buffer, length);
    http->used -= length;

    if (http->used > 0)
      memcpy(http->buffer, http->buffer + length, http->used);
  }
#ifdef HAVE_LIBSSL
  else if (http->tls)
    bytes = SSL_read((SSL *)(http->tls), buffer, length);
#endif /* HAVE_LIBSSL */
  else
  {
    DEBUG_printf(("httpRead: reading %d bytes from socket...\n", length));
    bytes = recv(http->fd, buffer, length, 0);
    DEBUG_printf(("httpRead: read %d bytes from socket...\n", bytes));
  }

  if (bytes > 0)
    http->data_remaining -= bytes;
  else if (bytes < 0)
#ifdef WIN32
    http->error = WSAGetLastError();
#else
    http->error = errno;
#endif /* WIN32 */

  if (http->data_remaining == 0)
  {
    if (http->data_encoding == HTTP_ENCODE_CHUNKED)
      httpGets(len, sizeof(len), http);

    if (http->data_encoding != HTTP_ENCODE_CHUNKED)
    {
      if (http->state == HTTP_POST_RECV)
	http->state ++;
      else
	http->state = HTTP_WAITING;
    }
  }

#ifdef DEBUG
  {
    int i, j, ch;
    printf("httpRead: Read %d bytes:\n", bytes);
    for (i = 0; i < bytes; i += 16)
    {
      printf("   ");

      for (j = 0; j < 16 && (i + j) < bytes; j ++)
        printf(" %02X", buffer[i + j] & 255);

      while (j < 16)
      {
        printf("   ");
	j ++;
      }

      printf("    ");
      for (j = 0; j < 16 && (i + j) < bytes; j ++)
      {
        ch = buffer[i + j] & 255;

	if (ch < ' ' || ch == 127)
	  ch = '.';

        putchar(ch);
      }
      putchar('\n');
    }
  }
#endif /* DEBUG */

  return (bytes);
}


/*
 * 'httpWrite()' - Write data to a HTTP connection.
 */
 
int					/* O - Number of bytes written */
httpWrite(http_t     *http,		/* I - HTTP data */
          const char *buffer,		/* I - Buffer for data */
	  int        length)		/* I - Number of bytes to write */
{
  int	tbytes,				/* Total bytes sent */
	bytes;				/* Bytes sent */


  if (http == NULL || buffer == NULL)
    return (-1);

  http->activity = time(NULL);

  if (http->data_encoding == HTTP_ENCODE_CHUNKED)
  {
    if (httpPrintf(http, "%x\r\n", length) < 0)
      return (-1);

    if (length == 0)
    {
     /*
      * A zero-length chunk ends a transfer; unless we are sending POST
      * data, go idle...
      */

      DEBUG_puts("httpWrite: changing states...");

      if (http->state == HTTP_POST_RECV)
	http->state ++;
      else if (http->state == HTTP_PUT_RECV)
        http->state = HTTP_STATUS;
      else
	http->state = HTTP_WAITING;

      if (httpPrintf(http, "\r\n") < 0)
	return (-1);

      return (0);
    }
  }

  tbytes = 0;

  while (length > 0)
  {
#ifdef HAVE_LIBSSL
    if (http->tls)
      bytes = SSL_write((SSL *)(http->tls), buffer, length);
    else
#endif /* HAVE_LIBSSL */
    bytes = send(http->fd, buffer, length, 0);

    if (bytes < 0)
    {
      DEBUG_puts("httpWrite: error writing data...\n");

      return (-1);
    }

    buffer += bytes;
    tbytes += bytes;
    length -= bytes;
    if (http->data_encoding == HTTP_ENCODE_LENGTH)
      http->data_remaining -= bytes;
  }

  if (http->data_encoding == HTTP_ENCODE_CHUNKED)
    if (httpPrintf(http, "\r\n") < 0)
      return (-1);

  if (http->data_remaining == 0 && http->data_encoding == HTTP_ENCODE_LENGTH)
  {
   /*
    * Finished with the transfer; unless we are sending POST data, go idle...
    */

    DEBUG_puts("httpWrite: changing states...");

    if (http->state == HTTP_POST_RECV)
      http->state ++;
    else
      http->state = HTTP_WAITING;
  }

#ifdef DEBUG
  {
    int i, j, ch;
    printf("httpWrite: wrote %d bytes: \n", tbytes);
    for (i = 0, buffer -= tbytes; i < tbytes; i += 16)
    {
      printf("   ");

      for (j = 0; j < 16 && (i + j) < tbytes; j ++)
        printf(" %02X", buffer[i + j] & 255);

      while (j < 16)
      {
        printf("   ");
	j ++;
      }

      printf("    ");
      for (j = 0; j < 16 && (i + j) < tbytes; j ++)
      {
        ch = buffer[i + j] & 255;

	if (ch < ' ' || ch == 127)
	  ch = '.';

        putchar(ch);
      }
      putchar('\n');
    }
  }
#endif /* DEBUG */
  return (tbytes);
}


/*
 * 'httpGets()' - Get a line of text from a HTTP connection.
 */

char *					/* O - Line or NULL */
httpGets(char   *line,			/* I - Line to read into */
         int    length,			/* I - Max length of buffer */
	 http_t *http)			/* I - HTTP data */
{
  char	*lineptr,			/* Pointer into line */
	*bufptr,			/* Pointer into input buffer */
	*bufend;			/* Pointer to end of buffer */
  int	bytes;				/* Number of bytes read */


  DEBUG_printf(("httpGets(%p, %d, %p)\n", line, length, http));

  if (http == NULL || line == NULL)
    return (NULL);

 /*
  * Pre-scan the buffer and see if there is a newline in there...
  */

#ifdef WIN32
  WSASetLastError(0);
#else
  errno = 0;
#endif /* WIN32 */

  do
  {
    bufptr  = http->buffer;
    bufend  = http->buffer + http->used;

    while (bufptr < bufend)
      if (*bufptr == 0x0a)
	break;
      else
	bufptr ++;

    if (bufptr >= bufend && http->used < HTTP_MAX_BUFFER)
    {
     /*
      * No newline; see if there is more data to be read...
      */

#ifdef HAVE_LIBSSL
      if (http->tls)
        bytes = SSL_read((SSL *)(http->tls), bufend,
	                 HTTP_MAX_BUFFER - http->used);
      else
#endif /* HAVE_LIBSSL */
      bytes = recv(http->fd, bufend, HTTP_MAX_BUFFER - http->used, 0);

      if (bytes < 0)
      {
       /*
	* Nope, can't get a line this time...
	*/

#ifdef WIN32
        if (WSAGetLastError() != http->error)
	{
	  http->error = WSAGetLastError();
	  continue;
	}

        DEBUG_printf(("httpGets(): recv() error %d!\n", WSAGetLastError()));
#else
        if (errno != http->error)
	{
	  http->error = errno;
	  continue;
	}

        DEBUG_printf(("httpGets(): recv() error %d!\n", errno));
#endif /* WIN32 */

        return (NULL);
      }
      else if (bytes == 0)
      {
        if (http->blocking)
	  http->error = EPIPE;

        return (NULL);
      }

     /*
      * Yup, update the amount used and the end pointer...
      */

      http->used += bytes;
      bufend     += bytes;
    }
  }
  while (bufptr >= bufend && http->used < HTTP_MAX_BUFFER);

  http->activity = time(NULL);

 /*
  * Read a line from the buffer...
  */
    
  lineptr = line;
  bufptr  = http->buffer;
  bytes   = 0;
  length --;

  while (bufptr < bufend && bytes < length)
  {
    bytes ++;

    if (*bufptr == 0x0a)
    {
      bufptr ++;
      break;
    }
    else if (*bufptr == 0x0d)
      bufptr ++;
    else
      *lineptr++ = *bufptr++;
  }

  if (bytes > 0)
  {
    *lineptr = '\0';

    http->used -= bytes;
    if (http->used > 0)
      memcpy(http->buffer, bufptr, http->used);

    DEBUG_printf(("httpGets(): Returning \"%s\"\n", line));
    return (line);
  }

  DEBUG_puts("httpGets(): No new line available!");

  return (NULL);
}


/*
 * 'httpPrintf()' - Print a formatted string to a HTTP connection.
 */

int					/* O - Number of bytes written */
httpPrintf(http_t     *http,		/* I - HTTP data */
           const char *format,		/* I - printf-style format string */
	   ...)				/* I - Additional args as needed */
{
  int		bytes,			/* Number of bytes to write */
		nbytes,			/* Number of bytes written */
		tbytes;			/* Number of bytes all together */
  char		buf[HTTP_MAX_BUFFER],	/* Buffer for formatted string */
		*bufptr;		/* Pointer into buffer */
  va_list	ap;			/* Variable argument pointer */


  va_start(ap, format);
  bytes = vsnprintf(buf, sizeof(buf), format, ap);
  va_end(ap);

  DEBUG_printf(("httpPrintf: %s", buf));

  for (tbytes = 0, bufptr = buf; tbytes < bytes; tbytes += nbytes, bufptr += nbytes)
  {
#ifdef HAVE_LIBSSL
    if (http->tls)
      nbytes = SSL_write((SSL *)(http->tls), bufptr, bytes - tbytes);
    else
#endif /* HAVE_LIBSSL */
    nbytes = send(http->fd, bufptr, bytes - tbytes, 0);

    if (nbytes < 0)
      return (-1);
  }

  return (bytes);
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


/*
 * 'httpGetDateString()' - Get a formatted date/time string from a time value.
 */

const char *				/* O - Date/time string */
httpGetDateString(time_t t)		/* I - UNIX time */
{
  struct tm	*tdate;
  static char	datetime[256];


  tdate = gmtime(&t);
  snprintf(datetime, sizeof(datetime), "%s, %02d %s %d %02d:%02d:%02d GMT",
           days[tdate->tm_wday], tdate->tm_mday, months[tdate->tm_mon],
	   tdate->tm_year + 1900, tdate->tm_hour, tdate->tm_min, tdate->tm_sec);

  return (datetime);
}


/*
 * 'httpGetDateTime()' - Get a time value from a formatted date/time string.
 */

time_t					/* O - UNIX time */
httpGetDateTime(const char *s)		/* I - Date/time string */
{
  int		i;			/* Looping var */
  struct tm	tdate;			/* Time/date structure */
  char		mon[16];		/* Abbreviated month name */
  int		day, year;		/* Day of month and year */
  int		hour, min, sec;		/* Time */


  if (sscanf(s, "%*s%d%15s%d%d:%d:%d", &day, mon, &year, &hour, &min, &sec) < 6)
    return (0);

  for (i = 0; i < 12; i ++)
    if (strcasecmp(mon, months[i]) == 0)
      break;

  if (i >= 12)
    return (0);

  tdate.tm_mon   = i;
  tdate.tm_mday  = day;
  tdate.tm_year  = year - 1900;
  tdate.tm_hour  = hour;
  tdate.tm_min   = min;
  tdate.tm_sec   = sec;
  tdate.tm_isdst = 0;

  return (mktime(&tdate));
}


/*
 * 'httpUpdate()' - Update the current HTTP state for incoming data.
 */

http_status_t				/* O - HTTP status */
httpUpdate(http_t *http)		/* I - HTTP data */
{
  char		line[1024],		/* Line from connection... */
		*value;			/* Pointer to value on line */
  http_field_t	field;			/* Field index */
  int		major, minor;		/* HTTP version numbers */
  http_status_t	status;			/* Authorization status */
#ifdef HAVE_LIBSSL
  SSL_CTX	*context;		/* Context for encryption */
  SSL		*conn;			/* Connection for encryption */
#endif /* HAVE_LIBSSL */


  DEBUG_printf(("httpUpdate(%p)\n", http));

 /*
  * If we haven't issued any commands, then there is nothing to "update"...
  */

  if (http->state == HTTP_WAITING)
    return (HTTP_CONTINUE);

 /*
  * Grab all of the lines we can from the connection...
  */

  while (httpGets(line, sizeof(line), http) != NULL)
  {
    DEBUG_puts(line);

    if (line[0] == '\0')
    {
     /*
      * Blank line means the start of the data section (if any).  Return
      * the result code, too...
      *
      * If we get status 100 (HTTP_CONTINUE), then we *don't* change states.
      * Instead, we just return HTTP_CONTINUE to the caller and keep on
      * tryin'...
      */

      if (http->status == HTTP_CONTINUE)
        return (http->status);

#ifdef HAVE_LIBSSL
      if (http->status == HTTP_SWITCHING_PROTOCOLS && !http->tls)
      {
	context = SSL_CTX_new(SSLv23_method());
	conn    = SSL_new(context);

	SSL_set_fd(conn, http->fd);
	if (SSL_connect(conn) != 1)
	{
	  SSL_CTX_free(context);
	  SSL_free(conn);

#ifdef WIN32
	  http->error  = WSAGetLastError();
#else
	  http->error  = errno;
#endif /* WIN32 */
	  http->status = HTTP_ERROR;

#ifdef WIN32
	  closesocket(http->fd);
#else
	  close(http->fd);
#endif

	  return (HTTP_ERROR);
	}

	http->tls = conn;

        return (HTTP_CONTINUE);
      }
      else if (http->status == HTTP_UPGRADE_REQUIRED &&
               http->encryption != HTTP_ENCRYPT_NEVER)
        http->encryption = HTTP_ENCRYPT_REQUIRED;
#endif /* HAVE_LIBSSL */

      httpGetLength(http);

      switch (http->state)
      {
        case HTTP_GET :
	case HTTP_POST :
	case HTTP_POST_RECV :
	case HTTP_PUT :
	    http->state ++;
	    break;

	default :
	    http->state = HTTP_WAITING;
	    break;
      }

      return (http->status);
    }
    else if (strncmp(line, "HTTP/", 5) == 0)
    {
     /*
      * Got the beginning of a response...
      */

      if (sscanf(line, "HTTP/%d.%d%d", &major, &minor, (int *)&status) != 3)
        return (HTTP_ERROR);

      http->version = (http_version_t)(major * 100 + minor);
      http->status  = status;
    }
    else if ((value = strchr(line, ':')) != NULL)
    {
     /*
      * Got a value...
      */

      *value++ = '\0';
      while (isspace(*value))
        value ++;

     /*
      * Be tolerants of servers that send unknown attribute fields...
      */

      if ((field = http_field(line)) == HTTP_FIELD_UNKNOWN)
      {
        DEBUG_printf(("httpUpdate: unknown field %s seen!\n", line));
        continue;
      }

      httpSetField(http, field, value);
    }
    else
    {
      http->status = HTTP_ERROR;
      return (HTTP_ERROR);
    }
  }

 /*
  * See if there was an error...
  */

  if (http->error)
  {
    http->status = HTTP_ERROR;
    return (HTTP_ERROR);
  }

 /*
  * If we haven't already returned, then there is nothing new...
  */

  return (HTTP_CONTINUE);
}


/*
 * 'httpDecode64()' - Base64-decode a string.
 */

char *				/* O - Decoded string */
httpDecode64(char       *out,	/* I - String to write to */
             const char *in)	/* I - String to read from */
{
  int	pos,			/* Bit position */
	base64;			/* Value of this character */
  char	*outptr;		/* Output pointer */


  for (outptr = out, pos = 0; *in != '\0'; in ++)
  {
   /*
    * Decode this character into a number from 0 to 63...
    */

    if (*in >= 'A' && *in <= 'Z')
      base64 = *in - 'A';
    else if (*in >= 'a' && *in <= 'z')
      base64 = *in - 'a' + 26;
    else if (*in >= '0' && *in <= '9')
      base64 = *in - '0' + 52;
    else if (*in == '+')
      base64 = 62;
    else if (*in == '/')
      base64 = 63;
    else if (*in == '=')
      break;
    else
      continue;

   /*
    * Store the result in the appropriate chars...
    */

    switch (pos)
    {
      case 0 :
          *outptr = base64 << 2;
	  pos ++;
	  break;
      case 1 :
          *outptr++ |= (base64 >> 4) & 3;
	  *outptr = (base64 << 4) & 255;
	  pos ++;
	  break;
      case 2 :
          *outptr++ |= (base64 >> 2) & 15;
	  *outptr = (base64 << 6) & 255;
	  pos ++;
	  break;
      case 3 :
          *outptr++ |= base64;
	  pos = 0;
	  break;
    }
  }

  *outptr = '\0';

 /*
  * Return the decoded string...
  */

  return (out);
}


/*
 * 'httpEncode64()' - Base64-encode a string.
 */

char *				/* O - Encoded string */
httpEncode64(char       *out,	/* I - String to write to */
             const char *in)	/* I - String to read from */
{
  char		*outptr;	/* Output pointer */
  static char	base64[] =	/* Base64 characters... */
  		{
		  "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
		  "abcdefghijklmnopqrstuvwxyz"
		  "0123456789"
		  "+/"
  		};


  for (outptr = out; *in != '\0'; in ++)
  {
   /*
    * Encode the up to 3 characters as 4 Base64 numbers...
    */

    *outptr ++ = base64[in[0] >> 2];
    *outptr ++ = base64[((in[0] << 4) | (in[1] >> 4)) & 63];

    in ++;
    if (*in == '\0')
    {
      *outptr ++ = '=';
      break;
    }

    *outptr ++ = base64[((in[0] << 2) | (in[1] >> 6)) & 63];

    in ++;
    if (*in == '\0')
      break;

    *outptr ++ = base64[in[0] & 63];
  }

  *outptr ++ = '=';
  *outptr = '\0';

 /*
  * Return the encoded string...
  */

  return (out);
}


/*
 * 'httpGetLength()' - Get the amount of data remaining from the
 *                     content-length or transfer-encoding fields.
 */

int				/* O - Content length */
httpGetLength(http_t *http)	/* I - HTTP data */
{
  DEBUG_printf(("httpGetLength(%p)\n", http));

  if (strcasecmp(http->fields[HTTP_FIELD_TRANSFER_ENCODING], "chunked") == 0)
  {
    DEBUG_puts("httpGetLength: chunked request!");

    http->data_encoding  = HTTP_ENCODE_CHUNKED;
    http->data_remaining = 0;
  }
  else
  {
    http->data_encoding = HTTP_ENCODE_LENGTH;

   /*
    * The following is a hack for HTTP servers that don't send a
    * content-length or transfer-encoding field...
    *
    * If there is no content-length then the connection must close
    * after the transfer is complete...
    */

    if (http->fields[HTTP_FIELD_CONTENT_LENGTH][0] == '\0')
      http->data_remaining = 2147483647;
    else
      http->data_remaining = atoi(http->fields[HTTP_FIELD_CONTENT_LENGTH]);

    DEBUG_printf(("httpGetLength: content_length = %d\n", http->data_remaining));
  }

  return (http->data_remaining);
}


/*
 * 'http_field()' - Return the field index for a field name.
 */

static http_field_t		/* O - Field index */
http_field(const char *name)	/* I - String name */
{
  int	i;			/* Looping var */


  for (i = 0; i < HTTP_FIELD_MAX; i ++)
    if (strcasecmp(name, http_fields[i]) == 0)
      return ((http_field_t)i);

  return (HTTP_FIELD_UNKNOWN);
}


/*
 * 'http_send()' - Send a request with all fields and the trailing blank line.
 */

static int			/* O - 0 on success, non-zero on error */
http_send(http_t       *http,	/* I - HTTP data */
          http_state_t request,	/* I - Request code */
	  const char   *uri)	/* I - URI */
{
  int		i;		/* Looping var */
  char		*ptr,		/* Pointer in buffer */
		buf[1024];	/* Encoded URI buffer */
  static const char *codes[] =	/* Request code strings */
		{
		  NULL,
		  "OPTIONS",
		  "GET",
		  NULL,
		  "HEAD",
		  "POST",
		  NULL,
		  NULL,
		  "PUT",
		  NULL,
		  "DELETE",
		  "TRACE",
		  "CLOSE"
		};
  static const char *hex = "0123456789ABCDEF";
				/* Hex digits */


  if (http == NULL || uri == NULL)
    return (-1);

 /*
  * Encode the URI as needed...
  */

  for (ptr = buf; *uri != '\0' && ptr < (buf + sizeof(buf) - 1); uri ++)
    if (*uri <= ' ' || *uri >= 127)
    {
      if (ptr < (buf + sizeof(buf) - 1))
        *ptr ++ = '%';
      if (ptr < (buf + sizeof(buf) - 1))
        *ptr ++ = hex[(*uri >> 4) & 15];
      if (ptr < (buf + sizeof(buf) - 1))
        *ptr ++ = hex[*uri & 15];
    }
    else
      *ptr ++ = *uri;

  *ptr = '\0';

 /*
  * See if we had an error the last time around; if so, reconnect...
  */

  if (http->status == HTTP_ERROR || http->status >= HTTP_BAD_REQUEST)
    httpReconnect(http);

 /*
  * Send the request header...
  */

  http->state = request;
  if (request == HTTP_POST || request == HTTP_PUT)
    http->state ++;

  http->status = HTTP_CONTINUE;

#ifdef HAVE_LIBSSL
  if (http->encryption == HTTP_ENCRYPT_REQUIRED && !http->tls)
  {
    httpSetField(http, HTTP_FIELD_CONNECTION, "Upgrade");
    httpSetField(http, HTTP_FIELD_UPGRADE, "TLS/1.0,SSL/2.0,SSL/3.0");
  }
#endif /* HAVE_LIBSSL */

  if (httpPrintf(http, "%s %s HTTP/1.1\r\n", codes[request], buf) < 1)
  {
    http->status = HTTP_ERROR;
    return (-1);
  }

  for (i = 0; i < HTTP_FIELD_MAX; i ++)
    if (http->fields[i][0] != '\0')
    {
      DEBUG_printf(("%s: %s\n", http_fields[i], http->fields[i]));

      if (httpPrintf(http, "%s: %s\r\n", http_fields[i], http->fields[i]) < 1)
      {
	http->status = HTTP_ERROR;
	return (-1);
      }
    }

  if (httpPrintf(http, "\r\n") < 1)
  {
    http->status = HTTP_ERROR;
    return (-1);
  }

  httpClearFields(http);

  return (0);
}


#ifdef HAVE_LIBSSL
/*
 * 'http_upgrade()' - Force upgrade to TLS encryption.
 */

static int			/* O - Status of connection */
http_upgrade(http_t *http)	/* I - HTTP data */
{
  int		ret;		/* Return value */
  http_t	myhttp;		/* Local copy of HTTP data */


  DEBUG_printf(("http_upgrade(%p)\n", http));

 /*
  * Copy the HTTP data to a local variable so we can do the OPTIONS
  * request without interfering with the existing request data...
  */

  memcpy(&myhttp, http, sizeof(myhttp));

 /*
  * Send an OPTIONS request to the server, requiring SSL or TLS
  * encryption on the link...
  */

  httpClearFields(&myhttp);
  httpSetField(&myhttp, HTTP_FIELD_CONNECTION, "upgrade");
  httpSetField(&myhttp, HTTP_FIELD_UPGRADE, "TLS/1.0, SSL/2.0, SSL/3.0");

  if ((ret = httpOptions(&myhttp, "*")) == 0)
  {
   /*
    * Wait for the secure connection...
    */

    while (httpUpdate(&myhttp) == HTTP_CONTINUE);
  }

  httpFlush(&myhttp);

 /*
  * Copy the HTTP data back over, if any...
  */

  http->fd         = myhttp.fd;
  http->error      = myhttp.error;
  http->activity   = myhttp.activity;
  http->status     = myhttp.status;
  http->version    = myhttp.version;
  http->keep_alive = myhttp.keep_alive;
  http->used       = myhttp.used;

  if (http->used)
    memcpy(http->buffer, myhttp.buffer, http->used);

  http->auth_type   = myhttp.auth_type;
  http->nonce_count = myhttp.nonce_count;

  memcpy(http->nonce, myhttp.nonce, sizeof(http->nonce));

  http->tls        = myhttp.tls;
  http->encryption = myhttp.encryption;

 /*
  * See if we actually went secure...
  */

  if (!http->tls)
  {
   /*
    * Server does not support HTTP upgrade...
    */

    DEBUG_puts("Server does not support HTTP upgrade!");

#ifdef WIN32
    closesocket(http->fd);
#else
    close(http->fd);
#endif

    http->fd = -1;

    return (-1);
  }
  else
    return (ret);
}
#endif /* HAVE_LIBSSL */


/*
 * End of "$Id: http.c,v 1.82.2.14 2002/05/16 13:59:58 mike Exp $".
 */
