/*
 * "$Id: classes.c,v 1.34.2.27 2004/10/04 19:48:56 mike Exp $"
 *
 *   Printer class routines for the Common UNIX Printing System (CUPS).
 *
 *   Copyright 1997-2004 by Easy Software Products, all rights reserved.
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
 *   AddClass()                 - Add a class to the system.
 *   AddPrinterToClass()        - Add a printer to a class...
 *   DeletePrinterFromClass()   - Delete a printer from a class.
 *   DeletePrinterFromClasses() - Delete a printer from all classes.
 *   DeleteAllClasses()         - Remove all classes from the system.
 *   FindAvailablePrinter()     - Find an available printer in a class.
 *   FindClass()                - Find the named class.
 *   LoadAllClasses()           - Load classes from the classes.conf file.
 *   SaveAllClasses()           - Save classes to the classes.conf file.
 *   UpdateImplicitClasses()    - Update the accepting state of implicit
 *                                classes.
 */

/*
 * Include necessary headers...
 */

#include "cupsd.h"


/*
 * 'AddClass()' - Add a class to the system.
 */

printer_t *			/* O - New class */
AddClass(const char *name)	/* I - Name of class */
{
  printer_t	*c;		/* New class */


 /*
  * Add the printer and set the type to "class"...
  */

  if ((c = AddPrinter(name)) != NULL)
  {
   /*
    * Change from a printer to a class...
    */

    c->type = CUPS_PRINTER_CLASS;

    SetStringf(&c->uri, "ipp://%s:%d/classes/%s", ServerName, LocalPort, name);
    SetString(&c->error_policy, "retry-job");
  }

  return (c);
}


/*
 * 'AddPrinterToClass()' - Add a printer to a class...
 */

void
AddPrinterToClass(printer_t *c,	/* I - Class to add to */
                  printer_t *p)	/* I - Printer to add */
{
  printer_t	**temp;		/* Pointer to printer array */


 /*
  * Allocate memory as needed...
  */

  if (c->num_printers == 0)
    temp = malloc(sizeof(printer_t *));
  else
    temp = realloc(c->printers, sizeof(printer_t *) * (c->num_printers + 1));

  if (temp == NULL)
  {
    LogMessage(L_ERROR, "Unable to add printer %s to class %s!",
               p->name, c->name);
    return;
  }

 /*
  * Add the printer to the end of the array and update the number of printers.
  */

  c->printers = temp;
  temp        += c->num_printers;
  c->num_printers ++;

  *temp = p;

 /*
  * Update the IPP attributes...
  */

  SetPrinterAttrs(c);
}


/*
 * 'DeletePrinterFromClass()' - Delete a printer from a class.
 */

void
DeletePrinterFromClass(printer_t *c,	/* I - Class to delete from */
                       printer_t *p)	/* I - Printer to delete */
{
  int		i;			/* Looping var */
  cups_ptype_t	type;			/* Class type */


 /*
  * See if the printer is in the class...
  */

  for (i = 0; i < c->num_printers; i ++)
    if (p == c->printers[i])
      break;

 /*
  * If it is, remove it from the list...
  */

  if (i < c->num_printers)
  {
   /*
    * Yes, remove the printer...
    */

    c->num_printers --;
    if (i < c->num_printers)
      memcpy(c->printers + i, c->printers + i + 1,
             (c->num_printers - i) * sizeof(printer_t *));
  }

 /*
  * Recompute the printer type mask as needed...
  */

  if (c->num_printers > 0)
  {
    type    = c->type & (CUPS_PRINTER_CLASS | CUPS_PRINTER_IMPLICIT);
    c->type = ~CUPS_PRINTER_REMOTE;

    for (i = 0; i < c->num_printers; i ++)
      c->type &= c->printers[i]->type;

    c->type |= type;

   /*
    * Update the IPP attributes...
    */

    SetPrinterAttrs(c);
  }
}


/*
 * 'DeletePrinterFromClasses()' - Delete a printer from all classes.
 */

void
DeletePrinterFromClasses(printer_t *p)	/* I - Printer to delete */
{
  printer_t	*c,			/* Pointer to current class */
		*next;			/* Pointer to next class */


 /*
  * Loop through the printer/class list and remove the printer
  * from each class listed...
  */

  for (c = Printers; c != NULL; c = next)
  {
    next = c->next;

    if (c->type & (CUPS_PRINTER_CLASS | CUPS_PRINTER_IMPLICIT))
      DeletePrinterFromClass(c, p);
  }

 /*
  * Then clean out any empty classes...
  */

  for (c = Printers; c != NULL; c = next)
  {
    next = c->next;

    if ((c->type & (CUPS_PRINTER_CLASS | CUPS_PRINTER_IMPLICIT)) &&
        c->num_printers == 0)
      DeletePrinter(c, 1);
  }
}


