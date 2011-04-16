/*
 * "$Id$"
 *
 *   Mini-daemon utility functions for CUPS.
 *
 *   Copyright 2007-2011 by Apple Inc.
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
 *   cupsdCheckProgram()       - Check the permissions of the given program and
 *                               its containing directory.
 *   cupsdCompareNames()       - Compare two names.
 *   cupsdCreateStringsArray() - Create a CUPS array of strings.
 *   cupsdExec()               - Run a program with the correct environment.
 *   cupsdPipeCommand()        - Read output from a command.
 *   cupsdSendIPPGroup()       - Send a group tag.
 *   cupsdSendIPPHeader()      - Send the IPP response header.
 *   cupsdSendIPPInteger()     - Send an integer attribute.
 *   cupsdSendIPPString()      - Send a string attribute.
 *   cupsdSendIPPTrailer()     - Send the end-of-message tag.
 */

/*
 * Include necessary headers...
 */

#include "util.h"
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#ifdef __APPLE__
#  include <libgen.h>
extern char **environ;
#endif /* __APPLE__ */ 


/*
 * 'cupsdCheckProgram()' - Check the permissions of the given program and its
 *                         containing directory.
 *
 * Note: This function is a parallel implementation of the scheduler function
 *       of the same name.
 */

int					/* O - 1 if OK, 0 if not OK */
cupsdCheckProgram(
    const char      *filename)		/* I - Filename to check */
{
  struct stat		fileinfo;	/* File information */
  char			temp[1024],	/* Parent directory filename */
			*ptr;		/* Pointer into parent directory */


 /*
  * Does the program even exist and is it accessible?
  */

  if (stat(filename, &fileinfo))
  {
   /*
    * Nope...
    */

    fprintf(stderr, "ERROR: Program \"%s\" not available: %s", filename,
            strerror(errno));

    return (0);
  }

 /*
  * Are we running as root?
  */

  if (geteuid())
  {
   /*
    * Nope, so anything goes...
    */

    return (1);
  }

 /*
  * Verify permission of the program itself:
  *
  * 1. Must be owned by root
  * 2. Must not be writable by group unless group is root/wheel/admin
  * 3. Must not be setuid
  * 4. Must not be writable by others
  */

  if (fileinfo.st_uid ||		/* 1. Must be owned by root */
#ifdef __APPLE__
      ((fileinfo.st_mode & S_IWGRP) && fileinfo.st_gid &&
       fileinfo.st_gid != 80) ||	/* 2. Must not be writable by group */
#else
      ((fileinfo.st_mode & S_IWGRP) && fileinfo.st_gid) ||
					/* 2. Must not be writable by group */
#endif /* __APPLE__ */
      (fileinfo.st_mode & S_ISUID) ||	/* 3. Must not be setuid */
      (fileinfo.st_mode & S_IWOTH))	/* 4. Must not be writable by others */
  {
    fprintf(stderr,
            "ERROR: Program \"%s\" has insecure permissions "
	    "(0%o/uid=%d/gid=%d).", filename, fileinfo.st_mode,
	    (int)fileinfo.st_uid, (int)fileinfo.st_gid);

    errno = EPERM;

    return (0);
  }

  fprintf(stderr, "DEBUG2: Program \"%s\" permissions OK (0%o/uid=%d/gid=%d).",
          filename, fileinfo.st_mode, (int)fileinfo.st_uid,
	  (int)fileinfo.st_gid);

 /*
  * Now check the containing directory...
  */

  strlcpy(temp, filename, sizeof(temp));
  if ((ptr = strrchr(temp, '/')) != NULL)
  {
    if (ptr == temp)
      ptr[1] = '\0';
    else
      *ptr = '\0';
  }

  if (stat(temp, &fileinfo))
  {
   /*
    * Doesn't exist...
    */

    fprintf(stderr, "ERROR: Program directory \"%s\" not available: %s", temp,
	    strerror(errno));

    return (0);
  }

  if (fileinfo.st_uid ||		/* 1. Must be owned by root */
#ifdef __APPLE__
      ((fileinfo.st_mode & S_IWGRP) && fileinfo.st_gid &&
       fileinfo.st_gid != 80) ||	/* 2. Must not be writable by group */
#else
      ((fileinfo.st_mode & S_IWGRP) && fileinfo.st_gid) ||
					/* 2. Must not be writable by group */
#endif /* __APPLE__ */
      (fileinfo.st_mode & S_ISUID) ||	/* 3. Must not be setuid */
      (fileinfo.st_mode & S_IWOTH))	/* 4. Must not be writable by others */
  {
    fprintf(stderr,
            "ERROR: Program directory \"%s\" has insecure permissions "
	    "(0%o/uid=%d/gid=%d).", temp, fileinfo.st_mode,
	    (int)fileinfo.st_uid, (int)fileinfo.st_gid);

    errno = EPERM;

    return (0);
  }

  fprintf(stderr,
          "DEBUG2: Program directory \"%s\" permissions OK "
	  "(0%o/uid=%d/gid=%d).", temp, fileinfo.st_mode, (int)fileinfo.st_uid,
	  (int)fileinfo.st_gid);

 /*
  * If we get here then we can "safely" run this program...
  */

  return (1);
}


