/*
 * "$Id$"
 *
 *   File functions for the Common UNIX Printing System (CUPS).
 *
 *   Since stdio files max out at 256 files on many systems, we have to
 *   write similar functions without this limit.  At the same time, using
 *   our own file functions allows us to provide transparent support of
 *   gzip'd print files, PPD files, etc.
 *
 *   Copyright 1997-2005 by Easy Software Products, all rights reserved.
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
 * Contents:
 *
 *   cupsFileClose()       - Close a CUPS file.
 *   cupsFileCompression() - Return whether a file is compressed.
 *   cupsFileEOF()         - Return the end-of-file status.
 *   cupsFileFind()        - Find a file using the specified path.
 *   cupsFileFlush()       - Flush pending output.
 *   cupsFileGetChar()     - Get a single character from a file.
 *   cupsFileGetConf()     - Get a line from a configuration file...
 *   cupsFileGets()        - Get a CR and/or LF-terminated line.
 *   cupsFileLock()        - Temporarily lock access to a file.
 *   cupsFileNumber()      - Return the file descriptor associated with a CUPS file.
 *   cupsFileOpen()        - Open a CUPS file.
 *   cupsFileOpenFd()      - Open a CUPS file using a file descriptor.
 *   cupsFilePeekChar()    - Peek at the next character from a file.
 *   cupsFilePrintf()      - Write a formatted string.
 *   cupsFilePutChar()     - Write a character.
 *   cupsFilePuts()        - Write a string.
 *   cupsFileRead()        - Read from a file.
 *   cupsFileRewind()      - Rewind a file.
 *   cupsFileSeek()        - Seek in a file.
 *   cupsFileTell()        - Return the current file position.
 *   cupsFileUnlock()      - Unlock access to a file.
 *   cupsFileWrite()       - Write to a file.
 *   cups_compress()       - Compress a buffer of data...
 *   cups_fill()           - Fill the input buffer...
 *   cups_read()           - Read from a file descriptor.
 *   cups_write()          - Write to a file descriptor.
 */

/*
 * Include necessary headers...
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include "http-private.h"
#include "string.h"
#include <errno.h>
#include <cups/debug.h>
#include <sys/types.h>
#include <fcntl.h>

#include "file.h"
#ifdef HAVE_LIBZ
#  include <zlib.h>
#endif /* HAVE_LIBZ */
#ifdef WIN32
#  include <io.h>
#  include <sys/locking.h>
#endif /* WIN32 */


/*
 * Some operating systems support large files via open flag O_LARGEFILE...
 */

#ifndef O_LARGEFILE
#  define O_LARGEFILE 0
#endif /* !O_LARGEFILE */


/*
 * Types and structures...
 */

struct _cups_file_s			/**** CUPS file structure... ****/

{
  int		fd;			/* File descriptor */
  char		mode,			/* Mode ('r' or 'w') */
		compressed,		/* Compression used? */
		eof,			/* End of file? */
		buf[4096],		/* Buffer */
		*ptr,			/* Pointer into buffer */
		*end;			/* End of buffer data */
  off_t		pos;			/* File position for start of buffer */

#ifdef HAVE_LIBZ
  z_stream	stream;			/* (De)compression stream */
  Bytef		cbuf[4096];		/* (De)compression buffer */
  uLong		crc;			/* (De)compression CRC */
#endif /* HAVE_LIBZ */
};


/*
 * Local functions...
 */

#ifdef HAVE_LIBZ
static ssize_t	cups_compress(cups_file_t *fp, const char *buf, size_t bytes);
#endif /* HAVE_LIBZ */
static ssize_t	cups_fill(cups_file_t *fp);
static ssize_t	cups_read(cups_file_t *fp, char *buf, size_t bytes);
static ssize_t	cups_write(cups_file_t *fp, const char *buf, size_t bytes);


/*
 * 'cupsFileClose()' - Close a CUPS file.
 */