/*
 * 'DeleteAllClasses()' - Remove all classes from the system.
 */

void
DeleteAllClasses(void)
{
  printer_t	*c,	/* Pointer to current printer/class */
		*next;	/* Pointer to next printer in list */


  for (c = Printers; c != NULL; c = next)
  {
    next = c->next;

    if (c->type & CUPS_PRINTER_CLASS)
      DeletePrinter(c, 0);
  }
}


/*
 * 'FindAvailablePrinter()' - Find an available printer in a class.
 */

printer_t *				/* O - Available printer or NULL */
FindAvailablePrinter(const char *name)	/* I - Class to check */
{
  int		i;			/* Looping var */
  printer_t	*c;			/* Printer class */


 /*
  * Find the class...
  */

  if ((c = FindClass(name)) == NULL)
  {
    LogMessage(L_ERROR, "Unable to find class \"%s\"!", name);
    return (NULL);
  }

 /*
  * Loop through the printers in the class and return the first idle
  * printer...  We keep track of the last printer that we used so that
  * a "round robin" type of scheduling is realized (otherwise the first
  * server might be saturated with print jobs...)
  *
  * Thanks to Joel Fredrikson for helping us get this right!
  */

  for (i = c->last_printer + 1; ; i ++)
  {
    if (i >= c->num_printers)
      i = 0;

    if (c->printers[i]->accepting &&
        (c->printers[i]->state == IPP_PRINTER_IDLE ||
         ((c->printers[i]->type & CUPS_PRINTER_REMOTE) && !c->printers[i]->job)))
    {
      c->last_printer = i;
      return (c->printers[i]);
    }

    if (i == c->last_printer)
      break;
  }

  return (NULL);
}


/*
 * 'FindClass()' - Find the named class.
 */

printer_t *			/* O - Matching class or NULL */
FindClass(const char *name)	/* I - Name of class */
{
  printer_t	*c;		/* Current class/printer */
  int		diff;		/* Difference */


  for (c = Printers; c != NULL; c = c->next)
    if ((diff = strcasecmp(name, c->name)) == 0 &&
        (c->type & (CUPS_PRINTER_CLASS | CUPS_PRINTER_IMPLICIT)))
      return (c);				/* name == c->name */
    else if (diff < 0)				/* name < c->name */
      return (NULL);

  return (NULL);
}


/*
 * 'LoadAllClasses()' - Load classes from the classes.conf file.
 */

