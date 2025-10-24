 /*
    libparted - a library for manipulating disk partitions
    Copyright (C) 1999, 2000, 2001, 2002 Free Software Foundation, Inc.

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
#  define N_(String) (String)
#else
#  define _(String) (String)
#  define N_(String) (String)
#endif /* ENABLE_NLS */

#include <string.h>

/* UPDATE MODE functions */
#ifdef DEBUG
static int _disk_check_sanity (PedDisk* disk);
#endif
static void _disk_push_update_mode (PedDisk* disk);
static void _disk_pop_update_mode (PedDisk* disk);
static int _disk_raw_insert_before (PedDisk* disk, PedPartition* loc,
				    PedPartition* part);
static int _disk_raw_insert_after (PedDisk* disk, PedPartition* loc,
				   PedPartition* part);
static int _disk_raw_remove (PedDisk* disk, PedPartition* part);
static int _disk_raw_add (PedDisk* disk, PedPartition* part);

static PedDiskType*	disk_types = NULL;

void
ped_register_disk_type (PedDiskType* type)
{
	PED_ASSERT (type != NULL, return);
	PED_ASSERT (type->ops != NULL, return);
	PED_ASSERT (type->name != NULL, return);
	
	((struct _PedDiskType*) type)->next = disk_types;
	disk_types = (struct _PedDiskType*) type;
}

void ped_unregister_disk_type (PedDiskType* type)
{
	PedDiskType*	walk;
	PedDiskType*	last = NULL;

	PED_ASSERT (type != NULL, return);

	for (walk = disk_types; walk != NULL; last = walk, walk = walk->next) {
		if (walk == type) break;
	}

	if (last)
		((struct _PedDiskType*) last)->next = type->next;
	else
		disk_types = type->next;
}

PedDiskType*
ped_disk_type_get_next (PedDiskType* type)
{
	if (type)
		return type->next;
	else
		return disk_types;
}

PedDiskType*
ped_disk_type_get (const char* name)
{
	PedDiskType*	walk = NULL;

	PED_ASSERT (name != NULL, return NULL);

	while (1) {
		walk = ped_disk_type_get_next (walk);
		if (!walk) break;
		if (strcasecmp (walk->name, name) == 0) break;
	}
	return walk;
}

PedDiskType*
ped_disk_probe (PedDevice* dev)
{
	PedDiskType*	walk = NULL;

	PED_ASSERT (dev != NULL, return NULL);

	if (!ped_device_open (dev))
		return NULL;

	ped_exception_fetch_all ();
	while (1) {
		walk = ped_disk_type_get_next (walk);
		if (!walk) break;
		if (walk->ops->probe (dev)) break;
	}
	if (ped_exception)
		ped_exception_catch ();
	ped_exception_leave_all ();

	ped_device_close (dev);
	return walk;
}

/* This function reads the partition table off a device (if one is found). */
PedDisk*
ped_disk_new (PedDevice* dev)
{
	PedDiskType*	type;
	PedDisk*	disk;

	PED_ASSERT (dev != NULL, return NULL);

	if (!ped_device_open (dev))
		goto error;

	type = ped_disk_probe (dev);
	if (!type) {
		ped_exception_throw (PED_EXCEPTION_ERROR, PED_EXCEPTION_CANCEL,
			_("Unable to open %s - unrecognised disk label."),
			dev->path);
		goto error_close_dev;
	}
	disk = ped_disk_new_fresh (dev, type);
	if (!disk)
		goto error_close_dev;
	if (!type->ops->read (disk))
		goto error_destroy_disk;
	disk->needs_clobber = 0;
	ped_device_close (dev);
	return disk;

error_destroy_disk:
	ped_disk_destroy (disk);
error_close_dev:
	ped_device_close (dev);
error:
	return NULL;
}

static int
_add_duplicate_part (PedDisk* disk, PedPartition* old_part)
{
	PedPartition*	new_part;
	PedConstraint*	constraint_exact;

	new_part = disk->type->ops->partition_duplicate (old_part);
	if (!new_part)
		goto error;
	new_part->disk = disk;

	constraint_exact = ped_constraint_exact (&new_part->geom);
	if (!constraint_exact)
		goto error_destroy_new_part;
	if (!ped_disk_add_partition (disk, new_part, constraint_exact))
       		goto error_destroy_constraint_exact;
	ped_constraint_destroy (constraint_exact);
	return 1;

error_destroy_constraint_exact:
	ped_constraint_destroy (constraint_exact);
error_destroy_new_part:
	ped_partition_destroy (new_part);
error:
	return 0;
}

/* This function clones the in-memory representation of a partition table. */
PedDisk*
ped_disk_duplicate (const PedDisk* old_disk)
{
	PedDisk*	new_disk;
	PedPartition*	old_part;

	PED_ASSERT (old_disk != NULL, return NULL);
	PED_ASSERT (!old_disk->update_mode, return NULL);
	PED_ASSERT (old_disk->type->ops->duplicate != NULL, return NULL);
	PED_ASSERT (old_disk->type->ops->partition_duplicate != NULL,
		    return NULL);

	new_disk = old_disk->type->ops->duplicate (old_disk);
	if (!new_disk)
		goto error;

	_disk_push_update_mode (new_disk);
	for (old_part = ped_disk_next_partition (old_disk, NULL); old_part;
	     old_part = ped_disk_next_partition (old_disk, old_part)) {
		if (ped_partition_is_active (old_part)) {
			if (!_add_duplicate_part (new_disk, old_part))
				goto error_destroy_new_disk;
		}
	}
	_disk_pop_update_mode (new_disk);
	return new_disk;

error_destroy_new_disk:
	ped_disk_destroy (new_disk);
error:
	return NULL;
}

/* This function removes all identifying signatures of a partition table,
 * except for partition tables of a given type.
 */
int
ped_disk_clobber_exclude (PedDevice* dev, const PedDiskType* exclude)
{
	PedDiskType*	walk;

	PED_ASSERT (dev != NULL, goto error);

	if (!ped_device_open (dev))
		goto error;

	for (walk = ped_disk_type_get_next (NULL); walk;
	     walk = ped_disk_type_get_next (walk)) {
		int	probed;

		if (walk == exclude)
			continue;

		ped_exception_fetch_all ();
		probed = walk->ops->probe (dev);
		if (!probed)
			ped_exception_catch ();
		ped_exception_leave_all ();

		if (probed && walk->ops->clobber) {
			if (!walk->ops->clobber (dev))
				goto error_close_dev;
		}
	}
	ped_device_close (dev);
	return 1;

error_close_dev:
	ped_device_close (dev);
error:
	return 0;
}

