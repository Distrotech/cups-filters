/*
 * "$Id$"
 *
 *   Scheduler speed test for the Common UNIX Printing System (CUPS).
 *
 *   Copyright 2007-2008 by Apple Inc.
 *   Copyright 1997-2005 by Easy Software Products.
 *
 *   These coded instructions, statements, and computer programs are the
 *   property of Apple Inc. and are protected by Federal copyright
 *   law.  Distribution and use rights are outlined in the file "LICENSE.txt"
 *   which should have been included with this file.  If this file is
 *   file is missing or damaged, see the license at "http://www.cups.org/".
 *
 * Contents:
 *
 *   main()    - Send multiple IPP requests and report on the average response
 *               time.
 *   do_test() - Run a test on a specific host...
 *   usage()   - Show program usage...
 */

/*
 * Include necessary headers...
 */

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <cups/cups.h>
#include <cups/language.h>
#include <cups/debug.h>
#include <errno.h>


/*
 * Local functions...
 */

static int	do_test(const char *server, int port,
		        http_encryption_t encryption, int requests,
			int verbose);
static void	usage(void);


/*
 * 'main()' - Send multiple IPP requests and report on the average response
 *            time.
 */

int
main(int  argc,				/* I - Number of command-line arguments */
     char *argv[])			/* I - Command-line arguments */
{
  int		i;			/* Looping var */
  char		*server,		/* Server to use */
		*ptr;			/* Pointer to port in server */
  int		port;			/* Port to use */
  http_encryption_t encryption;		/* Encryption to use */
  int		requests;		/* Number of requests to send */
  int		children;		/* Number of children to fork */
  int		good_children;		/* Number of children that exited normally */
  int		pid;			/* Child PID */
  int		status;			/* Child status */
  time_t	start,			/* Start time */
		end;			/* End time */
  double	elapsed;		/* Elapsed time */
  int		verbose;		/* Verbosity */


 /*
  * Parse command-line options...
  */

  requests   = 100;
  children   = 5;
  server     = (char *)cupsServer();
  port       = ippPort();
  encryption = HTTP_ENCRYPT_IF_REQUESTED;
  verbose    = 0;

  for (i = 1; i < argc; i ++)
    if (argv[i][0] == '-')
    {
      for (ptr = argv[i] + 1; *ptr; ptr ++)
        switch (*ptr)
	{
	  case 'c' : /* Number of children */
	      i ++;
	      if (i >= argc)
		usage();

	      children = atoi(argv[i]);
	      break;

          case 'r' : /* Number of requests */
	      i ++;
	      if (i >= argc)
		usage();

	      requests = atoi(argv[i]);
	      break;

	  case 'E' : /* Enable encryption */
	      encryption = HTTP_ENCRYPT_REQUIRED;
	      break;

          case 'v' : /* Verbose logging */
              verbose ++;
	      break;

          default :
              usage();
	      break;
        }
    }
    else
    {
      server = argv[i];

      if ((ptr = strrchr(server, ':')) != NULL)
      {
        *ptr++ = '\0';
	port   = atoi(ptr);
      }
    }

 /*
  * Then create child processes to act as clients...
  */

  printf("testspeed: Simulating %d clients with %d requests to %s with %sencryption...\n",
         children, requests, server,
	 encryption == HTTP_ENCRYPT_IF_REQUESTED ? "no " : "");

  start = time(NULL);

  if (children == 1)
  {
    do_test(server, port, encryption, requests, verbose);
    good_children = 1;
  }
  else
  {
    for (i = 0; i < children; i ++)
    {
      fflush(stdout);

      if ((pid = fork()) == 0)
      {
       /*
	* Child goes here...
	*/

	exit(do_test(server, port, encryption, requests, verbose));
      }
      else if (pid < 0)
      {
	printf("testspeed: Fork failed: %s\n", strerror(errno));
	break;
      }
      else
	printf("testspeed: Started child %d...\n", pid);
    }

   /*
    * Wait for children to finish...
    */

    puts("testspeed: Waiting for children to finish...");

    for (good_children = 0;;)
    {
      pid = wait(&status);

      if (pid < 0 && errno != EINTR)
	break;

      printf("testspeed: Ended child %d (%d)...\n", pid, status / 256);

      if (!status)
        good_children ++;
    }
  }

 /*
  * Compute the total run time...
  */

  end     = time(NULL);
  elapsed = end - start;
  i       = good_children * requests;

  printf("testspeed: %dx%d=%d requests in %.1fs (%.3fs/r, %.1fr/s)\n",
         good_children, requests, i, elapsed, elapsed / i, i / elapsed);

 /*
  * Exit with no errors...
  */

  return (0);
}


