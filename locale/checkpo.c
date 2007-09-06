/*
 * "$Id$"
 *
 * Verify that translations in the .po file have the same number and type of
 * printf-style format strings.
 *
 * Usage:
 *
 *   checkpo filename.po [... filenameN.po]
 *
 * Compile with:
 *
 *   gcc -o checkpo checkpo.c `cups-config --libs`
 *
 * Contents:
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <cups/string.h>
#include <cups/i18n.h>


/*
 * Local functions...
 */

static char		*abbreviate(const char *s, char *buf, int bufsize);
static cups_array_t	*collect_formats(const char *id);
static void		free_formats(cups_array_t *fmts);


/*
 *   main() - Convert .po file to .strings.
 */

int					/* O - Exit code */
main(int  argc,				/* I - Number of command-line args */
     char *argv[])			/* I - Command-line arguments */
{
  int			i;		/* Looping var */
  cups_array_t		*po;		/* .po file */
  _cups_message_t	*msg;		/* Current message */
  const char		*str;		/* Pointer into msgstr */
  cups_array_t		*fmts;		/* Format strings in msgid */
  char			*fmt;		/* Current format string */
  int			fmtidx,		/* Format index */
			fmtlen,		/* Length of format string */
			fmtcount;	/* Format count */
  int			status,		/* Exit status */
			pass,		/* Pass/fail status */
			untranslated;	/* Untranslated messages */
  char			idbuf[80],	/* Abbreviated msgid */
			strbuf[80];	/* Abbreviated msgstr */


  if (argc < 2)
  {
    puts("Usage: checkpo filename.po [... filenameN.po]");
    return (1);
  }

 /*
  * Check every .po file on the command-line...
  */

  for (i = 1, status = 0; i < argc; i ++)
  {
   /*
    * Use the CUPS .po loader to get the message strings...
    */

    if ((po = _cupsMessageLoad(argv[i])) == NULL)
    {
      perror(argv[i]);
      return (1);
    }

    printf("%s: ", argv[i]);
    fflush(stdout);

   /*
    * Scan every message for a % string and then match them up with
    * the corresponding string in the translation...
    */

    pass         = 1;
    untranslated = 0;

    for (msg = (_cups_message_t *)cupsArrayFirst(po);
         msg;
	 msg = (_cups_message_t *)cupsArrayNext(po))
    {
      if (!msg->str || !msg->str[0])
        untranslated ++;
      else if (strchr(msg->id, '%'))
      {
        fmts     = collect_formats(msg->id);
	fmt      = NULL;
	fmtidx   = 0;
	fmtcount = 0;

        for (str = strchr(msg->str, '%'); str; str = strchr(str, '%'))
	{
	  if (str[1] == '%')
	  {
	    str += 2;
	    continue;
	  }
	  else if (isdigit(str[1] & 255) && str[2] == '$')
	  {
	   /*
	    * Handle positioned format stuff...
	    */

            fmtidx = str[1] - '1';
            str    += 3;
	    fmt    = (char *)cupsArrayIndex(fmts, fmtidx);
            fmtlen = fmt ? strlen(fmt) - 1 : 0;
	  }
	  else
	  {
	   /*
	    * Compare against the current format...
	    */

	    fmt    = (char *)cupsArrayIndex(fmts, fmtidx);
            fmtlen = fmt ? strlen(fmt) : 0;
          }

	  fmtidx ++;

	  if (!fmt || strncmp(str, fmt, fmtlen))
	    break;

          str += fmtlen;
	  fmtcount ++;
	}

        if (fmtcount != cupsArrayCount(fmts))
	{
	  if (pass)
	  {
	    pass = 0;
	    puts("FAIL");
	  }

	  printf("    Bad translation string \"%s\"\n        for \"%s\"\n",
	         abbreviate(msg->str, strbuf, sizeof(strbuf)),
		 abbreviate(msg->id, idbuf, sizeof(idbuf)));
	}

	free_formats(fmts);
      }
    }

    if (pass)
    {
      if ((untranslated * 10) >= cupsArrayCount(po))
      {
       /*
        * Only allow 10% of messages to be untranslated before we fail...
	*/

        puts("FAIL");
	printf("    Too many untranslated messages (%d of %d)\n", untranslated,
	       cupsArrayCount(po));
      }
      else if (untranslated > 0)
        printf("PASS (%d of %d untranslated)\n", untranslated,
	       cupsArrayCount(po));
      else
        puts("PASS");
    }

    _cupsMessageFree(po);
  }

  return (0);
}


/*
 * 'abbreviate()' - Abbreviate a message string as needed.
 */

static char *				/* O - Abbreviated string */
abbreviate(const char *s,		/* I - String to abbreviate */
           char       *buf,		/* I - Buffer */
	   int        bufsize)		/* I - Size of buffer */
{
  char	*bufptr;			/* Pointer into buffer */


  for (bufptr = buf, bufsize -= 4; *s && bufsize > 0; s ++)
  {
    if (*s == '\n')
    {
      if (bufsize < 2)
        break;

      *bufptr++ = '\\';
      *bufptr++ = 'n';
      bufsize -= 2;
    }
    else if (*s == '\t')
    {
      if (bufsize < 2)
        break;

      *bufptr++ = '\\';
      *bufptr++ = 't';
      bufsize -= 2;
    }
    else if (*s >= 0 && *s < ' ')
    {
      if (bufsize < 4)
        break;

      sprintf(bufptr, "\\%03o", *s);
      bufptr += 4;
      bufsize -= 4;
    }
    else
    {
      *bufptr++ = *s;
      bufsize --;
    }
  }

  if (*s)
    strcpy(bufptr, "...");
  else
    *bufptr = '\0';

  return (buf);
}


/*
 * 'collect_formats()' - Collect all of the format strings in the msgid.
 */

static cups_array_t *			/* O - Array of format strings */
collect_formats(const char *id)		/* I - msgid string */
{
  cups_array_t	*fmts;			/* Array of format strings */
  char		buf[255],		/* Format string buffer */
		*bufptr;		/* Pointer into format string */


  fmts = cupsArrayNew(NULL, NULL);

  while ((id = strchr(id, '%')) != NULL)
  {
    if (id[1] == '%')
    {
     /*
      * Skip %%...
      */

      id += 2;
      continue;
    }

    for (bufptr = buf; *id && bufptr < (buf + sizeof(buf) - 1); id ++)
    {
      *bufptr++ = *id;

      if (strchr("CDEFGIOSUXcdeifgopsux", *id))
      {
        id ++;
        break;
      }
    }

    *bufptr = '\0';
    cupsArrayAdd(fmts, strdup(buf));
  }

  return (fmts);
}


/*
 * 'free_formats()' - Free all of the format strings.
 */

static void
free_formats(cups_array_t *fmts)	/* I - Array of format strings */
{
  char	*s;				/* Current string */


  for (s = (char *)cupsArrayFirst(fmts); s; s = (char *)cupsArrayNext(fmts))
    free(s);

  cupsArrayDelete(fmts);
}


/*
 * End of "$Id$".
 */