int
ped_disk_clobber (PedDevice* dev)
{
	return ped_disk_clobber_exclude (dev, NULL);
}

/* This function creates a new partition table.  This new partition
 * table is only created in-memory, and nothing is written to disk
 * until ped_disk_commit_to_dev() is called.
 */
PedDisk*
ped_disk_new_fresh (PedDevice* dev, const PedDiskType* type)
{
	PedDisk*	disk;

	PED_ASSERT (dev != NULL, return NULL);
	PED_ASSERT (type != NULL, return NULL);
	PED_ASSERT (type->ops->alloc != NULL, return NULL);

	disk = type->ops->alloc (dev);
	if (!disk)
       		goto error;
	_disk_pop_update_mode (disk);
	PED_ASSERT (disk->update_mode == 0, goto error_destroy_disk);

	disk->needs_clobber = 1;
	return disk;

error_destroy_disk:
	ped_disk_destroy (disk);
error:
	return NULL;
}

PedDisk*
_ped_disk_alloc (PedDevice* dev, const PedDiskType* disk_type)
{
	PedDisk*	disk;

	disk = (PedDisk*) ped_malloc (sizeof (PedDisk));
	if (!disk)
		goto error;

	disk->dev = dev;
	disk->type = disk_type;
	disk->update_mode = 1;
	disk->part_list = NULL;
	return disk;

error_free_disk:
	ped_free (disk);
error:
	return NULL;
}

void
_ped_disk_free (PedDisk* disk)
{
	_disk_push_update_mode (disk);
	ped_disk_delete_all (disk);
	ped_free (disk);
}

void
ped_disk_destroy (PedDisk* disk)
{
	PED_ASSERT (disk != NULL, return);
	PED_ASSERT (!disk->update_mode, return);

	disk->type->ops->free (disk);
}

/* This function informs the operating system of changes made to a
 * partition table.
 */
int
ped_disk_commit_to_os (PedDisk* disk)
{
	PED_ASSERT (disk != NULL, return 0);

	if (!ped_device_open (disk->dev))
		goto error;
	if (!ped_architecture->disk_ops->disk_commit (disk))
		goto error_close_dev;
	ped_device_close (disk->dev);
	return 1;

error_close_dev:
	ped_device_close (disk->dev);
error:
	return 0;
}

/* This function writes the changes made to the in-memory description
 * of a partition table to disk.
 */
int
ped_disk_commit_to_dev (PedDisk* disk)
{
	PED_ASSERT (disk != NULL, goto error);
	PED_ASSERT (!disk->update_mode, goto error);

	if (!disk->type->ops->write) {
		ped_exception_throw (
			PED_EXCEPTION_ERROR,
			PED_EXCEPTION_CANCEL,
			_("This libparted doesn't have write support for "
			  "%s.  Perhaps it was compiled read-only."),
			disk->type->name);
		goto error;
	}

	if (!ped_device_open (disk->dev))
		goto error;

	if (disk->needs_clobber) {
		if (!ped_disk_clobber_exclude (disk->dev, disk->type))
			goto error_close_dev;
		disk->needs_clobber = 0;
	}
	if (!disk->type->ops->write (disk))
		goto error_close_dev;
	ped_device_close (disk->dev);
	return 1;

error_close_dev:
	ped_device_close (disk->dev);
error:
	return 0;
}

/* This function writes the in-memory changes to a partition table to
 * disk, and informs the operating system of the changes.
 */
int
ped_disk_commit (PedDisk* disk)
{
	if (!ped_disk_commit_to_dev (disk))
		return 0;
	return ped_disk_commit_to_os (disk);
}

/* This function returns 1 if a partition is mounted, or busy in some
 * way.
 */
int
ped_partition_is_busy (const PedPartition* part)
{
	PED_ASSERT (part != NULL, return 1);

	return ped_architecture->disk_ops->partition_is_busy (part);
}

/* This function returns the operating system path of a partition. */
char*
ped_partition_get_path (const PedPartition* part)
{
	PED_ASSERT (part != NULL, return NULL);

	return ped_architecture->disk_ops->partition_get_path (part);
}

/* This function does a sanity check on a partition table. */
int
ped_disk_check (PedDisk* disk)
{
	PedPartition*	walk;

	PED_ASSERT (disk != NULL, return 0);

	for (walk = disk->part_list; walk;
	     walk = ped_disk_next_partition (disk, walk)) {
		const PedFileSystemType*	fs_type = walk->fs_type;
		PedGeometry*			geom;
		PedSector			length_error;
		PedSector			max_length_error;

		if (!ped_partition_is_active (walk) || !fs_type)
			continue;

		geom = ped_file_system_probe_specific (fs_type, &walk->geom);
		if (!geom)
			continue;

		length_error = abs (walk->geom.length - geom->length);
		max_length_error = PED_MAX (4096, walk->geom.length / 100);
		if (!ped_geometry_test_inside (&walk->geom, geom)
		    || length_error > max_length_error) {
			char* part_size = ped_unit_format (disk->dev, walk->geom.length);
			char* fs_size = ped_unit_format (disk->dev, geom->length);
			PedExceptionOption choice;

			choice = ped_exception_throw (
				PED_EXCEPTION_WARNING,
				PED_EXCEPTION_IGNORE_CANCEL,
				_("Partition %d is %s, but the file system is "
				  "%s."),
				walk->num, part_size, fs_size);

			ped_free (part_size);
			ped_free (fs_size);

			if (choice != PED_EXCEPTION_IGNORE)
				return 0;
		}
	}

	return 1;
}

/* This function checks if a particular type of partition table supports
 * a feature.
 */
int
ped_disk_type_check_feature (const PedDiskType* disk_type,
			     PedDiskTypeFeature feature)
{
	return (disk_type->features & feature) != 0;
}

int
ped_disk_get_primary_partition_count (PedDisk* disk)
{
	PedPartition*	walk;
	int		count = 0;

	PED_ASSERT (disk != NULL, return 0);

	for (walk = disk->part_list; walk;
	     walk = ped_disk_next_partition (disk, walk)) {
		if (ped_partition_is_active (walk)
				&& ! (walk->type & PED_PARTITION_LOGICAL))
			count++;
	}

	return count;
}

int
ped_disk_get_last_partition_num (PedDisk* disk)
{
	PedPartition*	walk;
	int		highest = -1;

	PED_ASSERT (disk != NULL, return 0);

	for (walk = disk->part_list; walk;
	     walk = ped_disk_next_partition (disk, walk)) {
		if (walk->num > highest)
			highest = walk->num;
	}

	return highest;
}

