/*
 * "$Id: cgi.h,v 1.12.2.5 2004/06/29 13:15:08 mike Exp $"
 *
 *   CGI support library definitions.
 *
 *   Copyright 1997-2004 by Easy Software Products.
 *
 *   This program is free software; you can redistribute it and/or modify it
 *   under the terms of the GNU General Public License as published by the Free
 *   Software Foundation; either version 2 of the License, or (at your option)
 *   any later version.
 *
 *   This program is distributed in the hope that it will be useful, but
 *   WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 *   or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 *   for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#ifndef _CGI_H_
#  define _CGI_H_

#  include <stdio.h>
#  include <stdlib.h>
#  include <time.h>

#  include <cups/string.h>

#  ifdef WIN32
#    include <direct.h>
#    include <io.h>
#    include <malloc.h>
#  else
#    include <unistd.h>
#  endif /* WIN32 */


/*
 * Prototypes...
 */

extern int		cgiInitialize(void);
extern void		cgiAbort(const char *title, const char *stylesheet,
			         const char *format, ...);
extern int		cgiCheckVariables(const char *names);
extern const char	*cgiGetArray(const char *name, int element);
extern int		cgiGetSize(const char *name);
extern void		cgiSetSize(const char *name, int size);
extern const char	*cgiGetVariable(const char *name);
extern void		cgiSetArray(const char *name, int element,
			            const char *value);
extern void		cgiSetVariable(const char *name, const char *value);
extern void		cgiCopyTemplateFile(FILE *out, const char *tmpl);
extern void		cgiCopyTemplateLang(FILE *out, const char *directory,
			                    const char *tmpl, const char *lang);

extern void		cgiStartHTML(FILE *out, const char *author,
			             const char *stylesheet,
			             const char *keywords,
			             const char *description,
				     const char *title, ...);
extern void		cgiEndHTML(FILE *out);

extern FILE		*cgiEMailOpen(const char *from, const char *to,
			              const char *cc, const char *subject,
				      int multipart);
extern void		cgiEMailPart(FILE *mail, const char *type,
			             const char *charset, const char *encoding);
extern void		cgiEMailClose(FILE *mail);

extern char		*cgiGetCookie(const char *name, char *buf, int buflen);
extern void		cgiSetCookie(const char *name, const char *value,
			             const char *path, const char *domain,
				     time_t expires, int secure);

#  define cgiGetUser()	getenv("REMOTE_USER")
#  define cgiGetHost()	(getenv("REMOTE_HOST") == NULL ? getenv("REMOTE_ADDR") : getenv("REMOTE_HOST"))

#endif /* !_CGI_H_ */

/*
 * End of "$Id: cgi.h,v 1.12.2.5 2004/06/29 13:15:08 mike Exp $".
 */
