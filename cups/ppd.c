/*
 * "$Id: ppd.c,v 1.3 1999/01/24 14:18:43 mike Exp $"
 *
 *   PPD file routines for the Common UNIX Printing System (CUPS).
 *
 *   Copyright 1997-1999 by Easy Software Products, all rights reserved.
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
 *       44145 Airport View Drive, Suite 204
 *       Hollywood, Maryland 20636-3111 USA
 *
 *       Voice: (301) 373-9603
 *       EMail: cups-info@cups.org
 *         WWW: http://www.cups.org
 *
 *   PostScript is a trademark of Adobe Systems, Inc.
 *
 * Contents:
 *
 *   ppdClose()        - Free all memory used by the PPD file.
 *   ppd_free_group()  - Free a single UI group.
 *   ppd_free_option() - Free a single option.
 *   ppdOpen()         - Read a PPD file into memory.
 *   ppdOpenFd()       - Read a PPD file into memory.
 *   ppdOpenFile()     - Read a PPD file into memory.
 *   ppd_read()        - Read a line from a PPD file, skipping comment lines
 *                       as necessary.
 */

/*
 * Include necessary headers.
 */

#include "ppd.h"
/*#define DEBUG*/


/*
 * Definitions...
 */

#if defined(WIN32) || defined(OS2)
#  define READ_BINARY	"rb"		/* Open a binary file for reading */
#  define WRITE_BINARY	"wb"		/* Open a binary file for writing */
#else
#  define READ_BINARY	"r"		/* Open a binary file for reading */
#  define WRITE_BINARY	"w"		/* Open a binary file for writing */
#endif /* WIN32 || OS2 */

#define PPD_KEYWORD	1		/* Line contained a keyword */
#define PPD_OPTION	2		/* Line contained an option name */
#define PPD_TEXT	4		/* Line contained human-readable text */
#define PPD_STRING	8		/* Line contained a string or code */


/*
 * Local functions...
 */

static int	ppd_read(FILE *fp, char *keyword, char *option,
		         unsigned char *text, unsigned char **string);
static void	ppd_decode(unsigned char *string);
static void	ppd_free_group(ppd_group_t *group);
static void	ppd_free_option(ppd_option_t *option);


/*
 * 'ppdClose()' - Free all memory used by the PPD file.
 */

void
ppdClose(ppd_file_t *ppd)	/* I - PPD file record */
{
  int		i;		/* Looping var */
  ppd_emul_t	*emul;		/* Current emulation */
  ppd_group_t	*group;		/* Current group */
  ppd_option_t	*option;	/* Current option */
  char		**font;		/* Current font */


 /*
  * Range check the PPD file record...
  */

  if (ppd == NULL)
    return;

 /*
  * Free all strings at the top level...
  */

  free(ppd->lang_encoding);
  free(ppd->lang_version);
  free(ppd->modelname);
  free(ppd->ttrasterizer);
  free(ppd->manufacturer);
  free(ppd->product);
  free(ppd->nickname);
  free(ppd->shortnickname);

 /*
  * Free any emulations...
  */

  if (ppd->num_emulations > 0)
  {
    for (i = ppd->num_emulations, emul = ppd->emulations; i > 0; i --, emul ++)
    {
      free(emul->start);
      free(emul->stop);
    };

    free(ppd->emulations);
  };

 /*
  * Free any JCLs...
  */

  if (ppd->num_jcls > 0)
  {
    for (i = ppd->num_jcls, option = ppd->jcls; i > 0; i --, option ++)
      ppd_free_option(option);

    free(ppd->jcls);
  };

 /*
  * Free any UI groups, subgroups, and options...
  */

  if (ppd->num_groups > 0)
  {
    for (i = ppd->num_groups, group = ppd->groups; i > 0; i --, group ++)
      ppd_free_group(group);

    free(ppd->groups);
  };

 /*
  * Free any Non-UI options...
  */

  if (ppd->num_nonuis > 0)
  {
    for (i = ppd->num_nonuis, option = ppd->nonuis; i > 0; i --, option ++)
      ppd_free_option(option);

    free(ppd->nonuis);
  };

 /*
  * Free any page sizes...
  */

  if (ppd->num_sizes > 0)
    free(ppd->sizes);

 /*
  * Free any constraints...
  */

  if (ppd->num_consts > 0)
    free(ppd->consts);

 /*
  * Free any fonts...
  */

  if (ppd->num_fonts > 0)
  {
    for (i = ppd->num_fonts, font = ppd->fonts; i > 0; i --, font ++)
      free(*font);

    free(ppd->fonts);
  };

 /*
  * Free the whole record...
  */

  free(ppd);
}


