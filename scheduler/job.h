/*
 * "$Id: job.h,v 1.25.2.4 2002/03/21 02:34:28 mike Exp $"
 *
 *   Print job definitions for the Common UNIX Printing System (CUPS) scheduler.
 *
 *   Copyright 1997-2002 by Easy Software Products, all rights reserved.
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
 * Constants...
 */

#define JOB_BUFFER_SIZE	1024	/* Bytes for job status buffer */


/*
 * Job request structure...
 */

typedef struct job_str
{
  struct job_str *next;			/* Next job in queue */
  int		id,			/* Job ID */
		priority;		/* Job priority */
  ipp_attribute_t *state;		/* Job state */
  ipp_attribute_t *sheets;		/* job-media-sheets-completed */
  time_t	hold_until;		/* Hold expiration date/time */
  char		username[33];		/* Printing user */
  char		dest[IPP_MAX_NAME];	/* Destination printer or class */
  cups_ptype_t	dtype;			/* Destination type (class/remote bits) */
  char		title[IPP_MAX_NAME];	/* Job name/title */
  ipp_attribute_t *job_sheets;		/* Job sheets (NULL if none) */
  int		num_files;		/* Number of files in job */
  int		current_file;		/* Current file in job */
  mime_type_t	**filetypes;		/* File types */
  ipp_t		*attrs;			/* Job attributes */
  int		status_pipe;		/* Status pipe for this job */
  int		print_pipes[2],		/* Print data pipes */
		back_pipes[2];		/* Backchannel pipes */
  int		cost;			/* Filtering cost */
  int		filters[MAX_FILTERS + 1];/* Filter process IDs, 0 terminated */
  int		backend;		/* Backend process ID */
  int		status;			/* Status code from filters */
  printer_t	*printer;		/* Printer this job is assigned to */
  char		*buffer;		/* Status buffer */
  int		bufused;		/* Amount of buffer in use */
} job_t;


/*
 * Globals...
 */

VAR int		JobHistory	VALUE(1);	/* Preserve job history? */
VAR int		JobFiles	VALUE(0);	/* Preserve job files? */
VAR int		MaxJobs		VALUE(0),	/* Max number of jobs */
		MaxActiveJobs	VALUE(0),	/* Max number of active jobs */
		MaxJobsPerUser	VALUE(0),	/* Max jobs per user */
		MaxJobsPerPrinter VALUE(0);	/* Max jobs per printer */
VAR int		JobAutoPurge	VALUE(0);	/* Automatically purge jobs */
VAR int		NumJobs		VALUE(0),	/* Number of jobs in queue */
		ActiveJobs	VALUE(0);	/* Number of active jobs */
VAR job_t	*Jobs		VALUE(NULL);	/* List of current jobs */
VAR int		NextJobId	VALUE(1);	/* Next job ID to use */


/*
 * Prototypes...
 */

extern job_t	*AddJob(int priority, const char *dest);
extern void	CancelJob(int id, int purge);
extern void	CancelJobs(const char *dest);
extern void	CheckJobs(void);
extern void	CleanJobs(void);
extern void	DeleteJob(int id);
extern job_t	*FindJob(int id);
extern int	GetPrinterJobCount(const char *dest);
extern int	GetUserJobCount(const char *username);
extern void	HoldJob(int id);
extern void	LoadAllJobs(void);
extern void	MoveJob(int id, const char *dest);
extern void	ReleaseJob(int id);
extern void	RestartJob(int id);
extern void	SaveJob(int id);
extern void	SetJobHoldUntil(int id, const char *when);
extern void	SetJobPriority(int id, int priority);
extern void	StartJob(int id, printer_t *printer);
extern void	StopAllJobs(void);
extern void	StopJob(int id, int force);
extern void	UpdateJob(job_t *job);


/*
 * End of "$Id: job.h,v 1.25.2.4 2002/03/21 02:34:28 mike Exp $".
 */
