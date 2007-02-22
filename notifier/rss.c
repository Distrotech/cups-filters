/*
 * "$Id$"
 *
 *   RSS notifier for the Common UNIX Printing System (CUPS).
 *
 *   Copyright 2007 by Easy Software Products.
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
 */

/*
 * Include necessary headers...
 */

#include <cups/cups.h>
#include <cups/language.h>
#include <cups/string.h>
#include <cups/array.h>
#include <errno.h>


/*
 * Structures...
 */

typedef struct _cups_rss_s		/**** RSS message data ****/
{
  int		sequence_number;	/* notify-sequence-number */
  char		*subject,		/* Message subject/summary */
		*text;			/* Message text */
  time_t	event_time;		/* When the event occurred */
} _cups_rss_t;


/*
 * Local globals...
 */

static char		*rss_password;	/* Password for remote RSS */


/*
 * Local functions...
 */

static int		compare_rss(_cups_rss_t *a, _cups_rss_t *b);
static void		delete_message(_cups_rss_t *rss);
static void		load_rss(cups_array_t *rss, const char *filename);
static _cups_rss_t	*new_message(int sequence_number, char *subject,
			             char *text, time_t event_time);
static const char	*password_cb(const char *prompt);
static int		save_rss(cups_array_t *rss, const char *filename);
static char		*xml_escape(const char *s);


/*
 * 'main()' - Main entry for the test notifier.
 */

