/*
 * "$Id: conf.c,v 1.77.2.37 2003/04/10 14:13:52 mike Exp $"
 *
 *   Configuration routines for the Common UNIX Printing System (CUPS).
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
 * Contents:
 *
 *   ReadConfiguration()  - Read the cupsd.conf file.
 *   read_configuration() - Read a configuration file.
 *   read_location()      - Read a <Location path> definition.
 *   get_address()        - Get an address + port number from a line.
 *   get_addr_and_mask()  - Get an IP address and netmask.
 *   CDSAGetServerCerts() - Convert a keychain name into the CFArrayRef
 *                          required by SSLSetCertificate.
 */

/*
 * Include necessary headers...
 */

#include "cupsd.h"
#include <stdarg.h>
#include <pwd.h>
#include <grp.h>

#ifdef HAVE_CDSASSL
#  include <Security/SecureTransport.h>
#  include <Security/SecIdentitySearch.h>
#endif /* HAVE_CDSASSL */

#ifdef HAVE_VSYSLOG
#  include <syslog.h>
#endif /* HAVE_VSYSLOG */


/*
 * Possibly missing network definitions...
 */

#ifndef INADDR_NONE
#  define INADDR_NONE	0xffffffff
#endif /* !INADDR_NONE */


/*
 * Some OS's don't have hstrerror(), most notably Solaris...
 */

#ifndef HAVE_HSTRERROR
const char *					/* O - Error string */
cups_hstrerror(int error)			/* I - Error number */
{
  static const char * const errors[] =
		{
		  "OK",
		  "Host not found.",
		  "Try again.",
		  "Unrecoverable lookup error.",
		  "No data associated with name."
		};


  if (error < 0 || error > 4)
    return ("Unknown hostname lookup error.");
  else
    return (errors[error]);
}
#elif defined(_AIX)
/*
 * AIX doesn't provide a prototype but does provide the function...
 */
extern const char *hstrerror(int);
#endif /* !HAVE_HSTRERROR */


/*
 * Configuration variable structure...
 */

typedef struct
{
  char	*name;		/* Name of variable */
  void	*ptr;		/* Pointer to variable */
  int	type;		/* Type (int, string, address) */
} var_t;

#define VAR_INTEGER	0
#define VAR_STRING	1
#define VAR_BOOLEAN	2


/*
 * Local globals...
 */

static var_t	variables[] =
{
  { "AccessLog",		&AccessLog,		VAR_STRING },
  { "AutoPurgeJobs", 		&JobAutoPurge,		VAR_BOOLEAN },
  { "BrowseInterval",		&BrowseInterval,	VAR_INTEGER },
  { "BrowsePort",		&BrowsePort,		VAR_INTEGER },
  { "BrowseShortNames",		&BrowseShortNames,	VAR_BOOLEAN },
  { "BrowseTimeout",		&BrowseTimeout,		VAR_INTEGER },
  { "Browsing",			&Browsing,		VAR_BOOLEAN },
  { "Classification",		&Classification,	VAR_STRING },
  { "ClassifyOverride",		&ClassifyOverride,	VAR_BOOLEAN },
  { "ConfigFilePerm",		&ConfigFilePerm,	VAR_INTEGER },
  { "DataDir",			&DataDir,		VAR_STRING },
  { "DefaultCharset",		&DefaultCharset,	VAR_STRING },
  { "DefaultLanguage",		&DefaultLanguage,	VAR_STRING },
  { "DocumentRoot",		&DocumentRoot,		VAR_STRING },
  { "ErrorLog",			&ErrorLog,		VAR_STRING },
  { "FileDevice",		&FileDevice,		VAR_BOOLEAN },
  { "FilterLimit",		&FilterLimit,		VAR_INTEGER },
  { "FilterNice",		&FilterNice,		VAR_INTEGER },
  { "FontPath",			&FontPath,		VAR_STRING },
  { "HideImplicitMembers",	&HideImplicitMembers,	VAR_BOOLEAN },
  { "ImplicitClasses",		&ImplicitClasses,	VAR_BOOLEAN },
  { "ImplicitAnyClasses",	&ImplicitAnyClasses,	VAR_BOOLEAN },
  { "KeepAliveTimeout",		&KeepAliveTimeout,	VAR_INTEGER },
  { "KeepAlive",		&KeepAlive,		VAR_BOOLEAN },
  { "LimitRequestBody",		&MaxRequestSize,	VAR_INTEGER },
  { "ListenBackLog",		&ListenBackLog,		VAR_INTEGER },
  { "LogFilePerm",		&LogFilePerm,		VAR_INTEGER },
  { "MaxActiveJobs",		&MaxActiveJobs,		VAR_INTEGER },
  { "MaxClients",		&MaxClients,		VAR_INTEGER },
  { "MaxClientsPerHost",	&MaxClientsPerHost,	VAR_INTEGER },
  { "MaxCopies",		&MaxCopies,		VAR_INTEGER },
  { "MaxJobs",			&MaxJobs,		VAR_INTEGER },
  { "MaxJobsPerPrinter",	&MaxJobsPerPrinter,	VAR_INTEGER },
  { "MaxJobsPerUser",		&MaxJobsPerUser,	VAR_INTEGER },
  { "MaxLogSize",		&MaxLogSize,		VAR_INTEGER },
  { "MaxPrinterHistory",	&MaxPrinterHistory,	VAR_INTEGER },
  { "MaxRequestSize",		&MaxRequestSize,	VAR_INTEGER },
  { "PageLog",			&PageLog,		VAR_STRING },
  { "PreserveJobFiles",		&JobFiles,		VAR_BOOLEAN },
  { "PreserveJobHistory",	&JobHistory,		VAR_BOOLEAN },
  { "Printcap",			&Printcap,		VAR_STRING },
  { "PrintcapGUI",		&PrintcapGUI,		VAR_STRING },
  { "RemoteRoot",		&RemoteRoot,		VAR_STRING },
  { "RequestRoot",		&RequestRoot,		VAR_STRING },
  { "RIPCache",			&RIPCache,		VAR_STRING },
  { "RunAsUser", 		&RunAsUser,		VAR_BOOLEAN },
  { "RootCertDuration",		&RootCertDuration,	VAR_INTEGER },
  { "ServerAdmin",		&ServerAdmin,		VAR_STRING },
  { "ServerBin",		&ServerBin,		VAR_STRING },
#ifdef HAVE_SSL
  { "ServerCertificate",	&ServerCertificate,	VAR_STRING },
#  if defined(HAVE_LIBSSL) || defined(HAVE_GNUTLS)
  { "ServerKey",		&ServerKey,		VAR_STRING },
#  endif /* HAVE_LIBSSL || HAVE_GNUTLS */
#endif /* HAVE_SSL */
  { "ServerName",		&ServerName,		VAR_STRING },
  { "ServerRoot",		&ServerRoot,		VAR_STRING },
  { "TempDir",			&TempDir,		VAR_STRING },
  { "Timeout",			&Timeout,		VAR_INTEGER }
};
#define NUM_VARS	(sizeof(variables) / sizeof(variables[0]))


static unsigned		ones[4] =
			{
			  0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff
			};
static unsigned		zeros[4] =
			{
			  0x00000000, 0x00000000, 0x00000000, 0x00000000
			};

#ifdef HAVE_CDSASSL
static CFArrayRef CDSAGetServerCerts();
#endif /* HAVE_CDSASSL */


/*
 * Local functions...
 */

static int	read_configuration(cups_file_t *fp);
static int	read_location(cups_file_t *fp, char *name, int linenum);
static int	get_address(const char *value, unsigned defaddress, int defport,
		            int deffamily, http_addr_t *address);
static int	get_addr_and_mask(const char *value, unsigned *ip,
		                  unsigned *mask);


/*
 * 'ReadConfiguration()' - Read the cupsd.conf file.
 */