/* This function returns the maximum number of (primary) partitions.
 * For example, Macintosh partition maps can have different sizes,
 * and support a different number of partitions.
 */
int
ped_disk_get_max_primary_partition_count (const PedDisk* disk)
{
	PED_ASSERT (disk->type != NULL, return 0);
	PED_ASSERT (disk->type->ops->get_max_primary_partition_count != NULL,
		    return 0);

	return disk->type->ops->get_max_primary_partition_count (disk);
}

/* We turned a really nasty bureaucracy problem into an elegant maths
 * problem :-)  Basically, there are some constraints to a partition's
 * geometry:
 *
 * (1) it must start and end on a "disk" block, determined by the disk label
 * (not the hardware).  (constraint represented by a PedAlignment)
 *
 * (2) if we're resizing a partition, we MIGHT need to keep each block aligned.
 * Eg: if an ext2 file system has 4k blocks, then we can only move the start
 * by a multiple of 4k.  (constraint represented by a PedAlignment)
 *
 * (3) we need to keep the start and end within the device's physical
 * boundaries.  (constraint represented by a PedGeometry)
 *
 * Satisfying (1) and (2) simultaneously required a bit of fancy maths ;-)  See
 * ped_alignment_intersect()
 *
 * The application of these constraints is in disk_*.c's *_partition_align()
 * function.
 */
static int
_partition_align (PedPartition* part, const PedConstraint* constraint)
{
	const PedDiskType*	disk_type;

	PED_ASSERT (part != NULL, return 0);
	PED_ASSERT (part->num != -1, return 0);
	PED_ASSERT (part->disk != NULL, return 0);
	disk_type = part->disk->type;
	PED_ASSERT (disk_type != NULL, return 0);
	PED_ASSERT (disk_type->ops->partition_align != NULL, return 0);
	PED_ASSERT (part->disk->update_mode, return 0);

	return disk_type->ops->partition_align (part, constraint);
}

static int
_partition_enumerate (PedPartition* part)
{
	const PedDiskType*	disk_type;

	PED_ASSERT (part != NULL, return 0);
	PED_ASSERT (part->disk != NULL, return 0);
	disk_type = part->disk->type;
	PED_ASSERT (disk_type != NULL, return 0);
	PED_ASSERT (disk_type->ops->partition_enumerate != NULL, return 0);

	return disk_type->ops->partition_enumerate (part);
}

/* gives all the (active) partitions a number.  It should preserve the numbers
 * and orders as much as possible.
 */
static int
ped_disk_enumerate_partitions (PedDisk* disk)
{
	PedPartition*	walk;
	int		i;
	int		end;

	PED_ASSERT (disk != NULL, return 0);

/* first "sort" already-numbered partitions.  (Eg: if a logical partition
 * is removed, then all logical partitions that were number higher MUST be
 * renumbered)
 */
	end = ped_disk_get_last_partition_num (disk);
	for (i=1; i<=end; i++) {
		walk = ped_disk_get_partition (disk, i);
		if (walk) {
			if (!_partition_enumerate (walk))
				return 0;
		}
	}

/* now, number un-numbered partitions */
	for (walk = disk->part_list; walk;
	     walk = ped_disk_next_partition (disk, walk)) {
		if (ped_partition_is_active (walk) && walk->num == -1) {
			if (!_partition_enumerate (walk))
				return 0;
		}
	}

	return 1;
}

static int
_disk_remove_metadata (PedDisk* disk)
{
	PedPartition*	walk = NULL;
	PedPartition*	next;

	PED_ASSERT (disk != NULL, return 0);

	next = ped_disk_next_partition (disk, walk);

	while (next) {
		walk = next;
		while (1) {
			next = ped_disk_next_partition (disk, next);
			if (!next || next->type & PED_PARTITION_METADATA)
				break;
		}
		if (walk->type & PED_PARTITION_METADATA)
			ped_disk_delete_partition (disk, walk);
	}
	return 1;
}

static int
_disk_alloc_metadata (PedDisk* disk)
{
	PED_ASSERT (disk != NULL, return 0);

	if (!disk->update_mode)
		_disk_remove_metadata (disk);

	return disk->type->ops->alloc_metadata (disk);
}

static int
_disk_remove_freespace (PedDisk* disk)
{
	PedPartition*	walk;
	PedPartition*	next;

	walk = ped_disk_next_partition (disk, NULL);
	for (; walk; walk = next) {
		next = ped_disk_next_partition (disk, walk);

		if (walk->type & PED_PARTITION_FREESPACE) {
			_disk_raw_remove (disk, walk);
			ped_partition_destroy (walk);
		}
	}

	return 1;
}

static int
_alloc_extended_freespace (PedDisk* disk)
{
	PedSector	last_end;
	PedPartition*	walk;
	PedPartition*	last;
	PedPartition*	free_space;
	PedPartition*	extended_part;

	extended_part = ped_disk_extended_partition (disk);
	if (!extended_part)
		return 1;

	last_end = extended_part->geom.start;
	last = NULL;
	
	for (walk = extended_part->part_list; walk; walk = walk->next) {
		if (walk->geom.start > last_end + 1) {
			free_space = ped_partition_new (
					disk,
					PED_PARTITION_FREESPACE
						| PED_PARTITION_LOGICAL,
					NULL,
					last_end + 1, walk->geom.start - 1);
			_disk_raw_insert_before (disk, walk, free_space);
		}

		last = walk;
		last_end = last->geom.end;
	}

	if (last_end < extended_part->geom.end) {
		free_space = ped_partition_new (
				disk,
				PED_PARTITION_FREESPACE | PED_PARTITION_LOGICAL,
				NULL,
				last_end + 1, extended_part->geom.end);

		if (last)
			return _disk_raw_insert_after (disk, last, free_space);
		else
			extended_part->part_list = free_space;
	}

	return 1;
}

static int
_disk_alloc_freespace (PedDisk* disk)
{
	PedSector	last_end;
	PedPartition*	walk;
	PedPartition*	last;
	PedPartition*	free_space;

	if (!_disk_remove_freespace (disk))
		return 0;
	if (!_alloc_extended_freespace (disk))
		return 0;

	last = NULL;
	last_end = -1;

	for (walk = disk->part_list; walk; walk = walk->next) {
		if (walk->geom.start > last_end + 1) {
			free_space = ped_partition_new (disk,
					PED_PARTITION_FREESPACE, NULL,
					last_end + 1, walk->geom.start - 1);
			_disk_raw_insert_before (disk, walk, free_space);
		}

		last = walk;
		last_end = last->geom.end;
	}

	if (last_end < disk->dev->length - 1) {
		free_space = ped_partition_new (disk,
					PED_PARTITION_FREESPACE, NULL,
					last_end + 1, disk->dev->length - 1);
		if (last)
			return _disk_raw_insert_after (disk, last, free_space);
		else
			disk->part_list = free_space;
	}

	return 1;
}

