/*
    libparted - a library for manipulating disk partitions
    Copyright (C) 1999, 2000, 2005 Free Software Foundation, Inc.

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

#if ENABLE_NLS
#  include <libintl.h>
#  define _(String) dgettext (PACKAGE, String)
#else
#  define _(String) (String)
#endif /* ENABLE_NLS */

int
ped_geometry_init (PedGeometry* geom, const PedDevice* dev,
		   PedSector start, PedSector length)
{
	PED_ASSERT (geom != NULL, return 0);
	PED_ASSERT (dev != NULL, return 0);

	geom->dev = (PedDevice*) dev;
	return ped_geometry_set (geom, start, length);
}

PedGeometry*
ped_geometry_new (const PedDevice* dev, PedSector start, PedSector length)
{
	PedGeometry*	geom;

	PED_ASSERT (dev != NULL, return NULL);

	geom = (PedGeometry*) ped_malloc (sizeof (PedGeometry));
	if (!geom)
		goto error;
	if (!ped_geometry_init (geom, dev, start, length))
		goto error_free_geom;
	return geom;

error_free_geom:
	ped_free (geom);
error:
	return NULL;
}

/* This function constructs a PedGeometry object that is an identical but
 * independent copy of geom.  Both the input, geom, and the output
 * should be destroyed with ped_geometry_destroy() when they are no
 * longer needed.
 */
PedGeometry*
ped_geometry_duplicate (const PedGeometry* geom)
{
	PED_ASSERT (geom != NULL, return NULL);
	return ped_geometry_new (geom->dev, geom->start, geom->length);
}

/* This function constructs a PedGeometry object that describes the
 * region that is common to both a and b.  If there is no such common
 * region, it returns NULL.  (This situation is not treated as an
 * error by much of Parted.)
 */
PedGeometry*
ped_geometry_intersect (const PedGeometry* a, const PedGeometry* b)
{
	PedSector	start;
	PedSector	end;

	if (!a || !b || a->dev != b->dev)
		return NULL;

	start = PED_MAX (a->start, b->start);
	end = PED_MIN (a->end, b->end);
	if (start > end)
		return NULL;

	return ped_geometry_new (a->dev, start, end - start + 1);
}

void
ped_geometry_destroy (PedGeometry* geom)
{
	PED_ASSERT (geom != NULL, return);

	ped_free (geom);
}

int
ped_geometry_set (PedGeometry* geom, PedSector start, PedSector length)
{
	PED_ASSERT (geom != NULL, return 0);
	PED_ASSERT (geom->dev != NULL, return 0);

	if (length < 1) {
		ped_exception_throw (
			PED_EXCEPTION_ERROR,
			PED_EXCEPTION_CANCEL,
			_("Can't have the end before the start!"));
		return 0;
	}
	if (start < 0 || start + length - 1 >= geom->dev->length) {
		ped_exception_throw (
			PED_EXCEPTION_ERROR,
			PED_EXCEPTION_CANCEL,
			_("Can't have a partition outside the disk!"));
		return 0;
 	}

	geom->start = start;
	geom->length = length;
	geom->end = start + length - 1;

	return 1;
}

/* Modifies the start of geom while keeping the end fixed. */
int
ped_geometry_set_start (PedGeometry* geom, PedSector start)
{
	return ped_geometry_set (geom, start, geom->end - start + 1);
}

/* Modifies the end of geom while keeping the start fixed. */
int
ped_geometry_set_end (PedGeometry* geom, PedSector end)
{
	return ped_geometry_set (geom, geom->start, end - geom->start + 1);
}

/* Returns 1 if the regions that a and b describe overlap. */
int
ped_geometry_test_overlap (const PedGeometry* a, const PedGeometry* b)
{
	PED_ASSERT (a != NULL, return 0);
	PED_ASSERT (b != NULL, return 0);

	if (a->dev != b->dev)
		return 0;

	if (a->start < b->start)
		return a->end >= b->start;
	else
		return b->end >= a->start;
}

/* Returns 1 if the region b describes is contained entirely inside a. */
int
ped_geometry_test_inside (const PedGeometry* a, const PedGeometry* b)
{
	PED_ASSERT (a != NULL, return 0);
	PED_ASSERT (b != NULL, return 0);

	if (a->dev != b->dev)
		return 0;

	return b->start >= a->start && b->end <= a->end;
}

/* Returns 1 if a and b describe the same regions. */
int
ped_geometry_test_equal (const PedGeometry* a, const PedGeometry* b)
{
	PED_ASSERT (a != NULL, return 0);
	PED_ASSERT (b != NULL, return 0);

	return a->dev == b->dev
	       && a->start == b->start
	       && a->end == b->end;
}

/* Returns 1 if sector lies within the region that geom describes. */
int
ped_geometry_test_sector_inside (const PedGeometry* geom, PedSector sector)
{
	PED_ASSERT (geom != NULL, return 0);

	return sector >= geom->start && sector <= geom->end;
}