int					/* O - 1 on success, 0 otherwise */
ReadConfiguration(void)
{
  int		i;			/* Looping var */
  cups_file_t	*fp;			/* Configuration file */
  int		status;			/* Return status */
  char		temp[1024],		/* Temporary buffer */
		*slash;			/* Directory separator */
  char		type[MIME_MAX_SUPER + MIME_MAX_TYPE];
					/* MIME type name */
  char		*language;		/* Language string */
  struct passwd	*user;			/* Default user */
  struct group	*group;			/* Default group */
  int		run_user;		/* User that will be running cupsd */


 /*
  * Shutdown the server...
  */

  StopServer();

 /*
  * Free all memory...
  */

  FreeAllJobs();
  DeleteAllClasses();
  DeleteAllLocations();
  DeleteAllPrinters();

  DefaultPrinter = NULL;

  if (Devices)
  {
    ippDelete(Devices);
    Devices = NULL;
  }

  if (PPDs)
  {
    ippDelete(PPDs);
    PPDs = NULL;
  }

  if (MimeDatabase != NULL)
    mimeDelete(MimeDatabase);

  if (NumMimeTypes)
  {
    for (i = 0; i < NumMimeTypes; i ++)
      free((void *)MimeTypes[i]);

    free(MimeTypes);
  }

  if (NumBrowsers > 0)
  {
    free(Browsers);

    NumBrowsers = 0;
  }

  if (NumPolled > 0)
  {
    free(Polled);

    NumPolled = 0;
  }

  if (NumRelays > 0)
  {
    for (i = 0; i < NumRelays; i ++)
      if (Relays[i].from.type == AUTH_NAME)
	free(Relays[i].from.mask.name.name);

    free(Relays);

    NumRelays = 0;
  }

  if (NumListeners > 0)
  {
    free(Listeners);

    NumListeners = 0;
  }

 /*
  * Reset the current configuration to the defaults...
  */

  NeedReload = FALSE;

 /*
  * String options...
  */

  gethostname(temp, sizeof(temp));
  SetString(&ServerName, temp);
  SetStringf(&ServerAdmin, "root@%s", temp);
  SetString(&ServerBin, CUPS_SERVERBIN);
  SetString(&RequestRoot, CUPS_REQUESTS);
  SetString(&DocumentRoot, CUPS_DOCROOT);
  SetString(&DataDir, CUPS_DATADIR);
  SetString(&AccessLog, CUPS_LOGDIR "/access_log");
  SetString(&ErrorLog, CUPS_LOGDIR "/error_log");
  SetString(&PageLog, CUPS_LOGDIR "/page_log");
  SetString(&Printcap, "/etc/printcap");
  SetString(&PrintcapGUI, "/usr/bin/glpoptions");
  SetString(&FontPath, CUPS_FONTPATH);
  SetString(&RemoteRoot, "remroot");

  strlcpy(temp, ConfigurationFile, sizeof(temp));
  if ((slash = strrchr(temp, '/')) != NULL)
    *slash = '\0';

  SetString(&ServerRoot, temp);

  ClearString(&Classification);
  ClassifyOverride  = 0;

#ifdef HAVE_SSL
#  ifdef HAVE_CDSASSL
  if (ServerCertificatesArray)
  {
    CFRelease(ServerCertificatesArray);
    ServerCertificatesArray = NULL;
  }

  SetString(&ServerCertificate, "/var/root/Library/Keychains/CUPS");

#  else
  SetString(&ServerCertificate, "ssl/server.crt");
  SetString(&ServerKey, "ssl/server.key");
#  endif /* HAVE_CDSASSL */
#endif /* HAVE_SSL */

  if ((language = DEFAULT_LANGUAGE) == NULL)
    language = "en";
  else if (strcmp(language, "C") == 0 || strcmp(language, "POSIX") == 0)
    language = "en";

  SetString(&DefaultLanguage, language);
  SetString(&DefaultCharset, DEFAULT_CHARSET);

  SetString(&RIPCache, "8m");

  if (getenv("TMPDIR") == NULL)
    SetString(&TempDir, CUPS_REQUESTS "/tmp");
  else
    SetString(&TempDir, getenv("TMPDIR"));

 /*
  * Find the default system group: "sys", "system", or "root"...
  */

  group = getgrnam(CUPS_DEFAULT_GROUP);
  endgrent();

  NumSystemGroups = 0;

  if (group != NULL)
  {
    SetString(&SystemGroups[0], CUPS_DEFAULT_GROUP);
    Group = group->gr_gid;
  }
  else
  {
    group = getgrgid(0);
    endgrent();

    if (group != NULL)
    {
      SetString(&SystemGroups[0], group->gr_name);
      Group = 0;
    }
    else
    {
      SetString(&SystemGroups[0], "unknown");
      Group = 0;
    }
  }

 /*
  * Find the default user...
  */

  if ((user = getpwnam(CUPS_DEFAULT_USER)) == NULL)
    User = 1;	/* Force to a non-priviledged account */
  else
    User = user->pw_uid;

  endpwent();

 /*
  * Numeric options...
  */

  ConfigFilePerm      = 0640;
  LogFilePerm         = 0644;

  FileDevice          = FALSE;
  FilterLevel         = 0;
  FilterLimit         = 0;
  FilterNice          = 0;
  HostNameLookups     = FALSE;
  ImplicitClasses     = TRUE;
  ImplicitAnyClasses  = FALSE;
  HideImplicitMembers = TRUE;
  KeepAlive           = TRUE;
  KeepAliveTimeout    = DEFAULT_KEEPALIVE;
  ListenBackLog       = SOMAXCONN;
  LogLevel            = L_ERROR;
  MaxClients          = 100;
  MaxClientsPerHost   = 0;
  MaxLogSize          = 1024 * 1024;
  MaxPrinterHistory   = 10;
  MaxRequestSize      = 0;
  RootCertDuration    = 300;
  RunAsUser           = FALSE;
  Timeout             = DEFAULT_TIMEOUT;

  BrowseInterval      = DEFAULT_INTERVAL;
  BrowsePort          = ippPort();
  BrowseProtocols     = BROWSE_CUPS;
  BrowseShortNames    = TRUE;
  BrowseTimeout       = DEFAULT_TIMEOUT;
  Browsing            = TRUE;

  JobHistory          = DEFAULT_HISTORY;
  JobFiles            = DEFAULT_FILES;
  JobAutoPurge        = 0;
  MaxJobs             = 0;
  MaxActiveJobs       = 0;
  MaxJobsPerPrinter   = 0;
  MaxJobsPerUser      = 0;
  MaxCopies           = 100;

 /*
  * Read the configuration file...
  */

  if ((fp = cupsFileOpen(ConfigurationFile, "r")) == NULL)
    return (0);

  status = read_configuration(fp);

  cupsFileClose(fp);

  if (!status)
    return (0);

  if (RunAsUser)
    run_user = User;
  else
    run_user = getuid();

 /*
  * Use the default system group if none was supplied in cupsd.conf...
  */

  if (NumSystemGroups == 0)
    NumSystemGroups ++;

 /*
  * Get the access control list for browsing...
  */

  BrowseACL = FindLocation("CUPS_INTERNAL_BROWSE_ACL");

 /*
  * Open the system log for cupsd if necessary...
  */

#ifdef HAVE_VSYSLOG
  if (strcmp(AccessLog, "syslog") == 0 ||
      strcmp(ErrorLog, "syslog") == 0 ||
      strcmp(PageLog, "syslog") == 0)
    openlog("cupsd", LOG_PID | LOG_NOWAIT | LOG_NDELAY, LOG_LPR);
#endif /* HAVE_VSYSLOG */

 /*
  * Log the configuration file that was used...
  */

  LogMessage(L_DEBUG, "ReadConfiguration() ConfigurationFile=\"%s\"",
             ConfigurationFile);

 /*
  * Update all relative filenames to include the full path from ServerRoot...
  */

  if (DocumentRoot[0] != '/')
    SetStringf(&DocumentRoot, "%s/%s", ServerRoot, DocumentRoot);

  if (RequestRoot[0] != '/')
    SetStringf(&RequestRoot, "%s/%s", ServerRoot, RequestRoot);

  if (ServerBin[0] != '/')
    SetStringf(&ServerBin, "%s/%s", ServerRoot, ServerBin);

#ifdef HAVE_SSL
  if (ServerCertificate[0] != '/')
    SetStringf(&ServerCertificate, "%s/%s", ServerRoot, ServerCertificate);

#  if defined(HAVE_LIBSSL) || defined(HAVE_GNUTLS)
  chown(ServerCertificate, run_user, Group);
  chmod(ServerCertificate, ConfigFilePerm);

  if (ServerKey[0] != '/')
    SetStringf(&ServerKey, "%s/%s", ServerRoot, ServerKey);

  chown(ServerKey, run_user, Group);
  chmod(ServerKey, ConfigFilePerm);
#  endif /* HAVE_LIBSSL || HAVE_GNUTLS */
#endif /* HAVE_SSL */

 /*
  * Make sure that ServerRoot and the config files are owned and
  * writable by the user and group in the cupsd.conf file...
  */

  chown(ServerRoot, run_user, Group);
  chmod(ServerRoot, 0775);

  snprintf(temp, sizeof(temp), "%s/certs", ServerRoot);
  chown(temp, run_user, Group);
  chmod(temp, 0711);

  snprintf(temp, sizeof(temp), "%s/ppd", ServerRoot);
  chown(temp, run_user, Group);
  chmod(temp, 0755);

  snprintf(temp, sizeof(temp), "%s/ssl", ServerRoot);
  chown(temp, run_user, Group);
  chmod(temp, 0700);

  snprintf(temp, sizeof(temp), "%s/cupsd.conf", ServerRoot);
  chown(temp, run_user, Group);
  chmod(temp, ConfigFilePerm);

  snprintf(temp, sizeof(temp), "%s/classes.conf", ServerRoot);
  chown(temp, run_user, Group);
#ifdef __APPLE__
  chmod(temp, 0600);
#else
  chmod(temp, ConfigFilePerm);
#endif /* __APPLE__ */

  snprintf(temp, sizeof(temp), "%s/printers.conf", ServerRoot);
  chown(temp, run_user, Group);
#ifdef __APPLE__
  chmod(temp, 0600);
#else
  chmod(temp, ConfigFilePerm);
#endif /* __APPLE__ */

  snprintf(temp, sizeof(temp), "%s/passwd.md5", ServerRoot);
  chown(temp, User, Group);
  chmod(temp, 0600);

 /*
  * Make sure the request and temporary directories have the right
  * permissions...
  */

  chown(RequestRoot, run_user, Group);
  chmod(RequestRoot, 0710);

  if (strncmp(TempDir, RequestRoot, strlen(RequestRoot)) == 0)
  {
   /*
    * Only update ownership and permissions if the CUPS temp directory
    * is under the spool directory...
    */

    chown(TempDir, run_user, Group);
    chmod(TempDir, 01770);
  }

 /*
  * Check the MaxClients setting, and then allocate memory for it...
  */

  if (MaxClients > (MaxFDs / 3) || MaxClients <= 0)
  {
    if (MaxClients > 0)
      LogMessage(L_INFO, "MaxClients limited to 1/3 of the file descriptor limit (%d)...",
                 MaxFDs);

    MaxClients = MaxFDs / 3;
  }

  if ((Clients = calloc(sizeof(client_t), MaxClients)) == NULL)
  {
    LogMessage(L_ERROR, "ReadConfiguration: Unable to allocate memory for %d clients: %s",
               MaxClients, strerror(errno));
    exit(1);
  }
  else
    LogMessage(L_INFO, "Configured for up to %d clients.", MaxClients);

 /*
  * Check the MaxActiveJobs setting; limit to 1/3 the available
  * file descriptors, since we need a pipe for each job...
  */

  if (MaxActiveJobs > (MaxFDs / 3))
    MaxActiveJobs = MaxFDs / 3;

  if (Classification && strcasecmp(Classification, "none") == 0)
    ClearString(&Classification);

  if (Classification)
    LogMessage(L_INFO, "Security set to \"%s\"", Classification);

 /*
  * Update the MaxClientsPerHost value, as needed...
  */

  if (MaxClientsPerHost <= 0)
    MaxClientsPerHost = MaxClients;

  if (MaxClientsPerHost > MaxClients)
    MaxClientsPerHost = MaxClients;

  LogMessage(L_INFO, "Allowing up to %d client connections per host.",
             MaxClientsPerHost);

 /*
  * Read the MIME type and conversion database...
  */

  snprintf(temp, sizeof(temp), "%s/filter", ServerBin);

  MimeDatabase = mimeNew();
  mimeMerge(MimeDatabase, ServerRoot, temp);

 /*
  * Create a list of MIME types for the document-format-supported
  * attribute...
  */

  NumMimeTypes = MimeDatabase->num_types;
  if (!mimeType(MimeDatabase, "application", "octet-stream"))
    NumMimeTypes ++;

  MimeTypes = calloc(NumMimeTypes, sizeof(const char *));

  for (i = 0; i < MimeDatabase->num_types; i ++)
  {
    snprintf(type, sizeof(type), "%s/%s", MimeDatabase->types[i]->super,
             MimeDatabase->types[i]->type);

    MimeTypes[i] = strdup(type);
  }

  if (i < NumMimeTypes)
    MimeTypes[i] = strdup("application/octet-stream");

 /*
  * Load banners...
  */

  snprintf(temp, sizeof(temp), "%s/banners", DataDir);
  LoadBanners(temp);

 /*
  * Load printers and classes...
  */

  LoadAllPrinters();
  LoadAllClasses();

 /*
  * Load devices and PPDs...
  */

  snprintf(temp, sizeof(temp), "%s/model", DataDir);
  LoadPPDs(temp);

  snprintf(temp, sizeof(temp), "%s/backend", ServerBin);
  LoadDevices(temp);

#ifdef HAVE_CDSASSL
  ServerCertificatesArray = CDSAGetServerCerts();
#endif /* HAVE_CDSASSL */

 /*
  * Startup the server...
  */

  StartServer();

 /*
  * Load queued jobs...
  */

  LoadAllJobs();

  return (1);
}