int					/* O - 0 on success, -1 on error */
cupsFileClose(cups_file_t *fp)		/* I - CUPS file */
{
  int	fd;				/* File descriptor */
  char	mode;				/* Open mode */
  int	status;				/* Return status */


  DEBUG_printf(("cupsFileClose(fp=%p)\n", fp));

 /*
  * Range check...
  */

  if (!fp)
    return (-1);

 /*
  * Flush pending write data...
  */

  if (fp->mode == 'w')
    status = cupsFileFlush(fp);
  else
    status = 0;

#ifdef HAVE_LIBZ
  if (fp->compressed && status >= 0)
  {
    if (fp->mode == 'r')
    {
     /*
      * Free decompression data...
      */

      inflateEnd(&fp->stream);
    }
    else
    {
     /*
      * Flush any remaining compressed data...
      */

      unsigned char	trailer[8];	/* Trailer CRC and length */
      int		done;		/* Done writing... */


      fp->stream.avail_in = 0;

      for (done = 0;;)
      {
        if (fp->stream.next_out > fp->cbuf)
	{
	  if (cups_write(fp, (char *)fp->cbuf,
	                 fp->stream.next_out - fp->cbuf) < 0)
	    status = -1;

	  fp->stream.next_out  = fp->cbuf;
	  fp->stream.avail_out = sizeof(fp->cbuf);
	}

        if (done || status < 0)
	  break;

        done = deflate(&fp->stream, Z_FINISH) == Z_STREAM_END &&
	       fp->stream.next_out == fp->cbuf;
      }

     /*
      * Write the CRC and length...
      */

      trailer[0] = fp->crc;
      trailer[1] = fp->crc >> 8;
      trailer[2] = fp->crc >> 16;
      trailer[3] = fp->crc >> 24;
      trailer[4] = fp->pos;
      trailer[5] = fp->pos >> 8;
      trailer[6] = fp->pos >> 16;
      trailer[7] = fp->pos >> 24;

      if (cups_write(fp, (char *)trailer, 8) < 0)
        status = -1;

     /*
      * Free all memory used by the compression stream...
      */

      deflateEnd(&(fp->stream));
    }
  }
#endif /* HAVE_LIBZ */

 /*
  * Save the file descriptor we used and free memory...
  */

  fd   = fp->fd;
  mode = fp->mode;

  free(fp);

 /*
  * Close the file, returning the close status...
  */

  if (mode == 's')
  {
    if (closesocket(fd) < 0)
      status = -1;
  }
  else
  {
    if (close(fd) < 0)
      status = -1;
  }

  return (status);
}


/*
 * 'cupsFileCompression()' - Return whether a file is compressed.
 */

int					/* O - CUPS_FILE_NONE or CUPS_FILE_GZIP */
cupsFileCompression(cups_file_t *fp)	/* I - CUPS file */
{
  return (fp->compressed);
}


/*
 * 'cupsFileEOF()' - Return the end-of-file status.
 */

int					/* O - 1 on EOF, 0 otherwise */
cupsFileEOF(cups_file_t *fp)		/* I - CUPS file */
{
  return (fp->eof);
}


/*
 * 'cupsFileFind()' - Find a file using the specified path.
 *
 * This function allows the paths in the path string to be separated by
 * colons (UNIX standard) or semicolons (Windows standard) and stores the
 * result in the buffer supplied.  If the file cannot be found in any of
 * the supplied paths, NULL is returned. A NULL path only matches the
 * current directory.
 */

const char *				/* O - Full path to file or NULL */
cupsFileFind(const char *filename,	/* I - File to find */
             const char *path,		/* I - Colon/semicolon-separated path */
             char       *buffer,	/* I - Filename buffer */
	     int        bufsize)	/* I - Size of filename buffer */
{
  char	*bufptr,			/* Current position in buffer */
	*bufend;			/* End of buffer */


 /*
  * Range check input...
  */

  if (!filename || !buffer || bufsize < 2)
    return (NULL);

  if (!path)
  {
   /*
    * No path, so check current directory...
    */

    if (!access(filename, 0))
    {
      strlcpy(buffer, filename, bufsize);
      return (buffer);
    }
    else
      return (NULL);
  }

 /*
  * Now check each path and return the first match...
  */

  bufend = buffer + bufsize - 1;
  bufptr = buffer;

  while (*path)
  {
    if (*path == ';' || *path == ':')
    {
      if (bufptr > buffer && bufptr[-1] != '/' && bufptr < bufend)
        *bufptr++ = '/';

      strlcpy(bufptr, filename, bufend - bufptr);

      if (!access(buffer, 0))
        return (buffer);

      bufptr = buffer;
    }
    else if (bufptr < bufend)
      *bufptr++ = *path;

    path ++;
  }

 /*
  * Check the last path...
  */

  if (bufptr > buffer && bufptr[-1] != '/' && bufptr < bufend)
    *bufptr++ = '/';

  strlcpy(bufptr, filename, bufend - bufptr);

  if (!access(buffer, 0))
    return (buffer);
  else
    return (NULL);
}


/*
 * 'cupsFileFlush()' - Flush pending output.
 */

int					/* O - 0 on success, -1 on error */
cupsFileFlush(cups_file_t *fp)		/* I - CUPS file */
{
  size_t	bytes;			/* Bytes to write */


  DEBUG_printf(("cupsFileFlush(fp=%p)\n", fp));

 /*
  * Range check input...
  */

  if (!fp || fp->mode != 'w')
  {
    DEBUG_puts("    Attempt to flush a read-only file...");
    return (-1);
  }

  bytes = fp->ptr - fp->buf;

  if (bytes > 0)
  {
#ifdef HAVE_LIBZ
    if (fp->compressed)
      bytes = cups_compress(fp, fp->buf, bytes);
    else
#endif /* HAVE_LIBZ */
      bytes = cups_write(fp, fp->buf, bytes);

    if (bytes < 0)
      return (-1);

    fp->ptr = fp->buf;
  }
   
  return (0);
}


/*
 * 'cupsFileGetChar()' - Get a single character from a file.
 */