/*
 * 'do_test()' - Run a test on a specific host...
 */

static int				/* O - Exit status */
do_test(const char        *server,	/* I - Server to use */
        int               port,		/* I - Port number to use */
        http_encryption_t encryption,	/* I - Encryption to use */
	int               requests,	/* I - Number of requests to send */
	int               verbose)	/* I - Verbose output? */
{
  int		i;			/* Looping var */
  http_t	*http;			/* Connection to server */
  ipp_t		*request;		/* IPP Request */
  struct timeval start,			/* Start time */
		end;			/* End time */
  double	reqtime,		/* Time for this request */
		elapsed;		/* Elapsed time */
  static ipp_op_t ops[4] =		/* Operations to test... */
		{
		  IPP_PRINT_JOB,
		  CUPS_GET_PRINTERS,
		  CUPS_GET_CLASSES,
		  IPP_GET_JOBS
		};


 /*
  * Connect to the server...
  */

  if ((http = httpConnectEncrypt(server, port, encryption)) == NULL)
  {
    printf("testspeed(%d): unable to connect to server - %s\n", (int)getpid(),
           strerror(errno));
    return (1);
  }

 /*
  * Do multiple requests...
  */

  for (elapsed = 0.0, i = 0; i < requests; i ++)
  {
    if (verbose && (i % 10) == 0)
      printf("testspeed(%d): %d%% complete...\n", (int)getpid(),
             i * 100 / requests);

   /*
    * Build a request which requires the following attributes:
    *
    *    attributes-charset
    *    attributes-natural-language
    *
    * In addition, IPP_GET_JOBS needs a printer-uri attribute.
    */

    request = ippNewRequest(ops[i & 3]);

    gettimeofday(&start, NULL);

    if (verbose > 1)
      printf("testspeed(%d): %d.%06d %s ", (int)getpid(),
             (int)(start.tv_sec % 86400), (int)start.tv_usec,
	     ippOpString(ops[i & 3]));

    switch (request->request.op.operation_id)
    {
      case IPP_GET_JOBS :
	  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_URI, "printer-uri",
                       NULL, "ipp://localhost/printers/");

      default :
	  ippDelete(cupsDoRequest(http, request, "/"));
          break;

      case IPP_PRINT_JOB :
	  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_URI, "printer-uri",
                       NULL, "ipp://localhost/printers/test");
          ippAddString(request, IPP_TAG_JOB, IPP_TAG_KEYWORD, "job-hold-until",
	               NULL, "indefinite");
	  ippDelete(cupsDoFileRequest(http, request, "/printers/test",
	                              "../data/testprint.ps"));
          break;
    }

    gettimeofday(&end, NULL);

    reqtime = (end.tv_sec - start.tv_sec) +
              0.000001 * (end.tv_usec - start.tv_usec);
    elapsed += reqtime;

    switch (cupsLastError())
    {
      case IPP_OK :
      case IPP_NOT_FOUND :
          if (verbose > 1)
	    printf("succeeded: %s (%.6f)\n", cupsLastErrorString(), reqtime);
          break;

      default :
          if (verbose <= 1)
	    printf("testspeed(%d): %s ", (int)getpid(),
	           ippOpString(ops[i & 3]));

	  printf("failed: %s\n", cupsLastErrorString());
          httpClose(http);
	  return (1);
    }
  }

  httpClose(http);

  printf("testspeed(%d): %d requests in %.1fs (%.3fs/r, %.1fr/s)\n",
         (int)getpid(), i, elapsed, elapsed / i, i / elapsed);

  return (0);
}


/*
 * 'usage()' - Show program usage...
 */

static void
usage(void)
{
  puts("Usage: testspeed [-c children] [-h] [-r requests] [-v] [-E] "
       "hostname[:port]");
  exit(0);
}



/*
 * End of "$Id$".
 */