/*
 * 'read_configuration()' - Read a configuration file.
 */

static int				/* O - 1 on success, 0 on failure */
read_configuration(cups_file_t *fp)	/* I - File to read from */
{
  int		i;			/* Looping var */
  int		linenum;		/* Current line number */
  int		len;			/* Length of line */
  char		line[HTTP_MAX_BUFFER],	/* Line from file */
		name[256],		/* Parameter name */
		*nameptr,		/* Pointer into name */
		*value;			/* Pointer to value */
  int		valuelen;		/* Length of value */
  var_t		*var;			/* Current variable */
  unsigned	ip[4],			/* Address value */
		mask[4];		/* Netmask value */
  dirsvc_relay_t *relay;		/* Relay data */
  dirsvc_poll_t	*poll;			/* Polling data */
  http_addr_t	polladdr;		/* Polling address */
  location_t	*location;		/* Browse location */
  cups_file_t	*incfile;		/* Include file */
  char		incname[1024];		/* Include filename */


 /*
  * Loop through each line in the file...
  */

  linenum = 0;

  while (cupsFileGets(fp, line, sizeof(line)) != NULL)
  {
    linenum ++;

   /*
    * Skip comment lines...
    */

    if (line[0] == '#')
      continue;

   /*
    * Strip trailing whitespace, if any...
    */

    len = strlen(line);

    while (len > 0 && isspace(line[len - 1]))
    {
      len --;
      line[len] = '\0';
    }

   /*
    * Extract the name from the beginning of the line...
    */

    for (value = line; isspace(*value); value ++);

    for (nameptr = name; *value != '\0' && !isspace(*value) &&
                             nameptr < (name + sizeof(name) - 1);)
      *nameptr++ = *value++;
    *nameptr = '\0';

    while (isspace(*value))
      value ++;

    if (name[0] == '\0')
      continue;

   /*
    * Decode the directive...
    */

    if (strcasecmp(name, "Include") == 0)
    {
     /*
      * Include filename
      */

      if (value[0] == '/')
        strlcpy(incname, value, sizeof(incname));
      else
        snprintf(incname, sizeof(incname), "%s/%s", ServerRoot, value);

      if ((incfile = cupsFileOpen(incname, "rb")) == NULL)
        LogMessage(L_ERROR, "Unable to include config file \"%s\" - %s",
	           incname, strerror(errno));
      else
      {
        read_configuration(incfile);
	cupsFileClose(incfile);
      }
    }
    else if (strcasecmp(name, "<Location") == 0)
    {
     /*
      * <Location path>
      */

      if (line[len - 1] == '>')
      {
        line[len - 1] = '\0';

	linenum = read_location(fp, value, linenum);
	if (linenum == 0)
	  return (0);
      }
      else
      {
        LogMessage(L_ERROR, "ReadConfiguration() Syntax error on line %d.",
	           linenum);
        return (0);
      }
    }
    else if (strcasecmp(name, "Port") == 0 ||
             strcasecmp(name, "Listen") == 0)
    {
     /*
      * Add a listening address to the list...
      */

      listener_t	*temp;		/* New listeners array */


      if (NumListeners == 0)
        temp = malloc(sizeof(listener_t));
      else
        temp = realloc(Listeners, (NumListeners + 1) * sizeof(listener_t));

      if (!temp)
      {
        LogMessage(L_ERROR, "Unable to allocate %s at line %d - %s.",
	           name, linenum, strerror(errno));
        continue;
      }

      Listeners = temp;
      temp      += NumListeners;

      if (get_address(value, INADDR_ANY, IPP_PORT, AF_INET, &(temp->address)))
      {
        httpAddrString(&(temp->address), line, sizeof(line));

#ifdef AF_INET6
        if (temp->address.addr.sa_family == AF_INET6)
          LogMessage(L_INFO, "Listening to %s:%d (IPv6)", line,
                     ntohs(temp->address.ipv6.sin6_port));
	else
#endif /* AF_INET6 */
        LogMessage(L_INFO, "Listening to %s:%d", line,
                   ntohs(temp->address.ipv4.sin_port));
	NumListeners ++;
      }
      else
        LogMessage(L_ERROR, "Bad %s address %s at line %d.", name,
	           value, linenum);
    }
#ifdef HAVE_SSL
    else if (strcasecmp(name, "SSLPort") == 0 ||
             strcasecmp(name, "SSLListen") == 0)
    {
     /*
      * Add a listening address to the list...
      */

      listener_t	*temp;		/* New listeners array */


      if (NumListeners == 0)
        temp = malloc(sizeof(listener_t));
      else
        temp = realloc(Listeners, (NumListeners + 1) * sizeof(listener_t));

      if (!temp)
      {
        LogMessage(L_ERROR, "Unable to allocate %s at line %d - %s.",
	           name, linenum, strerror(errno));
        continue;
      }

      Listeners = temp;
      temp      += NumListeners;

      if (get_address(value, INADDR_ANY, IPP_PORT, AF_INET, &(temp->address)))
      {
        httpAddrString(&(temp->address), line, sizeof(line));

#ifdef AF_INET6
        if (temp->address.addr.sa_family == AF_INET6)
          LogMessage(L_INFO, "Listening to %s:%d (IPv6)", line,
                     ntohs(temp->address.ipv6.sin6_port));
	else
#endif /* AF_INET6 */
        LogMessage(L_INFO, "Listening to %s:%d", line,
                   ntohs(temp->address.ipv4.sin_port));
        temp->encryption = HTTP_ENCRYPT_ALWAYS;
	NumListeners ++;
      }
      else
        LogMessage(L_ERROR, "Bad %s address %s at line %d.", name,
	           value, linenum);
    }
#endif /* HAVE_SSL */
    else if (strcasecmp(name, "BrowseAddress") == 0)
    {
     /*
      * Add a browse address to the list...
      */

      dirsvc_addr_t	*temp;		/* New browse address array */


      if (NumBrowsers == 0)
        temp = malloc(sizeof(dirsvc_addr_t));
      else
        temp = realloc(Browsers, (NumBrowsers + 1) * sizeof(dirsvc_addr_t));

      if (!temp)
      {
        LogMessage(L_ERROR, "Unable to allocate BrowseAddress at line %d - %s.",
	           linenum, strerror(errno));
        continue;
      }

      Browsers = temp;
      temp     += NumBrowsers;

      memset(temp, 0, sizeof(dirsvc_addr_t));

      if (strcasecmp(value, "@LOCAL") == 0)
      {
       /*
	* Send browse data to all local interfaces...
	*/

	strcpy(temp->iface, "*");
	NumBrowsers ++;
      }
      else if (strncasecmp(value, "@IF(", 4) == 0)
      {
       /*
	* Send browse data to the named interface...
	*/

	strlcpy(temp->iface, value + 4, sizeof(Browsers[0].iface));

        nameptr = temp->iface + strlen(temp->iface) - 1;
        if (*nameptr == ')')
	  *nameptr = '\0';

	NumBrowsers ++;
      }
      else if (get_address(value, INADDR_NONE, BrowsePort, AF_INET, &(temp->to)))
      {
        httpAddrString(&(temp->to), line, sizeof(line));

#ifdef AF_INET6
        if (temp->to.addr.sa_family == AF_INET6)
          LogMessage(L_INFO, "Sending browsing info to %s:%d (IPv6)", line,
                     ntohs(temp->to.ipv6.sin6_port));
	else
#endif /* AF_INET6 */
        LogMessage(L_INFO, "Sending browsing info to %s:%d", line,
                   ntohs(temp->to.ipv4.sin_port));

	NumBrowsers ++;
      }
      else
        LogMessage(L_ERROR, "Bad BrowseAddress %s at line %d.", value,
	           linenum);
    }
    else if (strcasecmp(name, "BrowseOrder") == 0)
    {
     /*
      * "BrowseOrder Deny,Allow" or "BrowseOrder Allow,Deny"...
      */

      if ((location = FindLocation("CUPS_INTERNAL_BROWSE_ACL")) == NULL)
        location = AddLocation("CUPS_INTERNAL_BROWSE_ACL");

      if (location == NULL)
        LogMessage(L_ERROR, "Unable to initialize browse access control list!");
      else if (strncasecmp(value, "deny", 4) == 0)
        location->order_type = AUTH_ALLOW;
      else if (strncasecmp(value, "allow", 5) == 0)
        location->order_type = AUTH_DENY;
      else
        LogMessage(L_ERROR, "Unknown BrowseOrder value %s on line %d.",
	           value, linenum);
    }
    else if (strcasecmp(name, "BrowseProtocols") == 0)
    {
     /*
      * "BrowseProtocol name [... name]"
      */

      BrowseProtocols = 0;

      for (; *value;)
      {
        for (valuelen = 0; value[valuelen]; valuelen ++)
	  if (isspace(value[valuelen]) || value[valuelen] == ',')
	    break;

        if (value[valuelen])
        {
	  value[valuelen] = '\0';
	  valuelen ++;
	}

        if (strcasecmp(value, "cups") == 0)
	  BrowseProtocols |= BROWSE_CUPS;
        else if (strcasecmp(value, "slp") == 0)
	  BrowseProtocols |= BROWSE_SLP;
        else if (strcasecmp(value, "ldap") == 0)
	  BrowseProtocols |= BROWSE_LDAP;
        else if (strcasecmp(value, "all") == 0)
	  BrowseProtocols |= BROWSE_ALL;
	else
	{
	  LogMessage(L_ERROR, "Unknown browse protocol \"%s\" on line %d.",
	             value, linenum);
          break;
	}

        for (value += valuelen; *value; value ++)
	  if (!isspace(*value) || *value != ',')
	    break;
      }
    }
    else if (strcasecmp(name, "BrowseAllow") == 0 ||
             strcasecmp(name, "BrowseDeny") == 0)
    {
     /*
      * BrowseAllow [From] host/ip...
      * BrowseDeny [From] host/ip...
      */

      if ((location = FindLocation("CUPS_INTERNAL_BROWSE_ACL")) == NULL)
        location = AddLocation("CUPS_INTERNAL_BROWSE_ACL");

      if (location == NULL)
        LogMessage(L_ERROR, "Unable to initialize browse access control list!");
      else
      {
	if (strncasecmp(value, "from ", 5) == 0)
	{
	 /*
          * Strip leading "from"...
	  */

	  value += 5;

	  while (isspace(*value))
	    value ++;
	}

       /*
	* Figure out what form the allow/deny address takes:
	*
	*    All
	*    None
	*    *.domain.com
	*    .domain.com
	*    host.domain.com
	*    nnn.*
	*    nnn.nnn.*
	*    nnn.nnn.nnn.*
	*    nnn.nnn.nnn.nnn
	*    nnn.nnn.nnn.nnn/mm
	*    nnn.nnn.nnn.nnn/mmm.mmm.mmm.mmm
	*/

	if (strcasecmp(value, "all") == 0)
	{
	 /*
          * All hosts...
	  */

          if (strcasecmp(name, "BrowseAllow") == 0)
	    AllowIP(location, zeros, zeros);
	  else
	    DenyIP(location, zeros, zeros);
	}
	else if (strcasecmp(value, "none") == 0)
	{
	 /*
          * No hosts...
	  */

          if (strcasecmp(name, "BrowseAllow") == 0)
	    AllowIP(location, ones, zeros);
	  else
	    DenyIP(location, ones, zeros);
	}
	else if (value[0] == '*' || value[0] == '.' || !isdigit(value[0]))
	{
	 /*
          * Host or domain name...
	  */

	  if (value[0] == '*')
	    value ++;

          if (strcasecmp(name, "BrowseAllow") == 0)
	    AllowHost(location, value);
	  else
	    DenyHost(location, value);
	}
	else
	{
	 /*
          * One of many IP address forms...
	  */

          if (!get_addr_and_mask(value, ip, mask))
	  {
            LogMessage(L_ERROR, "Bad netmask value %s on line %d.",
	               value, linenum);
	    break;
	  }

          if (strcasecmp(name, "BrowseAllow") == 0)
	    AllowIP(location, ip, mask);
	  else
	    DenyIP(location, ip, mask);
	}
      }
    }
    else if (strcasecmp(name, "BrowseRelay") == 0)
    {
     /*
      * BrowseRelay [from] source [to] destination
      */

      if (NumRelays == 0)
        relay = malloc(sizeof(dirsvc_relay_t));
      else
        relay = realloc(Relays, (NumRelays + 1) * sizeof(dirsvc_relay_t));

      if (!relay)
      {
        LogMessage(L_ERROR, "Unable to allocate BrowseRelay at line %d - %s.",
	           linenum, strerror(errno));
        continue;
      }

      Relays = relay;
      relay  += NumRelays;

      memset(relay, 0, sizeof(dirsvc_relay_t));

      if (strncasecmp(value, "from ", 5) == 0)
      {
       /*
        * Strip leading "from"...
	*/

	value += 5;

	while (isspace(*value))
	  value ++;
      }

     /*
      * Figure out what form the from address takes:
      *
      *    *.domain.com
      *    .domain.com
      *    host.domain.com
      *    nnn.*
      *    nnn.nnn.*
      *    nnn.nnn.nnn.*
      *    nnn.nnn.nnn.nnn
      *    nnn.nnn.nnn.nnn/mm
      *    nnn.nnn.nnn.nnn/mmm.mmm.mmm.mmm
      */

      if (value[0] == '*' || value[0] == '.' || !isdigit(value[0]))
      {
       /*
        * Host or domain name...
	*/

	if (value[0] == '*')
	  value ++;

        relay->from.type             = AUTH_NAME;
	relay->from.mask.name.name   = strdup(value);
	relay->from.mask.name.length = strlen(value);
      }
      else
      {
       /*
        * One of many IP address forms...
	*/

        if (!get_addr_and_mask(value, ip, mask))
	{
          LogMessage(L_ERROR, "Bad netmask value %s on line %d.",
	             value, linenum);
	  break;
	}

        relay->from.type = AUTH_IP;
	memcpy(relay->from.mask.ip.address, ip,
	       sizeof(relay->from.mask.ip.address));
	memcpy(relay->from.mask.ip.netmask, mask,
	       sizeof(relay->from.mask.ip.netmask));
      }

     /*
      * Skip value and trailing whitespace...
      */

      for (; *value; value ++)
	if (isspace(*value))
	  break;

      while (isspace(*value))
        value ++;

      if (strncasecmp(value, "to ", 3) == 0)
      {
       /*
        * Strip leading "to"...
	*/

	value += 3;

	while (isspace(*value))
	  value ++;
      }

     /*
      * Get "to" address and port...
      */

      if (get_address(value, INADDR_BROADCAST, BrowsePort, AF_INET, &(relay->to)))
      {
        httpAddrString(&(relay->to), line, sizeof(line));

        if (relay->from.type == AUTH_IP)
	  snprintf(name, sizeof(name), "%u.%u.%u.%u/%u.%u.%u.%u",
		   relay->from.mask.ip.address[0],
		   relay->from.mask.ip.address[1],
		   relay->from.mask.ip.address[2],
		   relay->from.mask.ip.address[3],
		   relay->from.mask.ip.netmask[0],
		   relay->from.mask.ip.netmask[1],
		   relay->from.mask.ip.netmask[2],
		   relay->from.mask.ip.netmask[3]);
	else
	{
	  strncpy(name, relay->from.mask.name.name, sizeof(name) - 1);
	  name[sizeof(name) - 1] = '\0';
	}

#ifdef AF_INET6
        if (relay->to.addr.sa_family == AF_INET6)
          LogMessage(L_INFO, "Relaying from %s to %s:%d", name, line,
                     ntohs(relay->to.ipv6.sin6_port));
	else
#endif /* AF_INET6 */
        LogMessage(L_INFO, "Relaying from %s to %s:%d", name, line,
                   ntohs(relay->to.ipv4.sin_port));

	NumRelays ++;
      }
      else
      {
        if (relay->from.type == AUTH_NAME)
	  free(relay->from.mask.name.name);

        LogMessage(L_ERROR, "Bad relay address %s at line %d.", value, linenum);
      }
    }
    else if (strcasecmp(name, "BrowsePoll") == 0)
    {
     /*
      * BrowsePoll address[:port]
      */

      if (NumPolled == 0)
        poll = malloc(sizeof(dirsvc_poll_t));
      else
        poll = realloc(Polled, (NumPolled + 1) * sizeof(dirsvc_poll_t));

      if (!poll)
      {
        LogMessage(L_ERROR, "Unable to allocate BrowsePoll at line %d - %s.",
	           linenum, strerror(errno));
        continue;
      }

      Polled = poll;
      poll   += NumPolled;

     /*
      * Get poll address and port...
      */

      if (get_address(value, INADDR_NONE, ippPort(), AF_INET, &polladdr))
      {
	NumPolled ++;
	memset(poll, 0, sizeof(dirsvc_poll_t));

        httpAddrString(&polladdr, poll->hostname, sizeof(poll->hostname));

#ifdef AF_INET6
        if (polladdr.addr.sa_family == AF_INET6)
          poll->port = ntohs(polladdr.ipv6.sin6_port);
	else
#endif /* AF_INET6 */
        poll->port = ntohs(polladdr.ipv4.sin_port);

        LogMessage(L_INFO, "Polling %s:%d", poll->hostname, poll->port);
      }
      else
        LogMessage(L_ERROR, "Bad poll address %s at line %d.", value, linenum);
    }
    else if (strcasecmp(name, "User") == 0)
    {
     /*
      * User ID to run as...
      */

      if (isdigit(value[0]))
        User = atoi(value);
      else
      {
        struct passwd *p;	/* Password information */

        endpwent();
	p = getpwnam(value);

	if (p != NULL)
	  User = p->pw_uid;
	else
	  LogMessage(L_WARN, "ReadConfiguration() Unknown username \"%s\"",
	             value);
      }
    }
    else if (strcasecmp(name, "Group") == 0)
    {
     /*
      * Group ID to run as...
      */

      if (isdigit(value[0]))
        Group = atoi(value);
      else
      {
        struct group *g;	/* Group information */

        endgrent();
	g = getgrnam(value);

	if (g != NULL)
	  Group = g->gr_gid;
	else
	  LogMessage(L_WARN, "ReadConfiguration() Unknown groupname \"%s\"",
	             value);
      }
    }
    else if (strcasecmp(name, "SystemGroup") == 0)
    {
     /*
      * System (admin) group(s)...
      */

      char *valueptr; /* Pointer into value */


      for (i = NumSystemGroups; *value && i < MAX_SYSTEM_GROUPS; i ++)
      {
        for (valueptr = value; *valueptr; valueptr ++)
	  if (isspace(*valueptr) || *valueptr == ',')
	    break;

        if (*valueptr)
          *valueptr++ = '\0';

        SetString(SystemGroups + i, value);

        value = valueptr;

        while (*value == ',' || isspace(*value))
	  value ++;
      }

      if (i)
        NumSystemGroups = i;
    }
    else if (strcasecmp(name, "HostNameLookups") == 0)
    {
     /*
      * Do hostname lookups?
      */

      if (strcasecmp(value, "off") == 0)
        HostNameLookups = 0;
      else if (strcasecmp(value, "on") == 0)
        HostNameLookups = 1;
      else if (strcasecmp(value, "double") == 0)
        HostNameLookups = 2;
      else
	LogMessage(L_WARN, "ReadConfiguration() Unknown HostNameLookups %s on line %d.",
	           value, linenum);
    }
    else if (strcasecmp(name, "LogLevel") == 0)
    {
     /*
      * Amount of logging to do...
      */

      if (strcasecmp(value, "debug2") == 0)
        LogLevel = L_DEBUG2;
      else if (strcasecmp(value, "debug") == 0)
        LogLevel = L_DEBUG;
      else if (strcasecmp(value, "info") == 0)
        LogLevel = L_INFO;
      else if (strcasecmp(value, "notice") == 0)
        LogLevel = L_NOTICE;
      else if (strcasecmp(value, "warn") == 0)
        LogLevel = L_WARN;
      else if (strcasecmp(value, "error") == 0)
        LogLevel = L_ERROR;
      else if (strcasecmp(value, "crit") == 0)
        LogLevel = L_CRIT;
      else if (strcasecmp(value, "alert") == 0)
        LogLevel = L_ALERT;
      else if (strcasecmp(value, "emerg") == 0)
        LogLevel = L_EMERG;
      else if (strcasecmp(value, "none") == 0)
        LogLevel = L_NONE;
      else
        LogMessage(L_WARN, "Unknown LogLevel %s on line %d.", value, linenum);
    }
    else if (strcasecmp(name, "PrintcapFormat") == 0)
    {
     /*
      * Format of printcap file?
      */

      if (strcasecmp(value, "bsd") == 0)
        PrintcapFormat = PRINTCAP_BSD;
      else if (strcasecmp(value, "solaris") == 0)
        PrintcapFormat = PRINTCAP_SOLARIS;
      else
	LogMessage(L_WARN, "ReadConfiguration() Unknown PrintcapFormat %s on line %d.",
	           value, linenum);
    }
    else
    {
     /*
      * Find a simple variable in the list...
      */

      for (i = NUM_VARS, var = variables; i > 0; i --, var ++)
        if (strcasecmp(name, var->name) == 0)
	  break;

      if (i == 0)
      {
       /*
        * Unknown directive!  Output an error message and continue...
	*/

        LogMessage(L_ERROR, "Unknown directive %s on line %d.", name,
	           linenum);
        continue;
      }

      switch (var->type)
      {
        case VAR_INTEGER :
	    {
	      int	n;	/* Number */
	      char	*units;	/* Units */


              n = strtol(value, &units, 0);

	      if (units && *units)
	      {
        	if (tolower(units[0]) == 'g')
		  n *= 1024 * 1024 * 1024;
        	else if (tolower(units[0]) == 'm')
		  n *= 1024 * 1024;
		else if (tolower(units[0]) == 'k')
		  n *= 1024;
		else if (tolower(units[0]) == 't')
		  n *= 262144;
	      }

	      *((int *)var->ptr) = n;
	    }
	    break;

	case VAR_BOOLEAN :
	    if (strcasecmp(value, "true") == 0 ||
	        strcasecmp(value, "on") == 0 ||
		strcasecmp(value, "enabled") == 0 ||
		strcasecmp(value, "yes") == 0 ||
		atoi(value) != 0)
              *((int *)var->ptr) = TRUE;
	    else if (strcasecmp(value, "false") == 0 ||
	             strcasecmp(value, "off") == 0 ||
		     strcasecmp(value, "disabled") == 0 ||
		     strcasecmp(value, "no") == 0 ||
		     strcasecmp(value, "0") == 0)
              *((int *)var->ptr) = FALSE;
	    else
              LogMessage(L_ERROR, "Unknown boolean value %s on line %d.",
	                 value, linenum);
	    break;

	case VAR_STRING :
	    SetString((char **)var->ptr, value);
	    break;
      }
    }
  }

  return (1);
}


