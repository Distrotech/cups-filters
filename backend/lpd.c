/*
 * "$Id: lpd.c,v 1.23 2001/01/24 17:14:01 mike Exp $"
 *
 *   Line Printer Daemon backend for the Common UNIX Printing System (CUPS).
 *
 *   Copyright 1997-2001 by Easy Software Products, all rights reserved.
 *
 *   These coded instructions, statements, and computer programs are the
 *   property of Easy Software Products and are protected by Federal
 *   copyright law.  Distribution and use rights are outlined in the file
 *   "LICENSE" which should have been included with this file.  If this
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
 *   main()        - Send a file to the printer or server.
 *   lpd_command() - Send an LPR command sequence and wait for a reply.
 *   lpd_queue()   - Queue a file using the Line Printer Daemon protocol.
 */

/*
 * Include necessary headers.
 */

#include <cups/cups.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <cups/string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <signal.h>

#if defined(WIN32) || defined(__EMX__)
#  include <winsock.h>
#else
#  include <sys/socket.h>
#  include <netinet/in.h>
#  include <arpa/inet.h>
#  include <netdb.h>
#endif /* WIN32 || __EMX__ */

extern int	rresvport(int *port);	/* Hello?  No prototype for this... */


/*
 * Local functions...
 */

static int	lpd_command(int lpd_fd, char *format, ...);
static int	lpd_queue(char *hostname, char *printer, char *filename,
		          char *user, char *title, int copies, int banner,
			  int format);


/*
 * 'main()' - Send a file to the printer or server.
 *
 * Usage:
 *
 *    printer-uri job-id user title copies options [file]
 */

int			/* O - Exit status */
main(int  argc,		/* I - Number of command-line arguments (6 or 7) */
     char *argv[])	/* I - Command-line arguments */
{
  char	method[255],	/* Method in URI */
	hostname[1024],	/* Hostname */
	username[255],	/* Username info (not used) */
	resource[1024],	/* Resource info (printer name) */
	*options,	/* Pointer to options */
	name[255],	/* Name of option */
	value[255],	/* Value of option */
	*ptr,		/* Pointer into name or value */
	filename[1024];	/* File to print */
  int	port;		/* Port number (not used) */
  int	status;		/* Status of LPD job */
  int	banner;		/* Print banner page? */
  int	format;		/* Print format */


 /*
  * Make sure status messages are not buffered...
  */

  setbuf(stderr, NULL);

 /*
  * Check command-line...
  */

  if (argc == 1)
  {
    puts("network lpd \"Unknown\" \"LPD/LPR Host or Printer\"");
    return (0);
  }
  else if (argc < 6 || argc > 7)
  {
    fprintf(stderr, "Usage: %s job-id user title copies options [file]\n",
            argv[0]);
    return (1);
  }

 /*
  * If we have 7 arguments, print the file named on the command-line.
  * Otherwise, copy stdin to a temporary file and print the temporary
  * file.
  */

  if (argc == 6)
  {
   /*
    * Copy stdin to a temporary file...
    */

    FILE *fp;		/* Temporary file */
    char buffer[8192];	/* Buffer for copying */
    int  bytes;		/* Number of bytes read */


    if ((fp = fopen(cupsTempFile(filename, sizeof(filename)), "w")) == NULL)
    {
      perror("ERROR: unable to create temporary file");
      return (1);
    }

    while ((bytes = fread(buffer, 1, sizeof(buffer), stdin)) > 0)
      if (fwrite(buffer, 1, bytes, fp) < bytes)
      {
        perror("ERROR: unable to write to temporary file");
	fclose(fp);
	unlink(filename);
	return (1);
      }

    fclose(fp);
  }
  else
  {
    strncpy(filename, argv[6], sizeof(filename) - 1);
    filename[sizeof(filename) - 1] = '\0';
  }

 /*
  * Extract the hostname and printer name from the URI...
  */

  httpSeparate(argv[0], method, username, hostname, &port, resource);

 /*
  * See if there are any options...
  */

  banner = 0;
  format = 'l';

  if ((options = strchr(resource, '?')) != NULL)
  {
   /*
    * Yup, terminate the device name string and move to the first
    * character of the options...
    */

    *options++ = '\0';

   /*
    * Parse options...
    */

    while (*options)
    {
     /*
      * Get the name...
      */

      for (ptr = name; *options && *options != '=';)
        *ptr++ = *options++;
      *ptr = '\0';

      if (*options == '=')
      {
       /*
        * Get the value...
	*/

        options ++;

	for (ptr = value; *options && *options != '+';)
          *ptr++ = *options++;
	*ptr = '\0';

	if (*options == '+')
	  options ++;
      }
      else
        value[0] = '\0';

     /*
      * Process the option...
      */

      if (strcasecmp(name, "banner") == 0)
      {
       /*
        * Set the banner...
	*/

        banner = !value[0] ||
	         strcasecmp(value, "on") == 0 ||
		 strcasecmp(value, "yes") == 0 ||
		 strcasecmp(value, "true") == 0;
      }
      else if (strcasecmp(name, "format") == 0 && value[0])
      {
       /*
        * Set output format...
	*/

        if (strchr("cdfglnoprtv", value[0]) != NULL)
	  format = value[0];
	else
	  fprintf(stderr, "ERROR: Unknown format character \"%c\"\n", value[0]);
      }
    }
  }

 /*
  * Queue the job...
  */

  if (argc > 6)
  {
    status = lpd_queue(hostname, resource + 1, filename,
                       argv[2] /* user */, argv[3] /* title */,
		       atoi(argv[4]) /* copies */, banner, format);

    if (!status)
      fprintf(stderr, "PAGE: 1 %d\n", atoi(argv[4]));
  }
  else
    status = lpd_queue(hostname, resource + 1, filename,
                       argv[2] /* user */, argv[3] /* title */, 1,
		       banner, format);

 /*
  * Remove the temporary file if necessary...
  */

  if (argc < 7)
    unlink(filename);

 /*
  * Return the queue status...
  */

  return (status);
}


