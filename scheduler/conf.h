/*
 * "$Id: conf.h,v 1.11 1999/06/18 18:36:45 mike Exp $"
 *
 *   Configuration file definitions for the Common UNIX Printing System (CUPS)
 *   scheduler.
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
 *       44141 Airport View Drive, Suite 204
 *       Hollywood, Maryland 20636-3111 USA
 *
 *       Voice: (301) 373-9603
 *       EMail: cups-info@cups.org
 *         WWW: http://www.cups.org
 */

/*
 * Log levels...
 */

#define LOG_PAGE	-1	/* Used internally for page logging */
#define LOG_NONE	0
#define LOG_ERROR	1
#define LOG_WARN	2
#define LOG_INFO	3
#define LOG_DEBUG	4


/*
 * Globals...
 */

VAR char		ConfigurationFile[256]	VALUE(CUPS_SERVERROOT "/conf/cupsd.conf"),
					/* Configuration file to use */
			ServerName[256]		VALUE(""),
					/* FQDN for server */
			ServerAdmin[256]	VALUE(""),
					/* Administrator's email */
			ServerRoot[1024]	VALUE(CUPS_SERVERROOT),
					/* Root directory for scheduler */
			DocumentRoot[1024]	VALUE(CUPS_DATADIR "/doc"),
					/* Root directory for documents */
			SystemGroup[32]		VALUE(DEFAULT_GROUP),
					/* System group name */
			AccessLog[1024]		VALUE("logs/access_log"),
					/* Access log filename */
			ErrorLog[1024]		VALUE("logs/error_log"),
					/* Error log filename */
			PageLog[1024]		VALUE("logs/page_log"),
					/* Page log filename */
			DefaultLanguage[32]	VALUE("C"),
					/* Default language encoding */
			DefaultCharset[32]	VALUE(DEFAULT_CHARSET),
					/* Default charset */
			RIPCache[32]		VALUE("32m");
VAR int			User			VALUE(DEFAULT_UID),
					/* User ID for server */
			Group			VALUE(DEFAULT_GID),
					/* Group ID for server */
			LogLevel		VALUE(LOG_ERROR),
					/* Log level */
			MaxLogSize		VALUE(1024 * 1024),
					/* Maximum size of log files */
			MaxRequestSize		VALUE(0),
					/* Maximum size of IPP requests */
			HostNameLookups		VALUE(FALSE),
					/* Do we do reverse lookups? */
			Timeout			VALUE(DEFAULT_TIMEOUT),
					/* Timeout during requests */
			KeepAlive		VALUE(TRUE),
					/* Support the Keep-Alive option? */
			KeepAliveTimeout	VALUE(DEFAULT_KEEPALIVE),
					/* Timeout between requests */
			ImplicitClasses		VALUE(TRUE);
					/* Are classes implicitly created? */
VAR FILE		*AccessFile		VALUE(NULL),
					/* Access log file */
			*ErrorFile		VALUE(NULL),
					/* Error log file */
			*PageFile		VALUE(NULL);
					/* Page log file */
VAR mime_t		*MimeDatabase		VALUE(NULL);
					/* MIME type database */


/*
 * Prototypes...
 */

extern int	ReadConfiguration(void);
extern int	LogRequest(client_t *con, http_status_t code);
extern int	LogMessage(int level, char *message, ...);
extern int	LogPage(job_t *job, char *page);


/*
 * End of "$Id: conf.h,v 1.11 1999/06/18 18:36:45 mike Exp $".
 */