/*
 * 'read_location()' - Read a <Location path> definition.
 */

static int				/* O - New line number or 0 on error */
read_location(cups_file_t *fp,		/* I - Configuration file */
              char        *location,	/* I - Location name/path */
	      int         linenum)	/* I - Current line number */
{
  int		i;			/* Looping var */
  location_t	*loc,			/* New location */
		*parent;		/* Parent location */
  int		len;			/* Length of line */
  char		line[HTTP_MAX_BUFFER],	/* Line buffer */
		name[256],		/* Configuration directive */
		*nameptr,		/* Pointer into name */
		*value,			/* Value for directive */
		*valptr;		/* Pointer into value */
  unsigned	ip[4],			/* IP address components */
 		mask[4];		/* IP netmask components */


  if ((parent = AddLocation(location)) == NULL)
    return (0);

  parent->limit = AUTH_LIMIT_ALL;
  loc           = parent;

  while (cupsFileGets(fp, line, sizeof(line)) != NULL)
  {
    linenum ++;

   /*
    * Skip comment lines...
    */

    if (line[0] == '#')
      continue;

   /*
    * Strip trailing whitespace, if any...
    */

    len = strlen(line);

    while (len > 0 && isspace(line[len - 1]))
    {
      len --;
      line[len] = '\0';
    }

   /*
    * Extract the name from the beginning of the line...
    */

    for (value = line; isspace(*value); value ++);

    for (nameptr = name; *value != '\0' && !isspace(*value) &&
                             nameptr < (name + sizeof(name) - 1);)
      *nameptr++ = *value++;
    *nameptr = '\0';

    while (isspace(*value))
      value ++;

    if (name[0] == '\0')
      continue;

   /*
    * Decode the directive...
    */

    if (strcasecmp(name, "</Location>") == 0)
      return (linenum);
    else if (strcasecmp(name, "<Limit") == 0 ||
             strcasecmp(name, "<LimitExcept") == 0)
    {
      if ((loc = CopyLocation(&parent)) == NULL)
        return (0);

      loc->limit = 0;
      while (*value)
      {
        for (valptr = value;
	     !isspace(*valptr) && *valptr != '>' && *valptr;
	     valptr ++);

	if (*valptr)
	  *valptr++ = '\0';

        if (strcmp(value, "ALL") == 0)
	  loc->limit = AUTH_LIMIT_ALL;
	else if (strcmp(value, "GET") == 0)
	  loc->limit |= AUTH_LIMIT_GET;
	else if (strcmp(value, "HEAD") == 0)
	  loc->limit |= AUTH_LIMIT_HEAD;
	else if (strcmp(value, "OPTIONS") == 0)
	  loc->limit |= AUTH_LIMIT_OPTIONS;
	else if (strcmp(value, "POST") == 0)
	  loc->limit |= AUTH_LIMIT_POST;
	else if (strcmp(value, "PUT") == 0)
	  loc->limit |= AUTH_LIMIT_PUT;
	else if (strcmp(value, "TRACE") == 0)
	  loc->limit |= AUTH_LIMIT_TRACE;
	else
	  LogMessage(L_WARN, "Unknown request type %s on line %d!", value,
	             linenum);

        for (value = valptr; isspace(*value) || *value == '>'; value ++);
      }

      if (strcasecmp(name, "<LimitExcept") == 0)
        loc->limit = AUTH_LIMIT_ALL ^ loc->limit;

      parent->limit &= ~loc->limit;
    }
    else if (strcasecmp(name, "</Limit>") == 0)
      loc = parent;
    else if (strcasecmp(name, "Encryption") == 0)
    {
     /*
      * "Encryption xxx" - set required encryption level...
      */

      if (strcasecmp(value, "never") == 0)
        loc->encryption = HTTP_ENCRYPT_NEVER;
      else if (strcasecmp(value, "always") == 0)
      {
        LogMessage(L_ERROR, "Encryption value \"%s\" on line %d is invalid in this context. "
	                    "Using \"required\" instead.", value, linenum);

        loc->encryption = HTTP_ENCRYPT_REQUIRED;
      }
      else if (strcasecmp(value, "required") == 0)
        loc->encryption = HTTP_ENCRYPT_REQUIRED;
      else if (strcasecmp(value, "ifrequested") == 0)
        loc->encryption = HTTP_ENCRYPT_IF_REQUESTED;
      else
        LogMessage(L_ERROR, "Unknown Encryption value %s on line %d.",
	           value, linenum);
    }
    else if (strcasecmp(name, "Order") == 0)
    {
     /*
      * "Order Deny,Allow" or "Order Allow,Deny"...
      */

      if (strncasecmp(value, "deny", 4) == 0)
        loc->order_type = AUTH_ALLOW;
      else if (strncasecmp(value, "allow", 5) == 0)
        loc->order_type = AUTH_DENY;
      else
        LogMessage(L_ERROR, "Unknown Order value %s on line %d.",
	           value, linenum);
    }
    else if (strcasecmp(name, "Allow") == 0 ||
             strcasecmp(name, "Deny") == 0)
    {
     /*
      * Allow [From] host/ip...
      * Deny [From] host/ip...
      */

      if (strncasecmp(value, "from", 4) == 0)
      {
       /*
        * Strip leading "from"...
	*/

	value += 4;

	while (isspace(*value))
	  value ++;
      }

     /*
      * Figure out what form the allow/deny address takes:
      *
      *    All
      *    None
      *    *.domain.com
      *    .domain.com
      *    host.domain.com
      *    nnn.*
      *    nnn.nnn.*
      *    nnn.nnn.nnn.*
      *    nnn.nnn.nnn.nnn
      *    nnn.nnn.nnn.nnn/mm
      *    nnn.nnn.nnn.nnn/mmm.mmm.mmm.mmm
      */

      if (strcasecmp(value, "all") == 0)
      {
       /*
        * All hosts...
	*/

        if (strcasecmp(name, "Allow") == 0)
	  AllowIP(loc, zeros, zeros);
	else
	  DenyIP(loc, zeros, zeros);
      }
      else  if (strcasecmp(value, "none") == 0)
      {
       /*
        * No hosts...
	*/

        if (strcasecmp(name, "Allow") == 0)
	  AllowIP(loc, ones, zeros);
	else
	  DenyIP(loc, ones, zeros);
      }
      else if (value[0] == '*' || value[0] == '.' || !isdigit(value[0]))
      {
       /*
        * Host or domain name...
	*/

	if (value[0] == '*')
	  value ++;

        if (strcasecmp(name, "Allow") == 0)
	  AllowHost(loc, value);
	else
	  DenyHost(loc, value);
      }
      else
      {
       /*
        * One of many IP address forms...
	*/

        if (!get_addr_and_mask(value, ip, mask))
	{
          LogMessage(L_ERROR, "Bad netmask value %s on line %d.",
	             value, linenum);
	  break;
	}

        if (strcasecmp(name, "Allow") == 0)
	  AllowIP(loc, ip, mask);
	else
	  DenyIP(loc, ip, mask);
      }
    }
    else if (strcasecmp(name, "AuthType") == 0)
    {
     /*
      * AuthType {none,basic,digest,basicdigest}
      */

      if (strcasecmp(value, "none") == 0)
      {
	loc->type  = AUTH_NONE;
	loc->level = AUTH_ANON;
      }
      else if (strcasecmp(value, "basic") == 0)
      {
	loc->type = AUTH_BASIC;

        if (loc->level == AUTH_ANON)
	  loc->level = AUTH_USER;
      }
      else if (strcasecmp(value, "digest") == 0)
      {
	loc->type = AUTH_DIGEST;

        if (loc->level == AUTH_ANON)
	  loc->level = AUTH_USER;
      }
      else if (strcasecmp(value, "basicdigest") == 0)
      {
	loc->type = AUTH_BASICDIGEST;

        if (loc->level == AUTH_ANON)
	  loc->level = AUTH_USER;
      }
      else
        LogMessage(L_WARN, "Unknown authorization type %s on line %d.",
	           value, linenum);
    }
    else if (strcasecmp(name, "AuthClass") == 0)
    {
     /*
      * AuthClass anonymous, user, system, group
      */

      if (strcasecmp(value, "anonymous") == 0)
      {
        loc->type  = AUTH_NONE;
        loc->level = AUTH_ANON;
      }
      else if (strcasecmp(value, "user") == 0)
        loc->level = AUTH_USER;
      else if (strcasecmp(value, "group") == 0)
        loc->level = AUTH_GROUP;
      else if (strcasecmp(value, "system") == 0)
      {
        loc->level = AUTH_GROUP;

       /*
        * Use the default system group if none is defined so far...
	*/

        if (NumSystemGroups == 0)
	  NumSystemGroups = 1;

	for (i = 0; i < NumSystemGroups; i ++)
	  AddName(loc, SystemGroups[i]);
      }
      else
        LogMessage(L_WARN, "Unknown authorization class %s on line %d.",
	           value, linenum);
    }
    else if (strcasecmp(name, "AuthGroupName") == 0)
      AddName(loc, value);
    else if (strcasecmp(name, "Require") == 0)
    {
     /*
      * Apache synonym for AuthClass and AuthGroupName...
      *
      * Get initial word:
      *
      *     Require valid-user
      *     Require group names
      *     Require user names
      */

      for (valptr = value;
	   !isspace(*valptr) && *valptr != '>' && *valptr;
	   valptr ++);

      if (*valptr)
	*valptr++ = '\0';

      if (strcasecmp(value, "valid-user") == 0 ||
          strcasecmp(value, "user") == 0)
        loc->level = AUTH_USER;
      else if (strcasecmp(value, "group") == 0)
        loc->level = AUTH_GROUP;
      else
      {
        LogMessage(L_WARN, "Unknown Require type %s on line %d.",
	           value, linenum);
	continue;
      }

     /*
      * Get the list of names from the line...
      */

      for (value = valptr; *value;)
      {
        for (valptr = value; !isspace(*valptr) && *valptr; valptr ++);

	if (*valptr)
	  *valptr++ = '\0';

        AddName(loc, value);

        for (value = valptr; isspace(*value); value ++);
      }
    }
    else if (strcasecmp(name, "Satisfy") == 0)
    {
      if (strcasecmp(value, "all") == 0)
        loc->satisfy = AUTH_SATISFY_ALL;
      else if (strcasecmp(value, "any") == 0)
        loc->satisfy = AUTH_SATISFY_ANY;
      else
        LogMessage(L_WARN, "Unknown Satisfy value %s on line %d.", value,
	           linenum);
    }
    else
      LogMessage(L_ERROR, "Unknown Location directive %s on line %d.",
	         name, linenum);
  }

  return (0);
}


