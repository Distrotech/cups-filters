/*
 * "$Id$"
 *
 *   User, system, and password routines for the Common UNIX Printing
 *   System (CUPS).
 *
 *   Copyright 1997-2005 by Easy Software Products.
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
 *   This file is subject to the Apple OS-Developed Software exception.
 *
 * Contents:
 *
 *   cupsEncryption()    - Get the default encryption settings...
 *   cupsGetPassword()   - Get a password from the user...
 *   cupsServer()        - Return the hostname of the default server...
 *   cupsSetEncryption() - Set the encryption preference.
 *   cupsSetPasswordCB() - Set the password callback for CUPS.
 *   cupsSetServer()     - Set the default server name...
 *   cupsSetUser()       - Set the default user name...
 *   cupsUser()          - Return the current users name.
 *   _cupsGetPassword()  - Get a password from the user...
 */

/*
 * Include necessary headers...
 */

#include "http-private.h"
#include "globals.h"
#include <stdlib.h>
#ifdef WIN32
#  include <windows.h>
#endif /* WIN32 */


/*
 * 'cupsEncryption()' - Get the default encryption settings...
 */

http_encryption_t
cupsEncryption(void)
{
  cups_file_t	*fp;			/* client.conf file */
  char		*encryption;		/* CUPS_ENCRYPTION variable */
  const char	*home;			/* Home directory of user */
  char		line[1024],		/* Line from file */
		*value;			/* Value on line */
  int		linenum;		/* Line number */
  cups_globals_t *cg = _cupsGlobals();	/* Pointer to library globals */


 /*
  * First see if we have already set the encryption stuff...
  */

  if (cg->encryption == (http_encryption_t)-1)
  {
   /*
    * Then see if the CUPS_ENCRYPTION environment variable is set...
    */

    if ((encryption = getenv("CUPS_ENCRYPTION")) == NULL)
    {
     /*
      * Next check to see if we have a $HOME/.cupsrc or client.conf file...
      */

      if ((home = getenv("HOME")) != NULL)
      {
	snprintf(line, sizeof(line), "%s/.cupsrc", home);
	fp = cupsFileOpen(line, "r");
      }
      else
	fp = NULL;

      if (fp == NULL)
      {
	if ((home = getenv("CUPS_SERVERROOT")) != NULL)
	{
	  snprintf(line, sizeof(line), "%s/client.conf", home);
	  fp = cupsFileOpen(line, "r");
	}
	else
	  fp = cupsFileOpen(CUPS_SERVERROOT "/client.conf", "r");
      }

      encryption = "IfRequested";

      if (fp != NULL)
      {
       /*
	* Read the config file and look for an Encryption line...
	*/

        linenum = 0;

	while (cupsFileGetConf(fp, line, sizeof(line), &value, &linenum) != NULL)
	  if (!strcasecmp(line, "Encryption") && value)
	  {
	   /*
	    * Got it!
	    */

	    encryption = value;
	    break;
	  }

	cupsFileClose(fp);
      }
    }

   /*
    * Set the encryption preference...
    */

    if (!strcasecmp(encryption, "never"))
      cg->encryption = HTTP_ENCRYPT_NEVER;
    else if (!strcasecmp(encryption, "always"))
      cg->encryption = HTTP_ENCRYPT_ALWAYS;
    else if (!strcasecmp(encryption, "required"))
      cg->encryption = HTTP_ENCRYPT_REQUIRED;
    else
      cg->encryption = HTTP_ENCRYPT_IF_REQUESTED;
  }

  return (cg->encryption);
}


/*
 * 'cupsGetPassword()' - Get a password from the user...
 */

const char *				/* O - Password */
cupsGetPassword(const char *prompt)	/* I - Prompt string */
{
  return ((*_cupsGlobals()->password_cb)(prompt));
}


/*
 * 'cupsSetEncryption()' - Set the encryption preference.
 */

void
cupsSetEncryption(http_encryption_t e)	/* I - New encryption preference */
{
  _cupsGlobals()->encryption = e;
}


/*
 * 'cupsServer()' - Return the hostname of the default server...
 */