int					/* O - Exit status */
main(int  argc,				/* I - Number of command-line arguments */
     char *argv[])			/* I - Command-line arguments */
{
  int		i;			/* Looping var */
  ipp_t		*event;			/* Event from scheduler */
  ipp_state_t	state;			/* IPP event state */
  char		scheme[32],		/* URI scheme ("rss") */
		username[256],		/* Username for remote RSS */
		host[1024],		/* Hostname for remote RSS */
		resource[1024],		/* RSS file */
		*options;		/* Options */
  int		port;			/* Port number for remote RSS */
  http_t	*http;			/* Connection to remote server */
  http_status_t	status;			/* HTTP GET/PUT status code */
  char		filename[1024],		/* Local filename */
		newname[1024];		/* filename.N */
  cups_lang_t	*language;		/* Language information */
  ipp_attribute_t *printer_up_time,	/* Timestamp on event */
		*notify_sequence_number;/* Sequence number */
  char		*subject,		/* Subject for notification message */
		*text;			/* Text for notification message */
  cups_array_t	*rss;			/* RSS message array */
  _cups_rss_t	*msg;			/* RSS message */


  fprintf(stderr, "DEBUG: argc=%d\n", argc);
  for (i = 0; i < argc; i ++)
    fprintf(stderr, "DEBUG: argv[%d]=\"%s\"\n", i, argv[i]);

 /*
  * See whether we are publishing this RSS feed locally or remotely...
  */

  if (httpSeparateURI(HTTP_URI_CODING_ALL, argv[1], scheme, sizeof(scheme),
                      username, sizeof(username), host, sizeof(host), &port,
		      resource, sizeof(resource)) < HTTP_URI_OK)
  {
    fprintf(stderr, "ERROR: Bad RSS URI \"%s\"!\n", argv[1]);
    return (1);
  }

  if ((options = strchr(resource, '?')) != NULL)
    *options++ = '\0';

  rss = cupsArrayNew((cups_array_func_t)compare_rss, NULL);

  if (host[0])
  {
   /*
    * Remote feed, see if we can get the current file...
    */

    int	fd;				/* Temporary file */


    if ((rss_password = strchr(username, ':')) != NULL)
      *rss_password++ = '\0';

    cupsSetPasswordCB(password_cb);
    cupsSetUser(username);

    if ((fd = cupsTempFd(filename, sizeof(filename))) < 0)
    {
      fprintf(stderr, "ERROR: Unable to create temporary file: %s\n",
              strerror(errno));

      return (1);
    }

    if ((http = httpConnect(host, port)) == NULL)
    {
      fprintf(stderr, "ERROR: Unable to connect to %s on port %d: %s\n",
              host, port, strerror(errno));

      close(fd);
      unlink(filename);

      return (1);
    }

    status = cupsGetFd(http, resource, fd);

    close(fd);

    if (status != HTTP_OK && status != HTTP_NOT_FOUND)
    {
      fprintf(stderr, "ERROR: Unable to GET %s from %s on port %d: %d %s\n",
	      resource, host, port, status, httpStatus(status));

      httpClose(http);
      unlink(filename);

      return (1);
    }

    strlcpy(newname, filename, sizeof(newname));
  }
  else
  {
    const char	*cachedir;		/* CUPS_CACHEDIR */


    http = NULL;

    if ((cachedir = getenv("CUPS_CACHEDIR")) == NULL)
      cachedir = CUPS_CACHEDIR;

    snprintf(filename, sizeof(filename), "%s/rss%s", cachedir, resource);
    snprintf(newname, sizeof(newname), "%s.N", filename);
  }

 /*
  * Load the previous RSS file, if any...
  */

  load_rss(rss, filename);

 /*
  * Localize for the user's chosen language...
  */

  language = cupsLangDefault();

 /*
  * Read events and update the RSS file until we are out of events.
  */

  for (;;)
  {
   /*
    * Read the next event...
    */

    event = ippNew();
    while ((state = ippReadFile(0, event)) != IPP_DATA)
    {
      if (state <= IPP_IDLE)
        break;
    }

    if (state == IPP_ERROR)
      fputs("DEBUG: ippReadFile() returned IPP_ERROR!\n", stderr);

    if (state <= IPP_IDLE)
    {
      ippDelete(event);

      if (http)
        unlink(filename);

      httpClose(http);

      return (0);
    }

   /*
    * Collect the info from the event...
    */

    printer_up_time        = ippFindAttribute(event, "printer-up-time",
                                              IPP_TAG_INTEGER);
    notify_sequence_number = ippFindAttribute(event, "notify-sequence-number",
                                	      IPP_TAG_INTEGER);
    subject                = cupsNotifySubject(language, event);
    text                   = cupsNotifyText(language, event);

    if (printer_up_time && notify_sequence_number && subject && text)
    {
     /*
      * Create a new RSS message...
      */

      msg = new_message(notify_sequence_number->values[0].integer,
                        xml_escape(subject), xml_escape(text),
			printer_up_time->values[0].integer);

      if (!msg)
      {
        fprintf(stderr, "ERROR: Unable to create message: %s\n",
	        strerror(errno));

        ippDelete(event);

	if (http)
          unlink(filename);

        httpClose(http);

	return (1);
      }

     /*
      * Add it to the array...
      */

      cupsArrayAdd(rss, msg);

     /*
      * Save the messages to the file again, uploading as needed...
      */ 

      if (save_rss(rss, newname))
      {
	if (http)
	{
	 /*
          * Upload the RSS file...
	  */

          if ((status = cupsPutFile(http, resource, filename)) != HTTP_CREATED)
            fprintf(stderr, "ERROR: Unable to PUT %s from %s on port %d: %d %s\n",
	            resource, host, port, status, httpStatus(status));
	}
	else
	{
	 /*
          * Move the new RSS file over top the old one...
	  */

          if (rename(newname, filename))
            fprintf(stderr, "ERROR: Unable to rename %s to %s: %s\n",
	            newname, filename, strerror(errno));
	}
      }
    }

    if (subject)
      free(subject);

    if (text)
      free(text);

    ippDelete(event);
  }
}


/*
 * 'compare_rss()' - Compare two messages.
 */

static int				/* O - Result of comparison */
compare_rss(_cups_rss_t *a,		/* I - First message */
            _cups_rss_t *b)		/* I - Second message */
{
  return (a->sequence_number - b->sequence_number);
}


/*
 * 'delete_message()' - Free all memory used by a message.
 */

static void
delete_message(_cups_rss_t *msg)	/* I - RSS message */
{
  if (msg->subject)
    free(msg->subject);

  if (msg->text)
    free(msg->text);

  free(msg);
}


/*
 * 'load_rss()' - Load an existing RSS feed file.
 */

static void
load_rss(cups_array_t *rss,		/* I - RSS messages */
         const char   *filename)	/* I - File to load */
{
}


/*
 * 'new_message()' - Create a new RSS message.
 */

static _cups_rss_t *			/* O - New message */
new_message(int    sequence_number,	/* I - notify-sequence-number */
            char   *subject,		/* I - Subject/summary */
            char   *text,		/* I - Text */
	    time_t event_time)		/* I - Date/time of event */
{
  _cups_rss_t	*msg;			/* New message */


  if ((msg = calloc(1, sizeof(_cups_rss_t))) == NULL)
    return (NULL);

  msg->sequence_number = sequence_number;
  msg->subject         = subject;
  msg->text            = text;
  msg->event_time      = event_time;

  return (msg);
}