/*
 * 'get_address()' - Get an address + port number from a line.
 */

static int				/* O - 1 if address good, 0 if bad */
get_address(const char  *value,		/* I - Value string */
            unsigned    defaddress,	/* I - Default address */
	    int         defport,	/* I - Default port */
	    int         deffamily,	/* I - Default family */
            http_addr_t *address)	/* O - Socket address */
{
  char			hostname[256],	/* Hostname or IP */
			portname[256];	/* Port number or name */
  struct hostent	*host;		/* Host address */
  struct servent	*port;		/* Port number */  


 /*
  * Initialize the socket address to the defaults...
  */

  memset(address, 0, sizeof(http_addr_t));

#ifdef AF_INET6
  if (deffamily == AF_INET6)
  {
    address->ipv6.sin6_family            = AF_INET6;
    address->ipv6.sin6_addr.s6_addr32[0] = htonl(defaddress);
    address->ipv6.sin6_addr.s6_addr32[1] = htonl(defaddress);
    address->ipv6.sin6_addr.s6_addr32[2] = htonl(defaddress);
    address->ipv6.sin6_addr.s6_addr32[3] = htonl(defaddress);
    address->ipv6.sin6_port              = htons(defport);
  }
  else
#endif /* AF_INET6 */
  {
    address->ipv4.sin_family      = AF_INET;
    address->ipv4.sin_addr.s_addr = htonl(defaddress);
    address->ipv4.sin_port        = htons(defport);
  }

 /*
  * Try to grab a hostname and port number...
  */

  switch (sscanf(value, "%255[^:]:%255s", hostname, portname))
  {
    case 1 :
        if (strchr(hostname, '.') == NULL && defaddress == INADDR_ANY)
	{
	 /*
	  * Hostname is a port number...
	  */

	  strlcpy(portname, hostname, sizeof(portname));
	  hostname[0] = '\0';
	}
        else
          portname[0] = '\0';
        break;

    case 2 :
        break;

    default :
	LogMessage(L_ERROR, "Unable to decode address \"%s\"!", value);
        return (0);
  }

 /*
  * Decode the hostname and port number as needed...
  */

  if (hostname[0] && strcmp(hostname, "*") != 0)
  {
    if ((host = httpGetHostByName(hostname)) == NULL)
    {
      LogMessage(L_ERROR, "httpGetHostByName(\"%s\") failed - %s!", hostname,
                 hstrerror(h_errno));
      return (0);
    }

    httpAddrLoad(host, defport, 0, address);
  }

  if (portname[0] != '\0')
  {
    if (isdigit(portname[0]))
    {
#ifdef AF_INET6
      if (address->addr.sa_family == AF_INET6)
        address->ipv6.sin6_port = htons(atoi(portname));
      else
#endif /* AF_INET6 */
      address->ipv4.sin_port = htons(atoi(portname));
    }
    else
    {
      if ((port = getservbyname(portname, NULL)) == NULL)
      {
        LogMessage(L_ERROR, "getservbyname(\"%s\") failed - %s!", portname,
                   strerror(errno));
        return (0);
      }
      else
      {
#ifdef AF_INET6
	if (address->addr.sa_family == AF_INET6)
          address->ipv6.sin6_port = htons(port->s_port);
	else
#endif /* AF_INET6 */
	address->ipv4.sin_port = htons(port->s_port);
      }
    }
  }

  return (1);
}


