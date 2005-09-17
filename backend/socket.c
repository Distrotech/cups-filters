/*
 * "$Id$"
 *
 *   AppSocket backend for the Common UNIX Printing System (CUPS).
 *
 *   Copyright 1997-2005 by Easy Software Products, all rights reserved.
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
 *   main() - Send a file to the printer or server.
 */

/*
 * Include necessary headers.
 */

#include <cups/cups.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <cups/http-private.h>
#include <cups/string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <signal.h>

#ifdef WIN32
#  include <winsock.h>
#else
#  include <unistd.h>
#  include <fcntl.h>
#  include <sys/socket.h>
#  include <netinet/in.h>
#  include <arpa/inet.h>
#  include <netdb.h>
#endif /* WIN32 */


/*
 * 'main()' - Send a file to the printer or server.
 *
 * Usage:
 *
 *    printer-uri job-id user title copies options [file]
 */

int					/* O - Exit status */
main(int  argc,				/* I - Number of command-line arguments (6 or 7) */
     char *argv[])			/* I - Command-line arguments */
{
  int		i;			/* Looping var */
  char		method[255],		/* Method in URI */
		hostname[1024],		/* Hostname */
		username[255],		/* Username info (not used) */
		resource[1024];		/* Resource info (not used) */
  int		fp;			/* Print file */
  int		copies;			/* Number of copies to print */
  int		port;			/* Port number */
  int		delay;			/* Delay for retries... */
  int		fd;			/* AppSocket */
  int		error;			/* Error code (if any) */
  http_addr_t	addr;			/* Socket address */
  struct hostent *hostaddr;		/* Host address */
  int		rbytes;			/* Number of bytes read */
  int		wbytes;			/* Number of bytes written */
  int		nbytes;			/* Number of bytes read */
  size_t	tbytes;			/* Total number of bytes written */
  char		buffer[8192],		/* Output buffer */
		*bufptr;		/* Pointer into buffer */
  struct timeval timeout;		/* Timeout for select() */
  fd_set	input,			/* Input set for select() */
		output;			/* Output set for select() */
#if defined(HAVE_SIGACTION) && !defined(HAVE_SIGSET)
  struct sigaction action;		/* Actions for POSIX signals */
#endif /* HAVE_SIGACTION && !HAVE_SIGSET */


 /*
  * Make sure status messages are not buffered...
  */

  setbuf(stderr, NULL);

 /*
  * Ignore SIGPIPE signals...
  */

#ifdef HAVE_SIGSET
  sigset(SIGPIPE, SIG_IGN);
#elif defined(HAVE_SIGACTION)
  memset(&action, 0, sizeof(action));
  action.sa_handler = SIG_IGN;
  sigaction(SIGPIPE, &action, NULL);
#else
  signal(SIGPIPE, SIG_IGN);
#endif /* HAVE_SIGSET */

 /*
  * Check command-line...
  */

  if (argc == 1)
  {
    puts("network socket \"Unknown\" \"AppSocket/HP JetDirect\"");
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
  * Otherwise, send stdin instead...
  */

  if (argc == 6)
  {
    fp     = 0;
    copies = 1;
  }
  else
  {
   /*
    * Try to open the print file...
    */

    if ((fp = open(argv[6], O_RDONLY)) < 0)
    {
      perror("ERROR: unable to open print file");
      return (1);
    }

    copies = atoi(argv[4]);
  }

 /*
  * Extract the hostname and port number from the URI...
  */

  httpSeparate(argv[0], method, username, hostname, &port, resource);

  if (port == 0)
    port = 9100;	/* Default to HP JetDirect/Tektronix PhaserShare */

 /*
  * Then try to connect to the remote host...
  */

  if ((hostaddr = httpGetHostByName(hostname)) == NULL)
  {
    fprintf(stderr, "ERROR: Unable to locate printer \'%s\' - %s\n",
            hostname, hstrerror(h_errno));
    return (1);
  }

  fprintf(stderr, "INFO: Attempting to connect to host %s on port %d\n",
          hostname, port);

  wbytes = 0;

  while (copies > 0)
  {
    for (delay = 5;;)
    {
      if ((fd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
      {
	perror("ERROR: Unable to create socket");
	return (1);
      }

      for (i = 0; hostaddr->h_addr_list[i]; i ++)
      {
        httpAddrLoad(hostaddr, port, i, &addr);

        if (!connect(fd, (struct sockaddr *)&addr, sizeof(addr)))
	  break;
      }

      if (!hostaddr->h_addr_list[i])
      {
	error = errno;
	close(fd);
	fd = -1;

	if (getenv("CLASS") != NULL)
	{
	 /*
          * If the CLASS environment variable is set, the job was submitted
	  * to a class and not to a specific queue.  In this case, we want
	  * to abort immediately so that the job can be requeued on the next
	  * available printer in the class.
	  */

          fprintf(stderr, "INFO: Unable to connect to \"%s\", queuing on next printer in class...\n",
		  hostname);

	 /*
          * Sleep 5 seconds to keep the job from requeuing too rapidly...
	  */

	  sleep(5);

          return (1);
	}

	if (error == ECONNREFUSED || error == EHOSTDOWN ||
            error == EHOSTUNREACH)
	{
	  fprintf(stderr, "INFO: Network host \'%s\' is busy; will retry in %d seconds...\n",
                  hostname, delay);
	  sleep(delay);

	  if (delay < 30)
	    delay += 5;
	}
	else
	{
	  perror("ERROR: Unable to connect to printer (retrying in 30 seconds)");
	  sleep(30);
	}
      }
      else
	break;
    }

   /*
    * Now that we are "connected" to the port, ignore SIGTERM so that we
    * can finish out any page data the driver sends (e.g. to eject the
    * current page...  Only ignore SIGTERM if we are printing data from
    * stdin (otherwise you can't cancel raw jobs...)
    */

    if (argc < 7)
    {
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
    }

   /*
    * Finally, send the print file...
    */

    copies --;

    if (fp != 0)
    {
      fputs("PAGE: 1 1\n", stderr);
      lseek(fp, 0, SEEK_SET);
    }

    fputs("INFO: Connected to host, sending print job...\n", stderr);

    tbytes = 0;
    while ((nbytes = read(fp, buffer, sizeof(buffer))) > 0)
    {
     /*
      * Write the print data to the printer...
      */

      tbytes += nbytes;
      bufptr = buffer;

      while (nbytes > 0)
      {
       /*
        * See if we are ready to read or write...
	*/

        do
	{
          FD_ZERO(&input);
	  FD_SET(fd, &input);
	  FD_ZERO(&output);
	  FD_SET(fd, &output);
        }
	while (select(fd + 1, &input, &output, NULL, NULL) < 0);

        if (FD_ISSET(fd, &input))
	{
	 /*
	  * Read backchannel data...
	  */

	  if ((rbytes = recv(fd, resource, sizeof(resource), 0)) > 0)
	  {
	    fprintf(stderr, "DEBUG: Received %d bytes of back-channel data!\n",
	            rbytes);
            cupsBackchannelWrite(resource, rbytes, 1.0);
          }
	}

        if (FD_ISSET(fd, &output))
	{
	 /*
	  * Write print data...
	  */

	  if ((wbytes = send(fd, bufptr, nbytes, 0)) < 0)
	  {
	   /*
	    * Check for retryable errors...
	    */

	    if (errno != EAGAIN && errno != EINTR)
	    {
	      perror("ERROR: Unable to send print file to printer");
	      break;
	    }
	  }
	  else
	  {
	   /*
	    * Update count and pointer...
	    */

	    nbytes -= wbytes;
	    bufptr += wbytes;
	  }
        }
      }

      if (wbytes < 0)
        break;

      if (argc > 6)
	fprintf(stderr, "INFO: Sending print file, %lu bytes...\n",
	        (unsigned long)tbytes);
    }

   /*
    * Shutdown the socket and wait for the other end to finish...
    */

    fputs("INFO: Print file sent, waiting for printer to finish...\n", stderr);

    shutdown(fd, 1);

    for (;;)
    {
     /*
      * Wait a maximum of 90 seconds for backchannel data or a closed
      * connection...
      */

      timeout.tv_sec  = 90;
      timeout.tv_usec = 0;

      FD_ZERO(&input);
      FD_SET(fd, &input);

#ifdef __hpux
      if (select(fd + 1, (int *)&input, NULL, NULL, &timeout) > 0)
#else
      if (select(fd + 1, &input, NULL, NULL, &timeout) > 0)
#endif /* __hpux */
      {
       /*
	* Grab the data coming back and spit it out to stderr...
	*/

	if ((rbytes = recv(fd, resource, sizeof(resource), 0)) > 0)
	{
	  fprintf(stderr, "DEBUG: Received %d bytes of back-channel data!\n",
	          rbytes);
          cupsBackchannelWrite(resource, rbytes, 1.0);
        }
	else
	  break;
      }
      else
        break;
    }

   /*
    * Close the socket connection...
    */

    close(fd);
  }

 /*
  * Close the input file and return...
  */

  if (fp != 0)
    close(fp);

  return (wbytes < 0);
}


/*
 * End of "$Id$".
 */
