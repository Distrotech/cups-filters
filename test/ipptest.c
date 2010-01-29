/*
 * "$Id$"
 *
 *   IPP test command for the Common UNIX Printing System (CUPS).
 *
 *   Copyright 2007-2010 by Apple Inc.
 *   Copyright 1997-2007 by Easy Software Products.
 *
 *   These coded instructions, statements, and computer programs are the
 *   property of Apple Inc. and are protected by Federal copyright
 *   law.  Distribution and use rights are outlined in the file "LICENSE.txt"
 *   which should have been included with this file.  If this file is
 *   file is missing or damaged, see the license at "http://www.cups.org/".
 *
 * Contents:
 *
 *   main()              - Parse options and do tests.
 *   compare_vars()      - Compare two variables.
 *   do_tests()          - Do tests as specified in the test file.
 *   expand_variables()  - Expand variables in a string.
 *   expect_matches()    - Return true if the tag matches the specification.
 *   get_filename()      - Get a filename based on the current test file.
 *   get_token()         - Get a token from a file.
 *   get_variable()      - Get the value of a variable.
 *   iso_date()          - Return an ISO 8601 date/time string for the given IPP
 *                         dateTime value.
 *   print_attr()        - Print an attribute on the screen.
 *   print_col()         - Print a collection attribute on the screen.
 *   print_fatal_error() - Print a fatal error message.
 *   print_test_error()  - Print a test error message.
 *   print_xml_header()  - Print a standard XML plist header.
 *   print_xml_string()  - Print an XML string with escaping.
 *   print_xml_trailer() - Print the XML trailer with success/fail value.
 *   set_variable()      - Set a variable value.
 *   usage()             - Show program usage.
 *   validate_attr()     - Determine whether an attribute is valid.
 *   with_value()        - Test a WITH-VALUE predicate.
 */

/*
 * Include necessary headers...
 */

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <ctype.h>
#include <regex.h>

#include <cups/globals.h>
#ifndef O_BINARY
#  define O_BINARY 0
#endif /* !O_BINARY */


/*
 * Types...
 */

typedef struct _cups_expect_s		/**** Expected attribute info ****/
{
  int		optional,		/* Optional attribute? */
		not_expect;		/* Don't expect attribute? */
  char		*name,			/* Attribute name */
		*of_type,		/* Type name */
		*same_count_as,		/* Parallel attribute name */
		*if_defined,		/* Only required if variable defined */
		*if_undefined,		/* Only required if variable is not defined */
		*with_value;		/* Attribute must include this value */
  int		with_regex,		/* WITH-VALUE is a regular expression */
		count;			/* Expected count if > 0 */
  ipp_tag_t	in_group;		/* IN-GROUP value */
} _cups_expect_t;

typedef struct _cups_var_s		/**** Variable ****/
{
  char		*name,			/* Name of variable */
		*value;			/* Value of variable */
} _cups_var_t;

typedef struct _cups_vars_s		/**** Set of variables ****/
{
  const char	*uri,			/* URI for printer */
		*filename;		/* Filename */
  char		scheme[64],		/* Scheme from URI */
		userpass[256],		/* Username/password from URI */
		hostname[256],		/* Hostname from URI */
		resource[1024];		/* Resource path from URI */
  int 		port;			/* Port number from URI */
  cups_array_t	*vars;			/* Array of variables */
} _cups_vars_t;


/*
 * Globals...
 */

int		Chunking = 1,		/* Use chunked requests */
		Verbosity = 0,		/* Show all attributes? */
		Version = 11,		/* Default IPP version */
		XML = 0,		/* Produce XML output? */
		XMLHeader = 0;		/* 1 if header is written */
const char * const URIStatusStrings[] =	/* URI status strings */
{
  "URI too large",
  "Bad arguments to function",
  "Bad resource in URI",
  "Bad port number in URI",
  "Bad hostname/address in URI",
  "Bad username in URI",
  "Bad scheme in URI",
  "Bad/empty URI",
  "OK",
  "Missing scheme in URI",
  "Unknown scheme in URI",
  "Missing resource in URI"
};


/*
 * Local functions...
 */

static int	compare_vars(_cups_var_t *a, _cups_var_t *b);
static int	do_tests(_cups_vars_t *vars, const char *testfile);
static void	expand_variables(_cups_vars_t *vars, char *dst, const char *src,
		                 size_t dstsize);
static int      expect_matches(_cups_expect_t *expect, ipp_tag_t value_tag);
static char	*get_filename(const char *testfile, char *dst, const char *src,
		              size_t dstsize);
static char	*get_token(FILE *fp, char *buf, int buflen,
		           int *linenum);
static char	*get_variable(_cups_vars_t *vars, const char *name);
static char	*iso_date(ipp_uchar_t *date);
static void	print_attr(ipp_attribute_t *attr);
static void	print_col(ipp_t *col);
static void	print_fatal_error(const char *s, ...)
#ifdef __GNUC__
__attribute__ ((__format__ (__printf__, 1, 2)))
#endif /* __GNUC__ */
;
static void	print_test_error(const char *s, ...)
#ifdef __GNUC__
__attribute__ ((__format__ (__printf__, 1, 2)))
#endif /* __GNUC__ */
;
static void	print_xml_header(void);
static void	print_xml_string(const char *element, const char *s);
static void	print_xml_trailer(int success, const char *message);
static void	set_variable(_cups_vars_t *vars, const char *name,
		             const char *value);
static void	usage(void);
static int	validate_attr(ipp_attribute_t *attr, int print);
static int      with_value(char *value, int regex, ipp_attribute_t *attr);


/*
 * 'main()' - Parse options and do tests.
 */

int					/* O - Exit status */
main(int  argc,				/* I - Number of command-line args */
     char *argv[])			/* I - Command-line arguments */
{
  int			i;		/* Looping var */
  int			status;		/* Status of tests... */
  char			*opt,		/* Current option */
			name[1024],	/* Name/value buffer */
			*value;		/* Pointer to value */
  const char		*testfile;	/* Test file to use */
  int			interval;	/* Test interval */
  _cups_vars_t		vars;		/* Variables */
  http_uri_status_t	uri_status;	/* URI separation status */


 /*
  * Initialize the locale and variables...
  */

  _cupsSetLocale(argv);

  memset(&vars, 0, sizeof(vars));
  vars.vars = cupsArrayNew((cups_array_func_t)compare_vars, NULL);

 /*
  * We need at least:
  *
  *     ipptest URI testfile
  */

  testfile = NULL;
  status   = 0;
  interval = 0;

  for (i = 1; i < argc; i ++)
  {
    if (argv[i][0] == '-')
    {
      for (opt = argv[i] + 1; *opt; opt ++)
      {
        switch (*opt)
        {
	  case 'E' : /* Encrypt */
#ifdef HAVE_SSL
	      cupsSetEncryption(HTTP_ENCRYPT_REQUIRED);
#else
	      _cupsLangPrintf(stderr,
			      _("%s: Sorry, no encryption support compiled in\n"),
			      argv[0]);
#endif /* HAVE_SSL */
	      break;

	  case 'V' : /* Set IPP version */
	      i ++;

	      if (i >= argc)
	      {
		_cupsLangPuts(stderr,
		              _("ipptest: Missing version for \"-V\".\n"));
		usage();
              }

	      if (!strcmp(argv[i], "1.0"))
	        Version = 10;
	      else if (!strcmp(argv[i], "1.1"))
	        Version = 11;
	      else if (!strcmp(argv[i], "2.0"))
	        Version = 20;
	      else if (!strcmp(argv[i], "2.1"))
	        Version = 21;
	      else if (!strcmp(argv[i], "2.2"))
	        Version = 22;
	      else
	      {
		_cupsLangPrintf(stderr,
		                _("ipptest: Bad version %s for \"-V\".\n"),
				argv[i]);
		usage();
	      }
	      break;

          case 'X' : /* Produce XML output */
	      XML = 1;

              if (interval)
	      {
	        _cupsLangPuts(stderr, _("ipptest: \"-i\" is incompatible with "
				        "\"-x\".\n"));
		usage();
	      }
	      break;

          case 'c' : /* Enable HTTP chunking */
              Chunking = 1;
              break;

          case 'd' : /* Define a variable */
	      i ++;

	      if (i >= argc)
	      {
		_cupsLangPuts(stderr,
		              _("ipptest: Missing name=value for \"-d\".\n"));
		usage();
              }

              strlcpy(name, argv[i], sizeof(name));
	      if ((value = strchr(name, '=')) != NULL)
	        *value++ = '\0';
	      else
	        value = name + strlen(name);

	      set_variable(&vars, name, value);
	      break;

          case 'f' : /* Set the default test filename */
	      i ++;

	      if (i >= argc)
	      {
		_cupsLangPuts(stderr,
		              _("ipptest: Missing filename for \"-f\".\n"));
		usage();
              }

	      vars.filename = argv[i];
	      break;

          case 'i' : /* Test every N seconds */
	      i++;

	      if (i >= argc)
	      {
		_cupsLangPuts(stderr,
		              _("ipptest: Missing seconds for \"-i\".\n"));
		usage();
              }
	      else
		interval = atoi(argv[i]);

              if (XML && interval)
	      {
	        _cupsLangPuts(stderr, _("ipptest: \"-i\" is incompatible with "
			                "\"-x\".\n"));
		usage();
	      }
	      break;

          case 'l' : /* Disable HTTP chunking */
              Chunking = 0;
              break;

          case 'v' : /* Be verbose */
	      Verbosity ++;
	      break;

	  default :
	      _cupsLangPrintf(stderr, _("ipptest: Unknown option \"-%c\".\n"),
	                      *opt);
	      usage();
	      break;
	}
      }
    }
    else if (!strncmp(argv[i], "ipp://", 6) ||
             !strncmp(argv[i], "http://", 7) ||
             !strncmp(argv[i], "https://", 8))
    {
     /*
      * Set URI...
      */

      if (vars.uri)
      {
        _cupsLangPuts(stderr, _("ipptest: May only specify a single URI.\n"));
        usage();
      }

      vars.uri   = argv[i];
      uri_status = httpSeparateURI(HTTP_URI_CODING_ALL, vars.uri,
                                   vars.scheme, sizeof(vars.scheme),
                                   vars.userpass, sizeof(vars.userpass),
				   vars.hostname, sizeof(vars.hostname),
				   &(vars.port),
				   vars.resource, sizeof(vars.resource));

      if (uri_status != HTTP_URI_OK)
      {
        _cupsLangPrintf(stderr, _("ipptest: Bad URI - %s.\n"),
	                URIStatusStrings[uri_status - HTTP_URI_OVERFLOW]);
        return (1);
      }

      if (strcmp(vars.scheme, "http") && strcmp(vars.scheme, "https") &&
          strcmp(vars.scheme, "ipp"))
      {
        _cupsLangPuts(stderr, _("ipptest: Only http, https, and ipp URIs are "
	                        "supported."));
	return (1);
      }
    }
    else
    {
     /*
      * Run test...
      */

      if (!vars.uri)
      {
        _cupsLangPuts(stderr, _("ipptest: URI required before test file."));
	usage();
      }

      testfile = argv[i];

      if (!do_tests(&vars, testfile))
        status = 1;
    }
  }

  if (!vars.uri || !testfile)
    usage();

 /*
  * Loop if the interval is set...
  */

  if (XML)
    print_xml_trailer(!status, NULL);
  else if (interval)
  {
    for (;;)
    {
      sleep(interval);
      do_tests(&vars, testfile);
    }
  }

 /*
  * Exit...
  */

  return (status);
}


/*
 * 'compare_vars()' - Compare two variables.
 */

static int				/* O - Result of comparison */
compare_vars(_cups_var_t *a,		/* I - First variable */
             _cups_var_t *b)		/* I - Second variable */
{
  return (strcasecmp(a->name, b->name));
}


/*
 * 'do_tests()' - Do tests as specified in the test file.
 */