/* update mode: used when updating the internal representation of the partition
 * table.  In update mode, the metadata and freespace placeholder/virtual
 * partitions are removed, making it much easier for various manipulation
 * routines...
 */
static void
_disk_push_update_mode (PedDisk* disk)
{
	if (!disk->update_mode) {
#ifdef DEBUG
		_disk_check_sanity (disk);
#endif

		_disk_remove_freespace (disk);
		disk->update_mode++;
		_disk_remove_metadata (disk);

#ifdef DEBUG
		_disk_check_sanity (disk);
#endif
	} else {
		disk->update_mode++;
	}
}

static void
_disk_pop_update_mode (PedDisk* disk)
{
	PED_ASSERT (disk->update_mode, return);

	if (disk->update_mode == 1) {
	/* re-allocate metadata BEFORE leaving update mode, to prevent infinite
	 * recursion (metadata allocation requires update mode)
	 */
#ifdef DEBUG
		_disk_check_sanity (disk);
#endif

		_disk_alloc_metadata (disk);
		disk->update_mode--;
		_disk_alloc_freespace (disk);

#ifdef DEBUG
		_disk_check_sanity (disk);
#endif
	} else {
		disk->update_mode--;
	}
}

PedPartition*
_ped_partition_alloc (const PedDisk* disk, PedPartitionType type,
		      const PedFileSystemType* fs_type,
		      PedSector start, PedSector end)
{
	PedPartition*	part;

	PED_ASSERT (disk != NULL, return 0);

	part = (PedPartition*) ped_malloc (sizeof (PedPartition));
	if (!part)
		goto error;

	part->prev = NULL;
	part->next = NULL;

	part->disk = (PedDisk*) disk;
	if (!ped_geometry_init (&part->geom, disk->dev, start, end - start + 1))
		goto error_free_part;

	part->num = -1;
	part->type = type;
	part->part_list = NULL;
	part->fs_type = fs_type;

	return part;

error_free_part:
	ped_free (part);
error:
	return NULL;
}

void
_ped_partition_free (PedPartition* part)
{
	ped_free (part);
}

int
_ped_partition_attempt_align (PedPartition* part,
			      const PedConstraint* external,
			      PedConstraint* internal)
{
	PedConstraint*		intersection;
	PedGeometry*		solution;

	intersection = ped_constraint_intersect (external, internal);
	ped_constraint_destroy (internal);
	if (!intersection)
		goto fail;

	solution = ped_constraint_solve_nearest (intersection, &part->geom);
	if (!solution)
		goto fail_free_intersection;
	ped_geometry_set (&part->geom, solution->start, solution->length);
	ped_geometry_destroy (solution);
	ped_constraint_destroy (intersection);
	return 1;

fail_free_intersection:
	ped_constraint_destroy (intersection);
fail:
	return 0;
}

PedPartition*
ped_partition_new (const PedDisk* disk, PedPartitionType type,
		   const PedFileSystemType* fs_type, PedSector start,
		   PedSector end)
{
	int		supports_extended;
	PedPartition*	part;

	PED_ASSERT (disk != NULL, return NULL);
	PED_ASSERT (disk->type->ops->partition_new != NULL, return NULL);

	supports_extended = ped_disk_type_check_feature (disk->type,
			    	PED_DISK_TYPE_EXTENDED);

	if (!supports_extended
	    && (type == PED_PARTITION_EXTENDED
			|| type == PED_PARTITION_LOGICAL)) {
		ped_exception_throw (
			PED_EXCEPTION_ERROR,
			PED_EXCEPTION_CANCEL,
			_("%s disk labels do not support extended "
			  "partitions."),
			disk->type->name);
		goto error;
	}

	part = disk->type->ops->partition_new (disk, type, fs_type, start, end);
	if (!part)
		goto error;

	if (fs_type || part->type == PED_PARTITION_EXTENDED) {
		if (!ped_partition_set_system (part, fs_type))
			goto error_destroy_part;
	}
	return part;

error_destroy_part:
	ped_partition_destroy (part);
error:
	return NULL;
}

void
ped_partition_destroy (PedPartition* part)
{
	PED_ASSERT (part != NULL, return);
	PED_ASSERT (part->disk != NULL, return);
	PED_ASSERT (part->disk->type->ops->partition_new != NULL, return);

	part->disk->type->ops->partition_destroy (part);
}

int
ped_partition_is_active (const PedPartition* part)
{
	PED_ASSERT (part != NULL, return 0);

	return !(part->type & PED_PARTITION_FREESPACE
		 || part->type & PED_PARTITION_METADATA);
}

int
ped_partition_set_flag (PedPartition* part, PedPartitionFlag flag, int state)
{
	PedDiskOps*	ops;

	PED_ASSERT (part != NULL, return 0);
	PED_ASSERT (part->disk != NULL, return 0);
	PED_ASSERT (ped_partition_is_active (part), return 0);

	ops = part->disk->type->ops;
	PED_ASSERT (ops->partition_set_flag != NULL, return 0);
	PED_ASSERT (ops->partition_is_flag_available != NULL, return 0);

	if (!ops->partition_is_flag_available (part, flag)) {
		ped_exception_throw (
			PED_EXCEPTION_ERROR,
			PED_EXCEPTION_CANCEL,
			"The flag '%s' is not available for %s disk labels.",
			ped_partition_flag_get_name (flag),
			part->disk->type->name);
		return 0;
	}

	return ops->partition_set_flag (part, flag, state);
}

int
ped_partition_get_flag (const PedPartition* part, PedPartitionFlag flag)
{
	PED_ASSERT (part != NULL, return 0);
	PED_ASSERT (part->disk != NULL, return 0);
	PED_ASSERT (part->disk->type->ops->partition_get_flag != NULL,
		    return 0);
	PED_ASSERT (ped_partition_is_active (part), return 0);

	return part->disk->type->ops->partition_get_flag (part, flag);
}

int
ped_partition_is_flag_available (const PedPartition* part,
	       			 PedPartitionFlag flag)
{
	PED_ASSERT (part != NULL, return 0);
	PED_ASSERT (part->disk != NULL, return 0);
	PED_ASSERT (part->disk->type->ops->partition_is_flag_available != NULL,
		    return 0);
	PED_ASSERT (ped_partition_is_active (part), return 0);

	return part->disk->type->ops->partition_is_flag_available (part, flag);
}