/*
 * 'lpd_command()' - Send an LPR command sequence and wait for a reply.
 */

static int			/* O - Status of command */
lpd_command(int  fd,		/* I - Socket connection to LPD host */
            char *format,	/* I - printf()-style format string */
            ...)		/* I - Additional args as necessary */
{
  va_list	ap;		/* Argument pointer */
  char		buf[1024];	/* Output buffer */
  int		bytes;		/* Number of bytes to output */
  char		status;		/* Status from command */


 /*
  * Format the string...
  */

  va_start(ap, format);
  bytes = vsnprintf(buf, sizeof(buf), format, ap);
  va_end(ap);

  fprintf(stderr, "DEBUG: lpd_command %2.2x %s", buf[0], buf + 1);

 /*
  * Send the command...
  */

  fprintf(stderr, "DEBUG: Sending command string (%d bytes)...\n", bytes);

  if (send(fd, buf, bytes, 0) < bytes)
    return (-1);

 /*
  * Read back the status from the command and return it...
  */

  fprintf(stderr, "DEBUG: Reading command status...\n");

  if (recv(fd, &status, 1, 0) < 1)
    return (-1);

  fprintf(stderr, "DEBUG: lpd_command returning %d\n", status);

  return (status);
}


/*
 * 'lpd_queue()' - Queue a file using the Line Printer Daemon protocol.
 */

static int			/* O - Zero on success, non-zero on failure */
lpd_queue(char *hostname,	/* I - Host to connect to */
          char *printer,	/* I - Printer/queue name */
	  char *filename,	/* I - File to print */
          char *user,		/* I - Requesting user */
	  char *title,		/* I - Job title */
	  int  copies,		/* I - Number of copies */
	  int  banner,		/* I - Print LPD banner? */
          int  format)		/* I - Format specifier */
{
  FILE			*fp;		/* Job file */
  char			localhost[255];	/* Local host name */
  int			error;		/* Error number */
  struct stat		filestats;	/* File statistics */
  int			port;		/* LPD connection port */
  int			fd;		/* LPD socket */
  char			control[10240],	/* LPD control 'file' */
			*cptr;		/* Pointer into control file string */
  char			status;		/* Status byte from command */
  struct sockaddr_in	addr;		/* Socket address */
  struct hostent	*hostaddr;	/* Host address */
  size_t		nbytes,		/* Number of bytes written */
			tbytes;		/* Total bytes written */
  char			buffer[8192];	/* Output buffer */
#if defined(HAVE_SIGACTION) && !defined(HAVE_SIGSET)
  struct sigaction	action;		/* Actions for POSIX signals */
#endif /* HAVE_SIGACTION && !HAVE_SIGSET */


 /*
  * First try to reserve a port for this connection...
  */

  if ((hostaddr = gethostbyname(hostname)) == NULL)
  {
    fprintf(stderr, "ERROR: Unable to locate printer \'%s\' - %s",
            hostname, strerror(errno));
    return (1);
  }

  fprintf(stderr, "INFO: Attempting to connect to host %s for printer %s\n",
          hostname, printer);

  memset(&addr, 0, sizeof(addr));
  memcpy(&(addr.sin_addr), hostaddr->h_addr, hostaddr->h_length);
  addr.sin_family = hostaddr->h_addrtype;
  addr.sin_port   = htons(515);	/* LPD/printer service */

  for (port = 732;;)
  {
    if ((fd = rresvport(&port)) < 0)
    {
      perror("ERROR: Unable to reserve port");
      sleep(30);
      continue;
    }

    if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0)
    {
      error = errno;
      close(fd);
      fd = -1;

      if (error == ECONNREFUSED)
      {
	fprintf(stderr, "INFO: Network host \'%s\' is busy; will retry in 30 seconds...",
                hostname);
	sleep(30);
      }
      else if (error == EADDRINUSE)
      {
	port --;
	if (port < 721)
	  port = 732;
      }
      else
      {
	perror("ERROR: Unable to connect to printer");
        sleep(30);
      }
    }
    else
      break;
  }

  fprintf(stderr, "INFO: Connected on port %d...\n", port);

 /*
  * Now that we are "connected" to the port, ignore SIGTERM so that we
  * can finish out any page data the driver sends (e.g. to eject the
  * current page...
  */