static int				/* 1 = success, 0 = failure */
do_tests(_cups_vars_t *vars,		/* I - Variables */
         const char   *testfile)	/* I - Test file to use */
{
  int		i,			/* Looping var */
		linenum,		/* Current line number */
		pass,			/* Did we pass the test? */
		request_id;		/* Current request ID */
  http_t	*http = NULL;		/* HTTP connection to server */
  FILE		*fp = NULL;		/* Test file */
  char		resource[512],		/* Resource for request */
		token[1024],		/* Token from file */
		*tokenptr,		/* Pointer into token */
		temp[1024];		/* Temporary string */
  ipp_t		*request = NULL;	/* IPP request */
  ipp_t		*response = NULL;	/* IPP response */
  char		attr[128];		/* Attribute name */
  ipp_op_t	op;			/* Operation */
  ipp_tag_t	group;			/* Current group */
  ipp_tag_t	value;			/* Current value type */
  ipp_attribute_t *attrptr,		/* Attribute pointer */
		*found;			/* Found attribute */
  char		name[1024];		/* Name of test */
  char		filename[1024];		/* Filename */
  int		chunking,		/* To chunk or not to chunk */
		version;		/* IPP version number to use */
  int		num_statuses;		/* Number of valid status codes */
  ipp_status_t	statuses[100];		/* Valid status codes */
  int		num_expects = 0;	/* Number of expected attributes */
  _cups_expect_t expects[200],		/* Expected attributes */
		*expect,		/* Current expected attribute */
		*last_expect;		/* Last EXPECT (for predicates) */
  int		num_displayed = 0;	/* Number of displayed attributes */
  char		*displayed[100];	/* Displayed attributes */


 /*
  * Open the test file...
  */

  if ((fp = fopen(testfile, "r")) == NULL)
  {
    print_fatal_error("Unable to open test file %s - %s", testfile,
                      strerror(errno));
    goto test_error;
  }

 /*
  * Connect to the server...
  */

  if ((http = httpConnectEncrypt(vars->hostname, vars->port,
                                 cupsEncryption())) == NULL)
  {
    print_fatal_error("Unable to connect to %s on port %d - %s", vars->hostname,
                      vars->port, strerror(errno));
    goto test_error;
  }

 /*
  * Loop on tests...
  */

  if (XML)
    print_xml_header();
  else
    printf("\"%s\":\n", testfile);

  CUPS_SRAND(time(NULL));

  pass       = 1;
  linenum    = 1;
  request_id = (CUPS_RAND() % 1000) * 137 + 1;

  while (get_token(fp, token, sizeof(token), &linenum) != NULL)
  {
   /*
    * Expect an open brace...
    */

    if (!strcmp(token, "DEFINE"))
    {
     /*
      * DEFINE name value
      */

      if (get_token(fp, attr, sizeof(attr), &linenum) &&
          get_token(fp, temp, sizeof(temp), &linenum))
      {
        expand_variables(vars, token, temp, sizeof(token));
	set_variable(vars, attr, token);
      }
      else
      {
        print_fatal_error("Missing DEFINE name and/or value on line %d.",
	                  linenum);
        goto test_error;
      }

      continue;
    }
    else if (!strcmp(token, "INCLUDE"))
    {
     /*
      * INCLUDE "filename"
      * INCLUDE <filename>
      */

      if (get_token(fp, temp, sizeof(temp), &linenum))
      {
       /*
        * Map the filename to and then run the tests...
	*/

        if (!do_tests(vars, get_filename(testfile, filename, temp,
	                                 sizeof(filename))))
	  goto test_error;
      }
      else
      {
        print_fatal_error("Missing INCLUDE filename on line %d.", linenum);
        goto test_error;
      }

      continue;
    }
    else if (!strcmp(token, "TRANSFER"))
    {
     /*
      * TRANSFER chunked
      * TRANSFER length
      */

      if (get_token(fp, temp, sizeof(temp), &linenum))
      {
        if (!strcmp(temp, "chunked"))
	  Chunking = 1;
	else if (!strcmp(temp, "length"))
	  Chunking = 0;
	else
	{
	  print_fatal_error("Bad TRANSFER value \"%s\" on line %d.", temp,
	                    linenum);
	  goto test_error;
	}
      }
      else
      {
        print_fatal_error("Missing TRANSFER value on line %d.", linenum);
        goto test_error;
      }

      continue;
    }
    else if (!strcmp(token, "VERSION"))
    {
      if (get_token(fp, temp, sizeof(temp), &linenum))
      {
        if (!strcmp(temp, "1.0"))
	  Version = 10;
	else if (!strcmp(temp, "1.1"))
	  Version = 11;
	else if (!strcmp(temp, "2.0"))
	  Version = 20;
	else if (!strcmp(temp, "2.1"))
	  Version = 21;
	else if (!strcmp(temp, "2.2"))
	  Version = 22;
	else
	{
	  print_fatal_error("Bad VERSION \"%s\" on line %d.", temp, linenum);
	  goto test_error;
	}
      }
      else
      {
        print_fatal_error("Missing VERSION number on line %d.", linenum);
        goto test_error;
      }

      continue;
    }
    else if (strcmp(token, "{"))
    {
      print_fatal_error("Unexpected token %s seen on line %d.", token, linenum);
      goto test_error;
    }

   /*
    * Initialize things...
    */

    strlcpy(resource, vars->resource, sizeof(resource));

    request_id ++;
    request       = ippNew();
    op            = (ipp_op_t)0;
    group         = IPP_TAG_ZERO;
    num_statuses  = 0;
    num_expects   = 0;
    num_displayed = 0;
    last_expect   = NULL;
    filename[0]   = '\0';
    version       = Version;
    chunking      = Chunking;

    strlcpy(name, testfile, sizeof(name));
    if (strrchr(name, '.') != NULL)
      *strrchr(name, '.') = '\0';

   /*
    * Parse until we see a close brace...
    */

    while (get_token(fp, token, sizeof(token), &linenum) != NULL)
    {
      if (strcasecmp(token, "COUNT") &&
          strcasecmp(token, "EXPECT") &&
          strcasecmp(token, "IF-DEFINED") &&
          strcasecmp(token, "IF-UNDEFINED") &&
          strcasecmp(token, "IN-GROUP") &&
          strcasecmp(token, "OF-TYPE") &&
          strcasecmp(token, "SAME-COUNT-AS") &&
          strcasecmp(token, "WITH-VALUE"))
        last_expect = NULL;

      if (!strcmp(token, "}"))
        break;
      else if (!strcmp(token, "DEFINE"))
      {
       /*
	* DEFINE name value
	*/

	if (get_token(fp, attr, sizeof(attr), &linenum) &&
	    get_token(fp, temp, sizeof(temp), &linenum))
	{
	  expand_variables(vars, token, temp, sizeof(token));
	  set_variable(vars, attr, token);
	}
	else
	{
	  print_fatal_error("Missing DEFINE name and/or value on line %d.",
			    linenum);
	  goto test_error;
	}
      }
      else if (!strcasecmp(token, "NAME"))
      {
       /*
        * Name of test...
	*/

	get_token(fp, name, sizeof(name), &linenum);
      }
      else if (!strcmp(token, "REQUEST-ID"))
      {
       /*
	* REQUEST-ID #
	* REQUEST-ID random
	*/

	if (get_token(fp, temp, sizeof(temp), &linenum))
	{
	  if (isdigit(temp[0] & 255))
	    request_id = atoi(temp);
	  else if (!strcasecmp(temp, "random"))
	    request_id = (CUPS_RAND() % 1000) * 137 + 1;
	  else
	  {
	    print_fatal_error("Bad REQUEST-ID value \"%s\" on line %d.", temp,
			      linenum);
	    goto test_error;
	  }
	}
	else
	{
	  print_fatal_error("Missing REQUEST-ID value on line %d.", linenum);
	  goto test_error;
	}
      }
      else if (!strcmp(token, "TRANSFER"))
      {
       /*
	* TRANSFER chunked
	* TRANSFER length
	*/

	if (get_token(fp, temp, sizeof(temp), &linenum))
	{
	  if (!strcmp(temp, "chunked"))
	    chunking = 1;
	  else if (!strcmp(temp, "length"))
	    chunking = 0;
	  else
	  {
	    print_fatal_error("Bad TRANSFER value \"%s\" on line %d.", temp,
			      linenum);
	    goto test_error;
	  }
	}
	else
	{
	  print_fatal_error("Missing TRANSFER value on line %d.", linenum);
	  goto test_error;
	}
      }
      else if (!strcasecmp(token, "VERSION"))
      {
	if (get_token(fp, temp, sizeof(temp), &linenum))
	{
	  if (!strcmp(temp, "0.0"))
	    version = 0;
	  else if (!strcmp(temp, "1.0"))
	    version = 10;
	  else if (!strcmp(temp, "1.1"))
	    version = 11;
	  else if (!strcmp(temp, "2.0"))
	    version = 20;
	  else if (!strcmp(temp, "2.1"))
	    version = 21;
	  else if (!strcmp(temp, "2.2"))
	    version = 22;
	  else
	  {
	    print_fatal_error("Bad VERSION \"%s\" on line %d.", temp, linenum);
	    goto test_error;
	  }
	}
	else
	{
	  print_fatal_error("Missing VERSION number on line %d.", linenum);
	  goto test_error;
	}
      }
      else if (!strcasecmp(token, "RESOURCE"))
      {
       /*
        * Resource name...
	*/

	if (!get_token(fp, resource, sizeof(resource), &linenum))
	{
	  print_fatal_error("Missing RESOURCE path on line %d.", linenum);
	  goto test_error;
	}
      }
      else if (!strcasecmp(token, "OPERATION"))
      {
       /*
        * Operation...
	*/

	if (!get_token(fp, token, sizeof(token), &linenum))
	{
	  print_fatal_error("Missing OPERATION code on line %d.", linenum);
	  goto test_error;
	}

	if ((op = ippOpValue(token)) < 0)
	{
	  print_fatal_error("Bad OPERATION code \"%s\" on line %d.", token,
	                    linenum);
	  goto test_error;
	}
      }
      else if (!strcasecmp(token, "GROUP"))
      {
       /*
        * Attribute group...
	*/

	if (!get_token(fp, token, sizeof(token), &linenum))
	{
	  print_fatal_error("Missing GROUP tag on line %d.", linenum);
	  goto test_error;
	}

	if ((value = ippTagValue(token)) < 0)
	{
	  print_fatal_error("Bad GROUP tag \"%s\" on line %d.", token, linenum);
	  goto test_error;
	}

	if (value == group)
	  ippAddSeparator(request);

        group = value;
      }
      else if (!strcasecmp(token, "DELAY"))
      {
       /*
        * Delay before operation...
	*/

        int delay;

	if (!get_token(fp, token, sizeof(token), &linenum))
	{
	  print_fatal_error("Missing DELAY value on line %d.", linenum);
	  goto test_error;
	}

	if ((delay = atoi(token)) <= 0)
	{
	  print_fatal_error("Bad DELAY value \"%s\" on line %d.", token,
	                    linenum);
	  goto test_error;
	}
	else
	  sleep(delay);
      }
      else if (!strcasecmp(token, "ATTR"))
      {
       /*
        * Attribute...
	*/

	if (!get_token(fp, token, sizeof(token), &linenum))
	{
	  print_fatal_error("Missing ATTR value tag on line %d.", linenum);
	  goto test_error;
	}

	if ((value = ippTagValue(token)) < 0)
	{
	  print_fatal_error("Bad ATTR value tag \"%s\" on line %d.", token,
	                    linenum);
	  goto test_error;
	}

	if (!get_token(fp, attr, sizeof(attr), &linenum))
	{
	  print_fatal_error("Missing ATTR name on line %d.", linenum);
	  goto test_error;
	}

	if (!get_token(fp, temp, sizeof(temp), &linenum))
	{
	  print_fatal_error("Missing ATTR value on line %d.", linenum);
	  goto test_error;
	}

        expand_variables(vars, token, temp, sizeof(token));

        switch (value)
	{
	  case IPP_TAG_BOOLEAN :
	      if (!strcasecmp(token, "true"))
		ippAddBoolean(request, group, attr, 1);
              else
		ippAddBoolean(request, group, attr, atoi(token));
	      break;

	  case IPP_TAG_INTEGER :
	  case IPP_TAG_ENUM :
	      ippAddInteger(request, group, value, attr, atoi(token));
	      break;

	  case IPP_TAG_RESOLUTION :
	      {
	        int	xres,		/* X resolution */
			yres;		/* Y resolution */
		char	units[6];	/* Units */

                if (sscanf(token, "%dx%d%5s", &xres, &yres, units) != 3 ||
		    (strcasecmp(units, "dpi") && strcasecmp(units, "dpc") &&
		     strcasecmp(units, "other")))
		{
		  print_fatal_error("Bad resolution value \"%s\" on line %d.",
		                    token, linenum);
		  goto test_error;
		}

		if (!strcasecmp(units, "dpi"))
		  ippAddResolution(request, group, attr, xres, yres,
				   IPP_RES_PER_INCH);
		else if (!strcasecmp(units, "dpc"))
		  ippAddResolution(request, group, attr, xres, yres,
				   IPP_RES_PER_CM);
		else
		  ippAddResolution(request, group, attr, xres, yres,
				   (ipp_res_t)0);
	      }
	      break;

	  case IPP_TAG_RANGE :
	      {
	        int	lowers[4],	/* Lower value */
			uppers[4],	/* Upper values */
			num_vals;	/* Number of values */


		num_vals = sscanf(token, "%d-%d,%d-%d,%d-%d,%d-%d",
		                  lowers + 0, uppers + 0,
				  lowers + 1, uppers + 1,
				  lowers + 2, uppers + 2,
				  lowers + 3, uppers + 3);

                if ((num_vals & 1) || num_vals == 0)
		{
		  print_fatal_error("Bad rangeOfInteger value \"%s\" on line "
		                    "%d.", token, linenum);
		  goto test_error;
		}

		ippAddRanges(request, group, attr, num_vals / 2, lowers,
		             uppers);
	      }
	      break;

	  default :
	      if (!strchr(token, ','))
	        ippAddString(request, group, value, attr, NULL, token);
	      else
	      {
	       /*
	        * Multiple string values...
		*/

                int	num_values;	/* Number of values */
                char	*values[100],	/* Values */
			*ptr;		/* Pointer to next value */


                values[0]  = token;
		num_values = 1;

                for (ptr = strchr(token, ','); ptr; ptr = strchr(ptr, ','))
		{
		  *ptr++ = '\0';
		  values[num_values] = ptr;
		  num_values ++;
		}

	        ippAddStrings(request, group, value, attr, num_values,
		              NULL, (const char **)values);
	      }
	      break;
	}
      }
      else if (!strcasecmp(token, "FILE"))
      {
       /*
        * File...
	*/

	if (!get_token(fp, temp, sizeof(temp), &linenum))
	{
	  print_fatal_error("Missing FILE filename on line %d.", linenum);
	  goto test_error;
	}

	get_filename(testfile, filename, temp, sizeof(filename));
      }
      else if (!strcasecmp(token, "STATUS"))
      {
       /*
        * Status...
	*/

        if (num_statuses >= (int)(sizeof(statuses) / sizeof(statuses[0])))
	{
	  print_fatal_error("Too many STATUS's on line %d.", linenum);
	  goto test_error;
	}

	if (!get_token(fp, token, sizeof(token), &linenum))
	{
	  print_fatal_error("Missing STATUS code on line %d.", linenum);
	  goto test_error;
	}

	if ((statuses[num_statuses] = ippErrorValue(token)) < 0)
	{
	  print_fatal_error("Bad STATUS code \"%s\" on line %d.", token,
	                    linenum);
	  goto test_error;
	}

	num_statuses ++;
      }
      else if (!strcasecmp(token, "EXPECT"))
      {
       /*
        * Expected attributes...
	*/

        if (num_expects >= (int)(sizeof(expects) / sizeof(expects[0])))
        {
	  print_fatal_error("Too many EXPECT's on line %d.", linenum);
	  goto test_error;
        }

	if (!get_token(fp, token, sizeof(token), &linenum))
	{
	  print_fatal_error("Missing EXPECT name on line %d.", linenum);
	  goto test_error;
	}

        last_expect = expects + num_expects;
	num_expects ++;

	memset(last_expect, 0, sizeof(_cups_expect_t));

        if (token[0] == '!')
        {
          last_expect->not_expect = 1;
          last_expect->name       = strdup(token + 1);
        }
        else if (token[0] == '?')
        {
          last_expect->optional = 1;
          last_expect->name     = strdup(token + 1);
        }
        else
	  last_expect->name = strdup(token);
      }
      else if (!strcasecmp(token, "COUNT"))
      {
	if (!get_token(fp, token, sizeof(token), &linenum))
	{
	  print_fatal_error("Missing COUNT number on line %d.", linenum);
	  goto test_error;
	}

        if ((i = atoi(token)) <= 0)
	{
	  print_fatal_error("Bad COUNT \"%s\" on line %d.", token, linenum);
	  goto test_error;
	}

	if (last_expect)
	  last_expect->count = i;
	else
	{
	  print_fatal_error("COUNT without a preceding EXPECT on line %d.",
	                    linenum);
	  goto test_error;
	}
      }
      else if (!strcasecmp(token, "OF-TYPE"))
      {
	if (!get_token(fp, token, sizeof(token), &linenum))
	{
	  print_fatal_error("Missing OF-TYPE value tag(s) on line %d.",
	                    linenum);
	  goto test_error;
	}

	if (last_expect)
	  last_expect->of_type = strdup(token);
	else
	{
	  print_fatal_error("OF-TYPE without a preceding EXPECT on line %d.",
	                    linenum);
	  goto test_error;
	}
      }
      else if (!strcasecmp(token, "IN-GROUP"))
      {
        ipp_tag_t	in_group;	/* IN-GROUP value */


	if (!get_token(fp, token, sizeof(token), &linenum))
	{
	  print_fatal_error("Missing IN-GROUP group tag on line %d.", linenum);
	  goto test_error;
	}

        if ((in_group = ippTagValue(token)) == (ipp_tag_t)-1)
	{
	}
	else if (last_expect)
	  last_expect->in_group = in_group;
	else
	{
	  print_fatal_error("IN-GROUP without a preceding EXPECT on line %d.",
	                    linenum);
	  goto test_error;
	}
      }
      else if (!strcasecmp(token, "SAME-COUNT-AS"))
      {
	if (!get_token(fp, token, sizeof(token), &linenum))
	{
	  print_fatal_error("Missing SAME-COUNT-AS name on line %d.", linenum);
	  goto test_error;
	}

	if (last_expect)
	  last_expect->same_count_as = strdup(token);
	else
	{
	  print_fatal_error("SAME-COUNT-AS without a preceding EXPECT on line "
	                    "%d.", linenum);
	  goto test_error;
	}
      }
      else if (!strcasecmp(token, "IF-DEFINED"))
      {
	if (!get_token(fp, token, sizeof(token), &linenum))
	{
	  print_fatal_error("Missing IF-DEFINED name on line %d.", linenum);
	  goto test_error;
	}

	if (last_expect)
	  last_expect->if_defined = strdup(token);
	else
	{
	  print_fatal_error("IF-DEFINED without a preceding EXPECT on line %d.",
		            linenum);
	  goto test_error;
	}
      }
      else if (!strcasecmp(token, "IF-UNDEFINED"))
      {
	if (!get_token(fp, token, sizeof(token), &linenum))
	{
	  print_fatal_error("Missing IF-UNDEFINED name on line %d.", linenum);
	  goto test_error;
	}

	if (last_expect)
	  last_expect->if_undefined = strdup(token);
	else
	{
	  print_fatal_error("IF-UNDEFINED without a preceding EXPECT on line "
	                    "%d.", linenum);
	  goto test_error;
	}
      }
      else if (!strcasecmp(token, "WITH-VALUE"))
      {
      	if (!get_token(fp, token, sizeof(token), &linenum))
	{
	  print_fatal_error("Missing WITH-VALUE value on line %d.", linenum);
	  goto test_error;
	}

        if (last_expect)
	{
	  tokenptr = token + strlen(token) - 1;
	  if (token[0] == '/' && tokenptr > token && *tokenptr == '/')
	  {
	   /*
	    * WITH-VALUE is a POSIX extended regular expression.
	    */

	    last_expect->with_value = calloc(1, tokenptr - token);
	    last_expect->with_regex = 1;

	    if (last_expect->with_value)
	      memcpy(last_expect->with_value, token + 1, tokenptr - token - 1);
	  }
	  else
	  {
	   /*
	    * WITH-VALUE is a literal value...
	    */

	    last_expect->with_value = strdup(token);
	  }
	}
	else
	{
	  print_fatal_error("WITH-VALUE without a preceding EXPECT on line %d.",
		            linenum);
	  goto test_error;
	}
      }
      else if (!strcasecmp(token, "DISPLAY"))
      {
       /*
        * Display attributes...
	*/

        if (num_displayed >= (int)(sizeof(displayed) / sizeof(displayed[0])))
	{
	  print_fatal_error("Too many DISPLAY's on line %d", linenum);
	  goto test_error;
	}

	if (!get_token(fp, token, sizeof(token), &linenum))
	{
	  print_fatal_error("Missing DISPLAY name on line %d.", linenum);
	  goto test_error;
	}

	displayed[num_displayed] = strdup(token);
	num_displayed ++;
      }
      else
      {
	print_fatal_error("Unexpected token %s seen on line %d.", token,
	                  linenum);
	goto test_error;
      }
    }

   /*
    * Submit the IPP request...
    */

    request->request.op.version[0]   = version / 10;
    request->request.op.version[1]   = version % 10;
    request->request.op.operation_id = op;
    request->request.op.request_id   = request_id;

    if (XML)
    {
      puts("<dict>");
      puts("<key>Name</key>");
      print_xml_string("string", name);
      puts("<key>Operation</key>");
      print_xml_string("string", ippOpString(op));
      puts("<key>RequestAttributes</key>");
      puts("<dict>");
      for (attrptr = request->attrs; attrptr; attrptr = attrptr->next)
	print_attr(attrptr);
      puts("</dict>");
    }
    else
    {
      if (Verbosity)
      {
        printf("    %s:\n", ippOpString(op));

        for (attrptr = request->attrs; attrptr; attrptr = attrptr->next)
          print_attr(attrptr);
      }

      printf("    %-60.60s [", name);
      fflush(stdout);
    }

    if (Chunking)
    {
      http_status_t status = cupsSendRequest(http, request, resource, 0);

      if (status == HTTP_CONTINUE && filename[0])
      {
        int	fd;			/* File to send */
        char	buffer[8192];		/* Copy buffer */
        ssize_t	bytes;			/* Bytes read/written */


        if ((fd = open(filename, O_RDONLY | O_BINARY)) >= 0)
        {
          while ((bytes = read(fd, buffer, sizeof(buffer))) > 0)
            if ((status = cupsWriteRequestData(http, buffer,
                                               bytes)) != HTTP_CONTINUE)
              break;
        }
        else
          status = HTTP_ERROR;
      }

      ippDelete(request);

      if (status == HTTP_CONTINUE)
	response = cupsGetResponse(http, resource);
      else
	response = NULL;
    }
    else if (filename[0])
      response = cupsDoFileRequest(http, request, resource, filename);
    else
      response = cupsDoIORequest(http, request, resource, -1,
                                 Verbosity ? 1 : -1);

    request = NULL;

    if (!response)
      pass = 0;
    else
    {
      if (http->version != HTTP_1_1)
        pass = 0;

      if (response->request.status.version[0] != (version / 10) ||
	  response->request.status.version[1] != (version % 10) ||
	  response->request.status.request_id != request_id)
        pass = 0;

      if ((attrptr = ippFindAttribute(response, "job-id",
                                      IPP_TAG_INTEGER)) != NULL)
      {
        snprintf(temp, sizeof(temp), "%d", attrptr->values[0].integer);
	set_variable(vars, "job-id", temp);
      }

      if ((attrptr = ippFindAttribute(response, "job-uri",
                                      IPP_TAG_URI)) != NULL)
	set_variable(vars, "job-uri", attrptr->values[0].string.text);

      if ((attrptr = ippFindAttribute(response, "notify-subscription-id",
                                      IPP_TAG_INTEGER)) != NULL)
      {
        snprintf(temp, sizeof(temp), "%d", attrptr->values[0].integer);
	set_variable(vars, "notify-subscription-id", temp);
      }

      attrptr = response->attrs;
      if (!attrptr || !attrptr->name ||
	  attrptr->value_tag != IPP_TAG_CHARSET ||
	  attrptr->group_tag != IPP_TAG_OPERATION ||
	  attrptr->num_values != 1 ||
          strcmp(attrptr->name, "attributes-charset"))
        pass = 0;

      if (attrptr)
      {
        attrptr = attrptr->next;
	if (!attrptr || !attrptr->name ||
	    attrptr->value_tag != IPP_TAG_LANGUAGE ||
	    attrptr->group_tag != IPP_TAG_OPERATION ||
	    attrptr->num_values != 1 ||
	    strcmp(attrptr->name, "attributes-natural-language"))
	  pass = 0;
      }

      if ((attrptr = ippFindAttribute(response, "status-message",
                                      IPP_TAG_ZERO)) != NULL &&
          (attrptr->value_tag != IPP_TAG_TEXT ||
	   attrptr->group_tag != IPP_TAG_OPERATION ||
	   attrptr->num_values != 1 ||
	   (attrptr->value_tag == IPP_TAG_TEXT &&
	    strlen(attrptr->values[0].string.text) > 255)))
	pass = 0;

      if ((attrptr = ippFindAttribute(response, "detailed-status-message",
                                      IPP_TAG_ZERO)) != NULL &&
          (attrptr->value_tag != IPP_TAG_TEXT ||
	   attrptr->group_tag != IPP_TAG_OPERATION ||
	   attrptr->num_values != 1 ||
	   (attrptr->value_tag == IPP_TAG_TEXT &&
	    strlen(attrptr->values[0].string.text) > 1023)))
	pass = 0;

      for (attrptr = response->attrs, group = attrptr->group_tag;
           attrptr;
	   attrptr = attrptr->next)
      {
        if (attrptr->group_tag < group && attrptr->group_tag != IPP_TAG_ZERO)
	{
	  pass = 0;
	  break;
	}

        if (!validate_attr(attrptr, 0))
	{
	  pass = 0;
	  break;
	}
      }

      for (i = 0; i < num_statuses; i ++)
        if (response->request.status.status_code == statuses[i])
	  break;

      if (i == num_statuses && num_statuses > 0)
	pass = 0;
      else
      {
        for (i = num_expects, expect = expects; i > 0; i --, expect ++)
        {
          if (expect->if_defined && !get_variable(vars, expect->if_defined))
            continue;

          if (expect->if_undefined && get_variable(vars, expect->if_undefined))
            continue;

          found = ippFindAttribute(response, expect->name, IPP_TAG_ZERO);

          if ((found && expect->not_expect) ||
	      (!found && !(expect->not_expect || expect->optional)) ||
	      (found && !expect_matches(expect, found->value_tag)) ||
	      (found && expect->in_group &&
	       found->group_tag != expect->in_group))
          {
      	    pass = 0;
      	    break;          
          }

          if (found &&
	      !with_value(expect->with_value, expect->with_regex, found))
          {
            pass = 0;
            break;
          }

          if (found && expect->count > 0 && found->num_values != expect->count)
	  {
            pass = 0;
            break;
	  }

          if (found && expect->same_count_as)
          {
            attrptr = ippFindAttribute(response, expect->same_count_as,
                                       IPP_TAG_ZERO);

            if (!attrptr || attrptr->num_values != found->num_values)
            {
              pass = 0;
              break;
            }
          }
        }
      }
    }

    if (XML)
    {
      puts("<key>Successful</key>");
      puts(pass ? "<true />" : "<false />");
      puts("<key>StatusCode</key>");
      print_xml_string("string", ippErrorString(cupsLastError()));
      puts("<key>ResponseAttributes</key>");
      puts("<dict>");
      for (attrptr = response ? response->attrs : NULL;
	   attrptr;
	   attrptr = attrptr->next)
	print_attr(attrptr);
      puts("</dict>");
    }
    else
    {
      puts(pass ? "PASS]" : "FAIL]");

      if (Verbosity && response)
      {
	printf("        RECEIVED: %lu bytes in response\n",
	       (unsigned long)ippLength(response));
	printf("        status-code = %x (%s)\n", cupsLastError(),
	       ippErrorString(cupsLastError()));

	for (attrptr = response->attrs;
	     attrptr != NULL;
	     attrptr = attrptr->next)
	{
	  print_attr(attrptr);
	}
      }
    }

    if (pass && !XML && !Verbosity && num_displayed > 0)
    {
      for (attrptr = response->attrs;
	   attrptr != NULL;
	   attrptr = attrptr->next)
	if (attrptr->name)
	  for (i = 0; i < num_displayed; i ++)
	    if (!strcmp(displayed[i], attrptr->name))
	    {
	      print_attr(attrptr);
	      break;
	    }
    }
    else if (!pass)
    {
      if (XML)
      {
	puts("<key>Errors</key>");
	puts("<array>");
      }

      if (http->version != HTTP_1_1)
	print_test_error("Bad HTTP version (%d.%d)", http->version / 100,
			 http->version % 100);

      if (!response)
	print_test_error("IPP request failed with status %04x (%s)",
			 cupsLastError(), cupsLastErrorString());
      else
      {
	if (response->request.status.version[0] != (version / 10) ||
	    response->request.status.version[1] != (version % 10))
          print_test_error("Bad version %d.%d in response - expected %d.%d "
	                   "(RFC 2911 section 3.1.8).",
	                   response->request.status.version[0],
			   response->request.status.version[1],
			   version / 10, version % 10);

	if (response->request.status.request_id != request_id)
	  print_test_error("Bad request ID %d in response - expected %d "
			   "(RFC 2911 section 3.1.1)",
			   response->request.status.request_id, request_id);

	attrptr = response->attrs;
	if (!attrptr)
	  print_test_error("Missing first attribute \"attributes-charset "
	                   "(charset)\" in group operation-attributes-tag "
			   "(RFC 2911 section 3.1.4).");
	else
	{
	  if (!attrptr->name ||
	      attrptr->value_tag != IPP_TAG_CHARSET ||
	      attrptr->group_tag != IPP_TAG_OPERATION ||
	      attrptr->num_values != 1 ||
	      strcmp(attrptr->name, "attributes-charset"))
	    print_test_error("Bad first attribute \"%s (%s%s)\" in group %s, "
			     "expected \"attributes-charset (charset)\" in "
			     "group operation-attributes-tag (RFC 2911 section "
			     "3.1.4).",
			     attrptr->name ? attrptr->name : "(null)",
			     attrptr->num_values > 1 ? "1setOf " : "",
			     ippTagString(attrptr->value_tag),
			     ippTagString(attrptr->group_tag));

	  attrptr = attrptr->next;
	  if (!attrptr)
	    print_test_error("Missing second attribute \"attributes-natural-"
			     "language (naturalLanguage)\" in group "
			     "operation-attributes-tag (RFC 2911 section "
			     "3.1.4).");
	  else if (!attrptr->name ||
	           attrptr->value_tag != IPP_TAG_LANGUAGE ||
	           attrptr->group_tag != IPP_TAG_OPERATION ||
	           attrptr->num_values != 1 ||
	           strcmp(attrptr->name, "attributes-natural-language"))
	    print_test_error("Bad first attribute \"%s (%s%s)\" in group %s, "
			     "expected \"attributes-natural-language "
			     "(naturalLanguage)\" in group "
			     "operation-attributes-tag (RFC 2911 section "
			     "3.1.4).",
			     attrptr->name ? attrptr->name : "(null)",
			     attrptr->num_values > 1 ? "1setOf " : "",
			     ippTagString(attrptr->value_tag),
			     ippTagString(attrptr->group_tag));
        }

	if ((attrptr = ippFindAttribute(response, "status-message",
					 IPP_TAG_ZERO)) != NULL)
	{
	  if (attrptr->value_tag != IPP_TAG_TEXT)
	    print_test_error("status-message (text(255)) has wrong value tag "
	                     "%s (RFC 2911 section 3.1.6.2).",
			     ippTagString(attrptr->value_tag));
	  if (attrptr->group_tag != IPP_TAG_OPERATION)
	    print_test_error("status-message (text(255)) has wrong group tag "
	                     "%s (RFC 2911 section 3.1.6.2).",
			     ippTagString(attrptr->group_tag));
	  if (attrptr->num_values != 1)
	    print_test_error("status-message (text(255)) has %d values "
	                     "(RFC 2911 section 3.1.6.2).",
			     attrptr->num_values);
	  if (attrptr->value_tag == IPP_TAG_TEXT &&
	      strlen(attrptr->values[0].string.text) > 255)
	    print_test_error("status-message (text(255)) has bad length %d"
	                     " (RFC 2911 section 3.1.6.2).",
	                     (int)strlen(attrptr->values[0].string.text));
        }

	if ((attrptr = ippFindAttribute(response, "detailed-status-message",
					 IPP_TAG_ZERO)) != NULL)
	{
	  if (attrptr->value_tag != IPP_TAG_TEXT)
	    print_test_error("detailed-status-message (text(MAX)) has wrong "
	                     "value tag %s (RFC 2911 section 3.1.6.3).",
			     ippTagString(attrptr->value_tag));
	  if (attrptr->group_tag != IPP_TAG_OPERATION)
	    print_test_error("detailed-status-message (text(MAX)) has wrong "
	                     "group tag %s (RFC 2911 section 3.1.6.3).",
			     ippTagString(attrptr->group_tag));
	  if (attrptr->num_values != 1)
	    print_test_error("detailed-status-message (text(MAX)) has %d values"
			     " (RFC 2911 section 3.1.6.3).",
			     attrptr->num_values);
	  if (attrptr->value_tag == IPP_TAG_TEXT &&
	      strlen(attrptr->values[0].string.text) > 1023)
	    print_test_error("detailed-status-message (text(MAX)) has bad "
			     "length %d (RFC 2911 section 3.1.6.3).",
	                     (int)strlen(attrptr->values[0].string.text));
        }

	for (attrptr = response->attrs, group = attrptr->group_tag;
	     attrptr;
	     attrptr = attrptr->next)
	{
	  if (attrptr->group_tag < group && attrptr->group_tag != IPP_TAG_ZERO)
	    print_test_error("Attribute groups out of order (%s < %s)",
	                     ippTagString(attrptr->group_tag),
			     ippTagString(group));

	  validate_attr(attrptr, 1);
	}

	for (i = 0; i < num_statuses; i ++)
	  if (response->request.status.status_code == statuses[i])
	    break;

	if (i == num_statuses && num_statuses > 0)
	  print_test_error("Bad status-code");

	for (i = num_expects, expect = expects; i > 0; i --, expect ++)
	{
	  if (expect->if_defined && !get_variable(vars, expect->if_defined))
	    continue;

	  if (expect->if_undefined && get_variable(vars, expect->if_undefined))
	    continue;
      
	  found = ippFindAttribute(response, expect->name, IPP_TAG_ZERO);

	  if (found && expect->not_expect)
            print_test_error("NOT EXPECTED: %s", expect->name);
	  else if (!found && !(expect->not_expect || expect->optional))
	    print_test_error("EXPECTED: %s", expect->name);
	  else if (found)
	  {
	    if (!expect_matches(expect, found->value_tag))
	      print_test_error("EXPECTED: %s OF-TYPE %s (got %s)", 
			       expect->name, expect->of_type,
			       ippTagString(found->value_tag));

	    if (expect->in_group && found->group_tag != expect->in_group)
	      print_test_error("EXPECTED: %s IN-GROUP %s (got %s).",
	                       expect->name, ippTagString(expect->in_group),
			       ippTagString(found->group_tag));

	    if (!with_value(expect->with_value, expect->with_regex, found))
	    {
	      if (expect->with_regex)
		print_test_error("EXPECTED: %s WITH-VALUE /%s/",
				 expect->name, expect->with_value);         
	      else
		print_test_error("EXPECTED: %s WITH-VALUE \"%s\"",
				 expect->name, expect->with_value);         
	    }

	    if (expect->count > 0 && found->num_values != expect->count)
	    {
	      print_test_error("EXPECTED: %s COUNT %d (got %d)", expect->name,
			       expect->count, found->num_values);
	    }

	    if (expect->same_count_as)
	    {
	      attrptr = ippFindAttribute(response, expect->same_count_as,
					 IPP_TAG_ZERO);

	      if (!attrptr)
		print_test_error("EXPECTED: %s (%d values) SAME-COUNT-AS %s "
				 "(not returned)", expect->name,
				 found->num_values, expect->same_count_as);
	      else if (attrptr->num_values != found->num_values)
		print_test_error("EXPECTED: %s (%d values) SAME-COUNT-AS %s "
				 "(%d values)", expect->name, found->num_values,
				 expect->same_count_as, attrptr->num_values);
	    }
	  }
	}
      }

      if (XML)
	puts("</array>");
    }

    if (XML)
      puts("</dict>");

    ippDelete(response);
    response = NULL;

    for (i = num_expects, expect = expects; i > 0; i --, expect ++)
    {
      free(expect->name);
      if (expect->of_type)
        free(expect->of_type);
      if (expect->same_count_as)
        free(expect->same_count_as);
      if (expect->if_defined)
        free(expect->if_defined);
      if (expect->if_undefined)
        free(expect->if_undefined);  
      if (expect->with_value)
        free(expect->with_value);
    }
    num_expects = 0;

    for (i = 0; i < num_displayed; i ++)
      free(displayed[i]);
    num_displayed = 0;

    if (!pass)
      break;
  }

  fclose(fp);
  httpClose(http);

  return (pass);

 /*
  * If we get here there was a fatal test error...
  */

  test_error:

  if (fp)
    fclose(fp);

  httpClose(http);
  ippDelete(request);
  ippDelete(response);

  for (i = num_expects, expect = expects; i > 0; i --, expect ++)
  {
    free(expect->name);
    if (expect->of_type)
      free(expect->of_type);
    if (expect->same_count_as)
      free(expect->same_count_as);
    if (expect->if_defined)
      free(expect->if_defined);
    if (expect->if_undefined)
      free(expect->if_undefined);  
    if (expect->with_value)
      free(expect->with_value);
  }

  for (i = 0; i < num_displayed; i ++)
    free(displayed[i]);

  return (0);
}