const char *				/* O - Server name */
cupsServer(void)
{
  cups_file_t	*fp;			/* client.conf file */
  char		*server;		/* Pointer to server name */
  const char	*home;			/* Home directory of user */
  char		*port;			/* Port number */
  char		line[1024],		/* Line from file */
		*value;			/* Value on line */
  int		linenum;		/* Line number in file */
  cups_globals_t *cg = _cupsGlobals();	/* Pointer to library globals */


 /*
  * First see if we have already set the server name...
  */

  if (!cg->server[0])
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
	fp = cupsFileOpen(line, "r");
      }
      else
	fp = NULL;

      if (fp == NULL)
      {
	if ((home = getenv("CUPS_SERVERROOT")) != NULL)
	{
	  snprintf(line, sizeof(line), "%s/client.conf", home);
	  fp = cupsFileOpen(line, "r");
	}
	else
	  fp = cupsFileOpen(CUPS_SERVERROOT "/client.conf", "r");
      }

      server = "localhost";

      if (fp != NULL)
      {
       /*
	* Read the config file and look for a ServerName line...
	*/

        linenum = 0;
	while (cupsFileGetConf(fp, line, sizeof(line), &value, &linenum) != NULL)
	  if (!strcasecmp(line, "ServerName") && value)
	  {
	   /*
	    * Got it!
	    */

	    server = value;
	    break;
	  }

	cupsFileClose(fp);
      }
    }

   /*
    * Copy the server name over and set the port number, if any...
    */

    strlcpy(cg->server, server, sizeof(cg->server));

    if (cg->server[0] != '/' && (port = strrchr(cg->server, ':')) != NULL &&
        isdigit(port[1] & 255))
    {
      *port++ = '\0';

      ippSetPort(atoi(port));
    }
  }

  return (cg->server);
}


/*
 * 'cupsSetPasswordCB()' - Set the password callback for CUPS.
 */

void
cupsSetPasswordCB(const char *(*cb)(const char *))	/* I - Callback function */
{
  cups_globals_t *cg = _cupsGlobals();	/* Pointer to library globals */


  if (cb == (const char *(*)(const char *))0)
    cg->password_cb = _cupsGetPassword;
  else
    cg->password_cb = cb;
}


/*
 * 'cupsSetServer()' - Set the default server name...
 */

void
cupsSetServer(const char *server)	/* I - Server name */
{
  cups_globals_t *cg = _cupsGlobals();	/* Pointer to library globals */


  if (server)
    strlcpy(cg->server, server, sizeof(cg->server));
  else
    cg->server[0] = '\0';
}


/*
 * 'cupsSetUser()' - Set the default user name...
 */

void
cupsSetUser(const char *user)		/* I - User name */
{
  cups_globals_t *cg = _cupsGlobals();	/* Pointer to library globals */


  if (user)
    strlcpy(cg->user, user, sizeof(cg->user));
  else
    cg->user[0] = '\0';
}


#if defined(WIN32)
/*
 * WIN32 username and password stuff...
 */

/*
 * 'cupsUser()' - Return the current user's name.
 */

const char *				/* O - User name */
cupsUser(void)
{
  cups_globals_t *cg = _cupsGlobals();	/* Pointer to library globals */


  if (!cg->user[0])
  {
    DWORD	size;		/* Size of string */


    size = sizeof(cg->user);
    if (!GetUserName(cg->user, &size))
    {
     /*
      * Use the default username...
      */

      strcpy(cg->user, "unknown");
    }
  }

  return (cg->user);
}


/*
 * '_cupsGetPassword()' - Get a password from the user...
 */

const char *				/* O - Password */
_cupsGetPassword(const char *prompt)	/* I - Prompt string */
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
  cups_globals_t *cg = _cupsGlobals();	/* Pointer to library globals */


  if (!cg->user[0])
  {
   /*
    * Rewind the password file...
    */

    setpwent();

   /*
    * Lookup the password entry for the current user.
    */

    if ((pwd = getpwuid(getuid())) == NULL)
      strcpy(cg->user, "unknown");	/* Unknown user! */
    else
    {
     /*
      * Copy the username...
      */

      setpwent();

      strlcpy(cg->user, pwd->pw_name, sizeof(cg->user));
    }

   /*
    * Rewind the password file again...
    */

    setpwent();
  }

  return (cg->user);
}


/*
 * '_cupsGetPassword()' - Get a password from the user...
 */

const char *				/* O - Password */
_cupsGetPassword(const char *prompt)	/* I - Prompt string */
{
  return (getpass(prompt));
}
#endif /* WIN32 */


/*
 * End of "$Id$".
 */