/*
 * 'get_addr_and_mask()' - Get an IP address and netmask.
 */

static int				/* O - 1 on success, 0 on failure */
get_addr_and_mask(const char *value,	/* I - String from config file */
                  unsigned   *ip,	/* O - Address value */
		  unsigned   *mask)	/* O - Mask value */
{
  int		i,			/* Looping var */
		family,			/* Address family */
		ipcount;		/* Count of fields in address */
  static unsigned netmasks[4][4] =	/* Standard netmasks... */
  {
    { 0xffffffff, 0x00000000, 0x00000000, 0x00000000 },
    { 0xffffffff, 0xffffffff, 0x00000000, 0x00000000 },
    { 0xffffffff, 0xffffffff, 0xffffffff, 0x00000000 },
    { 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff }
  };


 /*
  * Get the address...
  */

  memset(ip, 0, sizeof(unsigned) * 4);
  family  = AF_INET;
  ipcount = sscanf(value, "%u.%u.%u.%u", ip + 0, ip + 1, ip + 2, ip + 3);

#ifdef AF_INET6
 /*
  * See if we have any values > 255; if so, this is an IPv6 address only.
  */

  for (i = 0; i < ipcount; i ++)
    if (ip[0] > 255)
    {
      family = AF_INET6;
      break;
    }
#endif /* AF_INET6 */

  if ((value = strchr(value, '/')) != NULL)
  {
   /*
    * Get the netmask value(s)...
    */

    value ++;
    memset(mask, 0, sizeof(unsigned) * 4);
    switch (sscanf(value, "%u.%u.%u.%u", mask + 0, mask + 1,
	           mask + 2, mask + 3))
    {
      case 1 :
#ifdef AF_INET6
          if (mask[0] >= 32)
	    family = AF_INET6;

          if (family == AF_INET6)
	  {
  	    i = 128 - mask[0];

	    if (i <= 96)
	      mask[0] = 0xffffffff;
	    else
	      mask[0] = (0xffffffff << (128 - mask[0])) & 0xffffffff;

	    if (i <= 64)
	      mask[1] = 0xffffffff;
	    else if (i >= 96)
	      mask[1] = 0;
	    else
	      mask[1] = (0xffffffff << (96 - mask[0])) & 0xffffffff;

	    if (i <= 32)
	      mask[1] = 0xffffffff;
	    else if (i >= 64)
	      mask[1] = 0;
	    else
	      mask[1] = (0xffffffff << (64 - mask[0])) & 0xffffffff;

	    if (i >= 32)
	      mask[1] = 0;
	    else
	      mask[1] = (0xffffffff << (32 - mask[0])) & 0xffffffff;
          }
	  else
#endif /* AF_INET6 */
	  {
  	    i = 32 - mask[0];

	    if (i <= 24)
	      mask[0] = 0xffffffff;
	    else
	      mask[0] = (0xffffffff << (32 - mask[0])) & 0xffffffff;

	    if (i <= 16)
	      mask[1] = 0xffffffff;
	    else if (i >= 24)
	      mask[1] = 0;
	    else
	      mask[1] = (0xffffffff << (24 - mask[0])) & 0xffffffff;

	    if (i <= 8)
	      mask[1] = 0xffffffff;
	    else if (i >= 16)
	      mask[1] = 0;
	    else
	      mask[1] = (0xffffffff << (16 - mask[0])) & 0xffffffff;

	    if (i >= 8)
	      mask[1] = 0;
	    else
	      mask[1] = (0xffffffff << (8 - mask[0])) & 0xffffffff;
          }

      case 4 :
	  break;

      default :
          return (0);
    }
  }
  else
    memcpy(mask, netmasks[ipcount - 1], sizeof(unsigned) * 4);

  return (1);
}