/*
 * 'expand_variables()' - Expand variables in a string.
 */

static void
expand_variables(_cups_vars_t *vars,	/* I - Variables */
                 char         *dst,	/* I - Destination string buffer */
		 const char   *src,	/* I - Source string */
		 size_t       dstsize)	/* I - Size of destination buffer */
{
  char		*dstptr,		/* Pointer into destination */
		*dstend,		/* End of destination */
		temp[256],		/* Temporary string */
		*tempptr;		/* Pointer into temporary string */
  const char	*value;			/* Value to substitute */


  dstptr = dst;
  dstend = dst + dstsize - 1;

  while (*src && dstptr < dstend)
  {
    if (*src == '$')
    {
     /*
      * Substitute a string/number...
      */

      if (!strncmp(src, "$$", 2))
      {
        value = "$";
	src   += 2;
      }
      else if (!strncmp(src, "$ENV[", 5))
      {
	strlcpy(temp, src + 5, sizeof(temp));

	for (tempptr = temp; *tempptr; tempptr ++)
	  if (*tempptr == ']')
	    break;

        if (*tempptr)
	  *tempptr++ = '\0';

	value = getenv(temp);
        src   += tempptr - temp + 5;
      }
      else
      {
	strlcpy(temp, src + 1, sizeof(temp));

	for (tempptr = temp; *tempptr; tempptr ++)
	  if (!isalnum(*tempptr & 255) && *tempptr != '-' && *tempptr != '_')
	    break;

        if (*tempptr)
	  *tempptr = '\0';

	if (!strcmp(temp, "uri"))
	  value = vars->uri;
	else if (!strcmp(temp, "filename"))
	  value = vars->filename;
	else if (!strcmp(temp, "scheme") || !strcmp(temp, "method"))
	  value = vars->scheme;
	else if (!strcmp(temp, "username"))
	  value = vars->userpass;
	else if (!strcmp(temp, "hostname"))
	  value = vars->hostname;
	else if (!strcmp(temp, "port"))
	{
	  snprintf(temp, sizeof(temp), "%d", vars->port);
	  value = temp;
	}
	else if (!strcmp(temp, "resource"))
	  value = vars->resource;
	else if (!strcmp(temp, "user"))
	  value = cupsUser();
	else
	  value = get_variable(vars, temp);

        src += tempptr - temp + 1;
      }

      if (value)
      {
        strlcpy(dstptr, value, dstend - dstptr + 1);
	dstptr += strlen(dstptr);
      }
    }
    else
      *dstptr++ = *src++;
  }

  *dstptr = '\0';
}