/*
 * 'ppd_free_group()' - Free a single UI group.
 */

static void
ppd_free_group(ppd_group_t *group)	/* I - Group to free */
{
  int		i;		/* Looping var */
  ppd_option_t	*option;	/* Current option */
  ppd_group_t	*subgroup;	/* Current sub-group */


  if (group->num_options > 0)
  {
    for (i = group->num_options, option = group->options;
         i > 0;
	 i --, option ++)
      ppd_free_option(option);

    free(group->options);
  };

  if (group->num_subgroups > 0)
  {
    for (i = group->num_subgroups, subgroup = group->subgroups;
         i > 0;
	 i --, subgroup ++)
      ppd_free_group(subgroup);

    free(group->subgroups);
  };
}


/*
 * 'ppd_free_option()' - Free a single option.
 */

static void
ppd_free_option(ppd_option_t *option)	/* I - Option to free */
{
  int		i;		/* Looping var */
  ppd_choice_t	*choice;	/* Current choice */


  if (option->num_choices > 0)
  {
    for (i = option->num_choices, choice = option->choices;
         i > 0;
         i --, choice ++)
      free(choice->code);

    free(option->choices);
  };
}


/*
 * 'ppdOpen()' - Read a PPD file into memory.
 */

ppd_file_t *			/* O - PPD file record */
ppdOpen(FILE *fp)		/* I - File to read from */
{
  int		i;		/* Looping var */
  ppd_file_t	*ppd;		/* PPD file record */
  ppd_group_t	*group,		/* Current group */
		*subgroup;	/* Current sub-group */
  ppd_option_t	*option;	/* Current option */
  ppd_choice_t	*choice;	/* Current choice */
  ppd_const_t	*constraint;	/* Current constraint */
  ppd_size_t	*size;		/* Current page size */
  int		mask;		/* Line data mask */
  char		keyword[41],	/* Keyword from file */
		name[41];	/* Option from file */
  unsigned char	text[81],	/* Human-readable text from file */
		*string;	/* Code/text from file */
  float		order;		/* Order dependency number */
  ppd_section_t	section;	/* Order dependency section */


 /*
  * Range check input...
  */

  if (fp == NULL)
    return (NULL);

 /*
  * Grab the first line and make sure it reads '*PPD-Adobe: "major.minor"'...
  */

  mask = ppd_read(fp, keyword, name, text, &string);

  if (mask == 0 ||
      strcmp(keyword, "PPD-Adobe") != 0 ||
      string == NULL || string[0] != '4')
  {
   /*
    * Either this is not a PPD file, or it is not a 4.x PPD file.
    */

    if (string != NULL)
      free(string);

    return (NULL);
  };

  if (string != NULL)
    free(string);

 /*
  * Allocate memory for the PPD file record...
  */

  if ((ppd = calloc(sizeof(ppd_file_t), 1)) == NULL)
    return (NULL);

  ppd->language_level = 1;
  ppd->color_device   = 0;
  ppd->colorspace     = PPD_CS_GRAY;
  ppd->landscape      = 90;

 /*
  * Read lines from the PPD file and add them to the file record...
  */

  group    = NULL;
  subgroup = NULL;
  option   = NULL;
  choice   = NULL;

  while ((mask = ppd_read(fp, keyword, name, text, &string)) != 0)
  {
#ifdef DEBUG
    printf("mask = %x, keyword = \"%s\"", mask, keyword);

    if (name[0] != '\0')
      printf(", name = \"%s\"", name);

    if (text[0] != '\0')
      printf(", text = \"%s\"", text);

    if (string != NULL)
    {
      if (strlen(string) > 40)
        printf(", string = %08x", string);
      else
        printf(", string = \"%s\"", string);
    };

    puts("");
#endif /* DEBUG */

    if (strcmp(keyword, "LanguageLevel") == 0)
      ppd->language_level = atoi(string);
    else if (strcmp(keyword, "LanguageEncoding") == 0)
    {
      ppd->lang_encoding = string;
      string = NULL;			/* Don't free this string below */
    }
    else if (strcmp(keyword, "LanguageVersion") == 0)
    {
      ppd->lang_version = string;
      string = NULL;			/* Don't free this string below */
    }
    else if (strcmp(keyword, "Manufacturer") == 0)
    {
      ppd->manufacturer = string;
      string = NULL;			/* Don't free this string below */
    }
    else if (strcmp(keyword, "ModelName") == 0)
    {
      ppd->modelname = string;
      string = NULL;			/* Don't free this string below */
    }
    else if (strcmp(keyword, "NickName") == 0)
    {
      ppd->nickname = string;
      string = NULL;			/* Don't free this string below */
    }
    else if (strcmp(keyword, "Product") == 0)
    {
      ppd->product = string;
      string = NULL;			/* Don't free this string below */
    }
    else if (strcmp(keyword, "ShortNickName") == 0)
    {
      ppd->shortnickname = string;
      string = NULL;			/* Don't free this string below */
    }
    else if (strcmp(keyword, "TTRasterizer") == 0)
    {
      ppd->ttrasterizer = string;
      string = NULL;			/* Don't free this string below */
    }
    else if (strcmp(keyword, "JCLBegin") == 0)
    {
      ppd_decode(string);		/* Decode quoted string */
      ppd->jcl_begin = string;
      string = NULL;			/* Don't free this string below */
    }
    else if (strcmp(keyword, "JCLEnd") == 0)
    {
      ppd_decode(string);		/* Decode quoted string */
      ppd->jcl_end = string;
      string = NULL;			/* Don't free this string below */
    }
    else if (strcmp(keyword, "JCLToPSInterpreter") == 0)
    {
      ppd_decode(string);		/* Decode quoted string */
      ppd->jcl_ps = string;
      string = NULL;			/* Don't free this string below */
    }
    else if (strcmp(keyword, "AccurateScreensSupport") == 0)
      ppd->accurate_screens = strcmp(string, "True") == 0;
    else if (strcmp(keyword, "ColorDevice") == 0)
      ppd->color_device = strcmp(string, "True") == 0;
    else if (strcmp(keyword, "ContoneOnly") == 0)
      ppd->contone_only = strcmp(string, "True") == 0;
    else if (strcmp(keyword, "DefaultColorSpace") == 0)
    {
      if (strcmp(string, "CMY") == 0)
        ppd->colorspace = PPD_CS_CMY;
      else if (strcmp(string, "CMYK") == 0)
        ppd->colorspace = PPD_CS_CMYK;
      else if (strcmp(string, "RGB") == 0)
        ppd->colorspace = PPD_CS_RGB;
      else
        ppd->colorspace = PPD_CS_GRAY;
    }
    else if (strcmp(keyword, "LandscapeOrientation") == 0)
    {
      if (strcmp(string, "Minus90") == 0)
        ppd->landscape = -90;
      else
        ppd->landscape = 90;
    }
    else if (strcmp(keyword, "OpenUI") == 0)
    {
     /*
      * Add an option record to the current sub-group, group, or file...
      */

      if (name[0] == '*')
        strcpy(name, name + 1);

      if (string == NULL)
      {
        ppdClose(ppd);
	return (NULL);
      };

      if (subgroup != NULL)
      {
        if (subgroup->num_options == 0)
	  option = malloc(sizeof(ppd_option_t));
	else
	  option = realloc(subgroup->options,
	                   (subgroup->num_options + 1) * sizeof(ppd_option_t));

        if (option == NULL)
	{
	  ppdClose(ppd);
	  free(string);
	  return (NULL);
	};

        subgroup->options = option;
	option += subgroup->num_options;
	subgroup->num_options ++;
      }
      else if (group != NULL)
      {
        if (group->num_options == 0)
	  option = malloc(sizeof(ppd_option_t));
	else
	  option = realloc(group->options,
	                   (group->num_options + 1) * sizeof(ppd_option_t));

        if (option == NULL)
	{
	  ppdClose(ppd);
	  free(string);
	  return (NULL);
	};

        group->options = option;
	option += group->num_options;
	group->num_options ++;
      }
      else if (strcmp(name, "PageSize") != 0 &&
               strcmp(name, "PageRegion") != 0 &&
               strcmp(name, "InputSlot") != 0 &&
               strcmp(name, "ManualFeed") != 0 &&
               strcmp(name, "MediaType") != 0 &&
               strcmp(name, "MediaColor") != 0 &&
               strcmp(name, "MediaWeight") != 0)
      {
        for (i = ppd->num_groups, group = ppd->groups; i > 0; i --, group ++)
	  if (strcmp(group->text, "Printer") == 0)
	    break;

	if (i <= 0)
	{
	  if (ppd->num_groups == 0)
	    group = malloc(sizeof(ppd_group_t));
	  else
	    group = realloc(ppd->groups,
	                    (ppd->num_groups + 1) * sizeof(ppd_group_t));

	  if (group == NULL)
	  {
	    ppdClose(ppd);
	    free(string);
	    return (NULL);
	  };

	  ppd->groups = group;
	  group += ppd->num_groups;
	  ppd->num_groups ++;

	  memset(group, 0, sizeof(ppd_group_t));

	  strcpy(group->text, "Printer");
        };

        if (group->num_options == 0)
	  option = malloc(sizeof(ppd_option_t));
	else
	  option = realloc(group->options,
	                   (group->num_options + 1) * sizeof(ppd_option_t));

        if (option == NULL)
	{
	  ppdClose(ppd);
	  free(string);
	  return (NULL);
	};

        group->options = option;
	option += group->num_options;
	group->num_options ++;
        group = NULL;
      }
      else
      {
        if (ppd->num_options == 0)
	  option = malloc(sizeof(ppd_option_t));
	else
	  option = realloc(ppd->options,
	                   (ppd->num_options + 1) * sizeof(ppd_option_t));

        if (option == NULL)
	{
	  ppdClose(ppd);
	  free(string);
	  return (NULL);
	};

        ppd->options = option;
	option += ppd->num_options;
	ppd->num_options ++;
      };

     /*
      * Now fill in the initial information for the option...
      */

      memset(option, 0, sizeof(ppd_option_t));

      if (strcmp(string, "PickMany") == 0)
        option->ui = PPD_UI_PICKMANY;
      else if (strcmp(string, "Boolean") == 0)
        option->ui = PPD_UI_BOOLEAN;
      else
        option->ui = PPD_UI_PICKONE;

      strcpy(option->keyword, name);
      strcpy(option->text, text);

      option->section = PPD_ORDER_ANY;
    }
    else if (strcmp(keyword, "JCLOpenUI") == 0)
    {
     /*
      * Add an option record to the current JCLs...
      */

      if (name[0] == '*')
        strcpy(name, name + 1);

      if (ppd->num_jcls == 0)
	option = malloc(sizeof(ppd_option_t));
      else
	option = realloc(ppd->jcls,
	                 (ppd->num_jcls + 1) * sizeof(ppd_option_t));

      if (option == NULL)
      {
	ppdClose(ppd);
	free(string);
	return (NULL);
      };

      ppd->jcls = option;
      option += ppd->num_jcls;
      ppd->num_jcls ++;

     /*
      * Now fill in the initial information for the option...
      */

      memset(option, 0, sizeof(ppd_option_t));

      if (strcmp(string, "PickMany") == 0)
        option->ui = PPD_UI_PICKMANY;
      else if (strcmp(string, "Boolean") == 0)
        option->ui = PPD_UI_BOOLEAN;
      else
        option->ui = PPD_UI_PICKONE;

      strcpy(option->keyword, name);
      strcpy(option->text, text);

      option->section = PPD_ORDER_JCL;
    }
    else if (strcmp(keyword, "CloseUI") == 0 ||
             strcmp(keyword, "JCLCloseUI") == 0)
      option = NULL;
    else if (strcmp(keyword, "OpenGroup") == 0)
    {
     /*
      * Open a new group...
      */

      if (group != NULL)
      {
        ppdClose(ppd);
	free(string);
	return (NULL);
      };

      if (strchr(string, '/') != NULL)	/* Just show human readable text */
        strcpy(string, strchr(string, '/') + 1);

      if (ppd->num_groups == 0)
	group = malloc(sizeof(ppd_group_t));
      else
	group = realloc(ppd->groups,
	                (ppd->num_groups + 1) * sizeof(ppd_group_t));

      if (group == NULL)
      {
	ppdClose(ppd);
	free(string);
	return (NULL);
      };

      ppd->groups = group;
      group += ppd->num_groups;
      ppd->num_groups ++;

      memset(group, 0, sizeof(ppd_group_t));

      strcpy(group->text, string);
    }
    else if (strcmp(keyword, "CloseGroup") == 0)
      group = NULL;
    else if (strcmp(keyword, "OpenSubGroup") == 0)
    {
     /*
      * Open a new sub-group...
      */

      if (group == NULL || subgroup != NULL)
      {
        ppdClose(ppd);
	free(string);
	return (NULL);
      };

      if (group->num_subgroups == 0)
	subgroup = malloc(sizeof(ppd_group_t));
      else
	subgroup = realloc(group->subgroups,
	                   (group->num_subgroups + 1) * sizeof(ppd_group_t));

      if (subgroup == NULL)
      {
	ppdClose(ppd);
	free(string);
	return (NULL);
      };

      group->subgroups = subgroup;
      subgroup += group->num_subgroups;
      group->num_subgroups ++;

      memset(subgroup, 0, sizeof(ppd_group_t));

      strcpy(subgroup->text, string);
    }
    else if (strcmp(keyword, "CloseSubGroup") == 0)
      subgroup = NULL;
    else if (strcmp(keyword, "OrderDependency") == 0 ||
             strcmp(keyword, "NonUIOrderDependency") == 0)
    {
      if (sscanf(string, "%f%s%s", &order, name, keyword) != 3)
      {
        ppdClose(ppd);
	free(string);
	return (NULL);
      };

      if (keyword[0] == '*')
        strcpy(keyword, keyword + 1);

      if (strcmp(name, "ExitServer") == 0)
        section = PPD_ORDER_EXIT;
      else if (strcmp(name, "Prolog") == 0)
        section = PPD_ORDER_PROLOG;
      else if (strcmp(name, "DocumentSetup") == 0)
        section = PPD_ORDER_DOCUMENT;
      else if (strcmp(name, "PageSetup") == 0)
        section = PPD_ORDER_PAGE;
      else if (strcmp(name, "JCLSetup") == 0)
        section = PPD_ORDER_JCL;
      else
        section = PPD_ORDER_ANY;

      if (option == NULL)
      {
       /*
        * Only valid for Non-UI options...
	*/

        for (i = 0; i < ppd->num_nonuis; i ++)
	  if (strcmp(keyword, ppd->nonuis[i].keyword) == 0)
	  {
	    ppd->nonuis[i].section = section;
	    ppd->nonuis[i].order   = order;
	    break;
	  };
      }
      else
      {
        option->section = section;
	option->order   = order;
      };
    }
    else if (strncmp(keyword, "Default", 7) == 0)
    {
      if (option == NULL)
      {
       /*
        * Only valid for Non-UI options...
	*/

        for (i = 0; i < ppd->num_nonuis; i ++)
	  if (strcmp(keyword + 7, ppd->nonuis[i].keyword) == 0)
	  {
	    strcpy(ppd->nonuis[i].defchoice, string);
	    break;
	  };
      }
      else
        strcpy(option->defchoice, string);
    }
    else if (strcmp(keyword, "UIConstraints") == 0 ||
             strcmp(keyword, "NonUIConstraints") == 0)
    {
      if (ppd->num_consts == 0)
	constraint = calloc(sizeof(ppd_const_t), 1);
      else
	constraint = realloc(ppd->consts,
	                     (ppd->num_consts + 1) * sizeof(ppd_const_t));

      if (constraint == NULL)
      {
	ppdClose(ppd);
	free(string);
	return (NULL);
      };

      ppd->consts = constraint;
      constraint += ppd->num_consts;
      ppd->num_consts ++;

      switch (sscanf(string, "%s%s%s%s", constraint->keyword1,
                     constraint->option1, constraint->keyword2,
		     constraint->option2))
      {
        case 0 : /* Error */
	case 1 : /* Error */
	    ppdClose(ppd);
  	    free(string);
	    break;

	case 2 : /* Two keywords... */
	    if (constraint->keyword1[0] == '*')
	      strcpy(constraint->keyword1, constraint->keyword1 + 1);

	    if (constraint->option1[0] == '*')
	      strcpy(constraint->keyword2, constraint->option1 + 1);
	    else
	      strcpy(constraint->keyword2, constraint->option1);

            constraint->option1[0] = '\0';
            constraint->option2[0] = '\0';
	    break;
	    
	case 3 : /* Two keywords, one option... */
	    if (constraint->keyword1[0] == '*')
	      strcpy(constraint->keyword1, constraint->keyword1 + 1);

	    if (constraint->option1[0] == '*')
	    {
	      strcpy(constraint->keyword2, constraint->option1 + 1);
              constraint->option1[0] = '\0';
	    }
	    else if (constraint->keyword2[0] == '*')
  	      strcpy(constraint->keyword2, constraint->keyword2 + 1);

            constraint->option2[0] = '\0';
	    break;
	    
	case 4 : /* Two keywords, two options... */
	    if (constraint->keyword1[0] == '*')
	      strcpy(constraint->keyword1, constraint->keyword1 + 1);

	    if (constraint->keyword2[0] == '*')
  	      strcpy(constraint->keyword2, constraint->keyword2 + 1);
	    break;
      };
    }
    else if (strcmp(keyword, "PaperDimension") == 0)
    {
      if ((size = ppdPageSize(ppd, name)) != NULL)
        sscanf(string, "%f%f", &(size->width), &(size->length));
    }
    else if (strcmp(keyword, "ImageableArea") == 0)
    {
      if ((size = ppdPageSize(ppd, name)) != NULL)
	sscanf(string, "%f%f%f%f", &(size->left), &(size->bottom),
	       &(size->right), &(size->top));
    }
    else if (option != NULL &&
             (mask & (PPD_KEYWORD | PPD_OPTION | PPD_STRING)) ==
	         (PPD_KEYWORD | PPD_OPTION | PPD_STRING))
    {
      if (strcmp(keyword, "PageSize") == 0)
      {
       /*
        * Add a page size...
	*/

        if (ppd->num_sizes == 0)
	  size = malloc(sizeof(ppd_size_t));
	else
	  size = realloc(ppd->sizes, sizeof(ppd_size_t) * (ppd->num_sizes + 1));

	if (size == NULL)
	{
	  ppdClose(ppd);
	  free(string);
	  return (NULL);
	};

        ppd->sizes = size;
	size += ppd->num_sizes;
	ppd->num_sizes ++;

        memset(size, 0, sizeof(ppd_size_t));

	strcpy(size->name, name);
      };

     /*
      * Add the option choice...
      */

      if (option->num_choices == 0)
	choice = malloc(sizeof(ppd_choice_t));
      else
	choice = realloc(option->choices,
	                 sizeof(ppd_choice_t) * (option->num_choices + 1));

      if (choice == NULL)
      {
	ppdClose(ppd);
	free(string);
	return (NULL);
      };

      option->choices = choice;
      choice += option->num_choices;
      option->num_choices ++;

      memset(choice, 0, sizeof(ppd_choice_t));

      strcpy(choice->option, name);

      if (text[0] != '\0')
        strcpy(choice->text, text);
      else if (strcmp(name, "True") == 0)
        strcpy(choice->text, "Yes");
      else if (strcmp(name, "False") == 0)
        strcpy(choice->text, "No");
      else
        strcpy(choice->text, name);

      if (strncmp(keyword, "JCL", 3) == 0)
        ppd_decode(string);		/* Decode quoted string */

      choice->code = string;
      string = NULL;			/* Don't free this string below */
    };

    if (string != NULL)
      free(string);
  };

#ifdef DEBUG
  if (!feof(fp))
    printf("Premature EOF at %d...\n", ftell(fp));
#endif /* DEBUG */

  return (ppd);
}