#ifdef HAVE_SIGSET /* Use System V signals over POSIX to avoid bugs */
  sigset(SIGTERM, SIG_IGN);
#elif defined(HAVE_SIGACTION)
  memset(&action, 0, sizeof(action));

  sigemptyset(&action.sa_mask);
  action.sa_handler = SIG_IGN;
  sigaction(SIGTERM, &action, NULL);
#else
  signal(SIGTERM, SIG_IGN);
#endif /* HAVE_SIGSET */

 /*
  * Next, open the print file and figure out its size...
  */

  if (stat(filename, &filestats))
  {
    perror("ERROR: unable to stat print file");
    return (1);
  }

  if ((fp = fopen(filename, "rb")) == NULL)
  {
    perror("ERROR: unable to open print file for reading");
    return (1);
  }

 /*
  * Send a job header to the printer, specifying no banner page and
  * literal output...
  */

  lpd_command(fd, "\002%s\n", printer);	/* Receive print job(s) */

  gethostname(localhost, sizeof(localhost));
  localhost[31] = '\0'; /* RFC 1179, Section 7.2 - host name < 32 chars */

  snprintf(control, sizeof(control), "H%s\nP%s\nJ%s\n", localhost, user, title);
  cptr = control + strlen(control);

  if (banner)
  {
    snprintf(cptr, sizeof(control) - (cptr - control), "L%s\n", user);
    cptr   += strlen(cptr);
  }

  while (copies > 0)
  {
    snprintf(cptr, sizeof(control) - (cptr - control), "%cdfA%03d%s\n", format,
             getpid() % 1000, localhost);
    cptr   += strlen(cptr);
    copies --;
  }

  snprintf(cptr, sizeof(control) - (cptr - control),
           "UdfA%03d%s\nNdfA%03d%s\n",
           getpid() % 1000, localhost,
           getpid() % 1000, localhost);

  fprintf(stderr, "DEBUG: Control file is:\n%s", control);

  lpd_command(fd, "\002%d cfA%03.3d%s\n", strlen(control), getpid() % 1000,
              localhost);

  fprintf(stderr, "INFO: Sending control file (%d bytes)\n", strlen(control));

  if (send(fd, control, strlen(control) + 1, 0) < (strlen(control) + 1))
  {
    perror("ERROR: Unable to write control file");
    status = 1;
  }
  else if (read(fd, &status, 1) < 1 || status != 0)
    fprintf(stderr, "ERROR: Remote host did not accept control file (%d)\n",
            status);
  else
  {
   /*
    * Send the print file...
    */

    fputs("INFO: Control file sent successfully\n", stderr);

    lpd_command(fd, "\003%u dfA%03.3d%s\n", (unsigned)filestats.st_size,
                getpid() % 1000, localhost);

    fprintf(stderr, "INFO: Sending data file (%u bytes)\n",
            (unsigned)filestats.st_size);

    tbytes = 0;
    while ((nbytes = fread(buffer, 1, sizeof(buffer), fp)) > 0)
    {
      fprintf(stderr, "INFO: Spooling LPR job, %u%% complete...\n",
              (unsigned)(100 * tbytes / filestats.st_size));

      if (send(fd, buffer, nbytes, 0) < nbytes)
      {
        perror("ERROR: Unable to send print file to printer");
        break;
      }
      else
        tbytes += nbytes;
    }

    send(fd, "", 1, 0);

    if (tbytes < filestats.st_size)
      status = 1;
    else if (recv(fd, &status, 1, 0) < 1 || status != 0)
      fprintf(stderr, "ERROR: Remote host did not accept data file (%d)\n",
              status);
    else
      fputs("INFO: Data file sent successfully\n", stderr);
  }

 /*
  * Close the socket connection and input file and return...
  */

  close(fd);
  fclose(fp);
  
  return (status);
}


/*
 * End of "$Id: lpd.c,v 1.23 2001/01/24 17:14:01 mike Exp $".
 */