void
LoadAllClasses(void)
{
  cups_file_t	*fp;			/* classes.conf file */
  int		linenum;		/* Current line number */
  int		len;			/* Length of line */
  char		line[1024],		/* Line from file */
		name[256],		/* Parameter name */
		*nameptr,		/* Pointer into name */
		*value,			/* Pointer to value */
		*valueptr;		/* Pointer into value */
  printer_t	*p,			/* Current printer class */
		*temp;			/* Temporary pointer to printer */


 /*
  * Open the classes.conf file...
  */

  snprintf(line, sizeof(line), "%s/classes.conf", ServerRoot);
  if ((fp = cupsFileOpen(line, "r")) == NULL)
  {
    LogMessage(L_ERROR, "LoadAllClasses: Unable to open %s - %s", line,
               strerror(errno));
    return;
  }

 /*
  * Read class configurations until we hit EOF...
  */

  linenum = 0;
  p       = NULL;

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

    while (len > 0 && isspace(line[len - 1] & 255))
    {
      len --;
      line[len] = '\0';
    }

   /*
    * Extract the name from the beginning of the line...
    */

    for (value = line; isspace(*value & 255); value ++);

    for (nameptr = name; *value != '\0' && !isspace(*value & 255) &&
	                     nameptr < (name + sizeof(name) - 1);)
      *nameptr++ = *value++;
    *nameptr = '\0';

    while (isspace(*value & 255))
      value ++;

    if (name[0] == '\0')
      continue;

   /*
    * Decode the directive...
    */

    if (!strcasecmp(name, "<Class") ||
        !strcasecmp(name, "<DefaultClass"))
    {
     /*
      * <Class name> or <DefaultClass name>
      */

      if (line[len - 1] == '>' && p == NULL)
      {
        line[len - 1] = '\0';

        LogMessage(L_DEBUG, "LoadAllClasses: Loading class %s...", value);

        p = AddClass(value);
	p->accepting = 1;
	p->state     = IPP_PRINTER_IDLE;

        if (!strcasecmp(name, "<DefaultClass"))
	  DefaultPrinter = p;
      }
      else
      {
        LogMessage(L_ERROR, "Syntax error on line %d of classes.conf.",
	           linenum);
        return;
      }
    }
    else if (!strcasecmp(name, "</Class>"))
    {
      if (p != NULL)
      {
        SetPrinterAttrs(p);
        p = NULL;
      }
      else
      {
        LogMessage(L_ERROR, "Syntax error on line %d of classes.conf.",
	           linenum);
        return;
      }
    }
    else if (p == NULL)
    {
      LogMessage(L_ERROR, "Syntax error on line %d of classes.conf.",
	         linenum);
      return;
    }
    
    else if (!strcasecmp(name, "Info"))
      SetString(&p->info, value);
    else if (!strcasecmp(name, "Location"))
      SetString(&p->location, value);
    else if (!strcasecmp(name, "Printer"))
    {
      if ((temp = FindPrinter(value)) == NULL)
      {
	LogMessage(L_WARN, "Unknown printer %s on line %d of classes.conf.",
	           value, linenum);

       /*
	* Add the missing remote printer...
	*/

	if ((temp = AddPrinter(value)) != NULL)
	{
	  SetString(&temp->make_model, "Remote Printer on unknown");

          temp->state       = IPP_PRINTER_STOPPED;
	  temp->type        |= CUPS_PRINTER_REMOTE;
	  temp->browse_time = 2147483647;

	  SetString(&temp->location, "Location Unknown");
	  SetString(&temp->info, "No Information Available");
	  temp->hostname[0] = '\0';

	  SetPrinterAttrs(temp);
	}
      }

      if (temp)
        AddPrinterToClass(p, temp);
    }
    else if (!strcasecmp(name, "State"))
    {
     /*
      * Set the initial queue state...
      */

      if (strcasecmp(value, "idle"))
        p->state = IPP_PRINTER_IDLE;
      else if (strcasecmp(value, "stopped"))
        p->state = IPP_PRINTER_STOPPED;
    }
    else if (!strcasecmp(name, "StateMessage"))
    {
     /*
      * Set the initial queue state message...
      */

      while (isspace(*value & 255))
        value ++;

      strlcpy(p->state_message, value, sizeof(p->state_message));
    }
    else if (!strcasecmp(name, "Accepting"))
    {
     /*
      * Set the initial accepting state...
      */

      if (strcasecmp(value, "yes") ||
          !strcasecmp(value, "on") ||
          !strcasecmp(value, "true"))
        p->accepting = 1;
      else
        p->accepting = 0;
    }
    else if (!strcasecmp(name, "JobSheets"))
    {
     /*
      * Set the initial job sheets...
      */

      for (valueptr = value; *valueptr && !isspace(*valueptr & 255); valueptr ++);

      if (*valueptr)
        *valueptr++ = '\0';

      SetString(&p->job_sheets[0], value);

      while (isspace(*valueptr & 255))
        valueptr ++;

      if (*valueptr)
      {
        for (value = valueptr; *valueptr && !isspace(*valueptr & 255); valueptr ++);

	if (*valueptr)
          *valueptr++ = '\0';

	SetString(&p->job_sheets[1], value);
      }
    }
    else if (!strcasecmp(name, "AllowUser"))
    {
      p->deny_users = 0;
      AddPrinterUser(p, value);
    }
    else if (!strcasecmp(name, "DenyUser"))
    {
      p->deny_users = 1;
      AddPrinterUser(p, value);
    }
    else if (!strcasecmp(name, "QuotaPeriod"))
      p->quota_period = atoi(value);
    else if (!strcasecmp(name, "PageLimit"))
      p->page_limit = atoi(value);
    else if (!strcasecmp(name, "KLimit"))
      p->k_limit = atoi(value);
    else if (!strcasecmp(name, "OpPolicy"))
      SetString(&p->op_policy, value);
    else if (!strcasecmp(name, "ErrorPolicy"))
      SetString(&p->error_policy, value);
    else
    {
     /*
      * Something else we don't understand...
      */

      LogMessage(L_ERROR, "Unknown configuration directive %s on line %d of classes.conf.",
	         name, linenum);
    }
  }

  cupsFileClose(fp);
}