/*
 * 'ppdOpenFd()' - Read a PPD file into memory.
 */

ppd_file_t *			/* O - PPD file record */
ppdOpenFd(int fd)		/* I - File to read from */
{
  FILE		*fp;		/* File pointer */
  ppd_file_t	*ppd;		/* PPD file record */


 /*
  * Range check input...
  */

  if (fd < 0)
    return (NULL);

 /*
  * Try to open the file and parse it...
  */

  if ((fp = fdopen(fd, "r")) != NULL)
  {
    setbuf(fp, NULL);

    ppd = ppdOpen(fp);

    free(fp);
  }
  else
    ppd = NULL;

  return (ppd);
}


/*
 * 'ppdOpenFile()' - Read a PPD file into memory.
 */

ppd_file_t *			/* O - PPD file record */
ppdOpenFile(char *filename)	/* I - File to read from */
{
  FILE		*fp;		/* File pointer */
  ppd_file_t	*ppd;		/* PPD file record */


 /*
  * Range check input...
  */

  if (filename == NULL)
    return (NULL);

 /*
  * Try to open the file and parse it...
  */

  if ((fp = fopen(filename, "r")) != NULL)
  {
    ppd = ppdOpen(fp);

    fclose(fp);
  }
  else
    ppd = NULL;

  return (ppd);
}