#ifdef HAVE_CDSASSL
/*
 * 'CDSAGetServerCerts()' - Convert a keychain name into the CFArrayRef
 *                          required by SSLSetCertificate.
 *
 * For now we assumes that there is exactly one SecIdentity in the
 * keychain - i.e. there is exactly one matching cert/private key pair.
 * In the future we will search a keychain for a SecIdentity matching a
 * specific criteria.  We also skip the operation of adding additional
 * non-signing certs from the keychain to the CFArrayRef.
 *
 * To create a self-signed certificate for testing use the certtool.
 * Executing the following as root will do it:
 *
 *     certtool c c v k=CUPS
 */

static CFArrayRef
CDSAGetServerCerts(void)
{
  OSStatus		err;		/* Error info */
  SecKeychainRef 	kcRef;		/* Keychain reference */
  SecIdentitySearchRef	srchRef;	/* Search reference */
  SecIdentityRef	identity;	/* Identity */
  CFArrayRef		ca;		/* Certificate array */


  kcRef    = NULL;
  srchRef  = NULL;
  identity = NULL;
  ca       = NULL;
  err      = SecKeychainOpen(ServerCertificate, &kcRef);

  if (err)
    LogMessage(L_ERROR, "Cannot open keychain \"%s\", error %d.",
               ServerCertificate, err);
  else
  {
   /*
    * Search for "any" identity matching specified key use; 
    * in this app, we expect there to be exactly one. 
    */

    err = SecIdentitySearchCreate(kcRef, CSSM_KEYUSE_SIGN, &srchRef);

    if (err)
      LogMessage(L_ERROR,
                 "Cannot find signing key in keychain \"%s\", error %d",
                 ServerCertificate, err);
    else
    {
      err = SecIdentitySearchCopyNext(srchRef, &identity);

      if (err)
	LogMessage(L_ERROR,
	           "Cannot find signing key in keychain \"%s\", error %d",
	           ServerCertificate, err);
      else
      {
	if (CFGetTypeID(identity) != SecIdentityGetTypeID())
	  LogMessage(L_ERROR, "SecIdentitySearchCopyNext CFTypeID failure!");
	else
	{
	 /* 
	  * Found one. Place it in a CFArray. 
	  * TBD: snag other (non-identity) certs from keychain and add them
	  * to array as well.
	  */

	  ca = CFArrayCreate(NULL, (const void **)&identity, 1, NULL);

	  if (ca == nil)
	    LogMessage(L_ERROR, "CFArrayCreate error");
	}

	/*CFRelease(identity);*/
      }

      /*CFRelease(srchRef);*/
    }

    /*CFRelease(kcRef);*/
  }

  return ca;
}
#endif /* HAVE_CDSASSL */


/*
 * End of "$Id: conf.c,v 1.77.2.37 2003/04/10 14:13:52 mike Exp $".
 */