/*
 * 'SaveAllClasses()' - Save classes to the classes.conf file.
 */

void
SaveAllClasses(void)
{
  cups_file_t	*fp;			/* classes.conf file */
  char		temp[1024];		/* Temporary string */
  printer_t	*pclass;		/* Current printer class */
  int		i;			/* Looping var */
  time_t	curtime;		/* Current time */
  struct tm	*curdate;		/* Current date */


 /*
  * Create the classes.conf file...
  */

  snprintf(temp, sizeof(temp), "%s/classes.conf", ServerRoot);
  if ((fp = cupsFileOpen(temp, "w")) == NULL)
  {
    LogMessage(L_ERROR, "Unable to save classes.conf - %s", strerror(errno));
    return;
  }
  else
    LogMessage(L_INFO, "Saving classes.conf...");

 /*
  * Restrict access to the file...
  */

  fchown(cupsFileNumber(fp), RunUser, Group);
  fchmod(cupsFileNumber(fp), ConfigFilePerm);

 /*
  * Write a small header to the file...
  */

  curtime = time(NULL);
  curdate = localtime(&curtime);
  strftime(temp, sizeof(temp) - 1, CUPS_STRFTIME_FORMAT, curdate);

  cupsFilePuts(fp, "# Class configuration file for " CUPS_SVERSION "\n");
  cupsFilePrintf(fp, "# Written by cupsd on %s\n", temp);

 /*
  * Write each local class known to the system...
  */

  for (pclass = Printers; pclass != NULL; pclass = pclass->next)
  {
   /*
    * Skip remote destinations and regular printers...
    */

    if ((pclass->type & CUPS_PRINTER_REMOTE) ||
        (pclass->type & CUPS_PRINTER_IMPLICIT) ||
        !(pclass->type & CUPS_PRINTER_CLASS))
      continue;

   /*
    * Write printers as needed...
    */

    if (pclass == DefaultPrinter)
      cupsFilePrintf(fp, "<DefaultClass %s>\n", pclass->name);
    else
      cupsFilePrintf(fp, "<Class %s>\n", pclass->name);

    if (pclass->info)
      cupsFilePrintf(fp, "Info %s\n", pclass->info);

    if (pclass->location)
      cupsFilePrintf(fp, "Location %s\n", pclass->location);

    if (pclass->state == IPP_PRINTER_STOPPED)
    {
      cupsFilePuts(fp, "State Stopped\n");
      cupsFilePrintf(fp, "StateMessage %s\n", pclass->state_message);
    }
    else
      cupsFilePuts(fp, "State Idle\n");

    if (pclass->accepting)
      cupsFilePuts(fp, "Accepting Yes\n");
    else
      cupsFilePuts(fp, "Accepting No\n");

    for (i = 0; i < pclass->num_printers; i ++)
      cupsFilePrintf(fp, "Printer %s\n", pclass->printers[i]->name);

    cupsFilePrintf(fp, "QuotaPeriod %d\n", pclass->quota_period);
    cupsFilePrintf(fp, "PageLimit %d\n", pclass->page_limit);
    cupsFilePrintf(fp, "KLimit %d\n", pclass->k_limit);

    for (i = 0; i < pclass->num_users; i ++)
      cupsFilePrintf(fp, "%sUser %s\n", pclass->deny_users ? "Deny" : "Allow",
        	     pclass->users[i]);

    cupsFilePrintf(fp, "OpPolicy %s\n", pclass->op_policy);
    cupsFilePrintf(fp, "ErrorPolicy %s\n", pclass->error_policy);

    cupsFilePuts(fp, "</Class>\n");
  }

  cupsFileClose(fp);
}


/*
 * 'UpdateImplicitClasses()' - Update the accepting state of implicit classes.
 */

void
UpdateImplicitClasses(void)
{
  int		i;			/* Looping var */
  printer_t	*pclass;		/* Current class */
  int		accepting;		/* printer-is-accepting-jobs value */


  for (pclass = Printers; pclass; pclass = pclass->next)
    if (pclass->type & CUPS_PRINTER_IMPLICIT)
    {
     /*
      * Implicit class, loop through the printers to come up with a
      * composite state...
      */

      for (i = 0, accepting = 0; i < pclass->num_printers; i ++)
        if ((accepting |= pclass->printers[i]->accepting) != 0)
	  break;

      pclass->accepting = accepting;
    }
}


/*
 * End of "$Id: classes.c,v 1.34.2.27 2004/10/04 19:48:56 mike Exp $".
 */