/*
 * 'ppdOpenRead()' - Read a PPD file using the given line reading function.
 */

ppd_file_t *
ppdOpenRead(char *(*readfunc)(char *,int,void*),
            void *data)
{
}


/*
 * 'ppd_read()' - Read a line from a PPD file, skipping comment lines as
 *                necessary.
 */

static int			/* O - Bitmask of fields read */
ppd_read(FILE          *fp,	/* I - File to read from */
         char          *keyword,/* O - Keyword from line */
	 char          *option,	/* O - Option from line */
         unsigned char *text,	/* O - Human-readable text from line */
	 unsigned char **string)/* O - Code/string data */
{
  int		ch,		/* Character from file */
		endquote,	/* Waiting for an end quote */
		mask;		/* Mask to be returned */
  char		*keyptr,	/* Keyword pointer */
		*optptr;	/* Option pointer */
  unsigned char	*textptr,	/* Text pointer */
		*strptr;	/* Pointer into string */
  unsigned char	*lineptr,	/* Current position in line buffer */
		line[262144];	/* Line buffer (256k) */


 /*
  * Range check everything...
  */

  if (fp == NULL || keyword == NULL || option == NULL || text == NULL ||
      string == NULL)
    return (0);

 /*
  * Now loop until we have a valid line...
  */

  do
  {
   /*
    * Read the line...
    */

    lineptr  = line;
    endquote = 0;

    while ((ch = getc(fp)) != EOF &&
           (lineptr - line) < (sizeof(line) - 1))
    {
      if (ch == '\r' || ch == '\n')
      {
       /*
	* Line feed or carriage return...
	*/

	if (lineptr == line)		/* Skip blank lines */
          continue;

	if (ch == '\r')
	{
	 /*
          * Check for a trailing line feed...
	  */

	  if ((ch = getc(fp)) == EOF)
	    break;
	  if (ch != 0x0a)
	    ungetc(ch, fp);
	};

	*lineptr++ = '\n';

	if (!endquote)			/* Continue for multi-line text */
          break;
      }
      else
      {
       /*
	* Any other character...
	*/

	*lineptr++ = ch;

	if (ch == '\"')
          endquote = !endquote;
      };
    };

    if (lineptr > line && lineptr[-1] == '\n')
      lineptr --;

    *lineptr = '\0';

    if (ch == EOF && lineptr == line)
      return (0);

   /*
    * Now parse it...
    */

    mask    = 0;
    lineptr = line + 1;

    keyword[0] = '\0';
    option[0]  = '\0';
    text[0]    = '\0';
    *string    = NULL;

    if (line[0] != '*')			/* All lines start with an asterisk */
      continue;

    if (strncmp(line, "*%", 2) == 0 ||	/* Comment line */
        strncmp(line, "*?", 2) == 0 ||	/* Query line */
        strcmp(line, "*End") == 0)	/* End of multi-line string */
      continue;

   /*
    * Get a keyword...
    */

    keyptr = keyword;

    while (*lineptr != '\0' && *lineptr != ':' && !isspace(*lineptr) &&
	   (keyptr - keyword) < 40)
      *keyptr++ = *lineptr++;

    *keyptr = '\0';
    mask |= PPD_KEYWORD;

    if (*lineptr == ' ' || *lineptr == '\t')
    {
     /*
      * Get an option name...
      */

      while (*lineptr == ' ' || *lineptr == '\t')
        lineptr ++;

      optptr = option;

      while (*lineptr != '\0' && *lineptr != '\n' && *lineptr != ':' &&
             *lineptr != '/' && (optptr - option) < 40)
	*optptr++ = *lineptr++;

      *optptr = '\0';
      mask |= PPD_OPTION;

      if (*lineptr == '/')
      {
       /*
        * Get human-readable text...
	*/

        lineptr ++;
	
	textptr = text;

	while (*lineptr != '\0' && *lineptr != '\n' && *lineptr != ':' &&
               (textptr - text) < 80)
	  *textptr++ = *lineptr++;

	*textptr = '\0';
	ppd_decode(text);

	mask |= PPD_TEXT;
      };
    };

    if (*lineptr == ':')
    {
     /*
      * Get string...
      */

      *string = malloc(strlen(lineptr));

      while (*lineptr == ':' || isspace(*lineptr))
        lineptr ++;

      strptr = *string;

      while (*lineptr != '\0')
      {
	if (*lineptr != '\"')
	  *strptr++ = *lineptr++;
	else
	  lineptr ++;
      };

      *strptr = '\0';
      mask |= PPD_STRING;
    };
  }
  while (mask == 0);

  return (mask);
}


/*
 * 'ppd_decode()' - Decode a string value...
 */

static void
ppd_decode(unsigned char *string)	/* I - String to decode */
{
  unsigned char	*inptr,			/* Input pointer */
		*outptr;		/* Output pointer */


  inptr  = string;
  outptr = string;

  while (*inptr != '\0')
    if (*inptr == '<' && isxdigit(inptr[1]))
    {
     /*
      * Convert hex to 8-bit values...
      */

      inptr ++;
      while (isxdigit(*inptr))
      {
	if (isalpha(*inptr))
	  *outptr = (tolower(*inptr) - 'a' + 10) << 16;
	else
	  *outptr = (*inptr - '0') << 16;

	inptr ++;

	if (isalpha(*inptr))
	  *outptr |= tolower(*inptr) - 'a' + 10;
	else
	  *outptr |= *inptr - '0';

	inptr ++;
	outptr ++;
      };

      while (*inptr != '>' && *inptr != '\0')
	inptr ++;
      while (*inptr == '>')
	inptr ++;
    }
    else
      *outptr++ = *inptr++;

  *outptr = '\0';
}




/*
 * End of "$Id: ppd.c,v 1.3 1999/01/24 14:18:43 mike Exp $".
 */