int
ped_partition_set_system (PedPartition* part, const PedFileSystemType* fs_type)
{
	PedFileSystem*		fs;
	const PedDiskType*	disk_type;

	PED_ASSERT (part != NULL, return 0);
	PED_ASSERT (ped_partition_is_active (part), return 0);
	PED_ASSERT (part->disk != NULL, return 0);
	disk_type = part->disk->type;
	PED_ASSERT (disk_type != NULL, return 0);
	PED_ASSERT (disk_type->ops != NULL, return 0);
	PED_ASSERT (disk_type->ops->partition_set_system != NULL, return 0);

	return disk_type->ops->partition_set_system (part, fs_type);
}

static int
_assert_partition_name_feature (const PedDiskType* disk_type)
{
	if (!ped_disk_type_check_feature (
			disk_type, PED_DISK_TYPE_PARTITION_NAME)) {
		ped_exception_throw (
			PED_EXCEPTION_ERROR,
			PED_EXCEPTION_CANCEL,
			"%s disk labels do not support partition names.",
			disk_type->name);
		return 0;
	}
	return 1;
}

int
ped_partition_set_name (PedPartition* part, const char* name)
{
	PED_ASSERT (part != NULL, return 0);
	PED_ASSERT (part->disk != NULL, return 0);
	PED_ASSERT (ped_partition_is_active (part), return 0);
	PED_ASSERT (name != NULL, return 0);

	if (!_assert_partition_name_feature (part->disk->type))
		return 0;

	PED_ASSERT (part->disk->type->ops->partition_set_name != NULL,
		    return 0);
	part->disk->type->ops->partition_set_name (part, name);
	return 1;
}

const char*
ped_partition_get_name (const PedPartition* part)
{
	PED_ASSERT (part != NULL, return NULL);
	PED_ASSERT (part->disk != NULL, return 0);
	PED_ASSERT (ped_partition_is_active (part), return 0);

	if (!_assert_partition_name_feature (part->disk->type))
		return NULL;

	PED_ASSERT (part->disk->type->ops->partition_get_name != NULL,
		    return NULL);
	return part->disk->type->ops->partition_get_name (part);
}

PedPartition*
ped_disk_extended_partition (const PedDisk* disk)
{
	PedPartition*		walk;

	PED_ASSERT (disk != NULL, return 0);

	for (walk = disk->part_list; walk; walk = walk->next) {
		if (walk->type == PED_PARTITION_EXTENDED)
			break;
	}
	return walk;
}

/* returns the next partition.  If the current partition is an extended
 * partition, returns the first logical partition.
 */
PedPartition*
ped_disk_next_partition (const PedDisk* disk, const PedPartition* part)
{
	PED_ASSERT (disk != NULL, return 0);

	if (!part)
		return disk->part_list;
	if (part->type == PED_PARTITION_EXTENDED)
		return part->part_list ? part->part_list : part->next;
	if (part->next)
		return part->next;
	if (part->type & PED_PARTITION_LOGICAL)
		return ped_disk_extended_partition (disk)->next;
	return NULL;
}

#ifdef DEBUG
static int
_disk_check_sanity (PedDisk* disk)
{
	PedPartition*	walk;

	PED_ASSERT (disk != NULL, return 0);

	for (walk = disk->part_list; walk; walk = walk->next) {
		PED_ASSERT (!(walk->type & PED_PARTITION_LOGICAL), return 0);
		PED_ASSERT (!walk->prev || walk->prev->next == walk, return 0);
	}

	if (!ped_disk_extended_partition (disk))
		return 1;

	for (walk = ped_disk_extended_partition (disk)->part_list; walk;
	     walk = walk->next) {
		PED_ASSERT (walk->type & PED_PARTITION_LOGICAL, return 0);
		if (walk->prev)
			PED_ASSERT (walk->prev->next == walk, return 0);
	}
	return 1;
}
#endif

PedPartition*
ped_disk_get_partition (const PedDisk* disk, int num)
{
	PedPartition*	walk;

	PED_ASSERT (disk != NULL, return 0);

	for (walk = disk->part_list; walk;
	     walk = ped_disk_next_partition (disk, walk)) {
		if (walk->num == num && !(walk->type & PED_PARTITION_FREESPACE))
			return walk;
	}

	return NULL;
}

/* Returns the partition that contains sect.  If sect lies within a logical
 * partition, then the logical partition is returned (not the extended
 * partition)
 */
PedPartition*
ped_disk_get_partition_by_sector (const PedDisk* disk, PedSector sect)
{
	PedPartition*	walk;

	PED_ASSERT (disk != NULL, return 0);

	for (walk = disk->part_list; walk;
	     walk = ped_disk_next_partition (disk, walk)) {
		if (ped_geometry_test_sector_inside (&walk->geom, sect)
		    && walk->type != PED_PARTITION_EXTENDED)
			return walk;
	}

	/* should never get here, unless sect is outside of disk's useable
	 * part, or we're in "update mode", and the free space place-holders
	 * have been removed with _disk_remove_freespace()
	 */
	return NULL;
}

/* I'm beginning to agree with Sedgewick :-/ */
static int
_disk_raw_insert_before (PedDisk* disk, PedPartition* loc, PedPartition* part)
{
	PED_ASSERT (disk != NULL, return 0);
	PED_ASSERT (loc != NULL, return 0);
	PED_ASSERT (part != NULL, return 0);

	part->prev = loc->prev;
	part->next = loc;
	if (part->prev) {
		part->prev->next = part;
	} else {
		if (loc->type & PED_PARTITION_LOGICAL)
			ped_disk_extended_partition (disk)->part_list = part;
		else
			disk->part_list = part;
	}
	loc->prev = part;

	return 1;
}

static int
_disk_raw_insert_after (PedDisk* disk, PedPartition* loc, PedPartition* part)
{
	PED_ASSERT (disk != NULL, return 0);
	PED_ASSERT (loc != NULL, return 0);
	PED_ASSERT (part != NULL, return 0);

	part->prev = loc;
	part->next = loc->next;
	if (loc->next)
		loc->next->prev = part;
	loc->next = part;

	return 1;
}

static int
_disk_raw_remove (PedDisk* disk, PedPartition* part)
{
	PED_ASSERT (disk != NULL, return 0);
	PED_ASSERT (part != NULL, return 0);

	if (part->prev) {
		part->prev->next = part->next;
		if (part->next)
			part->next->prev = part->prev;
	} else {
		if (part->type & PED_PARTITION_LOGICAL) {
			ped_disk_extended_partition (disk)->part_list
				= part->next;
		} else {
			disk->part_list = part->next;
		}
		if (part->next)
			part->next->prev = NULL;
	}

	return 1;
}