/*
 * 'expect_matches()' - Return true if the tag matches the specification.
 */
 
static int				/* O - 1 if matches, 0 otherwise */
expect_matches(
    _cups_expect_t *expect,		/* I - Expected attribute */
    ipp_tag_t      value_tag)		/* I - Value tag for attribute */
{
  int	match;				/* Match? */
  char	*of_type,			/* Type name to match */
	*next;				/* Next name to match */


 /*
  * If we don't expect a particular type, return immediately...
  */

  if (!expect->of_type)
    return (1);

 /*
  * Parse the "of_type" value since the string can contain multiple attribute
  * types separated by "|"...
  */

  for (of_type = expect->of_type, match = 0; !match && of_type; of_type = next)
  {
   /*
    * Find the next separator, and set it (temporarily) to nul if present.
    */

    if ((next = strchr(of_type, '|')) != NULL)
      *next = '\0';
  
   /*
    * Support some meta-types to make it easier to write the test file.
    */

    if (!strcmp(of_type, "text"))
      match = value_tag == IPP_TAG_TEXTLANG || value_tag == IPP_TAG_TEXT;            
    else if (!strcmp(of_type, "name"))
      match = value_tag == IPP_TAG_NAMELANG || value_tag == IPP_TAG_NAME;    
    else if (!strcmp(of_type, "collection"))
      match = value_tag == IPP_TAG_BEGIN_COLLECTION;   
    else
      match = value_tag == ippTagValue(of_type);

   /*
    * Restore the separator if we have one...
    */

    if (next)
      *next++ = '|';
  }

  return (match);
}


