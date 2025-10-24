/*
    libparted - a library for manipulating disk partitions
    Copyright (C) 1999 - 2001, 2005 Free Software Foundation, Inc.

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

#define _GNU_SOURCE 1

#include "config.h"

#include <parted/parted.h>
#include <parted/debug.h>

#include <limits.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>

static PedDevice*	devices; /* legal advice says: initialized to NULL,
				    under section 6.7.8 part 10
				    of ISO/EIC 9899:1999 */

#ifndef HAVE_CANONICALIZE_FILE_NAME
char *
canonicalize_file_name (const char *name)
{
	char *	buf;
	int	size;
	char *	result;

#ifdef PATH_MAX
	size = PATH_MAX;
#else
	/* Bigger is better; realpath has no way todo bounds checking.  */
	size = 4096;
#endif

	/* Just in case realpath does not NULL terminate the string
	 * or it just fits in SIZE without a NULL terminator.  */
	buf = calloc (size + 1, 1);
	if (! buf) {
		errno = ENOMEM;
		return NULL;
	}

	result = realpath (name, buf);
	if (! result)
		free (buf);

	return result;
}
#endif /* !HAVE_CANONICALIZE_FILE_NAME */

static void
_device_register (PedDevice* dev)
{
	PedDevice*	walk;
	for (walk = devices; walk && walk->next; walk = walk->next);
	if (walk)
		walk->next = dev;
	else
		devices = dev;
	dev->next = NULL;
}

static void
_device_unregister (PedDevice* dev)
{
	PedDevice*	walk;
	PedDevice*	last = NULL;

	for (walk = devices; walk != NULL; last = walk, walk = walk->next) {
		if (walk == dev) break;
	}

	if (last)
		last->next = dev->next;
	else
		devices = dev->next;
}

PedDevice*
ped_device_get_next (const PedDevice* dev)
{
	if (dev)
		return dev->next;
	else
		return devices;
}

void
_ped_device_probe (const char* path)
{
	PedDevice*	dev;

	PED_ASSERT (path != NULL, return);

	ped_exception_fetch_all ();
	dev = ped_device_get (path);
	if (!dev)
		ped_exception_catch ();
	ped_exception_leave_all ();
}

void
ped_device_probe_all ()
{
	ped_architecture->dev_ops->probe_all ();
}

void
ped_device_free_all ()
{
	while (devices)
		ped_device_destroy (devices);
}

/* First searches through probed devices, then attempts to open the device
 * regardless.
 */
PedDevice*
ped_device_get (const char* path)
{
	PedDevice*	walk;
	char*		normal_path;

	PED_ASSERT (path != NULL, return NULL);
	normal_path = canonicalize_file_name (path);
	if (!normal_path)
		/* Well, maybe it is just that the file does not exist.
		 * Try it anyway.  */
		normal_path = strdup (path);
	if (!normal_path)
		return NULL;

	for (walk = devices; walk != NULL; walk = walk->next) {
		if (!strcmp (walk->path, normal_path)) {
			ped_free (normal_path);
			return walk;
		}
	}

	walk = ped_architecture->dev_ops->_new (normal_path);
	ped_free (normal_path);
	if (!walk)
		return NULL;
	_device_register (walk);
	return walk;
}

void
ped_device_destroy (PedDevice* dev)
{
	_device_unregister (dev);

	while (dev->open_count) {
		if (!ped_device_close (dev))
			break;
	}

	ped_architecture->dev_ops->destroy (dev);
}

int
ped_device_is_busy (PedDevice* dev)
{
	return ped_architecture->dev_ops->is_busy (dev);
}

/* The meaning of "open" is architecture-dependent.  Apart from requesting
 * access to the device from the operating system, it does things like flushing
 * caches.
 */
int
ped_device_open (PedDevice* dev)
{
	int	status;

	PED_ASSERT (dev != NULL, return 0);
	PED_ASSERT (!dev->external_mode, return 0);

	if (dev->open_count)
		status = ped_architecture->dev_ops->refresh_open (dev);
	else
		status = ped_architecture->dev_ops->open (dev);
	if (status)
		dev->open_count++;
	return status;
}

int
ped_device_close (PedDevice* dev)
{
	PED_ASSERT (dev != NULL, return 0);
	PED_ASSERT (!dev->external_mode, return 0);
	PED_ASSERT (dev->open_count > 0, return 0);

	if (--dev->open_count)
		return ped_architecture->dev_ops->refresh_close (dev);
	else
		return ped_architecture->dev_ops->close (dev);
}

/* This function closes a device, while pretending it is still open.  This is
 * useful for temporarily suspending libparted access to the device in order
 * for an external program to access it.  (Running external programs while the
 * device is open can cause cache coherency problems.)
 *
 * In particular, this function keeps track of dev->open_count, so that
 * reference counting isn't screwed up.
 */
int
ped_device_begin_external_access (PedDevice* dev)
{
	PED_ASSERT (dev != NULL, return 0);
	PED_ASSERT (!dev->external_mode, return 0);

	dev->external_mode = 1;
	if (dev->open_count)
		return ped_architecture->dev_ops->close (dev);
	else
		return 1;
}

int
ped_device_end_external_access (PedDevice* dev)
{
	PED_ASSERT (dev != NULL, return 0);
	PED_ASSERT (dev->external_mode, return 0);

	dev->external_mode = 0;
	if (dev->open_count)
		return ped_architecture->dev_ops->open (dev);
	else
		return 1;
}

int
ped_device_read (PedDevice* dev, void* buffer, PedSector start,
		 PedSector count)
{
	PED_ASSERT (dev != NULL, return 0);
	PED_ASSERT (buffer != NULL, return 0);
	PED_ASSERT (!dev->external_mode, return 0);
	PED_ASSERT (dev->open_count > 0, return 0);

	return (ped_architecture->dev_ops->read) (dev, buffer, start, count);
}

int
ped_device_write (PedDevice* dev, const void* buffer, PedSector start,
		  PedSector count)
{
	PED_ASSERT (dev != NULL, return 0);
	PED_ASSERT (buffer != NULL, return 0);
	PED_ASSERT (!dev->external_mode, return 0);
	PED_ASSERT (dev->open_count > 0, return 0);

	return (ped_architecture->dev_ops->write) (dev, buffer, start, count);
}

PedSector
ped_device_check (PedDevice* dev, void* buffer, PedSector start,
		  PedSector count)
{
	PED_ASSERT (dev != NULL, return 0);
	PED_ASSERT (!dev->external_mode, return 0);
	PED_ASSERT (dev->open_count > 0, return 0);

	return (ped_architecture->dev_ops->check) (dev, buffer, start, count);
}

/* This function flushes all write-behind caches that might be holding up
 * writes.  It is slow, because it guarantees cache coherency among all
 * relevant caches.
 */
int
ped_device_sync (PedDevice* dev)
{
	PED_ASSERT (dev != NULL, return 0);
	PED_ASSERT (!dev->external_mode, return 0);
	PED_ASSERT (dev->open_count > 0, return 0);

	return ped_architecture->dev_ops->sync (dev);
}

/* This function flushes all write-behind caches that might be holding writes.
 * It does NOT ensure cache coherency with other caches that cache data.  If
 * you need cache coherency, use ped_device_sync() instead.
 */
int
ped_device_sync_fast (PedDevice* dev)
{
	PED_ASSERT (dev != NULL, return 0);
	PED_ASSERT (!dev->external_mode, return 0);
	PED_ASSERT (dev->open_count > 0, return 0);

	return ped_architecture->dev_ops->sync_fast (dev);
}