int					/* O - Character or -1 on EOF */
cupsFileGetChar(cups_file_t *fp)	/* I - CUPS file */
{
 /*
  * Range check input...
  */

  if (!fp || (fp->mode != 'r' && fp->mode != 's'))
    return (-1);

 /*
  * If the input buffer is empty, try to read more data...
  */

  if (fp->ptr >= fp->end)
    if (cups_fill(fp) < 0)
      return (-1);

 /*
  * Return the next character in the buffer...
  */

  return (*(fp->ptr)++ & 255);
}


/*
 * 'cupsFileGetConf()' - Get a line from a configuration file...
 */

char *					/* O  - Line read of NULL on eof/error */
cupsFileGetConf(cups_file_t *fp,	/* I  - CUPS file */
                char        *buf,	/* O  - String buffer */
		size_t      buflen,	/* I  - Size of string buffer */
                char        **value,	/* O  - Pointer to value */
		int         *linenum)	/* IO - Current line number */
{
  char	*ptr;				/* Pointer into line */


 /*
  * Range check input...
  */

  if (!fp || (fp->mode != 'r' && fp->mode != 's') ||
      !buf || buflen < 2 || !value)
  {
    if (value)
      *value = NULL;

    return (NULL);
  }

 /*
  * Read the next non-comment line...
  */

  *value = NULL;
  
  while (cupsFileGets(fp, buf, buflen))
  {
    (*linenum) ++;

   /*
    * Strip any comments...
    */

    if ((ptr = strchr(buf, '#')) != NULL)
    {
      while (ptr > buf)
      {
	if (!isspace(ptr[-1] & 255))
	  break;

        ptr --;
      }

      *ptr = '\0';
    }

   /*
    * Strip leading whitespace...
    */

    for (ptr = buf; isspace(*ptr & 255); ptr ++);

    if (ptr > buf)
      _cups_strcpy(buf, ptr);

   /*
    * See if there is anything left...
    */

    if (buf[0])
    {
     /*
      * Yes, grab any value and return...
      */

      for (ptr = buf; *ptr; ptr ++)
        if (isspace(*ptr & 255))
	  break;

      if (*ptr)
      {
       /*
        * Have a value, skip any other spaces...
	*/

        while (isspace(*ptr & 255))
	  *ptr++ = '\0';

        if (*ptr)
	  *value = ptr;

       /*
        * Strip trailing whitespace and > for lines that begin with <...
	*/

        ptr += strlen(ptr) - 1;

        if (buf[0] == '<' && *ptr == '>')
	  *ptr-- = '\0';
	else if (buf[0] == '<' && *ptr != '>')
        {
	 /*
	  * Syntax error...
	  */

	  *value = NULL;
	  return (buf);
	}

        while (ptr > *value && isspace(*ptr & 255))
	  *ptr-- = '\0';
      }

     /*
      * Return the line...
      */

      return (buf);
    }
  }

  return (NULL);
}


/*
 * 'cupsFileGets()' - Get a CR and/or LF-terminated line.
 */

char *					/* O - Line read or NULL on eof/error */
cupsFileGets(cups_file_t *fp,		/* I - CUPS file */
             char        *buf,		/* O - String buffer */
	     size_t      buflen)	/* I - Size of string buffer */
{
  int		ch;			/* Character from file */
  char		*ptr,			/* Current position in line buffer */
		*end;			/* End of line buffer */


 /*
  * Range check input...
  */

  if (!fp || (fp->mode != 'r' && fp->mode != 's') || !buf || buflen < 2)
    return (NULL);

 /*
  * Now loop until we have a valid line...
  */

  for (ptr = buf, end = buf + buflen - 1; ptr < end ;)
  {
    if (fp->ptr >= fp->end)
      if (cups_fill(fp) <= 0)
      {
        if (ptr == buf)
	  return (NULL);
	else
          break;
      }

    ch = *(fp->ptr)++;

    if (ch == '\r')
    {
     /*
      * Check for CR LF...
      */

      if (fp->ptr >= fp->end)
	if (cups_fill(fp) <= 0)
          break;

      if (*(fp->ptr) == '\n')
        fp->ptr ++;      

      break;
    }
    else if (ch == '\n')
    {
     /*
      * Line feed ends a line...
      */

      break;
    }
    else
      *ptr++ = ch;
  }

  *ptr = '\0';

  return (buf);
}


/*
 * 'cupsFileLock()' - Temporarily lock access to a file.
 */

int					/* O - 0 on success, -1 on error */
cupsFileLock(cups_file_t *fp,		/* I - File to lock */
             int         block)		/* I - 1 to wait for the lock, 0 to fail right away */
{
 /*
  * Range check...
  */

  if (!fp || fp->mode == 's')
    return (-1);

 /*
  * Try the lock...
  */

#ifdef WIN32
  return (locking(fp->fd, block ? _LK_LOCK : _LK_NBLCK, 0));
#else
  return (lockf(fp->fd, block ? F_LOCK : F_TLOCK, 0));
#endif /* WIN32 */
}