/*
 * 'password_cb()' - Return the cached password.
 */

static const char *			/* O - Cached password */
password_cb(const char *prompt)		/* I - Prompt string, unused */
{
  (void)prompt;

  return (rss_password);
}


/*
 * 'save_rss()' - Save messages to a RSS file.
 */

static int				/* O - 1 on success, 0 on failure */
save_rss(cups_array_t *rss,		/* I - RSS messages */
         const char   *filename)	/* I - File to save to */
{
  FILE		*fp;			/* File pointer */
  _cups_rss_t	*msg;			/* Current message */
  time_t	curtime;		/* Current time */
  struct tm	*curdate;		/* Current date */


  if ((fp = fopen(filename, "w")) == NULL)
  {
    fprintf(stderr, "ERROR: Unable to create %s: %s\n", filename,
            strerror(errno));
    return (0);
  }

  fputs("<?xml version=\"1.0\"?>\n", fp);
  fputs("<rss version=\"2.0\">\n", fp);
  fputs("  <channel>\n", fp);
  fputs("    <title>CUPS RSS Feed</title>\n", fp);

  curtime = time(NULL);
  curdate = gmtime(&curtime);

  fprintf(fp, "    <pubDate>%04d-%02d-%02dT%02d:%02d:%02d+00:00</pubDate>\n",
          curdate->tm_year + 1900, curdate->tm_mon + 1, curdate->tm_mday,
	  curdate->tm_hour, curdate->tm_min, curdate->tm_sec);

  for (msg = (_cups_rss_t *)cupsArrayLast(rss);
       msg;
       msg = (_cups_rss_t *)cupsArrayPrev(rss))
  {
    fputs("    <item>\n", fp);
    fprintf(fp, "      <title>%s</title>\n", msg->subject);
    fprintf(fp, "      <description>%s</description>\n", msg->text);

    curdate = gmtime(&(msg->event_time));

    fprintf(fp, "      <pubDate>%04d-%02d-%02dT%02d:%02d:%02d+00:00</pubDate>\n",
            curdate->tm_year + 1900, curdate->tm_mon + 1, curdate->tm_mday,
	    curdate->tm_hour, curdate->tm_min, curdate->tm_sec);

    fprintf(fp, "      <guid>%d</guid>\n", msg->sequence_number);
    fputs("    </item>\n", fp);
  }

  fputs(" </channel>\n", fp);
  fputs("</rss>\n", fp);

  return (!fclose(fp));
}


/*
 * 'xml_escape()' - Copy a string, escaping &, <, and > as needed.
 */

static char *				/* O - Escaped string */
xml_escape(const char *s)		/* I - String to escape */
{
  char		*e,			/* Escaped string */
		*eptr;			/* Pointer into escaped string */
  const char	*sptr;			/* Pointer into string */
  size_t	bytes;			/* Bytes needed for string */


 /*
  * First figure out how many extra bytes we need...
  */

  for (bytes = 0, sptr = s; *sptr; sptr ++)
    if (*sptr == '&')
      bytes += 4;			/* &amp; */
    else if (*sptr == '<' || *sptr == '>')
      bytes += 3;			/* &lt; and &gt; */

 /*
  * If there is nothing to escape, just strdup() it...
  */

  if (bytes == 0)
    return (strdup(s));

 /*
  * Otherwise allocate memory and copy...
  */

  if ((e = malloc(bytes + 1 + strlen(s))) == NULL)
    return (NULL);

  for (eptr = e, sptr = s; *sptr; sptr ++)
    if (*sptr == '&')
    {
      *eptr++ = '&';
      *eptr++ = 'a';
      *eptr++ = 'm';
      *eptr++ = 'p';
      *eptr++ = ';';
    }
    else if (*sptr == '<')
    {
      *eptr++ = '&';
      *eptr++ = 'l';
      *eptr++ = 't';
      *eptr++ = ';';
    }
    else if (*sptr == '>')
    {
      *eptr++ = '&';
      *eptr++ = 'g';
      *eptr++ = 't';
      *eptr++ = ';';
    }
    else
      *eptr++ = *sptr;

  *eptr = '\0';

  return (e);
}


/*
 * End of "$Id$".
 */