int
ped_geometry_read (const PedGeometry* geom, void* buffer, PedSector start,
		   PedSector count)
{
	int		exception_status;
	PedSector	real_start;

	PED_ASSERT (geom != NULL, return 0);
	PED_ASSERT (buffer != NULL, return 0);
	PED_ASSERT (start >= 0, return 0);
	PED_ASSERT (count >= 0, return 0);
	
	real_start = geom->start + start;

	if (real_start + count - 1 > geom->end) {
		exception_status = ped_exception_throw (
			PED_EXCEPTION_ERROR, PED_EXCEPTION_IGNORE_CANCEL,
			_("Attempt to read sectors %ld-%ld outside of "
			  "partition on %s."),
			(long) start, (long) (start + count - 1),
			geom->dev->path);
		return exception_status == PED_EXCEPTION_IGNORE;
	}

	if (!ped_device_read (geom->dev, buffer, real_start, count))
		return 0;
	return 1;
}

/* This function flushes all write-behind caches that might be holding
 * writes made by ped_geometry_write() to geom.  It is slow, because
 * it guarantees cache coherency among all relevant caches.
 */
int
ped_geometry_sync (PedGeometry* geom)
{
	PED_ASSERT (geom != NULL, return 0);
	return ped_device_sync (geom->dev);
}

/* This function flushes all write-behind caches that might be holding writes
 * made by ped_geometry_write() to geom.  It does NOT ensure cache coherency
 * with other caches that cache data in the region described by geom.  If you
 * need cache coherency, use ped_geometry_sync() instead.
 */
int
ped_geometry_sync_fast (PedGeometry* geom)
{
	PED_ASSERT (geom != NULL, return 0);
	return ped_device_sync_fast (geom->dev);
}

int
ped_geometry_write (PedGeometry* geom, const void* buffer, PedSector start,
		    PedSector count)
{
	int		exception_status;
	PedSector	real_start;

	PED_ASSERT (geom != NULL, return 0);
	PED_ASSERT (buffer != NULL, return 0);
	PED_ASSERT (start >= 0, return 0);
	PED_ASSERT (count >= 0, return 0);
	
	real_start = geom->start + start;

	if (real_start + count - 1 > geom->end) {
		exception_status = ped_exception_throw (
			PED_EXCEPTION_ERROR,
			PED_EXCEPTION_IGNORE_CANCEL,
			_("Attempt to write sectors %ld-%ld outside of "
			  "partition on %s."),
			(long) start, (long) (start + count - 1),
			geom->dev->path);
		return exception_status == PED_EXCEPTION_IGNORE;
	}

	if (!ped_device_write (geom->dev, buffer, real_start, count))
		return 0;
	return 1;
}

/* Checks for physical disk errors.  FIXME: use ped_device_check()
 */
PedSector
ped_geometry_check (PedGeometry* geom, void* buffer, PedSector buffer_size,
		    PedSector offset, PedSector granularity, PedSector count,
		    PedTimer* timer)
{
	PedSector	group;
	PedSector	i;
	PedSector	read_len;

	PED_ASSERT (geom != NULL, return 0);
	PED_ASSERT (buffer != NULL, return 0);

	ped_timer_reset (timer);
	ped_timer_set_state_name (timer, _("checking for bad blocks"));

retry:
	ped_exception_fetch_all();
	for (group = offset; group < offset + count; group += buffer_size) {
		ped_timer_update (timer, 1.0 * (group - offset) / count);
		read_len = PED_MIN (buffer_size, offset + count - group);
		if (!ped_geometry_read (geom, buffer, group, read_len))
			goto found_error;
	}
	ped_exception_leave_all();
	ped_timer_update (timer, 1.0);
	return 0;

found_error:
	ped_exception_catch();
	for (i = group; i + granularity < group + count; i += granularity) {
		if (!ped_geometry_read (geom, buffer, i, granularity)) {
			ped_exception_catch();
			ped_exception_leave_all();
			return i;
		}
	}
	ped_exception_leave_all();
	goto retry;   /* weird: failure on group read, but not individually */
}

/* This function takes a sector inside the region described by src, and
 * returns that sector's address inside dst.  This means that
 *
 * 	ped_geometry_read (dst, buf, ped_geometry_map(dst, src, sector), 1)
 *
 * does the same thing as
 * 
 * 	ped_geometry_read (src, buf, sector, 1)
 *
 * Clearly, this will only work if src and dst overlap.  If the sector
 * is not contained in dst, then this function returns -1.
 */
PedSector
ped_geometry_map (const PedGeometry* dst, const PedGeometry* src,
		  PedSector sector)
{
	PedSector	result;

	PED_ASSERT (dst != NULL, return 0);
	PED_ASSERT (src != NULL, return 0);

	if (!ped_geometry_test_sector_inside (src, sector))
		return -1;
	if (dst->dev != src->dev)
		return -1;

	result = src->start + sector - dst->start;
	if (result < 0 || result > dst->length)
		return -1;

	return result;
}