/* UPDATE MODE ONLY
 */
static int
_disk_raw_add (PedDisk* disk, PedPartition* part)
{
	PedPartition*	walk;
	PedPartition*	last;
	PedPartition*	ext_part;

	PED_ASSERT (disk->update_mode, return 0);

	ext_part = ped_disk_extended_partition (disk);

	last = NULL;
	walk = (part->type & PED_PARTITION_LOGICAL) ?
			ext_part->part_list : disk->part_list;

	for (; walk; last = walk, walk = walk->next) {
		if (walk->geom.start > part->geom.end)
			break;
	}

	if (walk) {
		return _disk_raw_insert_before (disk, walk, part);
	} else {
		if (last) {
			return _disk_raw_insert_after (disk, last, part);
		} else {
			if (part->type & PED_PARTITION_LOGICAL)
				ext_part->part_list = part;
			else
				disk->part_list = part;
		} 
	}

	return 1;
}

static PedConstraint*
_partition_get_overlap_constraint (PedPartition* part, PedGeometry* geom)
{
	PedSector	min_start;
	PedSector	max_end;
	PedPartition*	walk;
	PedGeometry	free_space;

	PED_ASSERT (part->disk->update_mode, return NULL);
	PED_ASSERT (part->geom.dev == geom->dev, return NULL);

	if (part->type & PED_PARTITION_LOGICAL) {
		PedPartition* ext_part;
		
		ext_part = ped_disk_extended_partition (part->disk);
		PED_ASSERT (ext_part != NULL, return NULL);

		min_start = ext_part->geom.start;
		max_end = ext_part->geom.end;
		walk = ext_part->part_list;
	} else {
		min_start = 0;
		max_end = part->disk->dev->length - 1;
		walk = part->disk->part_list;
	}

	while (walk != NULL
	       && (walk->geom.start < geom->start
			    || min_start >= walk->geom.start)) {
		if (walk != part)
			min_start = walk->geom.end + 1;
		walk = walk->next;
	}

	if (walk == part)
		walk = walk->next;

	if (walk)
		max_end = walk->geom.start - 1;

	if (min_start >= max_end)
		return NULL;

	ped_geometry_init (&free_space, part->disk->dev,
			   min_start, max_end - min_start + 1);
	return ped_constraint_new_from_max (&free_space);
}

/* returns 0 if the partition, "part" overlaps with any partitions on the
 * "disk".  The geometry of "part" is taken to be "geom", NOT "part->geom"
 * (the idea here is to check if "geom" is valid, before changing "part").
 * 
 * This is useful for seeing if a resized partitions new geometry is going to
 * fit, without the existing geomtry getting in the way.
 *
 * Note: overlap with an extended partition is also allowed, provided that
 * "geom" lies completely inside the extended partition.
 */
static int
_disk_check_part_overlaps (PedDisk* disk, PedPartition* part)
{
	PedPartition*	walk;

	PED_ASSERT (disk != NULL, return 0);
	PED_ASSERT (part != NULL, return 0);

	for (walk = ped_disk_next_partition (disk, NULL); walk;
	     walk = ped_disk_next_partition (disk, walk)) {
		if (walk->type & PED_PARTITION_FREESPACE)
			continue;
		if (walk == part)
			continue;
		if (part->type & PED_PARTITION_EXTENDED
		    && walk->type & PED_PARTITION_LOGICAL)
			continue;

		if (ped_geometry_test_overlap (&walk->geom, &part->geom)) {
			if (walk->type & PED_PARTITION_EXTENDED
			    && part->type & PED_PARTITION_LOGICAL
			    && ped_geometry_test_inside (&walk->geom,
							 &part->geom))
				continue;
			return 0;
		}
	}

	return 1;
}

static int
_partition_check_basic_sanity (PedDisk* disk, PedPartition* part)
{
	PedPartition*	ext_part = ped_disk_extended_partition (disk);

	PED_ASSERT (part->disk == disk, return 0);

	PED_ASSERT (part->geom.start >= 0, return 0);
	PED_ASSERT (part->geom.end < disk->dev->length, return 0);
	PED_ASSERT (part->geom.start <= part->geom.end, return 0);

	if (!ped_disk_type_check_feature (disk->type, PED_DISK_TYPE_EXTENDED)
	    && (part->type == PED_PARTITION_EXTENDED
		    || part->type == PED_PARTITION_LOGICAL)) {
		ped_exception_throw (
			PED_EXCEPTION_ERROR,
			PED_EXCEPTION_CANCEL,
			_("%s disk labels don't support logical or extended "
			  "partitions."),
			disk->type->name);
		return 0;
	}

	if (ped_partition_is_active (part)
			&& ! (part->type & PED_PARTITION_LOGICAL)) {
		if (ped_disk_get_primary_partition_count (disk) + 1
		    > ped_disk_get_max_primary_partition_count (disk)) {
			ped_exception_throw (
				PED_EXCEPTION_ERROR,
				PED_EXCEPTION_CANCEL,
				_("Too many primary partitions."));
			return 0;
		}
	}

	if ((part->type & PED_PARTITION_LOGICAL) && !ext_part) {
		ped_exception_throw (
			PED_EXCEPTION_ERROR,
			PED_EXCEPTION_CANCEL,
			_("Can't add a logical partition to %s, because "
			"there is no extended partition."),
			disk->dev->path);
		return 0;
	}

	return 1;
}

static int
_check_extended_partition (PedDisk* disk, PedPartition* part)
{
	PedPartition*		walk;
	PedPartition*		ext_part;

	PED_ASSERT (disk != NULL, return 0);
	ext_part = ped_disk_extended_partition (disk);
	if (!ext_part) ext_part = part;
	PED_ASSERT (ext_part != NULL, return 0);

	if (part != ext_part) {
		ped_exception_throw (
			PED_EXCEPTION_ERROR,
			PED_EXCEPTION_CANCEL,
			_("Can't have more than one extended partition on %s."),
			disk->dev->path);
		return 0;
	}

	for (walk = ext_part->part_list; walk; walk = walk->next) {
		if (!ped_geometry_test_inside (&ext_part->geom, &walk->geom)) {
			ped_exception_throw (
				PED_EXCEPTION_ERROR,
				PED_EXCEPTION_CANCEL,
				_("Can't have logical partitions outside of "
				  "the extended partition."));
			return 0;
		}
	}
	return 1;
}

