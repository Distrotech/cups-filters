/*
 * "$Id: usersys.c,v 1.8 2000/09/07 20:42:23 mike Exp $"
 *
 *   User, system, and password routines for the Common UNIX Printing
 *   System (CUPS).
 *
 *   Copyright 1997-2000 by Easy Software Products.
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
 *   cupsGetPassword()   - Get a password from the user...
 *   cupsServer()        - Return the hostname of the default server...
 *   cupsSetPasswordCB() - Set the password callback for CUPS.
 *   cupsSetServer()     - Set the default server name...
 *   cupsSetUser()       - Set the default user name...
 *   cupsUser()          - Return the current users name.
 *   cups_get_password() - Get a password from the user...
 */

/*
 * Include necessary headers...
 */

#include "cups.h"
#include "string.h"
#include <stdlib.h>
#include <ctype.h>


/*
 * Local functions...
 */

static const char	*cups_get_password(const char *prompt);


/*
 * Local globals...
 */

static char		cups_user[65] = "",
			cups_server[256] = "";
static const char	*(*cups_pwdcb)(const char *) = cups_get_password;


/*
 * 'cupsGetPassword()' - Get a password from the user...
 */

const char *				/* O - Password */
cupsGetPassword(const char *prompt)	/* I - Prompt string */
{
  return ((*cups_pwdcb)(prompt));
}


/*
 * 'cupsServer()' - Return the hostname of the default server...
 */

const char *				/* O - Server name */
cupsServer(void)
{
  FILE		*fp;			/* client.conf file */
  char		*server;		/* Pointer to server name */
  const char	*home;			/* Home directory of user */
  static char	line[1024];		/* Line from file */


 /*
  * First see if we have already set the server name...
  */

  if (!cups_server[0])
  {
   /*
    * Then see if the CUPS_SERVER environment variable is set...
    */

    if ((server = getenv("CUPS_SERVER")) == NULL)
    {
     /*
      * Next check to see if we have a $HOME/.cupsrc or client.conf file...
      */

      if ((home = getenv("HOME")) != NULL)
      {
	snprintf(line, sizeof(line), "%s/.cupsrc", home);
	fp = fopen(line, "r");
      }
      else
	fp = NULL;

      if (fp == NULL)
      {
	if ((home = getenv("CUPS_SERVERROOT")) != NULL)
	{
	  snprintf(line, sizeof(line), "%s/client.conf", home);
	  fp = fopen(line, "r");
	}
	else
	  fp = fopen(CUPS_SERVERROOT "/client.conf", "r");
      }

      server = "localhost";

      if (fp != NULL)
      {
       /*
	* Read the config file and look for a ServerName line...
	*/

	while (fgets(line, sizeof(line), fp) != NULL)
	  if (strncmp(line, "ServerName ", 11) == 0)
	  {
	   /*
	    * Got it!  Drop any trailing newline and find the name...
	    */

	    server = line + strlen(line) - 1;
	    if (*server == '\n')
              *server = '\0';

	    for (server = line + 11; isspace(*server); server ++);
	    break;
	  }

	fclose(fp);
      }
    }

   /*
    * Copy the server name over...
    */

    strncpy(cups_server, server, sizeof(cups_server) - 1);
    cups_server[sizeof(cups_server) - 1] = '\0';
  }

  return (cups_server);
}


/*
 * 'cupsSetPasswordCB()' - Set the password callback for CUPS.
 */

void
cupsSetPasswordCB(const char *(*cb)(const char *))	/* I - Callback function */
{
  if (cb == (const char *(*)(const char *))0)
    cups_pwdcb = cups_get_password;
  else
    cups_pwdcb = cb;
}


/*
 * 'cupsSetServer()' - Set the default server name...
 */

void
cupsSetServer(const char *server)	/* I - Server name */
{
  if (server)
  {
    strncpy(cups_server, server, sizeof(cups_server) - 1);
    cups_server[sizeof(cups_server) - 1] = '\0';
  }
  else
    cups_server[0] = '\0';
}


/*
 * 'cupsSetUser()' - Set the default user name...
 */

void
cupsSetUser(const char *user)		/* I - User name */
{
  if (user)
  {
    strncpy(cups_user, user, sizeof(cups_user) - 1);
    cups_user[sizeof(cups_user) - 1] = '\0';
  }
  else
    cups_user[0] = '\0';
}


#if defined(WIN32) || defined(__EMX__)
/*
 * WIN32 and OS/2 username and password stuff...
 */

/*
 * 'cupsUser()' - Return the current user's name.
 */

const char *				/* O - User name */
cupsUser(void)
{
  if (!cups_user[0])
    strcpy(cups_user, "WindowsUser");

  return (cups_user);
}


/*
 * 'cups_get_password()' - Get a password from the user...
 */

static const char *			/* O - Password */
cups_get_password(const char *prompt)	/* I - Prompt string */
{
  return (NULL);
}
#else
/*
 * UNIX username and password stuff...
 */

#  include <pwd.h>

/*
 * 'cupsUser()' - Return the current user's name.
 */

const char *				/* O - User name */
cupsUser(void)
{
  struct passwd	*pwd;			/* User/password entry */


  if (!cups_user[0])
  {
   /*
    * Rewind the password file...
    */

    setpwent();

   /*
    * Lookup the password entry for the current user.
    */

    if ((pwd = getpwuid(getuid())) == NULL)
      strcpy(cups_user, "unknown");	/* Unknown user! */
    else
    {
     /*
      * Rewind the password file again and copy the username...
      */

      setpwent();

      strncpy(cups_user, pwd->pw_name, sizeof(cups_user) - 1);
      cups_user[sizeof(cups_user) - 1] = '\0';
    }
  }

  return (cups_user);
}


/*
 * 'cups_get_password()' - Get a password from the user...
 */

static const char *			/* O - Password */
cups_get_password(const char *prompt)	/* I - Prompt string */
{
  return (getpass(prompt));
}
#endif /* WIN32 || __EMX__ */


/*
 * End of "$Id: usersys.c,v 1.8 2000/09/07 20:42:23 mike Exp $".
 */