/*
 * 'get_filename()' - Get a filename based on the current test file.
 */

static char *				/* O - Filename */
get_filename(const char *testfile,	/* I - Current test file */
             char       *dst,		/* I - Destination filename */
	     const char *src,		/* I - Source filename */
             size_t     dstsize)	/* I - Size of destination buffer */
{
  char			*dstptr;	/* Pointer into destination */
  _cups_globals_t	*cg = _cupsGlobals();
					/* Global data */


  if (*src == '<' && src[strlen(src) - 1] == '>')
  {
   /*
    * Map <filename> to CUPS_DATADIR/ipptest/filename...
    */

    snprintf(dst, dstsize, "%s/ipptest/%s", cg->cups_datadir, src + 1);
    dstptr = dst + strlen(dst) - 1;
    if (*dstptr == '>')
      *dstptr = '\0';
  }
  else if (*src == '/' || !strchr(testfile, '/'))
  {
   /*
    * Use the path as-is...
    */

    strlcpy(dst, src, dstsize);
  }
  else
  {
   /*
    * Make path relative to testfile...
    */

    strlcpy(dst, testfile, dstsize);
    if ((dstptr = strrchr(dst, '/')) != NULL)
      dstptr ++;
    else
      dstptr = dst; /* Should never happen */

    strlcpy(dstptr, src, dstsize - (dstptr - dst));
  }

  return (dst);
}


/*
 * 'get_token()' - Get a token from a file.
 */

static char *				/* O  - Token from file or NULL on EOF */
get_token(FILE *fp,			/* I  - File to read from */
          char *buf,			/* I  - Buffer to read into */
	  int  buflen,			/* I  - Length of buffer */
	  int  *linenum)		/* IO - Current line number */
{
  int	ch,				/* Character from file */
	quote;				/* Quoting character */
  char	*bufptr,			/* Pointer into buffer */
	*bufend;			/* End of buffer */


  for (;;)
  {
   /*
    * Skip whitespace...
    */

    while (isspace(ch = getc(fp)))
    {
      if (ch == '\n')
        (*linenum) ++;
    }

   /*
    * Read a token...
    */

    if (ch == EOF)
      return (NULL);
    else if (ch == '\'' || ch == '\"')
    {
     /*
      * Quoted text or regular expression...
      */

      quote  = ch;
      bufptr = buf;
      bufend = buf + buflen - 1;

      while ((ch = getc(fp)) != EOF)
      {
        if (ch == '\\')
	{
	 /*
	  * Escape next character...
	  */

	  if (bufptr < bufend)
	    *bufptr++ = ch;

	  if ((ch = getc(fp)) != EOF && bufptr < bufend)
	    *bufptr++ = ch;
	}
	else if (ch == quote)
          break;
	else if (bufptr < bufend)
          *bufptr++ = ch;
      }

      *bufptr = '\0';

      return (buf);
    }
    else if (ch == '#')
    {
     /*
      * Comment...
      */

      while ((ch = getc(fp)) != EOF)
	if (ch == '\n')
          break;

      (*linenum) ++;
    }
    else
    {
     /*
      * Whitespace delimited text...
      */

      ungetc(ch, fp);

      bufptr = buf;
      bufend = buf + buflen - 1;

      while ((ch = getc(fp)) != EOF)
	if (isspace(ch) || ch == '#')
          break;
	else if (bufptr < bufend)
          *bufptr++ = ch;

      if (ch == '#')
        ungetc(ch, fp);
      else if (ch == '\n')
        (*linenum) ++;
        
      *bufptr = '\0';

      return (buf);
    }
  }
}


/*
 * 'get_variable()' - Get the value of a variable.
 */

static char *				/* O - Value or NULL */
get_variable(_cups_vars_t *vars,	/* I - Variables */
             const char   *name)	/* I - Variable name */
{
  _cups_var_t	key,			/* Search key */
		*match;			/* Matching variable, if any */


  key.name = (char *)name;
  match    = cupsArrayFind(vars->vars, &key);

  return (match ? match->value : NULL);
}


/*
 * 'iso_date()' - Return an ISO 8601 date/time string for the given IPP dateTime
 *                value.
 */

static char *				/* O - ISO 8601 date/time string */
iso_date(ipp_uchar_t *date)		/* I - IPP (RFC 1903) date/time value */
{
  unsigned	year = (date[0] << 8) + date[1];
					/* Year */
  static char	buffer[255];		/* String buffer */


  if (date[9] == 0 && date[10] == 0)
    snprintf(buffer, sizeof(buffer), "%04u-%02u-%02uT%02u:%02u:%02uZ",
	     year, date[2], date[3], date[4], date[5], date[6]);
  else
    snprintf(buffer, sizeof(buffer), "%04u-%02u-%02uT%02u:%02u:%02u%c%02u%02u",
	     year, date[2], date[3], date[4], date[5], date[6],
	     date[8], date[9], date[10]);

  return (buffer);
}


/*
 * 'print_attr()' - Print an attribute on the screen.
 */