static int
_check_partition (PedDisk* disk, PedPartition* part)
{
	PedPartition*	ext_part = ped_disk_extended_partition (disk);

	if (part->type == PED_PARTITION_EXTENDED) {
		if (!_check_extended_partition (disk, part))
			return 0;
	}

	if (part->type & PED_PARTITION_LOGICAL
	    && !ped_geometry_test_inside (&ext_part->geom, &part->geom)) {
		ped_exception_throw (
			PED_EXCEPTION_ERROR,
			PED_EXCEPTION_CANCEL,
			_("Can't have a logical partition outside of the "
			  "extended partition on %s."),
			disk->dev->path);
		return 0;
	}

	if (!_disk_check_part_overlaps (disk, part)) {
		ped_exception_throw (
			PED_EXCEPTION_ERROR,
			PED_EXCEPTION_CANCEL,
			_("Can't have overlapping partitions."));
		return 0;
	}

	if (! (part->type & PED_PARTITION_LOGICAL)
	    && ext_part && ext_part != part
	    && ped_geometry_test_inside (&ext_part->geom, &part->geom)) {
		ped_exception_throw (PED_EXCEPTION_ERROR, PED_EXCEPTION_CANCEL,
			_("Can't have a primary partition inside an extended "
			 "partition."));
		return 0;
	}

	return 1;
}

/* This function adds a new partition to a partition table, subject to a
 * constraint.  WARNING!  This function may modify the partition's start
 * and end before adding it to the partition table, in order to get it
 * to satisfy the given constraint and further constraints imposed by
 * the partition table.  If you don't want ped_disk_add_partition() to
 * do this, then pass a restrictive constraint, such as
 * ped_constraint_exact(&part->geom).
 */
int
ped_disk_add_partition (PedDisk* disk, PedPartition* part,
			const PedConstraint* constraint)
{
	PedConstraint*	overlap_constraint = NULL;
	PedConstraint*	constraints = NULL;

	PED_ASSERT (disk != NULL, return 0);
	PED_ASSERT (part != NULL, return 0);

	if (!_partition_check_basic_sanity (disk, part))
		return 0;

	_disk_push_update_mode (disk);

	if (ped_partition_is_active (part)) {
		overlap_constraint
			= _partition_get_overlap_constraint (part, &part->geom);
		constraints = ped_constraint_intersect (overlap_constraint,
							constraint);

		if (!_partition_enumerate (part))
			goto error;
		if (!_partition_align (part, constraints))
			goto error;
	}
	if (!_check_partition (disk, part))
		goto error;
	if (!_disk_raw_add (disk, part))
		goto error;

	ped_constraint_destroy (overlap_constraint);
	ped_constraint_destroy (constraints);
	_disk_pop_update_mode (disk);
#ifdef DEBUG
	if (!_disk_check_sanity (disk))
		return 0;
#endif
	return 1;

error:
	ped_constraint_destroy (overlap_constraint);
	ped_constraint_destroy (constraints);
	_disk_pop_update_mode (disk);
	return 0;
}

int
ped_disk_remove_partition (PedDisk* disk, PedPartition* part)
{
	PED_ASSERT (disk != NULL, return 0);
	PED_ASSERT (part != NULL, return 0);

	_disk_push_update_mode (disk);
	PED_ASSERT (part->part_list == NULL, goto error);
	_disk_raw_remove (disk, part);
	_disk_pop_update_mode (disk);
	ped_disk_enumerate_partitions (disk);
	return 1;

error:
	_disk_pop_update_mode (disk);
	return 0;
}

static int
ped_disk_delete_all_logical (PedDisk* disk);

int
ped_disk_delete_partition (PedDisk* disk, PedPartition* part)
{
	PED_ASSERT (disk != NULL, return 0);
	PED_ASSERT (part != NULL, return 0);

	_disk_push_update_mode (disk);
	if (part->type == PED_PARTITION_EXTENDED)
		ped_disk_delete_all_logical (disk);
	ped_disk_remove_partition (disk, part);
	ped_partition_destroy (part);
	_disk_pop_update_mode (disk);

	return 1;
}

static int
ped_disk_delete_all_logical (PedDisk* disk)
{
	PedPartition*		walk;
	PedPartition*		next;
	PedPartition*		ext_part;

	PED_ASSERT (disk != NULL, return 0);
	ext_part = ped_disk_extended_partition (disk);
	PED_ASSERT (ext_part != NULL, return 0);

	for (walk = ext_part->part_list; walk; walk = next) {
		next = walk->next;

		if (!ped_disk_delete_partition (disk, walk))
			return 0;
	}
	return 1;
}

int
ped_disk_delete_all (PedDisk* disk)
{
	PedPartition*		walk;
	PedPartition*		next;

	PED_ASSERT (disk != NULL, return 0);

	_disk_push_update_mode (disk);

	for (walk = disk->part_list; walk; walk = next) {
		next = walk->next;

		if (!ped_disk_delete_partition (disk, walk))
			return 0;
	}

	_disk_pop_update_mode (disk);

	return 1;
}

/* This function changes a partition's location.  WARNING!  It might
 * change it to something different to what you asked for, subject to
 * the given constraint, and further constraints imposed by the
 * partition table.  If you don't want ped_disk_set_partition_geom() to
 * do this, then pass a restrictive constraint, such as
 * ped_constraint_exact(&part->geom).
 *
 * Note that this function does not modify the contents of the partition.
 * You need to call ped_file_system_resize() separately.
 */
int
ped_disk_set_partition_geom (PedDisk* disk, PedPartition* part,
			     const PedConstraint* constraint,
			     PedSector start, PedSector end)
{
	PedConstraint*	overlap_constraint = NULL;
	PedConstraint*	constraints = NULL;
	PedGeometry	old_geom;
	PedGeometry	new_geom;

	PED_ASSERT (disk != NULL, return 0);
	PED_ASSERT (part != NULL, return 0);
	PED_ASSERT (part->disk == disk, return 0);

	old_geom = part->geom;
	ped_geometry_init (&new_geom, part->geom.dev, start, end - start + 1);

	_disk_push_update_mode (disk);

	overlap_constraint
		= _partition_get_overlap_constraint (part, &new_geom);
	constraints = ped_constraint_intersect (overlap_constraint, constraint);

	part->geom = new_geom;
	if (!_partition_align (part, constraints))
		goto error_pop_update_mode;
	if (!_check_partition (disk, part))
		goto error_pop_update_mode;

	/* remove and add, to ensure the ordering gets updated if necessary */
	_disk_raw_remove (disk, part);
	_disk_raw_add (disk, part);

	_disk_pop_update_mode (disk);

	ped_constraint_destroy (overlap_constraint);
	ped_constraint_destroy (constraints);
	return 1;

error_pop_update_mode:
	_disk_pop_update_mode (disk);
error:
	ped_constraint_destroy (overlap_constraint);
	ped_constraint_destroy (constraints);
	part->geom = old_geom;
	return 0;
}