/*
 * 'cupsdCompareNames()' - Compare two names.
 *
 * This function basically does a strcasecmp() of the two strings,
 * but is also aware of numbers so that "a2" < "a100".
 */

int					/* O - Result of comparison */
cupsdCompareNames(const char *s,	/* I - First string */
                  const char *t)	/* I - Second string */
{
  int		diff,			/* Difference between digits */
		digits;			/* Number of digits */


 /*
  * Loop through both names, returning only when a difference is
  * seen.  Also, compare whole numbers rather than just characters, too!
  */

  while (*s && *t)
  {
    if (isdigit(*s & 255) && isdigit(*t & 255))
    {
     /*
      * Got a number; start by skipping leading 0's...
      */

      while (*s == '0')
        s ++;
      while (*t == '0')
        t ++;

     /*
      * Skip equal digits...
      */

      while (isdigit(*s & 255) && *s == *t)
      {
        s ++;
	t ++;
      }

     /*
      * Bounce out if *s and *t aren't both digits...
      */

      if (isdigit(*s & 255) && !isdigit(*t & 255))
        return (1);
      else if (!isdigit(*s & 255) && isdigit(*t & 255))
        return (-1);
      else if (!isdigit(*s & 255) || !isdigit(*t & 255))
        continue;     

      if (*s < *t)
        diff = -1;
      else
        diff = 1;

     /*
      * Figure out how many more digits there are...
      */

      digits = 0;
      s ++;
      t ++;

      while (isdigit(*s & 255))
      {
        digits ++;
	s ++;
      }

      while (isdigit(*t & 255))
      {
        digits --;
	t ++;
      }

     /*
      * Return if the number or value of the digits is different...
      */

      if (digits < 0)
        return (-1);
      else if (digits > 0)
        return (1);
      else if (diff)
        return (diff);
    }
    else if (tolower(*s) < tolower(*t))
      return (-1);
    else if (tolower(*s) > tolower(*t))
      return (1);
    else
    {
      s ++;
      t ++;
    }
  }

 /*
  * Return the results of the final comparison...
  */

  if (*s)
    return (1);
  else if (*t)
    return (-1);
  else
    return (0);
}


/*
 * 'cupsdCreateStringsArray()' - Create a CUPS array of strings.
 */

