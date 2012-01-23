/*
 * Copyright 2011 Canonical Ltd.
 *
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 3, as published
 * by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranties of
 * MERCHANTABILITY, SATISFACTORY QUALITY, or FITNESS FOR A PARTICULAR
 * PURPOSE.  See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdio.h>
#include <stdarg.h>
#include <math.h>

#include <cups/cups.h>
#include <cups/ppd.h>

#include "banner.h"
#include "pdf.h"


static float get_float_option(const char *name,
                              int noptions,
                              cups_option_t *options,
                              float def)
{
    const char *value = cupsGetOption(name, noptions, options);
    return value ? atof(value) : def;
}


static void get_pagerect(float rect[4],            /* x, y, width, height */
                         ppd_file_t *ppd,
                         int noptions,
                         cups_option_t *options)
{
    static const ppd_size_t defaultsize = {
        0,          /* marked */
        "",         /* name */
        612.0,      /* width */
        792.0,      /* length */
        18.0,       /* left */
        36.0,       /* bottom */
        594.0,      /* right */
        756.0,      /* top */
    };
    const ppd_size_t *pagesize;

    if (!(pagesize = ppdPageSize(ppd, NULL)))
        pagesize = &defaultsize;

    rect[0] = get_float_option("page-left",
                               noptions, options,
                               pagesize->left);
    rect[1] = get_float_option("page-bottom",
                               noptions, options,
                               pagesize->bottom);
    rect[2] = get_float_option("page-right",
                               noptions, options,
                               fabs(pagesize->right - pagesize->left));
    rect[3] = get_float_option("page-top",
                               noptions, options,
                               fabs(pagesize->top - pagesize->bottom));
}


static void info_linef(FILE *s,
                       const char *key,
                       const char *valuefmt, ...)
{
    va_list ap;

    va_start(ap, valuefmt);
    fprintf(s, "(%s: ", key);
    vfprintf(s, valuefmt, ap);
    fprintf(s, ") Tj T*\n");
    va_end(ap);
}


static void info_line(FILE *s,
                      const char *key,
                      const char *value)
{
    info_linef(s, key, "%s", value);
}


static info_line_time(FILE *s,
                      const char *key,
                      const char *timestamp)
{
    char buf[40];
    time_t time;

    if (timestamp) {
        strftime(buf, sizeof buf, "%c", localtime(&time));
        info_line(s, key, buf);
    }
    else
        info_line(s, key, "unknown");
}


static int generate_banner_pdf(banner_t *banner,
                               ppd_file_t *ppd,
                               const char *jobid,
                               const char *user,
                               const char *jobtitle,
                               int noptions,
                               cups_option_t *options)
{
    char *buf;
    size_t len;
    FILE *s;
    pdf_t *doc;
    float rect[4];
    ppd_attr_t *attr;

    if (!(doc = pdf_load_template(banner->template_file)))
        return 1;

    get_pagerect(rect, ppd, noptions, options);
    pdf_add_type1_font(doc, 1, "Courier");

    s = open_memstream(&buf, &len);

    if (banner->infos & INFO_IMAGEABLE_AREA) {
        fprintf(s, "q\n");
        fprintf(s, "0 0 0 RG /a0 gs\n");
        fprintf(s, "%f %f %f %f re S\n", rect[0], rect[1], rect[2], rect[3]);
        fprintf(s, "Q\n");
    }

    fprintf(s, "BT\n");
    fprintf(s, "/bannertopdf-font 14 Tf\n");
    fprintf(s, "150 400 Td\n");
    fprintf(s, "17 TL\n");

    if (banner->infos & INFO_IMAGEABLE_AREA)
        info_linef(s, "Media Limits", "%f x %f inches",
                   rect[2] / 72.0, rect[3] / 72.0);

    if (banner->infos & INFO_JOB_BILLING)
        info_line(s, "Billing Information\n",
                  cupsGetOption("job-billing", noptions, options));

    if (banner->infos & INFO_JOB_ID)
        info_linef(s, "Job ID", "%s-%s", getenv("PRINTER"), jobid);

    if (banner->infos & INFO_JOB_NAME)
        info_line(s, "Job Title", jobtitle);

    if (banner->infos & INFO_JOB_ORIGINATING_HOST_NAME)
        info_line(s, "Printed from",
                  cupsGetOption("jobs-originating-host-name",
                                noptions, options));

    if (banner->infos & INFO_JOB_ORIGINATING_USER_NAME)
        info_line(s, "Printed by", user);

    if (banner->infos & INFO_JOB_UUID)
        info_line(s, "Job UUID",
                  cupsGetOption("job-uuid", noptions, options));

    if (banner->infos & INFO_PRINTER_DRIVER_NAME)
        info_line(s, "Driver", ppd->pcfilename);

    if (banner->infos & INFO_PRINTER_DRIVER_VERSION)
        info_line(s, "Driver Version",
                  ppdFindAttr(ppd, "FileVersion", NULL) ? attr->value : "");

    if (banner->infos & INFO_PRINTER_INFO)
        info_line(s, "Description", getenv("PRINTER_INFO"));

    if (banner->infos & INFO_PRINTER_LOCATION)
        info_line(s, "Driver Version", getenv("PRINTER_INFO"));

    if (banner->infos & INFO_PRINTER_MAKE_AND_MODEL)
        info_line(s, "Make and Model", ppd->nickname);

    if (banner->infos & INFO_PRINTER_NAME)
        info_line(s, "Printer", getenv("PRINTER"));

    if (banner->infos & INFO_TIME_AT_CREATION)
        info_line_time(s, "Created at",
                       cupsGetOption("time-at-creation", noptions, options));

    if (banner->infos & INFO_TIME_AT_PROCESSING)
        info_line_time(s, "Printed at",
                       cupsGetOption("time-at-processing", noptions, options));

    fprintf(s, "ET\n");
    fclose(s);

    pdf_append_stream(doc, 1, buf, len);
    pdf_write(doc, stdout);
    free(buf);
    return 0;
}


int main(int argc, char *argv[])
{
    banner_t *banner;
    int noptions;
    cups_option_t *options;
    ppd_file_t *ppd;
    int ret;

    if (argc < 6) {
        fprintf(stderr,
                "Usage: %s job-id user job-title nr-copies options [file]\n",
                argv[0]);
        return 1;
    }

    noptions = cupsParseOptions(argv[5], 0, &options);
    ppd = ppdOpenFile(getenv("PPD"));
    ppdMarkDefaults(ppd);
    cupsMarkOptions(ppd, noptions, options);

    banner = banner_new_from_file(argc == 7 ? argv[6] : "-");

    ret = generate_banner_pdf(banner,
                              ppd,
                              argv[1],
                              argv[2],
                              argv[3],
                              noptions,
                              options);

    banner_free(banner);
    cupsFreeOptions(noptions, options);
    return ret;
}

