/*
    libparted - a library for manipulating disk partitions
    Copyright (C) 1999, 2000 Free Software Foundation, Inc.

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

#include "config.h"

#include <parted/parted.h>
#include <parted/debug.h>

#define N_(String) String
#if ENABLE_NLS
#  include <libintl.h>
#  define _(String) dgettext (PACKAGE, String)
#else
#  define _(String) (String)
#endif /* ENABLE_NLS */

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>

int				ped_exception = 0;

static PedExceptionOption default_handler (PedException* ex);

static PedExceptionHandler*	ex_handler = default_handler;
static PedException*		ex = NULL;
static int			ex_fetch_count = 0;

static char*	type_strings [] = {
	N_("Information"),
	N_("Warning"),
	N_("Error"),
	N_("Fatal"),
	N_("Bug"),
	N_("No Implementation")
};

static char*	option_strings [] = {
	N_("Fix"),
	N_("Yes"),
	N_("No"),
	N_("OK"),
	N_("Retry"),
	N_("Ignore"),
	N_("Cancel")
};

char*
ped_exception_get_type_string (PedExceptionType ex_type)
{
	return type_strings [ex_type - 1];
}

static int
ped_log2 (int n)
{
	int	x;
	for (x=0; 1 << x <= n; x++);
	return x - 1;
}

char*
ped_exception_get_option_string (PedExceptionOption ex_opt)
{
	return option_strings [ped_log2 (ex_opt)];
}

static PedExceptionOption
default_handler (PedException* ex)
{
	if (ex->type == PED_EXCEPTION_BUG)
		fprintf (stderr,
			_("A bug has been detected in GNU parted.  "
			"Please email a bug report to "
			"bug-parted@gnu.org containing the version (%s) "
			"and the following message:"),
			VERSION);
	else
		fprintf (stderr, "%s: ",
			 ped_exception_get_type_string (ex->type));
	fprintf (stderr, "%s\n", ex->message);

	switch (ex->options) {
		case PED_EXCEPTION_OK:
		case PED_EXCEPTION_CANCEL:
		case PED_EXCEPTION_IGNORE:
			return ex->options;

		default:
			return PED_EXCEPTION_UNHANDLED;
	}
}

void
ped_exception_set_handler (PedExceptionHandler* handler)
{
	if (handler)
		ex_handler = handler;
	else
		ex_handler = default_handler;
}

void
ped_exception_catch ()
{
	if (ped_exception) {
		ped_exception = 0;

		ped_free (ex->message);
		ped_free (ex);
		ex = NULL;
	}
}

static PedExceptionOption
do_throw ()
{
	PedExceptionOption	ex_opt;

	ped_exception = 1;

	if (ex_fetch_count) {
		return PED_EXCEPTION_UNHANDLED;
	} else {
		ex_opt = ex_handler (ex);
		ped_exception_catch ();
		return ex_opt;
	}
}

PedExceptionOption
ped_exception_throw (PedExceptionType ex_type,
		     PedExceptionOption ex_opts, const char* message, ...)
{
	va_list		arg_list;

	if (ex)
		ped_exception_catch ();

	ex = (PedException*) malloc (sizeof (PedException));
	if (!ex)
		goto no_memory;

	ex->message = (char*) malloc (8192);
	if (!ex->message)
		goto no_memory;

	ex->type = ex_type;
	ex->options = ex_opts;

	va_start (arg_list, message);
	vsnprintf (ex->message, 8192, message, arg_list);
	va_end (arg_list);

	return do_throw ();

no_memory:
	fprintf (stderr, "Out of memory in exception handler!\n");

	va_start (arg_list, message);
	vfprintf (stderr, message, arg_list);
	va_end (arg_list);

	return PED_EXCEPTION_UNHANDLED;
}

PedExceptionOption
ped_exception_rethrow ()
{
	return do_throw ();
}

void
ped_exception_fetch_all ()
{
	ex_fetch_count++;
}

void
ped_exception_leave_all ()
{
	PED_ASSERT (ex_fetch_count > 0, return);
	ex_fetch_count--;
}
