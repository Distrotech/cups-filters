/*
 * "$Id: server.c,v 1.2.2.10 2003/03/28 22:29:49 mike Exp $"
 *
 *   Server start/stop routines for the Common UNIX Printing System (CUPS).
 *
 *   Copyright 1997-2003 by Easy Software Products, all rights reserved.
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
 *   StartServer() - Start the server.
 *   StopServer()  - Stop the server.
 */

/*
 * Include necessary headers...
 */

#include <cups/http-private.h>
#include "cupsd.h"

#include <grp.h>


/*
 * 'StartServer()' - Start the server.
 */

void
StartServer(void)
{
#ifdef HAVE_LIBSSL
  int		i;		/* Looping var */
  struct timeval curtime;	/* Current time in microseconds */
  unsigned char	data[1024];	/* Seed data */
#endif /* HAVE_LIBSSL */


#ifdef HAVE_LIBSSL
 /*
  * Initialize the encryption libraries...
  */

  SSL_library_init();
  SSL_load_error_strings();

 /*
  * Using the current time is a dubious random seed, but on some systems
  * it is the best we can do (on others, this seed isn't even used...)
  */

  gettimeofday(&curtime, NULL);
  srand(curtime.tv_sec + curtime.tv_usec);

  for (i = 0; i < sizeof(data); i ++)
    data[i] = rand(); /* Yes, this is a poor source of random data... */

  RAND_seed(&data, sizeof(data));
#elif defined(HAVE_GNUTLS)
 /*
  * Initialize the encryption libraries...
  */

  gnutls_global_init();
#endif /* HAVE_LIBSSL */

 /*
  * Startup all the networking stuff...
  */

  StartListening();
  StartBrowsing();
  StartPolling();

 /*
  * Create a pipe for CGI processes...
  */

  pipe(CGIPipes);

  LogMessage(L_DEBUG2, "StartServer: Adding fd %d to InputSet...", CGIPipes[0]);
  FD_SET(CGIPipes[0], InputSet);
}


/*
 * 'StopServer()' - Stop the server.
 */

void
StopServer(void)
{
 /*
  * Close all network clients and stop all jobs...
  */

  CloseAllClients();
  StopListening();
  StopPolling();
  StopBrowsing();

  if (Clients != NULL)
  {
    free(Clients);
    Clients = NULL;
  }

  StopAllJobs();

 /*
  * Close the pipe for CGI processes...
  */

  if (CGIPipes[0] >= 0)
  {
    close(CGIPipes[0]);
    close(CGIPipes[1]);

    CGIPipes[0] = -1;
    CGIPipes[1] = -1;
  }

 /*
  * Close all log files...
  */

  if (AccessFile != NULL)
  {
    fclose(AccessFile);

    AccessFile = NULL;
  }

  if (ErrorFile != NULL)
  {
    fclose(ErrorFile);

    ErrorFile = NULL;
  }

  if (PageFile != NULL)
  {
    fclose(PageFile);

    PageFile = NULL;
  }

 /*
  * Clear the input and output sets...
  */

  memset(InputSet, 0, SetSize);
  memset(OutputSet, 0, SetSize);
}


/*
 * End of "$Id: server.c,v 1.2.2.10 2003/03/28 22:29:49 mike Exp $".
 */