static void
print_attr(ipp_attribute_t *attr)	/* I - Attribute to print */
{
  int			i;		/* Looping var */
  ipp_attribute_t	*colattr;	/* Collection attribute */


  if (XML)
  {
    if (!attr->name)
    {
      printf("<key>%s</key>\n<true />\n", ippTagString(attr->group_tag));
      return;
    }

    print_xml_string("key", attr->name);
    if (attr->num_values > 1)
      puts("<array>");
  }
  else
  {
    if (!attr->name)
    {
      puts("        -- separator --");
      return;
    }

    printf("        %s (%s%s) = ", attr->name,
	   attr->num_values > 1 ? "1setOf " : "",
	   ippTagString(attr->value_tag));
  }

  switch (attr->value_tag)
  {
    case IPP_TAG_INTEGER :
    case IPP_TAG_ENUM :
	for (i = 0; i < attr->num_values; i ++)
	  if (XML)
	    printf("<integer>%d</integer>\n", attr->values[i].integer);
	  else
	    printf("%d ", attr->values[i].integer);
	break;

    case IPP_TAG_BOOLEAN :
	for (i = 0; i < attr->num_values; i ++)
	  if (XML)
	    puts(attr->values[i].boolean ? "<true />" : "<false />");
	  else if (attr->values[i].boolean)
	    fputs("true ", stdout);
	  else
	    fputs("false ", stdout);
	break;

    case IPP_TAG_RANGE :
	for (i = 0; i < attr->num_values; i ++)
	  if (XML)
	    printf("<dict><key>lower</key><integer>%d</integer>"
	           "<key>upper</key><integer>%d</integer></dict>\n",
		   attr->values[i].range.lower, attr->values[i].range.upper);
	  else
	    printf("%d-%d ", attr->values[i].range.lower,
		   attr->values[i].range.upper);
	break;

    case IPP_TAG_RESOLUTION :
	for (i = 0; i < attr->num_values; i ++)
	  if (XML)
	    printf("<dict><key>xres</key><integer>%d</integer>"
	           "<key>yres</key><integer>%d</integer>"
		   "<key>units</key><string>%s</string></dict>\n",
	           attr->values[i].resolution.xres,
		   attr->values[i].resolution.yres,
		   attr->values[i].resolution.units == IPP_RES_PER_INCH ?
		       "dpi" : "dpc");
	  else
	    printf("%dx%d%s ", attr->values[i].resolution.xres,
		   attr->values[i].resolution.yres,
		   attr->values[i].resolution.units == IPP_RES_PER_INCH ?
		       "dpi" : "dpc");
	break;

    case IPP_TAG_DATE :
	for (i = 0; i < attr->num_values; i ++)
	  if (XML)
            printf("<date>%s</date>\n", iso_date(attr->values[i].date));
          else
	    printf("%s ", iso_date(attr->values[i].date));
        break;

    case IPP_TAG_STRING :
    case IPP_TAG_TEXT :
    case IPP_TAG_NAME :
    case IPP_TAG_KEYWORD :
    case IPP_TAG_CHARSET :
    case IPP_TAG_URI :
    case IPP_TAG_MIMETYPE :
    case IPP_TAG_LANGUAGE :
	for (i = 0; i < attr->num_values; i ++)
	  if (XML)
	    print_xml_string("string", attr->values[i].string.text);
	  else
	    printf("\"%s\" ", attr->values[i].string.text);
	break;

    case IPP_TAG_TEXTLANG :
    case IPP_TAG_NAMELANG :
	for (i = 0; i < attr->num_values; i ++)
	  if (XML)
	  {
	    fputs("<dict><key>language</key><string>", stdout);
	    print_xml_string(NULL, attr->values[i].string.charset);
	    fputs("</string><key>string</key><string>", stdout);
	    print_xml_string(NULL, attr->values[i].string.text);
	    puts("</string></dict>");
	  }
	  else
	    printf("\"%s\",%s ", attr->values[i].string.text,
		   attr->values[i].string.charset);
	break;

    case IPP_TAG_BEGIN_COLLECTION :
	for (i = 0; i < attr->num_values; i ++)
	{
	  if (XML)
	  {
	    puts("<dict>");
	    for (colattr = attr->values[i].collection->attrs;
	         colattr;
		 colattr = colattr->next)
	      print_attr(colattr);
	    puts("</dict>");
	  }
	  else
	  {
	    if (i)
	      putchar(' ');

	    print_col(attr->values[i].collection);
          }
	}
	break;

    default :
	if (XML)
	  printf("<string>&lt;&lt;%s&gt;&gt;</string>\n",
	         ippTagString(attr->value_tag));
	else
	  fputs(ippTagString(attr->value_tag), stdout);
	break;
  }

  if (XML)
  {
    if (attr->num_values > 1)
      puts("</array>");
  }
  else
    putchar('\n');
}


/*
 * 'print_col()' - Print a collection attribute on the screen.
 */

static void
print_col(ipp_t *col)			/* I - Collection attribute to print */
{
  int			i;		/* Looping var */
  ipp_attribute_t	*attr;		/* Current attribute in collection */


  putchar('{');
  for (attr = col->attrs; attr; attr = attr->next)
  {
    printf("%s(%s%s)=", attr->name, attr->num_values > 1 ? "1setOf " : "",
	   ippTagString(attr->value_tag));

    switch (attr->value_tag)
    {
      case IPP_TAG_INTEGER :
      case IPP_TAG_ENUM :
	  for (i = 0; i < attr->num_values; i ++)
	    printf("%d ", attr->values[i].integer);
	  break;

      case IPP_TAG_BOOLEAN :
	  for (i = 0; i < attr->num_values; i ++)
	    if (attr->values[i].boolean)
	      printf("true ");
	    else
	      printf("false ");
	  break;

      case IPP_TAG_NOVALUE :
	  printf("novalue");
	  break;

      case IPP_TAG_RANGE :
	  for (i = 0; i < attr->num_values; i ++)
	    printf("%d-%d ", attr->values[i].range.lower,
		   attr->values[i].range.upper);
	  break;

      case IPP_TAG_RESOLUTION :
	  for (i = 0; i < attr->num_values; i ++)
	    printf("%dx%d%s ", attr->values[i].resolution.xres,
		   attr->values[i].resolution.yres,
		   attr->values[i].resolution.units == IPP_RES_PER_INCH ?
		       "dpi" : "dpc");
	  break;

      case IPP_TAG_STRING :
      case IPP_TAG_TEXT :
      case IPP_TAG_NAME :
      case IPP_TAG_KEYWORD :
      case IPP_TAG_CHARSET :
      case IPP_TAG_URI :
      case IPP_TAG_MIMETYPE :
      case IPP_TAG_LANGUAGE :
	  for (i = 0; i < attr->num_values; i ++)
	    printf("\"%s\" ", attr->values[i].string.text);
	  break;

      case IPP_TAG_TEXTLANG :
      case IPP_TAG_NAMELANG :
	  for (i = 0; i < attr->num_values; i ++)
	    printf("\"%s\",%s ", attr->values[i].string.text,
		   attr->values[i].string.charset);
	  break;

      case IPP_TAG_BEGIN_COLLECTION :
	  for (i = 0; i < attr->num_values; i ++)
	  {
	    print_col(attr->values[i].collection);
	    putchar(' ');
	  }
	  break;

      default :
	  break; /* anti-compiler-warning-code */
    }
  }

  putchar('}');
}


/*
 * 'print_fatal_error()' - Print a fatal error message.
 */

static void
print_fatal_error(const char *s,	/* I - Printf-style format string */
                  ...)			/* I - Additional arguments as needed */
{
  char		buffer[10240];		/* Format buffer */
  va_list	ap;			/* Pointer to arguments */


 /*
  * Format the error message...
  */

  va_start(ap, s);
  vsnprintf(buffer, sizeof(buffer), s, ap);
  va_end(ap);

 /*
  * Then output it...
  */

  if (XML)
  {
    print_xml_header();
    print_xml_trailer(0, buffer);
  }
  else
    _cupsLangPrintf(stderr, "ipptest: %s\n", buffer);
}


/*
 * 'print_test_error()' - Print a test error message.
 */

static void
print_test_error(const char *s,		/* I - Printf-style format string */
                 ...)			/* I - Additional arguments as needed */
{
  char		buffer[10240];		/* Format buffer */
  va_list	ap;			/* Pointer to arguments */


 /*
  * Format the error message...
  */

  va_start(ap, s);
  vsnprintf(buffer, sizeof(buffer), s, ap);
  va_end(ap);

 /*
  * Then output it...
  */

  if (XML)
    print_xml_string("string", buffer);
  else
    printf("        %s\n", buffer);
}


/*
 * 'print_xml_header()' - Print a standard XML plist header.
 */

static void
print_xml_header(void)
{
  if (!XMLHeader)
  {
    puts("<?xml version=\"1.0\" encoding=\"UTF-8\"?>");
    puts("<!DOCTYPE plist PUBLIC \"-//Apple Computer//DTD PLIST 1.0//EN\" "
         "\"http://www.apple.com/DTDs/PropertyList-1.0.dtd\">");
    puts("<plist version=\"1.0\">");
    puts("<dict>");
    puts("<key>Chunking</key>");
    puts(Chunking ? "<true />" : "<false />");
    puts("<key>Tests</key>");
    puts("<array>");

    XMLHeader = 1;
  }
}


/*
 * 'print_xml_string()' - Print an XML string with escaping.
 */

static void
print_xml_string(const char *element,	/* I - Element name or NULL */
		 const char *s)		/* I - String to print */
{
  if (element)
    printf("<%s>", element);

  while (*s)
  {
    if (*s == '&')
      fputs("&amp;", stdout);
    else if (*s == '<')
      fputs("&lt;", stdout);
    else if (*s == '>')
      fputs("&gt;", stdout);
    else
      putchar(*s);

    s ++;
  }

  if (element)
    printf("</%s>\n", element);
}


/*
 * 'print_xml_trailer()' - Print the XML trailer with success/fail value.
 */

static void
print_xml_trailer(int        success,	/* I - 1 on success, 0 on failure */
                  const char *message)	/* I - Error message or NULL */
{
  if (XMLHeader)
  {
    puts("</array>");
    puts("<key>Successful</key>");
    puts(success ? "<true />" : "<false />");
    if (message)
    {
      puts("<key>ErrorMessage</key>");
      print_xml_string("string", message);
    }
    puts("</dict>");
    puts("</plist>");

    XMLHeader = 0;
  }
}


/*
 * 'set_variable()' - Set a variable value.
 */

static void
set_variable(_cups_vars_t *vars,	/* I - Variables */
             const char   *name,	/* I - Variable name */
             const char   *value)	/* I - Value string */
{
  _cups_var_t	key,			/* Search key */
		*var;			/* New variable */


  key.name = (char *)name;
  if ((var = cupsArrayFind(vars->vars, &key)) != NULL)
  {
    free(var->value);
    var->value = strdup(value);
  }
  else if ((var = malloc(sizeof(_cups_var_t))) == NULL)
  {
    print_fatal_error("Unable to allocate memory for variable \"%s\".", name);
    exit(1);
  }
  else
  {
    var->name  = strdup(name);
    var->value = strdup(value);

    cupsArrayAdd(vars->vars, var);
  }
}


/*
 * 'usage()' - Show program usage.
 */

static void
usage(void)
{
  _cupsLangPuts(stderr,
                _("Usage: ipptest [options] URI filename.test [ ... "
		  "filenameN.test ]\n"
		  "\n"
		  "Options:\n"
		  "\n"
		  "-E             Test with encryption.\n"
		  "-V version     Set default IPP version.\n"
		  "-X             Produce XML instead of plain text.\n"
		  "-c             Send requests using chunking (default)\n"
		  "-d name=value  Define variable.\n"
		  "-f filename    Set default test file.\n"
		  "-i seconds     Repeat the last test file with the given "
		  "interval.\n"
		  "-l             Send requests using content-length\n"
		  "-v             Show all attributes sent and received.\n"));

  exit(1);
}


/*
 * 'validate_attr()' - Determine whether an attribute is valid.
 */