/*
 * 'cupsFileNumber()' - Return the file descriptor associated with a CUPS file.
 */

int					/* O - File descriptor */
cupsFileNumber(cups_file_t *fp)		/* I - CUPS file */
{
  return (fp->fd);
}


/*
 * 'cupsFileOpen()' - Open a CUPS file.
 */

cups_file_t *				/* O - CUPS file or NULL */
cupsFileOpen(const char *filename,	/* I - Name of file */
             const char *mode)		/* I - Open mode */
{
  cups_file_t	*fp;			/* New CUPS file */
  int		fd;			/* File descriptor */
  char		hostname[1024],		/* Hostname */
		*portname;		/* Port "name" (number or service) */
  http_addrlist_t *addrlist;		/* Host address list */


 /*
  * Range check input...
  */

  if (!filename || !mode ||
      (*mode != 'r' && *mode != 'w' && *mode != 'a' && *mode != 's'))
    return (NULL);

 /*
  * Open the file...
  */

  switch (*mode)
  {
    case 'a' : /* Append file */
        fd = open(filename, O_RDWR | O_CREAT | O_APPEND | O_LARGEFILE, 0666);
        break;

    case 'r' : /* Read file */
	fd = open(filename, O_RDONLY | O_LARGEFILE, 0);
	break;

    case 'w' : /* Write file */
        fd = open(filename, O_WRONLY | O_TRUNC | O_CREAT | O_LARGEFILE, 0666);
        break;

    case 's' : /* Read/write socket */
        strlcpy(hostname, filename, sizeof(hostname));
	if ((portname = strrchr(hostname, ':')) != NULL)
	  *portname++ = '\0';
	else
	  return (NULL);

       /*
        * Lookup the hostname and service...
	*/

        if ((addrlist = httpAddrGetList(hostname, AF_UNSPEC, portname)) == NULL)
	  return (NULL);

       /*
	* Connect to the server...
	*/

        if (!httpAddrConnect(addrlist, &fd))
	{
	  httpAddrFreeList(addrlist);
	  return (NULL);
	}

	httpAddrFreeList(addrlist);
	break;

    default : /* Remove bogus compiler warning... */
        return (NULL);
  }

  if (fd < 0)
    return (NULL);

 /*
  * Create the CUPS file structure...
  */

  if ((fp = cupsFileOpenFd(fd, mode)) == NULL)
  {
    if (*mode == 's')
      closesocket(fd);
    else
      close(fd);
  }

 /*
  * Return it...
  */

  return (fp);
}

/*
 * 'cupsFileOpenFd()' - Open a CUPS file using a file descriptor.
 */

cups_file_t *				/* O - CUPS file or NULL */
cupsFileOpenFd(int        fd,		/* I - File descriptor */
	       const char *mode)	/* I - Open mode */
{
  cups_file_t	*fp;			/* New CUPS file */


 /*
  * Range check input...
  */

  if (fd < 0 || !mode ||
      (*mode != 'r' && *mode != 'w' && *mode != 'a' && *mode != 's'))
    return (NULL);

 /*
  * Allocate memory...
  */

  if ((fp = calloc(1, sizeof(cups_file_t))) == NULL)
    return (NULL);

 /*
  * Open the file...
  */

  fp->fd = fd;

  switch (*mode)
  {
    case 'w' :
    case 'a' :
	fp->mode = 'w';
	fp->ptr  = fp->buf;
	fp->end  = fp->buf + sizeof(fp->buf);

#ifdef HAVE_LIBZ
	if (mode[1] >= '1' && mode[1] <= '9')
	{
	 /*
	  * Open a compressed stream, so write the standard gzip file
	  * header...
	  */

          unsigned char header[10];	/* gzip file header */
	  time_t	curtime;	/* Current time */


          curtime   = time(NULL);
	  header[0] = 0x1f;
	  header[1] = 0x8b;
	  header[2] = Z_DEFLATED;
	  header[3] = 0;
	  header[4] = curtime;
	  header[5] = curtime >> 8;
	  header[6] = curtime >> 16;
	  header[7] = curtime >> 24;
	  header[8] = 0;
	  header[9] = 0x03;

	  cups_write(fp, (char *)header, 10);

         /*
	  * Initialize the compressor...
	  */

          deflateInit2(&(fp->stream), mode[1] - '0', Z_DEFLATED, -15, 8,
	               Z_DEFAULT_STRATEGY);

	  fp->stream.next_out  = fp->cbuf;
	  fp->stream.avail_out = sizeof(fp->cbuf);
	  fp->compressed       = 1;
	  fp->crc              = crc32(0L, Z_NULL, 0);
	}
#endif /* HAVE_LIBZ */
        break;

    case 'r' :
	fp->mode = 'r';
	break;

    case 's' :
        fp->mode = 's';
	break;

    default : /* Remove bogus compiler warning... */
        return (NULL);
  }

 /*
  * Don't pass this file to child processes...
  */

#ifndef WIN32
  fcntl(fp->fd, F_SETFD, fcntl(fp->fd, F_GETFD) | FD_CLOEXEC);
#endif /* !WIN32 */

  return (fp);
}


