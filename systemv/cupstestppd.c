/*
 * "$Id: cupstestppd.c,v 1.1.2.15 2003/02/20 14:12:03 mike Exp $"
 *
 *   PPD test program for the Common UNIX Printing System (CUPS).
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
 *   PostScript is a trademark of Adobe Systems, Inc.
 *
 *   This file is subject to the Apple OS-Developed Software exception.
 *
 * Contents:
 *
 *   main()  - Main entry for test program.
 *   usage() - Show program usage...
 */

/*
 * Include necessary headers...
 */

#include <cups/cups.h>
#include <cups/string.h>
#include <errno.h>
#include <stdlib.h>


/*
 * Error codes...
 */

#define ERROR_NONE		0
#define ERROR_USAGE		1
#define ERROR_FILE_OPEN		2
#define ERROR_PPD_FORMAT	3
#define ERROR_CONFORMANCE	4


/*
 * Local functions...
 */

void	usage(void);


/*
 * 'main()' - Main entry for test program.
 */

int				/* O - Exit status */
main(int  argc,			/* I - Number of command-line arguments */
     char *argv[])		/* I - Command-line arguments */
{
  int		i, j, k, m, n;	/* Looping vars */
  int		len;		/* Length of option name */
  char		*opt;		/* Option character */
  const char	*ptr;		/* Pointer into string */
  int		files;		/* Number of files */
  int		verbose;	/* Want verbose output? */
  int		status;		/* Exit status */
  int		errors;		/* Number of conformance errors */
  int		ppdversion;	/* PPD spec version in PPD file */
  ppd_status_t	error;		/* Status of ppdOpen*() */
  int		line;		/* Line number for error */
  ppd_file_t	*ppd;		/* PPD file record */
  ppd_size_t	*size;		/* Size record */
  ppd_group_t	*group;		/* UI group */
  ppd_option_t	*option;	/* Standard UI option */
  ppd_group_t	*group2;	/* UI group */
  ppd_option_t	*option2;	/* Standard UI option */
  ppd_choice_t	*choice;	/* Standard UI option choice */
  static char	*uis[] = { "BOOLEAN", "PICKONE", "PICKMANY" };
  static char	*sections[] = { "ANY", "DOCUMENT", "EXIT",
                                "JCL", "PAGE", "PROLOG" };


  setbuf(stdout, NULL);

 /*
  * Display PPD files for each file listed on the command-line...
  */

  verbose = 0;
  ppd     = NULL;
  files   = 0;
  status  = ERROR_NONE;

  for (i = 1; i < argc; i ++)
    if (argv[i][0] == '-' && argv[i][1])
    {
      for (opt = argv[i] + 1; *opt; opt ++)
        switch (*opt)
	{
	  case 'q' :
	      if (verbose > 0)
	      {
        	fputs("cupstestppd: The -q option is incompatible with the -v option.\n",
		      stderr);
		return (1);
	      }

	      verbose --;
	      break;

	  case 'v' :
	      if (verbose < 0)
	      {
        	fputs("cupstestppd: The -v option is incompatible with the -q option.\n",
		      stderr);
		return (1);
	      }

	      verbose ++;
	      break;

	  default :
	      usage();
	      break;
	}
    }
    else
    {
     /*
      * Open the PPD file...
      */

      if (files)
        putchar('\n');

      files ++;

      if (argv[i][0] == '-')
      {
       /*
        * Read from stdin...
	*/

        if (verbose >= 0)
          printf("(stdin):");

        ppd = ppdOpen(stdin);
      }
      else if (strlen(argv[i]) > 3 &&
               !strcmp(argv[i] + strlen(argv[i]) - 3, ".gz"))
      {
       /*
        * Read from a gzipped file...
	*/

        char	command[1024];	/* Command string */
	FILE	*gunzip;	/* Pipe file */


        if (verbose >= 0)
          printf("%s:", argv[i]);

        snprintf(command, sizeof(command), "gunzip -c %s", argv[i]);
	gunzip = popen(command, "r");
	ppd    = ppdOpen(gunzip);

	if (gunzip != NULL)
	  pclose(gunzip);
      }
      else
      {
       /*
        * Read from a file...
	*/

        if (verbose >= 0)
          printf("%s:", argv[i]);

        ppd = ppdOpenFile(argv[i]);
      }

      if (ppd == NULL)
      {
        error = ppdLastError(&line);

        printf(" FAIL\n      **FAIL**  Unable to open PPD file - ");

	if (error <= PPD_ALLOC_ERROR)
	{
	  status = ERROR_FILE_OPEN;

	  if (verbose >= 0)
	    puts(strerror(errno));
	}
	else
	{
	  status = ERROR_PPD_FORMAT;

          if (verbose >= 0)
	  {
	    printf("%s on line %d.\n", ppdErrorString(error), line);

            switch (error)
	    {
	      case PPD_MISSING_PPDADOBE4 :
	          puts("                REF: Page 42, section 5.2.");
	          break;
	      case PPD_MISSING_VALUE :
	          puts("                REF: Page 20, section 3.4.");
	          break;
	      case PPD_BAD_OPEN_GROUP :
	      case PPD_NESTED_OPEN_GROUP :
	          puts("                REF: Pages 45-46, section 5.2.");
	          break;
	      case PPD_BAD_OPEN_UI :
	      case PPD_NESTED_OPEN_UI :
	          puts("                REF: Pages 42-45, section 5.2.");
	          break;
	      case PPD_BAD_ORDER_DEPENDENCY :
	          puts("                REF: Pages 48-49, section 5.2.");
	          break;
	      case PPD_BAD_UI_CONSTRAINTS :
	          puts("                REF: Pages 52-54, section 5.2.");
	          break;
	      case PPD_MISSING_ASTERISK :
	          puts("                REF: Page 15, section 3.2.");
	          break;
	      case PPD_LINE_TOO_LONG :
	          puts("                REF: Page 15, section 3.1.");
	          break;
	      case PPD_ILLEGAL_CHARACTER :
	          puts("                REF: Page 15, section 3.1.");
	          break;
	      case PPD_ILLEGAL_MAIN_KEYWORD :
	          puts("                REF: Pages 16-17, section 3.2.");
	          break;
	      case PPD_ILLEGAL_OPTION_KEYWORD :
	          puts("                REF: Page 19, section 3.3.");
	          break;
	      case PPD_ILLEGAL_TRANSLATION :
	          puts("                REF: Page 27, section 3.5.");
	          break;
              default :
	          break;
	    }
	  }
        }

	continue;
      }

     /*
      * Show the header and then perform basic conformance tests (limited
      * only by what the CUPS PPD functions actually load...)
      */

      errors     = 0;
      ppdversion = 43;

      if (verbose > 0)
        puts("\n    DETAILED CONFORMANCE TEST RESULTS");

      if ((ptr = ppdFindAttr(ppd, "FormatVersion", NULL)) != NULL)
        ppdversion = (int)(10 * atof(ptr) + 0.5);

      if (ppdFindAttr(ppd, "DefaultImageableArea", NULL) != NULL)
      {
	if (verbose > 0)
	  puts("        PASS    DefaultImageableArea");
      }
      else
      {
	if (verbose >= 0)
	{
	  if (!errors && !verbose)
	    puts(" FAIL");

	  puts("      **FAIL**  REQUIRED DefaultImageableArea");
	  puts("                REF: Page 102, section 5.15.");
        }

	errors ++;
      }

      if (ppdFindAttr(ppd, "DefaultPaperDimension", NULL) != NULL)
      {
	if (verbose > 0)
	  puts("        PASS    DefaultPaperDimension");
      }
      else
      {
	if (verbose >= 0)
	{
	  if (!errors && !verbose)
	    puts(" FAIL");

	  puts("      **FAIL**  REQUIRED DefaultPaperDimension");
	  puts("                REF: Page 103, section 5.15.");
        }

	errors ++;
      }

      for (j = 0, group = ppd->groups; j < ppd->num_groups; j ++, group ++)
	for (k = 0, option = group->options; k < group->num_options; k ++, option ++)
	{
	 /*
	  * Verify that we have a default choice...
	  */

	  if (option->defchoice[0])
	  {
	    if (verbose > 0)
	      printf("        PASS    Default%s\n", option->keyword);
	  }
	  else
	  {
	    if (verbose >= 0)
	    {
	      if (!errors && !verbose)
		puts(" FAIL");

	      printf("      **FAIL**  REQUIRED Default%s\n", option->keyword);
	      puts("                REF: Page 40, section 4.5.");
            }

	    errors ++;
	  }

         /*
	  * Then verify that no other options share a common root name.
	  */

	  len = strlen(option->keyword);

	  for (m = 0, group2 = ppd->groups;
	       m < ppd->num_groups;
	       m ++, group2 ++)
	    for (n = 0, option2 = group2->options;
	         n < group2->num_options;
		 n ++, option2 ++)
	      if (option != option2 &&
	          len < strlen(option2->keyword) &&
	          !strncmp(option->keyword, option2->keyword, len))
	      {
		if (verbose >= 0)
		{
		  if (!errors && !verbose)
		    puts(" FAIL");

		  printf("      **FAIL**  %s shares a common prefix with %s\n",
		         option->keyword, option2->keyword);
		  puts("                REF: Page 15, section 3.2.");
        	}

		errors ++;
	      }
	}

      if (ppdFindAttr(ppd, "FileVersion", NULL) != NULL)
      {
	if (verbose > 0)
	  puts("        PASS    FileVersion");
      }
      else
      {
	if (verbose >= 0)
	{
	  if (!errors && !verbose)
	    puts(" FAIL");

	  puts("      **FAIL**  REQUIRED FileVersion");
	  puts("                REF: Page 56, section 5.3.");
        }

	errors ++;
      }

      if (ppdFindAttr(ppd, "FormatVersion", NULL) != NULL)
      {
	if (verbose > 0)
	  puts("        PASS    FormatVersion");
      }
      else
      {
	if (verbose >= 0)
	{
	  if (!errors && !verbose)
	    puts(" FAIL");

	  puts("      **FAIL**  REQUIRED FormatVersion");
	  puts("                REF: Page 56, section 5.3.");
        }

	errors ++;
      }

      if (ppd->lang_encoding != NULL)
      {
	if (verbose > 0)
	  puts("        PASS    LanguageEncoding");
      }
      else if (ppdversion > 40)
      {
	if (verbose >= 0)
	{
	  if (!errors && !verbose)
	    puts(" FAIL");

	  puts("      **FAIL**  REQUIRED LanguageEncoding");
	  puts("                REF: Pages 56-57, section 5.3.");
        }

	errors ++;
      }

      if (ppd->lang_version != NULL)
      {
	if (verbose > 0)
	  puts("        PASS    LanguageVersion");
      }
      else
      {
	if (verbose >= 0)
	{
	  if (!errors && !verbose)
	    puts(" FAIL");

	  puts("      **FAIL**  REQUIRED LanguageVersion");
	  puts("                REF: Pages 57-58, section 5.3.");
        }

	errors ++;
      }

      if (ppd->manufacturer != NULL)
      {
	if (verbose > 0)
	  puts("        PASS    Manufacturer");
      }
      else if (ppdversion >= 43)
      {
	if (verbose >= 0)
	{
	  if (!errors && !verbose)
	    puts(" FAIL");

	  puts("      **FAIL**  REQUIRED Manufacturer");
	  puts("                REF: Pages 58-59, section 5.3.");
        }

	errors ++;
      }

      if (ppd->modelname != NULL)
      {
        for (ptr = ppd->modelname; *ptr; ptr ++)
	  if (!isalnum(*ptr) && !strchr(" ./-+", *ptr))
	    break;

	if (*ptr)
	{
	  if (verbose >= 0)
	  {
	    if (!errors && !verbose)
	      puts(" FAIL");

	    printf("      **FAIL**  BAD ModelName - \"%c\" not allowed in string.\n",
	           *ptr);
	    puts("                REF: Pages 59-60, section 5.3.");
          }

	  errors ++;
	}
	else if (verbose > 0)
	  puts("        PASS    ModelName");
      }
      else
      {
	if (verbose >= 0)
	{
	  if (!errors && !verbose)
	    puts(" FAIL");

	  puts("      **FAIL**  REQUIRED ModelName");
	  puts("                REF: Pages 59-60, section 5.3.");
        }

	errors ++;
      }

      if (ppd->nickname != NULL)
      {
	if (verbose > 0)
	  puts("        PASS    NickName");
      }
      else
      {
	if (verbose >= 0)
	{
	  if (!errors && !verbose)
	    puts(" FAIL");

	  puts("      **FAIL**  REQUIRED NickName");
	  puts("                REF: Page 60, section 5.3.");
        }

	errors ++;
      }

      if (ppdFindOption(ppd, "PageSize") != NULL)
      {
	if (verbose > 0)
	  puts("        PASS    PageSize");
      }
      else
      {
	if (verbose >= 0)
	{
	  if (!errors && !verbose)
	    puts(" FAIL");

	  puts("      **FAIL**  REQUIRED PageSize");
	  puts("                REF: Pages 99-100, section 5.14.");
        }

	errors ++;
      }

      if (ppdFindOption(ppd, "PageRegion") != NULL)
      {
	if (verbose > 0)
	  puts("        PASS    PageRegion");
      }
      else
      {
	if (verbose >= 0)
	{
	  if (!errors && !verbose)
	    puts(" FAIL");

	  puts("      **FAIL**  REQUIRED PageRegion");
	  puts("                REF: Page 100, section 5.14.");
        }

	errors ++;
      }

      if (ppd->pcfilename != NULL)
      {
	if (verbose > 0)
          puts("        PASS    PCFileName");
      }
      else
      {
	if (verbose >= 0)
	{
	  if (!errors && !verbose)
	    puts(" FAIL");

	  puts("      **FAIL**  REQUIRED PCFileName");
	  puts("                REF: Pages 61-62, section 5.3.");
        }

	errors ++;
      }

      if (ppd->product != NULL)
      {
        if (ppd->product[0] != '(' ||
	    ppd->product[strlen(ppd->product) - 1] != ')')
	{
	  if (verbose >= 0)
	  {
	    if (!errors && !verbose)
	      puts(" FAIL");

	    puts("      **FAIL**  BAD Product - not \"(string)\".");
	    puts("                REF: Page 62, section 5.3.");
          }

	  errors ++;
	}
	else if (verbose > 0)
	  puts("        PASS    Product");
      }
      else
      {
	if (verbose >= 0)
	{
	  if (!errors && !verbose)
	    puts(" FAIL");

	  puts("      **FAIL**  REQUIRED Product");
	  puts("                REF: Page 62, section 5.3.");
        }

	errors ++;
      }

      if ((ptr = ppdFindAttr(ppd, "PSVersion", NULL)) != NULL)
      {
        char	junkstr[255];			/* Temp string */
	int	junkint;			/* Temp integer */


        if (sscanf(ptr, "(%[^)])%d", junkstr, &junkint) != 2)
	{
	  if (verbose >= 0)
	  {
	    if (!errors && !verbose)
	      puts(" FAIL");

	    puts("      **FAIL**  BAD PSVersion - not \"(string) int\".");
	    puts("                REF: Pages 62-64, section 5.3.");
          }

	  errors ++;
	}
	else if (verbose > 0)
	  puts("        PASS    PSVersion");
      }
      else
      {
	if (verbose >= 0)
	{
	  if (!errors && !verbose)
	    puts(" FAIL");

	  puts("      **FAIL**  REQUIRED PSVersion");
	  puts("                REF: Pages 62-64, section 5.3.");
        }

	errors ++;
      }

      if (ppd->shortnickname != NULL)
      {
        if (strlen(ppd->shortnickname) > 31)
	{
	  if (verbose >= 0)
	  {
	    if (!errors && !verbose)
	      puts(" FAIL");

	    puts("      **FAIL**  BAD ShortNickName - longer than 31 chars.");
	    puts("                REF: Pages 64-65, section 5.3.");
          }

	  errors ++;
	}
	else if (verbose > 0)
	  puts("        PASS    ShortNickName");
      }
      else if (ppdversion >= 43)
      {
	if (verbose >= 0)
	{
	  if (!errors && !verbose)
	    puts(" FAIL");

	  puts("      **FAIL**  REQUIRED ShortNickName");
	  puts("                REF: Page 64-65, section 5.3.");
        }

	errors ++;
      }

      if (errors)
	status = ERROR_CONFORMANCE;
      else if (!verbose)
	puts(" PASS");
	 
      if (verbose >= 0)
      {
        if (ppdversion < 43)
	{
          printf("        WARN    Obsolete PPD version %s!\n",
	         ppdFindAttr(ppd, "FormatVersion", NULL));
	  puts("                REF: Page 42, section 5.2.");
	}

        if (!ppd->lang_encoding && ppdversion < 41)
	{
	  puts("        WARN    LanguageEncoding required by PPD 4.3 spec.");
	  puts("                REF: Pages 56-57, section 5.3.");
	}

        if (!ppd->manufacturer && ppdversion < 43)
	{
	  puts("        WARN    Manufacturer required by PPD 4.3 spec.");
	  puts("                REF: Pages 58-59, section 5.3.");
	}

       /*
	* Treat a PCFileName attribute longer than 12 characters as
	* a warning and not a hard error...
	*/

	if (ppd->pcfilename && strlen(ppd->pcfilename) > 12)
	{
	  puts("        WARN    PCFileName longer than 8.3 in violation of PPD spec.");
	  puts("                REF: Pages 61-62, section 5.3.");
        }

        if (!ppd->shortnickname && ppdversion < 43)
	{
	  puts("        WARN    ShortNickName required by PPD 4.3 spec.");
	  puts("                REF: Pages 64-65, section 5.3.");
	}
      }

      if (verbose > 0)
      {
        if (errors)
          printf("    %d ERROR%s FOUND\n", errors, errors == 1 ? "" : "S");
	else
	  puts("    NO ERRORS FOUND");
      }


     /*
      * Then list the options, if "-v" was provided...
      */ 

      if (verbose > 1)
      {
	puts("");
	printf("    language_level = %d\n", ppd->language_level);
	printf("    color_device = %s\n", ppd->color_device ? "TRUE" : "FALSE");
	printf("    variable_sizes = %s\n", ppd->variable_sizes ? "TRUE" : "FALSE");
	printf("    landscape = %d\n", ppd->landscape);

	switch (ppd->colorspace)
	{
	  case PPD_CS_CMYK :
              puts("    colorspace = PPD_CS_CMYK");
	      break;
	  case PPD_CS_CMY :
              puts("    colorspace = PPD_CS_CMY");
	      break;
	  case PPD_CS_GRAY :
              puts("    colorspace = PPD_CS_GRAY");
	      break;
	  case PPD_CS_RGB :
              puts("    colorspace = PPD_CS_RGB");
	      break;
	  default :
              puts("    colorspace = <unknown>");
	      break;
	}

	printf("    num_emulations = %d\n", ppd->num_emulations);
	for (j = 0; j < ppd->num_emulations; j ++)
	  printf("        emulations[%d] = %s\n", j, ppd->emulations[j].name);

	printf("    lang_encoding = %s\n", ppd->lang_encoding);
	printf("    lang_version = %s\n", ppd->lang_version);
	printf("    modelname = %s\n", ppd->modelname);
	printf("    ttrasterizer = %s\n",
               ppd->ttrasterizer == NULL ? "None" : ppd->ttrasterizer);
	printf("    manufacturer = %s\n", ppd->manufacturer);
	printf("    product = %s\n", ppd->product);
	printf("    nickname = %s\n", ppd->nickname);
	printf("    shortnickname = %s\n", ppd->shortnickname);
	printf("    patches = %d bytes\n",
               ppd->patches == NULL ? 0 : (int)strlen(ppd->patches));

	printf("    num_groups = %d\n", ppd->num_groups);
	for (j = 0, group = ppd->groups; j < ppd->num_groups; j ++, group ++)
	{
	  printf("        group[%d] = %s\n", j, group->text);

	  for (k = 0, option = group->options; k < group->num_options; k ++, option ++)
	  {
	    printf("            options[%d] = %s (%s) %s %s %.0f (%d choices)\n", k,
		   option->keyword, option->text, uis[option->ui],
		   sections[option->section], option->order,
		   option->num_choices);

            if (strcmp(option->keyword, "PageSize") == 0 ||
        	strcmp(option->keyword, "PageRegion") == 0)
            {
              for (m = option->num_choices, choice = option->choices;
		   m > 0;
		   m --, choice ++)
	      {
		size = ppdPageSize(ppd, choice->choice);

		if (size == NULL)
		  printf("                %s (%s) = ERROR", choice->choice, choice->text);
        	else
		  printf("                %s (%s) = %.2fx%.2fin (%.1f,%.1f,%.1f,%.1f)", choice->choice,
	        	 choice->text, size->width / 72.0, size->length / 72.0,
			 size->left / 72.0, size->bottom / 72.0,
			 size->right / 72.0, size->top / 72.0);

        	if (strcmp(option->defchoice, choice->choice) == 0)
		  puts(" *");
		else
		  putchar('\n');
              }
	    }
	    else
	    {
	      for (m = option->num_choices, choice = option->choices;
		   m > 0;
		   m --, choice ++)
	      {
		printf("                %s (%s)", choice->choice, choice->text);

        	if (strcmp(option->defchoice, choice->choice) == 0)
		  puts(" *");
		else
		  putchar('\n');
	      }
            }
	  }
	}

	printf("    num_profiles = %d\n", ppd->num_profiles);
	for (j = 0; j < ppd->num_profiles; j ++)
	  printf("        profiles[%d] = %s/%s %.3f %.3f [ %.3f %.3f %.3f %.3f %.3f %.3f %.3f %.3f %.3f ]\n",
        	 j, ppd->profiles[j].resolution, ppd->profiles[j].media_type,
		 ppd->profiles[j].gamma, ppd->profiles[j].density,
		 ppd->profiles[j].matrix[0][0], ppd->profiles[j].matrix[0][1],
		 ppd->profiles[j].matrix[0][2], ppd->profiles[j].matrix[1][0],
		 ppd->profiles[j].matrix[1][1], ppd->profiles[j].matrix[1][2],
		 ppd->profiles[j].matrix[2][0], ppd->profiles[j].matrix[2][1],
		 ppd->profiles[j].matrix[2][2]);

	printf("    num_fonts = %d\n", ppd->num_fonts);
	for (j = 0; j < ppd->num_fonts; j ++)
	  printf("        fonts[%d] = %s\n", j, ppd->fonts[j]);
      }

      ppdClose(ppd);
    }

  if (!files)
    usage();

  return (status);
}


/*
 * 'usage()' - Show program usage...
 */

void
usage(void)
{
  puts("Usage: cupstestppd [-q] [-v[v]] filename1.ppd[.gz] [... filenameN.ppd[.gz]]");
  puts("       program | cupstestppd [-q] [-v[v]] -");

  exit(ERROR_USAGE);
}


/*
 * End of "$Id: cupstestppd.c,v 1.1.2.15 2003/02/20 14:12:03 mike Exp $".
 */