int
ped_disk_maximize_partition (PedDisk* disk, PedPartition* part,
			     const PedConstraint* constraint)
{
	PedGeometry	old_geom;
	PedSector	global_min_start;
	PedSector	global_max_end;
	PedSector	new_start;
	PedSector	new_end;
	PedPartition*	ext_part = ped_disk_extended_partition (disk);
	PedConstraint*	constraint_any;

	PED_ASSERT (disk != NULL, return 0);
	PED_ASSERT (part != NULL, return 0);

	if (part->type & PED_PARTITION_LOGICAL) {
		PED_ASSERT (ext_part != NULL, return 0);
		global_min_start = ext_part->geom.start;
		global_max_end = ext_part->geom.end;
	} else {
		global_min_start = 0;
		global_max_end = disk->dev->length - 1;
	}

	old_geom = part->geom;

	_disk_push_update_mode (disk);

	if (part->prev)
		new_start = part->prev->geom.end + 1;
	else
		new_start = global_min_start;

	if (part->next)
		new_end = part->next->geom.start - 1;
	else
		new_end = global_max_end;

	if (!ped_disk_set_partition_geom (disk, part, constraint, new_start,
					  new_end))
		goto error;

	_disk_pop_update_mode (disk);
	return 1;

error:
	constraint_any = ped_constraint_any (disk->dev);
	ped_disk_set_partition_geom (disk, part, constraint_any,
				     old_geom.start, old_geom.end);
	ped_constraint_destroy (constraint_any);
	_disk_pop_update_mode (disk);
	return 0;
}

PedGeometry*
ped_disk_get_max_partition_geometry (PedDisk* disk, PedPartition* part,
				     const PedConstraint* constraint)
{
	PedGeometry	old_geom;
	PedGeometry*	max_geom;
	PedConstraint*	constraint_exact;

	PED_ASSERT(disk != NULL, return NULL);
	PED_ASSERT(part != NULL, return NULL);
	PED_ASSERT(ped_partition_is_active (part), return NULL);

	old_geom = part->geom;
	if (!ped_disk_maximize_partition (disk, part, constraint))
		return NULL;
	max_geom = ped_geometry_duplicate (&part->geom);

	constraint_exact = ped_constraint_exact (&old_geom);
	ped_disk_set_partition_geom (disk, part, constraint_exact,
				     old_geom.start, old_geom.end);
	ped_constraint_destroy (constraint_exact);

	/* this assertion should never fail, because the old
	 * geometry was valid
	 */
	PED_ASSERT (ped_geometry_test_equal (&part->geom, &old_geom),
		    return NULL);

	return max_geom;
}

/* Reduces the size of the extended partition to wrap the logical partitions.
 * If there are no logical partitions, it removes the extended partition.
 */
int
ped_disk_minimize_extended_partition (PedDisk* disk)
{
	PedPartition*		first_logical;
	PedPartition*		last_logical;
	PedPartition*		walk;
	PedPartition*		ext_part;
	PedConstraint*		constraint;
	int			status;

	PED_ASSERT (disk != NULL, return 0);

	ext_part = ped_disk_extended_partition (disk);
	if (!ext_part)
		return 1;

	_disk_push_update_mode (disk);

	first_logical = ext_part->part_list;
	if (!first_logical) {
		_disk_pop_update_mode (disk);
		return ped_disk_delete_partition (disk, ext_part);
	}

	for (walk = first_logical; walk->next; walk = walk->next);
	last_logical = walk;

	constraint = ped_constraint_any (disk->dev);
	status = ped_disk_set_partition_geom (disk, ext_part, constraint,
					      first_logical->geom.start,
					      last_logical->geom.end);
	ped_constraint_destroy (constraint);

	_disk_pop_update_mode (disk);
	return status;
}

const char*
ped_partition_type_get_name (PedPartitionType type)
{
	if (type & PED_PARTITION_METADATA)
		return N_("metadata");
	else if (type & PED_PARTITION_FREESPACE)
		return N_("free");
	else if (type & PED_PARTITION_EXTENDED)
		return N_("extended");
	else if (type & PED_PARTITION_LOGICAL)
		return N_("logical");
	else
		return N_("primary");
}

/* returns the English name of the flag.  Get the native name by calling
 * dgettext("parted", XXX) on the English name.
 */
const char*
ped_partition_flag_get_name (PedPartitionFlag flag)
{
	switch (flag) {
	case PED_PARTITION_BOOT:
		return N_("boot");
	case PED_PARTITION_ROOT:
		return N_("root");
	case PED_PARTITION_SWAP:
		return N_("swap");
	case PED_PARTITION_HIDDEN:
		return N_("hidden");
	case PED_PARTITION_RAID:
		return N_("raid");
	case PED_PARTITION_LVM:
		return N_("lvm");
	case PED_PARTITION_LBA:
		return N_("lba");
	case PED_PARTITION_HPSERVICE:
		return N_("hp-service");
	case PED_PARTITION_PALO:
		return N_("palo");
	case PED_PARTITION_PREP:
		return N_("prep");

	default:
		ped_exception_throw (
			PED_EXCEPTION_BUG,
			PED_EXCEPTION_CANCEL,
			_("Unknown partition flag, %d."),
			flag);
		return NULL;
	}
}

PedPartitionFlag
ped_partition_flag_next (PedPartitionFlag flag)
{
	return (flag + 1) % (PED_PARTITION_LAST_FLAG + 1);
}

PedPartitionFlag
ped_partition_flag_get_by_name (const char* name)
{
	PedPartitionFlag	flag;
	const char*		flag_name;

	for (flag = ped_partition_flag_next (0); flag;
	     		flag = ped_partition_flag_next (flag)) {
		flag_name = ped_partition_flag_get_name (flag);
		if (strcasecmp (name, flag_name) == 0
		    || strcasecmp (name, _(flag_name)) == 0)
			return flag;
	}

	return 0;
}


void
ped_partition_print (PedPartition* part)
{
	PED_ASSERT (part != NULL, return);

	printf ("  %-10s %02d  (%d->%d)\n",
		ped_partition_type_get_name (part->type),
		part->num,
		(int) part->geom.start, (int) part->geom.end);
}

void
ped_disk_print (PedDisk* disk)
{
	PedPartition*	part;

	PED_ASSERT (disk != NULL, return);

	for (part = disk->part_list; part;
	     part = ped_disk_next_partition (disk, part))
		ped_partition_print (part);
}