/*
 * 'cupsFilePeekChar()' - Peek at the next character from a file.
 */

int					/* O - Character or -1 on EOF */
cupsFilePeekChar(cups_file_t *fp)	/* I - CUPS file */
{
 /*
  * Range check input...
  */

  if (!fp || (fp->mode != 'r' && fp->mode != 's'))
    return (-1);

 /*
  * If the input buffer is empty, try to read more data...
  */

  if (fp->ptr >= fp->end)
    if (cups_fill(fp) < 0)
      return (-1);

 /*
  * Return the next character in the buffer...
  */

  return (*(fp->ptr) & 255);
}


/*
 * 'cupsFilePrintf()' - Write a formatted string.
 */

int					/* O - Number of bytes written or -1 */
cupsFilePrintf(cups_file_t *fp,		/* I - CUPS file */
               const char  *format,	/* I - Printf-style format string */
	       ...)			/* I - Additional args as necessary */
{
  va_list	ap;			/* Argument list */
  size_t	bytes;			/* Formatted size */
  char		buf[2048];		/* Formatted text */


  if (!fp || !format || (fp->mode != 'w' && fp->mode != 's'))
    return (-1);

  va_start(ap, format);
  bytes = vsnprintf(buf, sizeof(buf), format, ap);
  va_end(ap);

  if (fp->mode == 's')
    return (cups_write(fp, buf, bytes));

  if ((fp->ptr + bytes) > fp->end)
    if (cupsFileFlush(fp))
      return (-1);

  fp->pos += bytes;

  if (bytes > sizeof(fp->buf))
  {
#ifdef HAVE_LIBZ
    if (fp->compressed)
      return (cups_compress(fp, buf, bytes));
    else
#endif /* HAVE_LIBZ */
      return (cups_write(fp, buf, bytes));
  }
  else
  {
    memcpy(fp->ptr, buf, bytes);
    fp->ptr += bytes;
    return (bytes);
  }
}


/*
 * 'cupsFilePutChar()' - Write a character.
 */

int					/* O - 0 on success, -1 on error */
cupsFilePutChar(cups_file_t *fp,	/* I - CUPS file */
                int         c)		/* I - Character to write */
{
 /*
  * Range check input...
  */

  if (!fp || (fp->mode != 'w' && fp->mode != 's'))
    return (-1);

  if (fp->mode == 's')
  {
   /*
    * Send character immediately over socket...
    */

    char ch;				/* Output character */


    ch = c;

    if (send(fp->fd, &ch, 1, 0) < 1)
      return (-1);
  }
  else
  {
   /*
    * Buffer it up...
    */

    if (fp->ptr >= fp->end)
      if (cupsFileFlush(fp))
	return (-1);

    *(fp->ptr) ++ = c;
  }

  fp->pos ++;

  return (0);
}


/*
 * 'cupsFilePuts()' - Write a string.
 */

int					/* O - Number of bytes written or -1 */
cupsFilePuts(cups_file_t *fp,		/* I - CUPS file */
             const char  *s)		/* I - String to write */
{
  size_t	bytes;			/* Bytes to write */


 /*
  * Range check input...
  */

  if (!fp || !s || (fp->mode != 'w' && fp->mode != 's'))
    return (-1);

 /*
  * Write the string...
  */

  bytes = strlen(s);

  if (fp->mode == 's')
  {
    if (cups_write(fp, s, bytes) < 0)
      return (-1);

    fp->pos += bytes;

    return (bytes);
  }

  if ((fp->ptr + bytes) > fp->end)
    if (cupsFileFlush(fp))
      return (-1);

  fp->pos += bytes;

  if (bytes > sizeof(fp->buf))
  {
#ifdef HAVE_LIBZ
    if (fp->compressed)
      return (cups_compress(fp, s, bytes));
    else
#endif /* HAVE_LIBZ */
      return (cups_write(fp, s, bytes));
  }
  else
  {
    memcpy(fp->ptr, s, bytes);
    fp->ptr += bytes;
    return (bytes);
  }
}


/*
 * 'cupsFileRead()' - Read from a file.
 */

ssize_t					/* O - Number of bytes read or -1 */
cupsFileRead(cups_file_t *fp,		/* I - CUPS file */
             char        *buf,		/* O - Buffer */
	     size_t      bytes)		/* I - Number of bytes to read */
{
  size_t	total,			/* Total bytes read */
		count;			/* Bytes read */


  DEBUG_printf(("cupsFileRead(fp=%p, buf=%p, bytes=%ld)\n", fp, buf,
                (long)bytes));

 /*
  * Range check input...
  */

  if (!fp || !buf || bytes < 0 || (fp->mode != 'r' && fp->mode != 's'))
    return (-1);

  if (bytes == 0)
    return (0);

 /*
  * Loop until all bytes are read...
  */

  total = 0;
  while (bytes > 0)
  {
    if (fp->ptr >= fp->end)
      if (cups_fill(fp) <= 0)
      {
        DEBUG_printf(("    cups_fill() returned -1, total=%d\n", total));

        if (total > 0)
          return (total);
	else
	  return (-1);
      }

    count = fp->end - fp->ptr;
    if (count > bytes)
      count = bytes;

    memcpy(buf, fp->ptr, count);
    fp->ptr += count;

   /*
    * Update the counts for the last read...
    */

    bytes -= count;
    total += count;
    buf   += count;
  }

 /*
  * Return the total number of bytes read...
  */

  DEBUG_printf(("    total=%d\n", total));

  return (total);
}


