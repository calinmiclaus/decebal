/*
    libparted - a library for manipulating disk partitions
    Copyright (C) 2000 Free Software Foundation, Inc.

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

#if ENABLE_NLS
#  include <libintl.h>
#  define _(String) dgettext (PACKAGE, String)
#else
#  define _(String) (String)
#endif /* ENABLE_NLS */

#ifdef DEBUG

int
ped_assert (int cond, const char* cond_text,
	    const char* file, int line, const char* function)
{
	PedExceptionOption	opt;

	if (cond)
		return 1;

	opt = ped_exception_throw (
		PED_EXCEPTION_BUG,
		PED_EXCEPTION_IGNORE_CANCEL,
		_("Assertion (%s) at %s:%d in function %s() failed."),
		cond_text, file, line, function);

	return opt == PED_EXCEPTION_IGNORE;
}

#endif /* DEBUG */