static int				/* O - 1 if valid, 0 otherwise */
validate_attr(ipp_attribute_t *attr,	/* I - Attribute to validate */
              int             print)	/* I - 1 = report issues to stdout */
{
  int		i;			/* Looping var */
  char		scheme[64],		/* Scheme from URI */
		userpass[256],		/* Username/password from URI */
		hostname[256],		/* Hostname from URI */
		resource[1024];		/* Resource from URI */
  int		port,			/* Port number from URI */
		uri_status,		/* URI separation status */
		valid = 1;		/* Is the attribute valid? */
  const char	*ptr;			/* Pointer into string */
  ipp_attribute_t *colattr;		/* Collection attribute */
  regex_t	re;			/* Regular expression */
  ipp_uchar_t	*date;			/* Current date value */


 /*
  * Skip separators.
  */

  if (!attr->name)
    return (1);

 /*
  * Validate the attribute name.
  */

  for (ptr = attr->name; *ptr; ptr ++)
    if (!isalnum(*ptr & 255) && *ptr != '-' && *ptr != '.' && *ptr != '_')
      break;

  if (*ptr || ptr == attr->name)
  {
    valid = 0;

    if (print)
      print_test_error("\"%s\": Bad attribute name - invalid character (RFC "
		       "2911 section 4.1.3).", attr->name);
  }

  if ((ptr - attr->name) > 255)
  {
    valid = 0;

    if (print)
      print_test_error("\"%s\": Bad attribute name - bad length (RFC 2911 "
		       "section 4.1.3).", attr->name);
  }

  switch (attr->value_tag)
  {
    case IPP_TAG_INTEGER :
        break;

    case IPP_TAG_BOOLEAN :
        for (i = 0; i < attr->num_values; i ++)
	{
	  if (attr->values[i].boolean != 0 &&
	      attr->values[i].boolean != 1)
	  {
	    valid = 0;

	    if (print)
	      print_test_error("\"%s\": Bad boolen value %d (RFC 2911 section "
	                       "4.1.10).", attr->name, attr->values[i].boolean);
            else
	      break;
	  }
	}
        break;

    case IPP_TAG_ENUM :
        for (i = 0; i < attr->num_values; i ++)
	{
	  if (attr->values[i].integer < 1)
	  {
	    valid = 0;

	    if (print)
	      print_test_error("\"%s\": Bad enum value %d - out of range "
	                       "(RFC 2911 section 4.1.4).", attr->name,
			       attr->values[i].integer);
            else
	      break;
	  }
	}
        break;

    case IPP_TAG_STRING :
        for (i = 0; i < attr->num_values; i ++)
	{
	  if (attr->values[i].unknown.length > 1023)
	  {
	    valid = 0;

	    if (print)
	      print_test_error("\"%s\": Bad octetString value - bad length %d "
	                       "(RFC 2911 section 4.1.10).", attr->name,
			       attr->values[i].unknown.length);
            else
	      break;
	  }
	}
        break;

    case IPP_TAG_DATE :
        for (i = 0; i < attr->num_values; i ++)
	{
	  date = attr->values[i].date;

          if (date[2] < 1 || date[2] > 12)
	  {
	    valid = 0;

	    if (print)
	      print_test_error("\"%s\": Bad dateTime month %u (RFC 2911 "
	                       "section 4.1.13).", attr->name, date[2]);
            else
	      break;
	  }

          if (date[3] < 1 || date[3] > 31)
	  {
	    valid = 0;

	    if (print)
	      print_test_error("\"%s\": Bad dateTime day %u (RFC 2911 "
	                       "section 4.1.13).", attr->name, date[3]);
            else
	      break;
	  }

          if (date[4] > 23)
	  {
	    valid = 0;

	    if (print)
	      print_test_error("\"%s\": Bad dateTime hours %u (RFC 2911 "
	                       "section 4.1.13).", attr->name, date[4]);
            else
	      break;
	  }

          if (date[5] > 59)
	  {
	    valid = 0;

	    if (print)
	      print_test_error("\"%s\": Bad dateTime minutes %u (RFC 2911 "
	                       "section 4.1.13).", attr->name, date[5]);
            else
	      break;
	  }

          if (date[6] > 60)
	  {
	    valid = 0;

	    if (print)
	      print_test_error("\"%s\": Bad dateTime seconds %u (RFC 2911 "
	                       "section 4.1.13).", attr->name, date[6]);
            else
	      break;
	  }

          if (date[7] > 9)
	  {
	    valid = 0;

	    if (print)
	      print_test_error("\"%s\": Bad dateTime deciseconds %u (RFC 2911 "
	                       "section 4.1.13).", attr->name, date[7]);
            else
	      break;
	  }

          if (date[8] != '-' && date[8] != '+')
	  {
	    valid = 0;

	    if (print)
	      print_test_error("\"%s\": Bad dateTime UTC sign '%c' (RFC 2911 "
	                       "section 4.1.13).", attr->name, date[8]);
            else
	      break;
	  }

          if (date[9] > 11)
	  {
	    valid = 0;

	    if (print)
	      print_test_error("\"%s\": Bad dateTime UTC hours %u (RFC 2911 "
	                       "section 4.1.13).", attr->name, date[9]);
            else
	      break;
	  }

          if (date[10] > 59)
	  {
	    valid = 0;

	    if (print)
	      print_test_error("\"%s\": Bad dateTime UTC minutes %u (RFC 2911 "
	                       "section 4.1.13).", attr->name, date[10]);
            else
	      break;
	  }
	}
        break;

    case IPP_TAG_RESOLUTION :
        for (i = 0; i < attr->num_values; i ++)
	{
	  if (attr->values[i].resolution.xres <= 0)
	  {
	    valid = 0;

	    if (print)
	      print_test_error("\"%s\": Bad resolution value %dx%d%s - cross "
	                       "feed resolution must be positive (RFC 2911 "
			       "section 4.1.13).", attr->name,
			       attr->values[i].resolution.xres,
			       attr->values[i].resolution.yres,
			       attr->values[i].resolution.units ==
			           IPP_RES_PER_INCH ? "dpi" :
			           attr->values[i].resolution.units ==
			               IPP_RES_PER_CM ? "dpc" : "unknown");
            else
	      break;
	  }

	  if (attr->values[i].resolution.yres <= 0)
	  {
	    valid = 0;

	    if (print)
	      print_test_error("\"%s\": Bad resolution value %dx%d%s - feed "
	                       "resolution must be positive (RFC 2911 section "
			       "4.1.13).", attr->name,
			       attr->values[i].resolution.xres,
			       attr->values[i].resolution.yres,
			       attr->values[i].resolution.units ==
			           IPP_RES_PER_INCH ? "dpi" :
			           attr->values[i].resolution.units ==
			               IPP_RES_PER_CM ? "dpc" : "unknown");
            else
	      break;
	  }

	  if (attr->values[i].resolution.units != IPP_RES_PER_INCH &&
	      attr->values[i].resolution.units != IPP_RES_PER_CM)
	  {
	    valid = 0;

	    if (print)
	      print_test_error("\"%s\": Bad resolution value %dx%d%s - bad "
	                       "units value (RFC 2911 section 4.1.13).",
			       attr->name, attr->values[i].resolution.xres,
			       attr->values[i].resolution.yres,
			       attr->values[i].resolution.units ==
			           IPP_RES_PER_INCH ? "dpi" :
			           attr->values[i].resolution.units ==
			               IPP_RES_PER_CM ? "dpc" : "unknown");
            else
	      break;
	  }
	}
        break;

    case IPP_TAG_RANGE :
        for (i = 0; i < attr->num_values; i ++)
	{
	  if (attr->values[i].range.lower > attr->values[i].range.upper)
	  {
	    valid = 0;

	    if (print)
	      print_test_error("\"%s\": Bad rangeOfInteger value %d-%d - lower "
	                       "greater than upper (RFC 2911 section 4.1.13).",
			       attr->name, attr->values[i].range.lower,
			       attr->values[i].range.upper);
            else
	      break;
	  }
	}
        break;

    case IPP_TAG_BEGIN_COLLECTION :
        for (i = 0; i < attr->num_values; i ++)
	{
	  for (colattr = attr->values[i].collection->attrs;
	       colattr;
	       colattr = colattr->next)
	  {
	    if (!validate_attr(colattr, 0))
	    {
	      valid = 0;
	      break;
	    }
	  }

	  if (colattr && print)
	  {
            print_test_error("\"%s\": Bad collection value.", attr->name);

	    while (colattr)
	    {
	      validate_attr(colattr, print);
	      colattr = colattr->next;
	    }
	  }
	}
        break;

    case IPP_TAG_TEXT :
    case IPP_TAG_TEXTLANG :
        for (i = 0; i < attr->num_values; i ++)
	{
	  for (ptr = attr->values[i].string.text; *ptr; ptr ++)
	  {
	    if ((*ptr & 0xe0) == 0xc0)
	    {
	      ptr ++;
	      if ((*ptr & 0xc0) != 0x80)
	        break;
	    }
	    else if ((*ptr & 0xf0) == 0xe0)
	    {
	      ptr ++;
	      if ((*ptr & 0xc0) != 0x80)
	        break;
	      ptr ++;
	      if ((*ptr & 0xc0) != 0x80)
	        break;
	    }
	    else if ((*ptr & 0xf8) == 0xf0)
	    {
	      ptr ++;
	      if ((*ptr & 0xc0) != 0x80)
	        break;
	      ptr ++;
	      if ((*ptr & 0xc0) != 0x80)
	        break;
	      ptr ++;
	      if ((*ptr & 0xc0) != 0x80)
	        break;
	    }
	    else if (*ptr & 0x80)
	      break;
	  }

	  if (*ptr)
	  {
	    valid = 0;

	    if (print)
	      print_test_error("\"%s\": Bad text value \"%s\" - bad UTF-8 "
			       "sequence (RFC 2911 section 4.1.1).", attr->name,
			       attr->values[i].string.text);
            else
	      break;
	  }

	  if ((ptr - attr->values[i].string.text) > 1023)
	  {
	    valid = 0;

	    if (print)
	      print_test_error("\"%s\": Bad text value \"%s\" - bad length %d "
			       "(RFC 2911 section 4.1.1).", attr->name,
			       attr->values[i].string.text,
			       (int)strlen(attr->values[i].string.text));
            else
	      break;
	  }
	}
        break;

    case IPP_TAG_NAME :
    case IPP_TAG_NAMELANG :
        for (i = 0; i < attr->num_values; i ++)
	{
	  for (ptr = attr->values[i].string.text; *ptr; ptr ++)
	  {
	    if ((*ptr & 0xe0) == 0xc0)
	    {
	      ptr ++;
	      if ((*ptr & 0xc0) != 0x80)
	        break;
	    }
	    else if ((*ptr & 0xf0) == 0xe0)
	    {
	      ptr ++;
	      if ((*ptr & 0xc0) != 0x80)
	        break;
	      ptr ++;
	      if ((*ptr & 0xc0) != 0x80)
	        break;
	    }
	    else if ((*ptr & 0xf8) == 0xf0)
	    {
	      ptr ++;
	      if ((*ptr & 0xc0) != 0x80)
	        break;
	      ptr ++;
	      if ((*ptr & 0xc0) != 0x80)
	        break;
	      ptr ++;
	      if ((*ptr & 0xc0) != 0x80)
	        break;
	    }
	    else if (*ptr & 0x80)
	      break;
	  }

	  if (*ptr)
	  {
	    valid = 0;

	    if (print)
	      print_test_error("\"%s\": Bad name value \"%s\" - bad UTF-8 "
			       "sequence (RFC 2911 section 4.1.2).", attr->name,
			       attr->values[i].string.text);
            else
	      break;
	  }

	  if ((ptr - attr->values[i].string.text) > 1023)
	  {
	    valid = 0;

	    if (print)
	      print_test_error("\"%s\": Bad name value \"%s\" - bad length %d "
			       "(RFC 2911 section 4.1.2).", attr->name,
			       attr->values[i].string.text,
			       (int)strlen(attr->values[i].string.text));
            else
	      break;
	  }
	}
        break;

    case IPP_TAG_KEYWORD :
        for (i = 0; i < attr->num_values; i ++)
	{
	  for (ptr = attr->values[i].string.text; *ptr; ptr ++)
	    if (!isalnum(*ptr & 255) && *ptr != '-' && *ptr != '.' &&
	        *ptr != '_')
	      break;

	  if (*ptr || ptr == attr->values[i].string.text)
	  {
	    valid = 0;

	    if (print)
	      print_test_error("\"%s\": Bad keyword value \"%s\" - invalid "
			       "character (RFC 2911 section 4.1.3).",
			       attr->name, attr->values[i].string.text);
            else
	      break;
	  }

	  if ((ptr - attr->values[i].string.text) > 255)
	  {
	    valid = 0;

	    if (print)
	      print_test_error("\"%s\": Bad keyword value \"%s\" - bad "
			       "length %d (RFC 2911 section 4.1.3).",
			       attr->name, attr->values[i].string.text,
			       (int)strlen(attr->values[i].string.text));
            else
	      break;
	  }
	}
        break;

    case IPP_TAG_URI :
        for (i = 0; i < attr->num_values; i ++)
	{
	  uri_status = httpSeparateURI(HTTP_URI_CODING_ALL,
	                               attr->values[i].string.text,
				       scheme, sizeof(scheme),
				       userpass, sizeof(userpass),
				       hostname, sizeof(hostname),
				       &port, resource, sizeof(resource));

	  if (uri_status < HTTP_URI_OK)
	  {
	    valid = 0;

	    if (print)
	      print_test_error("\"%s\": Bad URI value \"%s\" - %s "
	                       "(RFC 2911 section 4.1.5).", attr->name,
	                       attr->values[i].string.text,
			       URIStatusStrings[uri_status -
			                        HTTP_URI_OVERFLOW]);
            else
	      break;
	  }

	  if (strlen(attr->values[i].string.text) > 1023)
	  {
	    valid = 0;

	    if (print)
	      print_test_error("\"%s\": Bad URI value \"%s\" - bad length %d "
	                       "(RFC 2911 section 4.1.5).", attr->name,
	                       attr->values[i].string.text,
			       (int)strlen(attr->values[i].string.text));
            else
	      break;
	  }
	}
        break;

    case IPP_TAG_URISCHEME :
        for (i = 0; i < attr->num_values; i ++)
	{
	  ptr = attr->values[i].string.text;
	  if (islower(*ptr & 255))
	  {
	    for (ptr ++; *ptr; ptr ++)
	      if (!islower(*ptr & 255) && !isdigit(*ptr & 255) &&
	          *ptr != '+' && *ptr != '-' && *ptr != '.')
                break;
	  }

	  if (*ptr || ptr == attr->values[i].string.text)
	  {
	    valid = 0;

            if (print)
	      print_test_error("\"%s\": Bad uriScheme value \"%s\" - bad "
	                       "characters (RFC 2911 section 4.1.6).",
			       attr->name, attr->values[i].string.text);
            else
	      break;
	  }

	  if ((ptr - attr->values[i].string.text) > 63)
	  {
	    valid = 0;

            if (print)
	      print_test_error("\"%s\": Bad uriScheme value \"%s\" - bad "
	                       "length %d (RFC 2911 section 4.1.6).",
			       attr->name, attr->values[i].string.text,
			       (int)strlen(attr->values[i].string.text));
            else
	      break;
	  }
	}
        break;

    case IPP_TAG_CHARSET :
        for (i = 0; i < attr->num_values; i ++)
	{
	  for (ptr = attr->values[i].string.text; *ptr; ptr ++)
	    if (!isprint(*ptr & 255) || isupper(*ptr & 255) ||
	        isspace(*ptr & 255))
	      break;

	  if (*ptr || ptr == attr->values[i].string.text)
	  {
	    valid = 0;

            if (print)
	      print_test_error("\"%s\": Bad charset value \"%s\" - bad "
	                       "characters (RFC 2911 section 4.1.7).",
			       attr->name, attr->values[i].string.text);
            else
	      break;
	  }

	  if ((ptr - attr->values[i].string.text) > 40)
	  {
	    valid = 0;

            if (print)
	      print_test_error("\"%s\": Bad charset value \"%s\" - bad "
	                       "length %d (RFC 2911 section 4.1.7).",
			       attr->name, attr->values[i].string.text,
			       (int)strlen(attr->values[i].string.text));
            else
	      break;
	  }
	}
        break;

    case IPP_TAG_LANGUAGE :
       /*
        * The following regular expression is derived from the ABNF for
	* language tags in RFC 4646.  All I can say is that this is the
	* easiest way to check the values...
	*/

        if ((i = regcomp(&re,
			 "^("
			 "(([a-z]{2,3}(-[a-z][a-z][a-z]){0,3})|[a-z]{4,8})"
								/* language */
			 "(-[a-z][a-z][a-z][a-z]){0,1}"		/* script */
			 "(-([a-z][a-z]|[0-9][0-9][0-9])){0,1}"	/* region */
			 "(-([a-z]{5,8}|[0-9][0-9][0-9]))*"	/* variant */
			 "(-[a-wy-z](-[a-z0-9]{2,8})+)*"	/* extension */
			 "(-x(-[a-z0-9]{1,8})+)*"		/* privateuse */
			 "|"
			 "x(-[a-z0-9]{1,8})+"			/* privateuse */
			 "|"
			 "[a-z]{1,3}(-[a-z][0-9]{2,8}){1,2}"	/* grandfathered */
			 ")$",
			 REG_NOSUB | REG_EXTENDED)) != 0)
        {
          char	temp[256];		/* Temporary error string */

          regerror(i, &re, temp, sizeof(temp));
	  print_fatal_error("Unable to compile naturalLanguage regular "
	                    "expression: %s.", temp);
        }

        for (i = 0; i < attr->num_values; i ++)
	{
	  if (regexec(&re, attr->values[i].string.text, 0, NULL, 0))
	  {
	    valid = 0;

            if (print)
	      print_test_error("\"%s\": Bad naturalLanguage value \"%s\" - bad "
	                       "characters (RFC 2911 section 4.1.8).",
			       attr->name, attr->values[i].string.text);
            else
	      break;
	  }

	  if (strlen(attr->values[i].string.text) > 63)
	  {
	    valid = 0;

            if (print)
	      print_test_error("\"%s\": Bad naturalLanguage value \"%s\" - bad "
	                       "length %d (RFC 2911 section 4.1.8).",
			       attr->name, attr->values[i].string.text,
			       (int)strlen(attr->values[i].string.text));
            else
	      break;
	  }
	}

	regfree(&re);
        break;

    case IPP_TAG_MIMETYPE :
       /*
        * The following regular expression is derived from the ABNF for
	* language tags in RFC 2045 and 4288.  All I can say is that this is
	* the easiest way to check the values...
	*/

        if ((i = regcomp(&re,
			 "^"
			 "[-a-zA-Z0-9!#$&.+^_]{1,127}"		/* type-name */
			 "/"
			 "[-a-zA-Z0-9!#$&.+^_]{1,127}"		/* subtype-name */
			 "(;[-a-zA-Z0-9!#$&.+^_]{1,127}="	/* parameter= */
			 "([-a-zA-Z0-9!#$&.+^_]{1,127}|\"[^\"]*\"))*"
			 					/* value */
			 "$",
			 REG_NOSUB | REG_EXTENDED)) != 0)
        {
          char	temp[256];		/* Temporary error string */

          regerror(i, &re, temp, sizeof(temp));
	  print_fatal_error("Unable to compile mimeMediaType regular "
	                    "expression: %s.", temp);
        }

        for (i = 0; i < attr->num_values; i ++)
	{
	  if (regexec(&re, attr->values[i].string.text, 0, NULL, 0))
	  {
	    valid = 0;

            if (print)
	      print_test_error("\"%s\": Bad mimeMediaType value \"%s\" - bad "
	                       "characters (RFC 2911 section 4.1.9).",
			       attr->name, attr->values[i].string.text);
            else
	      break;
	  }

	  if (strlen(attr->values[i].string.text) > 255)
	  {
	    valid = 0;

            if (print)
	      print_test_error("\"%s\": Bad mimeMediaType value \"%s\" - bad "
	                       "length %d (RFC 2911 section 4.1.9).",
			       attr->name, attr->values[i].string.text,
			       (int)strlen(attr->values[i].string.text));
            else
	      break;
	  }
	}
        break;

    default :
        break;
  }

  return (valid);
}