/*
 * 'cupsFileRewind()' - Rewind a file.
 */

off_t					/* O - New file position or -1 */
cupsFileRewind(cups_file_t *fp)		/* I - CUPS file */
{
  return (cupsFileSeek(fp, 0L));
}


/*
 * 'cupsFileSeek()' - Seek in a file.
 */

off_t					/* O - New file position or -1 */
cupsFileSeek(cups_file_t *fp,		/* I - CUPS file */
             off_t       pos)		/* I - Position in file */
{
  size_t	bytes;			/* Number bytes in buffer */


  DEBUG_printf(("cupsFileSeek(fp=%p, pos=%ld)\n", fp, (long)pos));
  DEBUG_printf(("    fp->pos=%ld\n", (long)fp->pos));

 /*
  * Range check input...
  */

  if (!fp || pos < 0 || fp->mode != 'r')
    return (-1);

 /*
  * Figure out the number of bytes in the current buffer, and then
  * see if we are outside of it...
  */

  bytes   = fp->end - fp->buf;
  fp->eof = 0;

  if (pos < fp->pos)
  {
   /*
    * Need to seek backwards...
    */

#ifdef HAVE_LIBZ
    if (fp->compressed)
    {
      inflateEnd(&fp->stream);

      lseek(fp->fd, 0, SEEK_SET);
      fp->pos = 0;
      fp->ptr = NULL;
      fp->end = NULL;

      while ((bytes = cups_fill(fp)) > 0)
        if (pos >= fp->pos && pos < (fp->pos + bytes))
	  break;

      if (bytes <= 0)
        return (-1);
    }
    else
#endif /* HAVE_LIBZ */
    {
      fp->pos = lseek(fp->fd, pos, SEEK_SET);
      DEBUG_printf(("    lseek() returned %ld...\n", (long)fp->pos));
      fp->ptr = NULL;
      fp->end = NULL;
    }
  }
  else if (pos >= (fp->pos + bytes))
  {
   /*
    * Need to seek forwards...
    */

#ifdef HAVE_LIBZ
    if (fp->compressed)
    {
      while ((bytes = cups_fill(fp)) > 0)
        if (pos >= fp->pos && pos < (fp->pos + bytes))
	  break;

      if (bytes <= 0)
        return (-1);
    }
    else
#endif /* HAVE_LIBZ */
    {
      fp->pos = lseek(fp->fd, pos, SEEK_SET);
      DEBUG_printf(("    lseek() returned %ld...\n", (long)fp->pos));
      fp->ptr = NULL;
      fp->end = NULL;
    }
  }
  else
  {
   /*
    * Just reposition the current pointer, since we have the right
    * range...
    */

    fp->ptr = fp->buf + pos - fp->pos;
    DEBUG_puts(("    seek inside buffer..."));
  }

  return (fp->pos);
}


/*
 * 'cupsFileTell()' - Return the current file position.
 */

off_t					/* O - File position */
cupsFileTell(cups_file_t *fp)		/* I - CUPS file */
{
  return (fp->pos);
}


/*
 * 'cupsFileUnlock()' - Unlock access to a file.
 */

int					/* O - 0 on success, -1 on error */
cupsFileUnlock(cups_file_t *fp)		/* I - File to lock */
{
 /*
  * Range check...
  */

  if (!fp || fp->mode == 's')
    return (-1);

 /*
  * Unlock...
  */

#ifdef WIN32
  return (locking(fp->fd, _LK_UNLCK, 0));
#else
  return (lockf(fp->fd, F_ULOCK, 0));
#endif /* WIN32 */
}


/*
 * 'cupsFileWrite()' - Write to a file.
 */

ssize_t					/* O - Number of bytes written */
cupsFileWrite(cups_file_t *fp,		/* I - CUPS file */
              const char  *buf,		/* I - Buffer */
	      size_t      bytes)	/* I - Number of bytes to write */
{
 /*
  * Range check input...
  */

  if (!fp || !buf || bytes < 0 || (fp->mode != 'w' && fp->mode != 's'))
    return (-1);

  if (bytes == 0)
    return (0);

 /*
  * Write the buffer...
  */

  if (fp->mode == 's')
  {
    if (cups_write(fp, buf, bytes) < 0)
      return (-1);

    fp->pos += bytes;

    return (bytes);
  }

  if ((fp->ptr + bytes) > fp->end)
    if (cupsFileFlush(fp))
      return (-1);

  fp->pos += bytes;

  if (bytes > sizeof(fp->buf))
  {
#ifdef HAVE_LIBZ
    if (fp->compressed)
      return (cups_compress(fp, buf, bytes));
    else
#endif /* HAVE_LIBZ */
      return (cups_write(fp, buf, bytes));
  }
  else
  {
    memcpy(fp->ptr, buf, bytes);
    fp->ptr += bytes;
    return (bytes);
  }
}