cups_array_t *				/* O - CUPS array */
cupsdCreateStringsArray(const char *s)	/* I - Comma-delimited strings */
{
  cups_array_t	*a;			/* CUPS array */
  const char	*start,			/* Start of string */
		*end;			/* End of string */
  char		buffer[8192];		/* New string */


  if (!s)
    return (NULL);

  if ((a = cupsArrayNew3((cups_array_func_t)strcmp, NULL,
                         (cups_ahash_func_t)NULL, 0,
			 (cups_acopy_func_t)_cupsStrAlloc,
			 (cups_afree_func_t)_cupsStrFree)) != NULL)
  {
    for (start = end = s; *end; start = end + 1)
    {
     /*
      * Find the end of the current delimited string...
      */

      if ((end = strchr(start, ',')) == NULL)
      {
       /*
        * Last delimited string...
	*/

        cupsArrayAdd(a, (char *)start);
	break;
      }

     /*
      * Copy the string and add it to the array...
      */

      if ((end - start + 1) > sizeof(buffer))
        break;

      memcpy(buffer, start, end - start);
      buffer[end - start] = '\0';

      cupsArrayAdd(a, buffer);
    }
  }

  return (a);
}


/*
 * 'cupsdExec()' - Run a program with the correct environment.
 *
 * On Mac OS X, we need to update the CFProcessPath environment variable that
 * is passed in the environment so the child can access its bundled resources.
 */

int					/* O - exec() status */
cupsdExec(const char *command,		/* I - Full path to program */
          char       **argv)		/* I - Command-line arguments */
{
#ifdef __APPLE__
  int	i, j;				/* Looping vars */
  char	*envp[500],			/* Array of environment variables */
	cfprocesspath[1024],		/* CFProcessPath environment variable */
	linkpath[1024];			/* Link path for symlinks... */
  int	linkbytes;			/* Bytes for link path */


 /*
  * Some Mac OS X programs are bundled and need the CFProcessPath environment
  * variable defined.  If the command is a symlink, resolve the link and point
  * to the resolved location, otherwise, use the command path itself.
  */

  if ((linkbytes = readlink(command, linkpath, sizeof(linkpath) - 1)) > 0)
  {
   /*
    * Yes, this is a symlink to the actual program, nul-terminate and
    * use it...
    */

    linkpath[linkbytes] = '\0';

    if (linkpath[0] == '/')
      snprintf(cfprocesspath, sizeof(cfprocesspath), "CFProcessPath=%s",
	       linkpath);
    else
      snprintf(cfprocesspath, sizeof(cfprocesspath), "CFProcessPath=%s/%s",
	       dirname((char *)command), linkpath);
  }
  else
    snprintf(cfprocesspath, sizeof(cfprocesspath), "CFProcessPath=%s", command);

  envp[0] = cfprocesspath;

 /*
  * Copy the rest of the environment except for any CFProcessPath that may
  * already be there...
  */

  for (i = 1, j = 0;
       environ[j] && i < (int)(sizeof(envp) / sizeof(envp[0]) - 1);
       j ++)
    if (strncmp(environ[j], "CFProcessPath=", 14))
      envp[i ++] = environ[j];

  envp[i] = NULL;

 /*
  * Use execve() to run the program...
  */

  return (execve(command, argv, envp));

#else
 /*
  * On other operating systems, just call execv() to use the same environment
  * variables as the parent...
  */

  return (execv(command, argv));
#endif /* __APPLE__ */
}


/*
 * 'cupsdPipeCommand()' - Read output from a command.
 */