/*
 * 'with_value()' - Test a WITH-VALUE predicate.
 */

static int				/* O - 1 on match, 0 on non-match */
with_value(char            *value,	/* I - Value string */
           int             regex,	/* I - Value is a regular expression */
           ipp_attribute_t *attr)	/* I - Attribute to compare */
{
  int	i;				/* Looping var */
  char	*valptr;			/* Pointer into value */


 /*
  * NULL matches everything.
  */

  if (!value)
    return (1);

 /*
  * Compare the value string to the attribute value.
  */

  switch (attr->value_tag)
  {
    case IPP_TAG_INTEGER :
    case IPP_TAG_ENUM :
        for (i = 0; i < attr->num_values; i ++)
        {
	  char	op;			/* Comparison operator */
	  int	intvalue;		/* Integer value */


          valptr = value;

	  while (isspace(*valptr & 255) || isdigit(*valptr & 255) ||
		 *valptr == '-' || *valptr == ',' || *valptr == '<' ||
		 *valptr == '=' || *valptr == '>')
	  {
	    op = '=';
	    while (*valptr && !isdigit(*valptr & 255) && *valptr != '-')
	    {
	      if (*valptr == '<' || *valptr == '>' || *valptr == '=')
		op = *valptr;
	      valptr ++;
	    }

            if (!*valptr)
	      break;

	    intvalue = strtol(valptr, &valptr, 0);
	    switch (op)
	    {
	      case '=' :
	          if (attr->values[i].integer == intvalue)
		    return (1);
		  break;
	      case '<' :
	          if (attr->values[i].integer < intvalue)
		    return (1);
		  break;
	      case '>' :
	          if (attr->values[i].integer > intvalue)
		    return (1);
		  break;
	    }
	  }
        }
	break;

    case IPP_TAG_BOOLEAN :
	for (i = 0; i < attr->num_values; i ++)
	{
          if (!strcmp(value, "true") == attr->values[i].boolean)
	    return (1);
	}
	break;

    case IPP_TAG_NOVALUE :
        return (!strcmp(value, "no-value"));

    case IPP_TAG_CHARSET :
    case IPP_TAG_KEYWORD :
    case IPP_TAG_LANGUAGE :
    case IPP_TAG_MIMETYPE :
    case IPP_TAG_NAME :
    case IPP_TAG_NAMELANG :
    case IPP_TAG_TEXT :
    case IPP_TAG_TEXTLANG :
    case IPP_TAG_URI :
    case IPP_TAG_URISCHEME :
        if (regex)
	{
	 /*
	  * Value is an extended, case-sensitive POSIX regular expression...
	  */

	  regex_t	re;		/* Regular expression */

          if ((i = regcomp(&re, value, REG_EXTENDED | REG_NOSUB)) != 0)
	  {
	    char temp[256];		/* Temporary string */

            regerror(i, &re, temp, sizeof(temp));

	    print_fatal_error("Unable to compile WITH-VALUE regular expression "
	                      "\"%s\" - %s", value, temp);
	    return (0);
	  }

         /*
	  * See if ALL of the values match the given regular expression.
	  */

	  for (i = 0; i < attr->num_values; i ++)
	  {
	    if (regexec(&re, attr->values[i].string.text, 0, NULL, 0))
	      break;
	  }

	  regfree(&re);

          return (i == attr->num_values);
	}
	else
	{
	 /*
	  * Value is a literal string, see if at least one value matches the
	  * literal string...
	  */

	  for (i = 0; i < attr->num_values; i ++)
	  {
	    if (!strcmp(value, attr->values[i].string.text))
	      return (1);
	  }
	}
	break;

    default :
        break;
  }

  return (0);
}


/*
 * End of "$Id$".
 */