#ifdef HAVE_LIBZ
/*
 * 'cups_compress()' - Compress a buffer of data...
 */

static ssize_t				/* O - Number of bytes written or -1 */
cups_compress(cups_file_t *fp,		/* I - CUPS file */
              const char  *buf,		/* I - Buffer */
	      size_t      bytes)	/* I - Number bytes */
{
 /*
  * Update the CRC...
  */

  fp->crc = crc32(fp->crc, (const Bytef *)buf, bytes);

 /*
  * Deflate the bytes...
  */

  fp->stream.next_in  = (Bytef *)buf;
  fp->stream.avail_in = bytes;

  while (fp->stream.avail_in > 0)
  {
   /*
    * Flush the current buffer...
    */

    if (fp->stream.avail_out < (int)(sizeof(fp->cbuf) / 8))
    {
      if (cups_write(fp, (char *)fp->cbuf, fp->stream.next_out - fp->cbuf) < 0)
        return (-1);
    }

    deflate(&(fp->stream), Z_NO_FLUSH);
  }

  return (bytes);
}
#endif /* HAVE_LIBZ */


/*
 * 'cups_fill()' - Fill the input buffer...
 */

static ssize_t				/* O - Number of bytes or -1 */
cups_fill(cups_file_t *fp)		/* I - CUPS file */
{
  ssize_t		bytes;		/* Number of bytes read */
#ifdef HAVE_LIBZ
  const unsigned char	*ptr,		/* Pointer into buffer */
			*end;		/* End of buffer */
#endif /* HAVE_LIBZ */


  DEBUG_printf(("cups_fill(fp=%p)\n", fp));
  DEBUG_printf(("    fp->ptr=%p, fp->end=%p, fp->buf=%p, fp->pos=%ld\n",
                fp->ptr, fp->end, fp->buf, (long)fp->pos));

 /*
  * Update the "pos" element as needed...
  */

  if (fp->ptr && fp->end)
    fp->pos += fp->end - fp->buf;

#ifdef HAVE_LIBZ
  while (!fp->ptr || fp->compressed)
  {
   /*
    * Check to see if we have read any data yet; if not, see if we have a
    * compressed file...
    */

    if (!fp->ptr)
    {
     /*
      * Reset the file position in case we are seeking...
      */

      fp->compressed = 0;
      fp->pos        = 0;

     /*
      * Read the first bytes in the file to determine if we have a gzip'd
      * file...
      */

      if ((bytes = cups_read(fp, (char *)fp->buf, sizeof(fp->buf))) < 0)
      {
       /*
	* Can't read from file!
	*/

	return (-1);
      }

      if (bytes < 10 || fp->buf[0] != 0x1f ||
          (unsigned char)fp->buf[1] != 0x8b ||
          fp->buf[2] != 8 || (fp->buf[3] & 0xe0) != 0)
      {
       /*
	* Not a gzip'd file!
	*/

	fp->ptr = fp->buf;
	fp->end = fp->buf + bytes;

	return (bytes);
      }

     /*
      * Parse header junk: extra data, original name, and comment...
      */

      ptr = (unsigned char *)fp->buf + 10;
      end = (unsigned char *)fp->buf + bytes;

      if (fp->buf[3] & 0x04)
      {
       /*
	* Skip extra data...
	*/

	if ((ptr + 2) > end)
	{
	 /*
	  * Can't read from file!
	  */

	  return (-1);
	}

	bytes = ((unsigned char)ptr[1] << 8) | (unsigned char)ptr[0];
	ptr   += 2 + bytes;

	if (ptr > end)
	{
	 /*
	  * Can't read from file!
	  */

	  return (-1);
	}
      }

      if (fp->buf[3] & 0x08)
      {
       /*
	* Skip original name data...
	*/

	while (ptr < end && *ptr)
          ptr ++;

	if (ptr < end)
          ptr ++;
	else
	{
	 /*
	  * Can't read from file!
	  */

	  return (-1);
	}
      }

      if (fp->buf[3] & 0x10)
      {
       /*
	* Skip comment data...
	*/

	while (ptr < end && *ptr)
          ptr ++;

	if (ptr < end)
          ptr ++;
	else
	{
	 /*
	  * Can't read from file!
	  */

	  return (-1);
	}
      }

      if (fp->buf[3] & 0x02)
      {
       /*
	* Skip header CRC data...
	*/

	ptr += 2;

	if (ptr > end)
	{
	 /*
	  * Can't read from file!
	  */

	  return (-1);
	}
      }

     /*
      * Copy the flate-compressed data to the compression buffer...
      */

      if ((bytes = end - ptr) > 0)
        memcpy(fp->cbuf, ptr, bytes);

     /*
      * Setup the decompressor data...
      */

      fp->stream.zalloc    = (alloc_func)0;
      fp->stream.zfree     = (free_func)0;
      fp->stream.opaque    = (voidpf)0;
      fp->stream.next_in   = (Bytef *)fp->cbuf;
      fp->stream.next_out  = NULL;
      fp->stream.avail_in  = bytes;
      fp->stream.avail_out = 0;
      fp->crc              = crc32(0L, Z_NULL, 0);

      if (inflateInit2(&(fp->stream), -15) != Z_OK)
	return (-1);

      fp->compressed = 1;
    }

    if (fp->compressed)
    {
     /*
      * If we have reached end-of-file, return immediately...
      */

      if (fp->eof)
	return (-1);

     /*
      * Fill the decompression buffer as needed...
      */

      if (fp->stream.avail_in == 0)
      {
	if ((bytes = cups_read(fp, (char *)fp->cbuf, sizeof(fp->cbuf))) <= 0)
          return (-1);

	fp->stream.next_in  = fp->cbuf;
	fp->stream.avail_in = bytes;
      }

     /*
      * Decompress data from the buffer...
      */

      fp->stream.next_out  = (Bytef *)fp->buf;
      fp->stream.avail_out = sizeof(fp->buf);

      if (inflate(&(fp->stream), Z_NO_FLUSH) == Z_STREAM_END)
      {
       /*
	* Read the CRC and length...
	*/

	unsigned char	trailer[8];	/* Trailer bytes */
	uLong		tcrc;		/* Trailer CRC */


	if (read(fp->fd, trailer, sizeof(trailer)) < sizeof(trailer))
	{
	 /*
          * Can't get it, so mark end-of-file...
	  */

          fp->eof = 1;
	}
	else
	{
	  tcrc = (((((trailer[3] << 8) | trailer[2]) << 8) | trailer[1]) << 8) |
        	 trailer[0];

	  if (tcrc != fp->crc)
	  {
	   /*
            * Bad CRC, mark end-of-file...
	    */

	    fp->eof = 1;

	    return (-1);
	  }

	 /*
	  * Otherwise, reset the compressed flag so that we re-read the
	  * file header...
	  */

	  fp->compressed = 0;
	}
      }

      bytes = sizeof(fp->buf) - fp->stream.avail_out;

     /*
      * Return the decompressed data...
      */

      fp->ptr = fp->buf;
      fp->end = fp->buf + bytes;

      if (bytes)
	return (bytes);
    }
  }
#endif /* HAVE_LIBZ */

 /*
  * Read a buffer's full of data...
  */

  if ((bytes = cups_read(fp, fp->buf, sizeof(fp->buf))) <= 0)
  {
   /*
    * Can't read from file!
    */

    fp->eof = 1;
    fp->ptr = fp->buf;
    fp->end = fp->buf;

    return (-1);
  }

 /*
  * Return the bytes we read...
  */

  fp->eof = 0;
  fp->ptr = fp->buf;
  fp->end = fp->buf + bytes;

  return (bytes);
}