cups_file_t *				/* O - CUPS file or NULL on error */
cupsdPipeCommand(int        *pid,	/* O - Process ID or 0 on error */
                 const char *command,	/* I - Command to run */
                 char       **argv,	/* I - Arguments to pass to command */
		 int        user)	/* I - User to run as or 0 for current */
{
  int	fd,				/* Temporary file descriptor */
	fds[2];				/* Pipe file descriptors */


 /*
  * First create the pipe...
  */

  if (pipe(fds))
  {
    *pid = 0;
    return (NULL);
  }

 /*
  * Set the "close on exec" flag on each end of the pipe...
  */

  if (fcntl(fds[0], F_SETFD, fcntl(fds[0], F_GETFD) | FD_CLOEXEC))
  {
    close(fds[0]);
    close(fds[1]);

    *pid = 0;

    return (NULL);
  }

  if (fcntl(fds[1], F_SETFD, fcntl(fds[1], F_GETFD) | FD_CLOEXEC))
  {
    close(fds[0]);
    close(fds[1]);

    *pid = 0;

    return (NULL);
  }

 /*
  * Then run the command...
  */

  if ((*pid = fork()) < 0)
  {
   /*
    * Unable to fork!
    */

    *pid = 0;
    close(fds[0]);
    close(fds[1]);

    return (NULL);
  }
  else if (!*pid)
  {
   /*
    * Child comes here...
    */

    if (!getuid() && user)
      setuid(user);			/* Run as restricted user */

    if ((fd = open("/dev/null", O_RDONLY)) > 0)
    {
      dup2(fd, 0);			/* </dev/null */
      close(fd);
    }

    dup2(fds[1], 1);			/* >pipe */
    close(fds[1]);

    cupsdExec(command, argv);
    exit(errno);
  }

 /*
  * Parent comes here, open the input side of the pipe...
  */

  close(fds[1]);

  return (cupsFileOpenFd(fds[0], "r"));
}


/*
 * 'cupsdSendIPPGroup()' - Send a group tag.
 */

void
cupsdSendIPPGroup(ipp_tag_t group_tag)	/* I - Group tag */
{
 /*
  * Send IPP group tag (1 byte)...
  */

  putchar(group_tag);
}


/*
 * 'cupsdSendIPPHeader()' - Send the IPP response header.
 */

void
cupsdSendIPPHeader(
    ipp_status_t status_code,		/* I - Status code */
    int          request_id)		/* I - Request ID */
{
 /*
  * Send IPP/1.1 response header: version number (2 bytes), status code
  * (2 bytes), and request ID (4 bytes)...
  *
  * TODO: Add version number (IPP/2.x and IPP/1.0) support.
  */

  putchar(1);
  putchar(1);

  putchar(status_code >> 8);
  putchar(status_code);

  putchar(request_id >> 24);
  putchar(request_id >> 16);
  putchar(request_id >> 8);
  putchar(request_id);
}


/*
 * 'cupsdSendIPPInteger()' - Send an integer attribute.
 */

void
cupsdSendIPPInteger(
    ipp_tag_t  value_tag,		/* I - Value tag */
    const char *name,			/* I - Attribute name */
    int        value)			/* I - Attribute value */
{
  size_t	len;			/* Length of attribute name */


 /*
  * Send IPP integer value: value tag (1 byte), name length (2 bytes),
  * name string (without nul), value length (2 bytes), and value (4 bytes)...
  */

  putchar(value_tag);

  len = strlen(name);
  putchar(len >> 8);
  putchar(len);

  fputs(name, stdout);

  putchar(0);
  putchar(4);

  putchar(value >> 24);
  putchar(value >> 16);
  putchar(value >> 8);
  putchar(value);
}


/*
 * 'cupsdSendIPPString()' - Send a string attribute.
 */

void
cupsdSendIPPString(
    ipp_tag_t  value_tag,		/* I - Value tag */
    const char *name,			/* I - Attribute name */
    const char *value)			/* I - Attribute value */
{
  size_t	len;			/* Length of attribute name */


 /*
  * Send IPP string value: value tag (1 byte), name length (2 bytes),
  * name string (without nul), value length (2 bytes), and value string
  * (without nul)...
  */

  putchar(value_tag);

  len = strlen(name);
  putchar(len >> 8);
  putchar(len);

  fputs(name, stdout);

  len = strlen(value);
  putchar(len >> 8);
  putchar(len);

  fputs(value, stdout);
}


/*
 * 'cupsdSendIPPTrailer()' - Send the end-of-message tag.
 */

void
cupsdSendIPPTrailer(void)
{
  putchar(IPP_TAG_END);
  fflush(stdout);
}


/*
 * End of "$Id$".
 */