/*
 * 'cups_read()' - Read from a file descriptor.
 */

static ssize_t				/* O - Number of bytes read or -1 */
cups_read(cups_file_t *fp,		/* I - CUPS file */
          char        *buf,		/* I - Buffer */
	  size_t      bytes)		/* I - Number bytes */
{
  ssize_t	total;			/* Total bytes read */


 /*
  * Loop until we read at least 0 bytes...
  */

  for (;;)
  {
    if (fp->mode == 's')
      total = recv(fp->fd, buf, bytes, 0);
    else
      total = read(fp->fd, buf, bytes);

    if (total >= 0)
      break;

   /*
    * Reads can be interrupted by signals and unavailable resources...
    */

    if (errno == EAGAIN || errno == EINTR)
      continue;
    else
      return (-1);
  }

 /*
  * Return the total number of bytes read...
  */

  return (total);
}


/*
 * 'cups_write()' - Write to a file descriptor.
 */

static ssize_t				/* O - Number of bytes written or -1 */
cups_write(cups_file_t *fp,		/* I - CUPS file */
           const char  *buf,		/* I - Buffer */
	   size_t      bytes)		/* I - Number bytes */
{
  size_t	total,			/* Total bytes written */
		count;			/* Count this time */


 /*
  * Loop until all bytes are written...
  */

  total = 0;
  while (bytes > 0)
  {
    if (fp->mode == 's')
      count = send(fp->fd, buf, bytes, 0);
    else
      count = write(fp->fd, buf, bytes);

    if (count < 0)
    {
     /*
      * Writes can be interrupted by signals and unavailable resources...
      */

      if (errno == EAGAIN || errno == EINTR)
        continue;
      else
        return (-1);
    }

   /*
    * Update the counts for the last write call...
    */

    bytes -= count;
    total += count;
    buf   += count;
  }

 /*
  * Return the total number of bytes written...
  */

  return (total);
}


/*
 * End of "$Id$".
 */
