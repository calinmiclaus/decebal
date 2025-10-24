/*
    parted - a frontend to libparted
    Copyright (C) 1999, 2000, 2001, 2002, 2003, 2005
    Free Software Foundation, Inc.

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

#include "../config.h"
#include "command.h"
#include "ui.h"

#define N_(String) String
#if ENABLE_NLS
#  include <libintl.h>
#  include <locale.h>
#  define _(String) dgettext (PACKAGE, String)
#else
#  define _(String) (String)
#endif /* ENABLE_NLS */

#include <parted/parted.h>
#include <parted/debug.h>

#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#ifdef ENABLE_MTRACE
#include <mcheck.h>
#endif

#ifdef HAVE_GETOPT_H
#include <getopt.h>

/* minimum amount of free space to leave, or maximum amount to gobble up,
 * depending on your POV ;)
 */
#define MIN_FREESPACE		(1000 * 2)	/* 1000k */

#define MEGABYTE_SECTORS (PED_MEGABYTE_SIZE / PED_SECTOR_SIZE)


typedef struct {
	time_t	last_update;
	time_t	predicted_time_left;
} TimerContext;

static struct option	options[] = {
	/* name, has-arg, string-return-val, char-return-val */
	{"help",	0, NULL, 'h'},
	{"interactive",	0, NULL, 'i'},
	{"script",	0, NULL, 's'},
	{"version",	0, NULL, 'v'},
	{NULL,		0, NULL, 0}
};
#endif

static char*	options_help [][2] = {
	{"help",	N_("displays this help message")},
	{"interactive",	N_("where necessary, prompts for user intervention")},
	{"script",	N_("never prompts for user intervention")},
	{"version",	N_("displays the version")},
	{NULL,		NULL}
};

int	opt_script_mode;

static char* number_msg = N_(
"NUMBER is the partition number used by Linux.  On msdos disk labels, the "
"primary partitions number from 1 to 4, logical partitions from 5 onwards.\n");

static char* label_type_msg_start = N_("LABEL-TYPE is one of: ");
static char* flag_msg_start =	N_("FLAG is one of: ");
static char* unit_msg_start =	N_("UNIT is one of: ");
static char* part_type_msg =	N_("PART-TYPE is one of: primary, logical, "
			           "extended\n");
static char* fs_type_msg_start = N_("FS-TYPE is one of: ");
static char* start_end_msg =	N_("START and END are disk locations, such as "
		"4GB or 10%.  A prefix of = indicates an exact location.  "
		"Negative values count from the end of the disk.  For example, "
		"=-1s specifies exactly the last sector.\n");
static char* state_msg =	N_("STATE is one of: on, off\n");
static char* device_msg =	N_("DEVICE is usually /dev/hda or /dev/sda\n");
static char* name_msg =		N_("NAME is any word you want\n");
static char* resize_msg_start = N_("The partition must have one of the "
				   "following FS-TYPEs: ");

static char* label_type_msg;
static char* flag_msg;
static char* unit_msg;

static char* mkfs_fs_type_msg;
static char* mkpart_fs_type_msg;
static char* resize_fs_type_msg;

static Command*	commands [256] = {NULL};
static PedTimer* timer;
static TimerContext timer_context;

static void _done (PedDevice* dev);

static void
_timer_handler (PedTimer* timer, void* context)
{
	TimerContext*	tcontext = (TimerContext*) context;
	int		draw_this_time;

	if (opt_script_mode || !isatty(fileno(stdout)))
		return;

	if (tcontext->last_update != timer->now && timer->now > timer->start) {
		tcontext->predicted_time_left
			= timer->predicted_end - timer->now;
		tcontext->last_update = timer->now;
		draw_this_time = 1;
	} else {
		draw_this_time = 0;
	}

	if (draw_this_time) {
		wipe_line ();

		if (timer->state_name)
			printf ("%s... ", timer->state_name);
		printf (_("%0.f%%\t(time left %.2d:%.2d)"),
			100.0 * timer->frac,
			tcontext->predicted_time_left / 60,
			tcontext->predicted_time_left % 60);

		fflush (stdout);
	}
}

static int
_partition_warn_busy (PedPartition* part)
{
	char* path = ped_partition_get_path (part);

	if (ped_partition_is_busy (part)) {
		ped_exception_throw (
			PED_EXCEPTION_ERROR,
			PED_EXCEPTION_CANCEL,
			_("Partition %s is being used.  You must unmount it "
			  "before you modify it with Parted."),
			path);
		ped_free (path);
		return 0;
	}
	ped_free (path);
	return 1;
}

static int
_disk_warn_busy (PedDisk* disk)
{
	if (ped_device_is_busy (disk->dev)) {
		if (ped_exception_throw (
			PED_EXCEPTION_WARNING,
			PED_EXCEPTION_IGNORE_CANCEL,
			_("Partition(s) on %s are being used."),
			disk->dev->path)
				!= PED_EXCEPTION_IGNORE)
			return 0;
	}
	return 1;
}

static inline PedSector
_ped_abs (PedSector sector)
{
	return sector > 0 ? sector : -sector;
}

/* check if what the user will get is roughly what the user wanted.  If it isn't
 * then ask them if they like what they're going to get.  (That's what msg is
 * for).   msg should contain 4 %f's... requested start/end, and the solution
 * start/end.
 * 	Returns 1 iff all is ok.
 */
static int
_solution_check_distant (PedDevice* dev,
		         PedSector req_start, PedSector req_end,
			 PedSector soln_start, PedSector soln_end,
			 const char* msg)
{
	PedSector	start_delta = _ped_abs (soln_start - req_start);
	PedSector	end_delta = _ped_abs (soln_end - req_end);
	PedSector	distance = start_delta + end_delta;
	float		distance_frac;

	distance_frac = PED_MAX (1.0 * distance / (req_end - req_start + 1),
				 1.0 * distance / (soln_end - soln_start + 1));

	if (distance == -1
	    || (distance > 10 * MEGABYTE_SECTORS && distance_frac > 0.1)) {
		char* req_start_str = ped_unit_format (dev, req_start);
		char* req_end_str = ped_unit_format (dev, req_end);
		char* soln_start_str = ped_unit_format (dev, soln_start);
		char* soln_end_str = ped_unit_format (dev, soln_end);
		int choice = ped_exception_throw (
				PED_EXCEPTION_WARNING,
				PED_EXCEPTION_OK_CANCEL,
				msg,
				req_start_str, req_end_str,
				soln_start_str, soln_end_str);
		ped_free (req_start_str);
		ped_free (req_end_str);
		ped_free (soln_start_str);
		ped_free (soln_end_str);
		if (choice == PED_EXCEPTION_CANCEL)
			return 0;
	}

	return 1;
}

static PedSector
_get_left_bound (PedSector sector, PedDisk* disk)
{
	PedPartition*	under_sector;

	under_sector = ped_disk_get_partition_by_sector (disk, sector);
	PED_ASSERT (under_sector != NULL, return sector);

	if (under_sector->type & PED_PARTITION_FREESPACE)
		return under_sector->geom.start;
	else
		return sector;
}

static PedSector
_get_right_bound (PedSector sector, PedDisk* disk)
{
	PedPartition*	under_sector;

	under_sector = ped_disk_get_partition_by_sector (disk, sector);
	PED_ASSERT (under_sector != NULL, return sector);

	if (under_sector->type & PED_PARTITION_FREESPACE)
		return under_sector->geom.end;
	else
		return sector;
}

/* if the gap between the new partition and the partition-to-be-aligned is
 * less than MIN_FREESPACE, then gobble up the gap!
 */
static int
_grow_over_small_freespace (PedGeometry* geom, PedDisk* disk)
{
	PedSector	start;
	PedSector	end;

	/* hack: give full control for small partitions */
	if (geom->length < MIN_FREESPACE * 5)
		return 1;

	start = _get_left_bound (geom->start, disk);
	PED_ASSERT (start < geom->end, return 0);
	if (geom->start - start < MIN_FREESPACE)
		ped_geometry_set_start (geom, start);

	end = _get_right_bound (geom->end, disk);
	PED_ASSERT (end > geom->start, return 0);
	if (end - geom->end < MIN_FREESPACE)
		ped_geometry_set_end (geom, end);
	return 1;
}

/* This functions constructs a constraint from the following information:
 * 	start, is_start_exact, end, is_end_exact.
 * 	
 * If is_start_exact == 1, then the constraint requires start be as given in
 * "start".  Otherwise, the constraint does not set any requirements on the
 * start.
 */
static PedConstraint*
constraint_from_start_end (PedDevice* dev, PedGeometry* range_start,
                           PedGeometry* range_end)
{
	return ped_constraint_new (ped_alignment_any, ped_alignment_any,
		range_start, range_end, 1, dev->length);
}

static PedConstraint*
constraint_intersect_and_destroy (PedConstraint* a, PedConstraint* b)
{
	PedConstraint* result = ped_constraint_intersect (a, b);
	ped_constraint_destroy (a);
	ped_constraint_destroy (b);
	return result;
}

void
help_on (char* topic)
{
	Command*	cmd;

	cmd = command_get (commands, topic);
	if (!cmd) return;

	command_print_help (cmd);
}

static int
do_check (PedDevice** dev)
{
	PedDisk*	disk;
	PedFileSystem*	fs;
	PedPartition*	part = NULL;
	int		part_num;

	disk = ped_disk_new (*dev);
	if (!disk)
		goto error;

	if (!command_line_get_partition (_("Partition number?"), disk, &part))
		goto error_destroy_disk;
	if (!_partition_warn_busy (part))
		goto error_destroy_disk;

	if (!ped_disk_check (disk))
		goto error_destroy_disk;

	fs = ped_file_system_open (&part->geom);
	if (!fs)
		goto error_destroy_disk;
	if (!ped_file_system_check (fs, timer))
		goto error_close_fs;
	ped_file_system_close (fs);
	ped_disk_destroy (disk);
	return 1;

error_close_fs:
	ped_file_system_close (fs);
error_destroy_disk:
	ped_disk_destroy (disk);
error:
	return 0;
}

static int
do_cp (PedDevice** dev)
{
	PedDisk*		src_disk;
	PedDisk*		dst_disk;
	PedDevice*		src_device;
	PedPartition*		src = NULL;
	PedPartition*		dst = NULL;
	PedFileSystem*		src_fs;
	PedFileSystem*		dst_fs;
	PedFileSystemType*	dst_fs_type;

	dst_disk = ped_disk_new (*dev);
	if (!dst_disk)
		goto error;

	src_disk = dst_disk;
	if (!command_line_is_integer ()) {
		if (!command_line_get_disk (_("Source device?"), &src_disk))
			goto error_destroy_disk;
	}

	if (!command_line_get_partition (_("Source partition number?"),
					 src_disk, &src))
		goto error_destroy_disk;
	if (src->type == PED_PARTITION_EXTENDED) {
		ped_exception_throw (PED_EXCEPTION_ERROR, PED_EXCEPTION_CANCEL,
			_("Can't copy an extended partition."));
		goto error_destroy_disk;
	}
	if (!_partition_warn_busy (src))
		goto error_destroy_disk;

	if (!command_line_get_partition (_("Destination partition number?"),
					 dst_disk, &dst))
		goto error_destroy_disk;
	if (!_partition_warn_busy (dst))
		goto error_destroy_disk;

/* do the copy */
	src_fs = ped_file_system_open (&src->geom);
	if (!src_fs)
		goto error_destroy_disk;
	dst_fs = ped_file_system_copy (src_fs, &dst->geom, timer);
	if (!dst_fs)
		goto error_close_src_fs;
	dst_fs_type = dst_fs->type;	/* may be different to src_fs->type */
	ped_file_system_close (src_fs);
	ped_file_system_close (dst_fs);

/* update the partition table, close disks */
	if (!ped_partition_set_system (dst, dst_fs_type))
		goto error_destroy_disk;
	if (!ped_disk_commit (dst_disk))
		goto error_destroy_disk;
	if (src_disk != dst_disk)
		ped_disk_destroy (src_disk);
	ped_disk_destroy (dst_disk);
	return 1;

error_close_src_fs:
	ped_file_system_close (src_fs);
error_destroy_disk:
	if (src_disk && src_disk != dst_disk)
		ped_disk_destroy (src_disk);
	ped_disk_destroy (dst_disk);
error:
	return 0;
}

void
print_commands_help ()
{
	int		i;

	for (i=0; commands [i]; i++)
		command_print_summary (commands [i]);
}

void
print_options_help ()
{
	int		i;

	for (i=0; options_help [i][0]; i++) {
		printf ("  -%c, --%-23.23s %s\n",
			options_help [i][0][0],
			options_help [i][0],
			_(options_help [i][1]));
	}
}

int
do_help (PedDevice** dev)
{
	if (command_line_get_word_count ()) {
		char*	word = command_line_pop_word ();
		if (word) {
			help_on (word);
			free (word);
		}
	} else {
		print_commands_help();
	}
	return 1;
}

static int
do_mklabel (PedDevice** dev)
{
	PedDisk*		disk;
	const PedDiskType*	type = ped_disk_probe (*dev);

	ped_exception_fetch_all ();
	disk = ped_disk_new (*dev);
	if (!disk) ped_exception_catch ();
	ped_exception_leave_all ();

	if (disk) {
		if (!_disk_warn_busy (disk)) {
			ped_disk_destroy (disk);
			goto error;
		}
		ped_disk_destroy (disk);
	}

	if (!command_line_get_disk_type (_("New disk label type?"), &type))
		goto error;

	disk = ped_disk_new_fresh (*dev, type);
	if (!disk)
		goto error;

	if (!ped_disk_commit (disk))
		goto error_destroy_disk;
	ped_disk_destroy (disk);
	return 1;

error_destroy_disk:
	ped_disk_destroy (disk);
error:
	return 0;
}

static int
do_mkfs (PedDevice** dev)
{
	PedDisk*		disk;
	PedPartition*		part = NULL;
	const PedFileSystemType* type = ped_file_system_type_get ("ext2");
	PedFileSystem*		fs;

	disk = ped_disk_new (*dev);
	if (!disk)
		goto error;

	if (!command_line_get_partition (_("Partition number?"), disk, &part))
		goto error_destroy_disk;
	if (!_partition_warn_busy (part))
		goto error_destroy_disk;
	if (!command_line_get_fs_type (_("File system?"), &type))
		goto error_destroy_disk;

	fs = ped_file_system_create (&part->geom, type, timer);
	if (!fs)
		goto error_destroy_disk;
	ped_file_system_close (fs);

	if (!ped_partition_set_system (part, type))
		goto error_destroy_disk;
	if (!ped_disk_commit (disk))
		goto error_destroy_disk;
	ped_disk_destroy (disk);
	return 1;

error_destroy_disk:
	ped_disk_destroy (disk);
error:
	return 0;
}

static int
do_mkpart (PedDevice** dev)
{
	PedDisk*	  	 disk;
	PedPartition* 		 part;
	PedPartitionType	 part_type;
	const PedFileSystemType* fs_type = ped_file_system_type_get ("ext2");
	PedSector		 start = 0, end = 0;
	PedGeometry 		 *range_start, *range_end;
	PedConstraint*		 constraint;
	char*			 peek_word;

	disk = ped_disk_new (*dev);
	if (!disk)
		goto error;

	if (!command_line_get_part_type (_("Partition type?"), disk,
					 &part_type))
		goto error_destroy_disk;

	peek_word = command_line_peek_word ();
	if (part_type == PED_PARTITION_EXTENDED
	    || (peek_word && isdigit (peek_word[0]))) {
		fs_type = NULL;
	} else {
		if (!command_line_get_fs_type (_("File system type?"),
					       &fs_type))
			goto error_destroy_disk;
	}
	if (peek_word)
		ped_free (peek_word);

	if (!command_line_get_sector (_("Start?"), *dev, &start, &range_start))
		goto error_destroy_disk;
	if (!command_line_get_sector (_("End?"), *dev, &end, &range_end))
		goto error_destroy_disk;
	part = ped_partition_new (disk, part_type, fs_type, start, end);
	if (!part)
		goto error_destroy_disk;
	if (!_grow_over_small_freespace (&part->geom, disk))
		goto error_destroy_part;
	constraint = constraint_from_start_end (*dev, range_start, range_end);
	if (!constraint)
		goto error_destroy_part;
	if (!ped_disk_add_partition (disk, part, constraint))
		goto error_destroy_constraint;
	ped_constraint_destroy (constraint);

	if (!_solution_check_distant (*dev, start, end,
				      part->geom.start, part->geom.end,
		_("You requested to create a partition at %s - %s. The "
		  "closest Parted can manage is %s - %s.")))
		goto error_destroy_disk;

	if (!ped_partition_set_system (part, fs_type))
		goto error_destroy_disk;
	if (!ped_disk_commit (disk))
		goto error_destroy_disk;
	ped_disk_destroy (disk);
	return 1;

error_destroy_constraint:
	ped_constraint_destroy (constraint);
error_destroy_part:
	ped_partition_destroy (part);
error_destroy_disk:
	ped_disk_destroy (disk);
error:
	return 0;
}

static int
do_mkpartfs (PedDevice** dev)
{
	PedDisk*            disk;
	PedPartition*       part;
	PedPartitionType    part_type;
	const PedFileSystemType* fs_type = ped_file_system_type_get ("ext2");
	PedSector           start = 0, end = 0;
	PedGeometry         *range_start, *range_end;
	PedConstraint*      constraint;
	PedFileSystem*      fs;

	disk = ped_disk_new (*dev);
	if (!disk)
		goto error;

	if (!command_line_get_part_type (_("Partition type?"), disk,
					 &part_type))
		goto error_destroy_disk;

	if (part_type == PED_PARTITION_EXTENDED) {
		ped_exception_throw (PED_EXCEPTION_ERROR, PED_EXCEPTION_CANCEL,
			_("An extended partition cannot hold a file system.  "
			  "Did you want mkpart?"));
		goto error_destroy_disk;
	}

	if (!command_line_get_fs_type (_("File system type?"), &fs_type))
		goto error_destroy_disk;
	if (!command_line_get_sector (_("Start?"), *dev, &start,
				      &range_start))
		goto error_destroy_disk;
	if (!command_line_get_sector (_("End?"), *dev, &end, &range_end))
		goto error_destroy_disk;
	part = ped_partition_new (disk, part_type, fs_type, start, end);
	if (!part)
		goto error_destroy_disk;
	if (!_grow_over_small_freespace (&part->geom, disk))
		goto error_destroy_part;
	constraint = constraint_intersect_and_destroy (
			ped_file_system_get_create_constraint (fs_type, *dev),
			constraint_from_start_end (*dev, range_start, range_end));
	if (!ped_disk_add_partition (disk, part, constraint))
		goto error_destroy_constraint;
	ped_constraint_destroy (constraint);

	if (!_solution_check_distant (*dev, start, end,
				      part->geom.start, part->geom.end,
		_("You requested to create a partition at %s - %s. The "
		  "closest Parted can manage is %s - %s.")))
		goto error_destroy_disk;

	fs = ped_file_system_create (&part->geom, fs_type, timer);
	if (!fs) 
		goto error_destroy_disk;
	ped_file_system_close (fs);

	if (!ped_partition_set_system (part, fs_type))
		goto error_destroy_disk;

	if (!ped_disk_commit (disk))
		goto error_destroy_disk;
	ped_disk_destroy (disk);
	return 1;

error_destroy_constraint:
	ped_constraint_destroy (constraint);
error_destroy_part:
	ped_partition_destroy (part);
error_destroy_disk:
	ped_disk_destroy (disk);
error:
	return 0;
}

static int
do_move (PedDevice** dev)
{
	PedDisk*	disk;
	PedPartition*	part = NULL;
	PedFileSystem*	fs;
	PedFileSystem*	fs_copy;
	PedConstraint*	constraint;
	PedSector	start = 0, end = 0;
	PedGeometry     *range_start, *range_end;
	PedGeometry	old_geom, new_geom;

	disk = ped_disk_new (*dev);
	if (!disk)
		goto error;

	if (!command_line_get_partition (_("Partition number?"), disk, &part))
		goto error_destroy_disk;
	if (!_partition_warn_busy (part))
		goto error_destroy_disk;
	if (part->type == PED_PARTITION_EXTENDED) {
		ped_exception_throw (PED_EXCEPTION_ERROR, PED_EXCEPTION_CANCEL,
			_("Can't move an extended partition."));
		goto error_destroy_disk;
	}
	old_geom = part->geom;
	fs = ped_file_system_open (&old_geom);
	if (!fs)
		goto error_destroy_disk;

	/* get new target */
	if (!command_line_get_sector (_("Start?"), *dev, &start, &range_start))
		goto error_close_fs;
	end = start + old_geom.length - 1;
	if (!command_line_get_sector (_("End?"), *dev, &end, &range_end))
		goto error_close_fs;

	/* set / test on "disk" */
	if (!ped_geometry_init (&new_geom, *dev, start, end - start + 1))
		goto error_close_fs;
	if (!_grow_over_small_freespace (&new_geom, disk))
		goto error_close_fs;

	constraint = constraint_intersect_and_destroy (
			ped_file_system_get_copy_constraint (fs, *dev),
			constraint_from_start_end (*dev, range_start, range_end));
	if (!ped_disk_set_partition_geom (disk, part, constraint,
					  new_geom.start, new_geom.end))
		goto error_destroy_constraint;
	ped_constraint_destroy (constraint);
	if (ped_geometry_test_overlap (&old_geom, &part->geom)) {
		ped_exception_throw (PED_EXCEPTION_ERROR, PED_EXCEPTION_CANCEL,
			_("Can't move a partition onto itself.  Try using "
			  "resize, perhaps?"));
		goto error_close_fs;
	}
	if (!_solution_check_distant (*dev, start, end,
				      part->geom.start, part->geom.end,
		_("You requested to move the partition to %s - %s. The "
		  "closest Parted can manage is %s - %s.")))
		goto error_close_fs;

/* do the move */
	fs_copy = ped_file_system_copy (fs, &part->geom, timer);
	if (!fs_copy)
		goto error_close_fs;
	ped_file_system_close (fs_copy);
	ped_file_system_close (fs);
	if (!ped_disk_commit (disk))
		goto error_destroy_disk;
	ped_disk_destroy (disk);
	return 1;

error_destroy_constraint:
	ped_constraint_destroy (constraint);
error_close_fs:
	ped_file_system_close (fs);
error_destroy_disk:
	ped_disk_destroy (disk);
error:
	return 0;
}

static int
do_name (PedDevice** dev)
{
	PedDisk*	disk;
	PedPartition*	part = NULL;
	char*		name;

	disk = ped_disk_new (*dev);
	if (!disk)
		goto error;

	if (!command_line_get_partition (_("Partition number?"), disk, &part))
		goto error_destroy_disk;

	name = command_line_get_word (_("Partition name?"),
			ped_partition_get_name (part), NULL, 0);
	if (!name)
		goto error_destroy_disk;
	if (!ped_partition_set_name (part, name))
		goto error_free_name;
	free (name);

	if (!ped_disk_commit (disk))
		goto error_destroy_disk;
	ped_disk_destroy (disk);
	return 1;

error_free_name:
	free (name);
error_destroy_disk:
	ped_disk_destroy (disk);
error:
	return 0;
}

static void
partition_print_flags (PedPartition* part)
{
	PedPartitionFlag	flag;
	int			first_flag;

	first_flag = 1;
	for (flag = ped_partition_flag_next (0); flag;
	     flag = ped_partition_flag_next (flag)) {
		if (ped_partition_get_flag (part, flag)) {
			if (first_flag)
				first_flag = 0;
			else
				printf (", ");
			printf (_(ped_partition_flag_get_name (flag)));
		}
	}
}

/* Prints a sector out, first in compact form, and then with a percentage.
 * Eg: 32Gb (40%)
 */
static void
print_sector_compact_and_percent (PedSector sector, PedDevice* dev)
{
	char* compact;
	char* percent;

	if (ped_unit_get_default() == PED_UNIT_PERCENT)
		compact = ped_unit_format (dev, sector);
	else
		compact = ped_unit_format_custom (dev, sector,
						  PED_UNIT_COMPACT);

	percent = ped_unit_format_custom (dev, sector, PED_UNIT_PERCENT);

	printf ("%s (%s)\n", compact, percent);

	ped_free (compact);
	ped_free (percent);
}

static int
partition_print (PedPartition* part)
{
	PedFileSystem*	fs;
	PedConstraint*	resize_constraint;

	fs = ped_file_system_open (&part->geom);
	if (!fs)
		return 1;

	printf (_("Minor: %d\n"), part->num);
	printf (_("Flags: ")); partition_print_flags (part); printf("\n");
	printf (_("File System: %s\n"), fs->type->name);
	printf (_("Size:         "));
	print_sector_compact_and_percent (part->geom.length, part->geom.dev);

	resize_constraint = ped_file_system_get_resize_constraint (fs);
	if (resize_constraint) {
		printf (_("Minimum size: "));
		print_sector_compact_and_percent (resize_constraint->min_size,
			part->geom.dev);
		printf (_("Maximum size: "));
		print_sector_compact_and_percent (resize_constraint->max_size,
			part->geom.dev);
		ped_constraint_destroy (resize_constraint);
	}

	ped_file_system_close (fs);
	return 1;
}

static int
do_print (PedDevice** dev)
{
	PedDisk*	disk;
	PedPartition*	part;
	int		has_extended;
	int		has_name;
	int		has_num_arg;
	char*		peek_word;
	char*		start;
	char*		end;
	char*		size;

	disk = ped_disk_new (*dev);
	if (!disk)
		goto error;

	peek_word = command_line_peek_word ();
	if (peek_word) {
		has_num_arg = isdigit (peek_word[0]);
		ped_free (peek_word);
	} else {
		has_num_arg = 0;
	}

	if (has_num_arg) {
		PedPartition*	part = NULL;
		int		status = 0;
		if (command_line_get_partition ("", disk, &part))
			status = partition_print (part);
		ped_disk_destroy (disk);
		return status;
	}

	start = ped_unit_format (*dev,0 );
	end = ped_unit_format (*dev, (*dev)->length - 1);
	printf (_("Disk geometry for %s: %s - %s\n"), (*dev)->path, start, end);
	ped_free (start);
	ped_free (end);

	if (ped_unit_get_default () == PED_UNIT_CHS
	    || ped_unit_get_default () == PED_UNIT_CYLINDER) {
		PedCHSGeometry* chs = &(*dev)->bios_geom;
		char* cyl_size = ped_unit_format_custom (*dev,
					chs->heads * chs->sectors,
					PED_UNIT_KILOBYTE);
		printf (_("BIOS cylinder,head,sector geometry: %d,%d,%d.  "
			  "Each cylinder is %s.\n"),
			chs->cylinders, chs->heads, chs->sectors, cyl_size);
		ped_free (cyl_size);
	}

	printf (_("Disk label type: %s\n"), disk->type->name);

	has_extended = ped_disk_type_check_feature (disk->type,
			       		 PED_DISK_TYPE_EXTENDED);
	has_name = ped_disk_type_check_feature (disk->type,
			       		 PED_DISK_TYPE_PARTITION_NAME);

	if (ped_unit_get_default() == PED_UNIT_CHS) {
		printf ("%-7s %-11s %-11s ", _("Number"), _("Start"),
			_("End"));
	} else  {
		printf ("%-7s %-7s %-7s %-7s ", _("Number"), _("Start"),
			_("End"), _("Size"));
	}
	if (has_extended)
		printf ("%-9s ", _("Type"));
	printf ("%-12s ", _("File system"));
	if (has_name)
		printf ("%-21s ", _("Name"));
	printf (_("Flags"));
	printf ("\n");

	for (part = ped_disk_next_partition (disk, NULL); part;
	     part = ped_disk_next_partition (disk, part)) {
		
		if (!ped_partition_is_active (part))
	       		continue;

		printf ("%-7d ", part->num);

		start = ped_unit_format (*dev, part->geom.start);
		end = ped_unit_format (*dev, part->geom.end);
		size = ped_unit_format (*dev, part->geom.length);
		if (ped_unit_get_default() == PED_UNIT_CHS)
			printf ("%-11s %-11s ", start, end);
		else
			printf ("%-7s %-7s %-7s ", start, end, size);
		ped_free (start);
		ped_free (end);
		ped_free (size);

		if (has_extended)
			printf ("%-9s ",
				_(ped_partition_type_get_name (part->type)));

		printf ("%-12s ", part->fs_type ? part->fs_type->name : "");

		if (has_name)
			printf ("%-21s ", ped_partition_get_name (part));

		partition_print_flags (part);
		printf ("\n");
	}

	ped_disk_destroy (disk);
	return 1;

error_destroy_disk:
	ped_disk_destroy (disk);
error:
	return 0;
}

static int
do_quit (PedDevice** dev)
{
	_done (*dev);
	exit (0);
}

static PedPartitionType
_disk_get_part_type_for_sector (PedDisk* disk, PedSector sector)
{
	PedPartition*	extended;

	extended = ped_disk_extended_partition (disk);
	if (!extended
	    || !ped_geometry_test_sector_inside (&extended->geom, sector))
		return 0;

	return PED_PARTITION_LOGICAL;
}

/* This function checks if "part" contains a file system, and returs
 * 	0 if either no file system was found, or the user declined to add it.
 * 	1 if a file system was found, and the user chose to add it.
 * 	-1 if the user chose to cancel the entire search.
 */
static int
_rescue_add_partition (PedPartition* part)
{
	const PedFileSystemType*	fs_type;
	PedGeometry*			probed;
	PedExceptionOption		ex_opt;
	PedConstraint*			constraint;
	char*				found_start;
	char*				found_end;

	fs_type = ped_file_system_probe (&part->geom);
	if (!fs_type)
		return 0;
	probed = ped_file_system_probe_specific (fs_type, &part->geom);
	if (!probed)
		return 0;

	if (!ped_geometry_test_inside (&part->geom, probed)) {
		ped_geometry_destroy (probed);
		return 0;
	}

	constraint = ped_constraint_exact (probed);
	if (!ped_disk_set_partition_geom (part->disk, part, constraint,
					  probed->start, probed->end)) {
		ped_constraint_destroy (constraint);
		return 0;
	}
	ped_constraint_destroy (constraint);

	found_start = ped_unit_format (probed->dev, probed->start);
	found_end = ped_unit_format (probed->dev, probed->end);
	ex_opt = ped_exception_throw (
		PED_EXCEPTION_INFORMATION,
		PED_EXCEPTION_YES_NO_CANCEL,
		_("A %s %s partition was found at %s -> %s.  "
		  "Do you want to add it to the partition table?"),
		fs_type->name, ped_partition_type_get_name (part->type),
		found_start, found_end);
	ped_geometry_destroy (probed);
	ped_free (found_start);
	ped_free (found_end);

	switch (ex_opt) {
		case PED_EXCEPTION_CANCEL: return -1;
		case PED_EXCEPTION_NO: return 0;
	}

	ped_partition_set_system (part, fs_type);
	ped_disk_commit (part->disk);
	return 1;
}

/* hack: we only iterate through the start, since most (all) fs's have their
 * superblocks at the start.  We'll need to change this if we generalize
 * for RAID, or something...
 */
static int
_rescue_pass (PedDisk* disk, PedGeometry* start_range, PedGeometry* end_range)
{
	PedSector		start;
	PedGeometry		start_geom_exact;
	PedGeometry		entire_dev;
	PedConstraint		constraint;
	PedPartition*		part;
	PedPartitionType	part_type;

	part_type = _disk_get_part_type_for_sector (
			disk, (start_range->start + end_range->end) / 2);

	ped_geometry_init (&entire_dev, disk->dev, 0, disk->dev->length);

	ped_timer_reset (timer);
	ped_timer_set_state_name (timer, _("searching for file systems"));
	for (start = start_range->start; start <= start_range->end; start++) {
		ped_timer_update (timer, 1.0 * (start - start_range->start)
					 / start_range->length);

		ped_geometry_init (&start_geom_exact, disk->dev, start, 1);
		ped_constraint_init (
			&constraint, ped_alignment_any, ped_alignment_any,
			&start_geom_exact, &entire_dev,
			1, disk->dev->length);
		part = ped_partition_new (disk, part_type, NULL, start,
				end_range->end);
		if (!part) {
			ped_constraint_done (&constraint);
			continue;
		}

		ped_exception_fetch_all ();
		if (ped_disk_add_partition (disk, part, &constraint)) {
			ped_exception_leave_all ();
			switch (_rescue_add_partition (part)) {
			case 1:
				ped_constraint_done (&constraint);
				return 1;

			case 0:
				ped_disk_remove_partition (disk, part);
				break;

			case -1:
				goto error_remove_partition;
			}
		} else {
			ped_exception_leave_all ();
		}
		ped_partition_destroy (part);
		ped_constraint_done (&constraint);
	}
	ped_timer_update (timer, 1.0);

	return 1;

error_remove_partition:
	ped_disk_remove_partition (disk, part);
error_partition_destroy:
	ped_partition_destroy (part);
error_constraint_done:
	ped_constraint_done (&constraint);
error:
	return 0;
}

static int
do_rescue (PedDevice** dev)
{
	PedDisk*		disk;
	PedSector		start = 0, end = 0;
	PedSector		fuzz;
	PedGeometry		probe_start_region;
	PedGeometry		probe_end_region;

	disk = ped_disk_new (*dev);
	if (!disk)
		goto error;

	if (!command_line_get_sector (_("Start?"), *dev, &start, NULL))
		goto error_destroy_disk;
	if (!command_line_get_sector (_("End?"), *dev, &end, NULL))
		goto error_destroy_disk;

	fuzz = PED_MAX (PED_MIN ((end - start) / 10, MEGABYTE_SECTORS),
		        MEGABYTE_SECTORS * 16);

	ped_geometry_init (&probe_start_region, *dev,
			   PED_MAX(start - fuzz, 0),
			   PED_MIN(2 * fuzz, (*dev)->length - (start - fuzz)));
	ped_geometry_init (&probe_end_region, *dev,
			   PED_MAX(end - fuzz, 0),
			   PED_MIN(2 * fuzz, (*dev)->length - (end - fuzz)));

	if (!_rescue_pass (disk, &probe_start_region, &probe_end_region))
		goto error_destroy_disk;

	ped_disk_destroy (disk);
	return 1;

error_destroy_disk:
	ped_disk_destroy (disk);
error:
	return 0;
}

static int
do_resize (PedDevice** dev)
{
	PedDisk*		disk;
	PedPartition*		part = NULL;
	PedFileSystem*		fs;
	PedConstraint*		constraint;
	PedSector		start, end;
	PedGeometry     *range_start, *range_end;
	PedGeometry		new_geom;

	disk = ped_disk_new (*dev);
	if (!disk)
		goto error;

	if (!command_line_get_partition (_("Partition number?"), disk, &part))
		goto error_destroy_disk;
	if (part->type != PED_PARTITION_EXTENDED) {
		if (!_partition_warn_busy (part))
			goto error_destroy_disk;
	}

	start = part->geom.start;
	end = part->geom.end;
	if (!command_line_get_sector (_("Start?"), *dev, &start, &range_start))
		goto error_destroy_disk;
	if (!command_line_get_sector (_("End?"), *dev, &end, &range_end))
		goto error_destroy_disk;

	if (!ped_geometry_init (&new_geom, *dev, start, end - start + 1))
		goto error_destroy_disk;
	if (!_grow_over_small_freespace (&new_geom, disk))
		goto error_destroy_disk;

	if (part->type == PED_PARTITION_EXTENDED) {
		constraint = constraint_from_start_end (*dev,
				range_start, range_end);
		if (!ped_disk_set_partition_geom (disk, part, constraint,
						  new_geom.start, new_geom.end))
			goto error_destroy_constraint;
		if (!_solution_check_distant (*dev, start, end,
					      part->geom.start, part->geom.end,
			_("You requested to resize the partition to %s - %s."
			  "  The closest Parted can manage is %s - %s.")))
 			goto error_destroy_constraint;
		ped_partition_set_system (part, NULL);
	} else {
		fs = ped_file_system_open (&part->geom);
		if (!fs)
			goto error_destroy_disk;
		constraint = constraint_intersect_and_destroy (
				ped_file_system_get_resize_constraint (fs),
				constraint_from_start_end (
					*dev, range_start, range_end));
		if (!ped_disk_set_partition_geom (disk, part, constraint,
						  new_geom.start, new_geom.end))
			goto error_close_fs;
		if (!_solution_check_distant (*dev, start, end,
					      part->geom.start, part->geom.end,
			_("You requested to resize the partition to %s - %s."
			  "  The closest Parted can manage is %s - %s.")))
 			goto error_close_fs;
		if (!ped_file_system_resize (fs, &part->geom, timer))
			goto error_close_fs;
		/* may have changed... eg fat16 -> fat32 */
		ped_partition_set_system (part, fs->type);
		ped_file_system_close (fs);
	}

	ped_disk_commit (disk);
	ped_constraint_destroy (constraint);
	ped_disk_destroy (disk);
	return 1;

error_close_fs:
	ped_file_system_close (fs);
error_destroy_constraint:
	ped_constraint_destroy (constraint);
error_destroy_disk:
	ped_disk_destroy (disk);
error:
	return 0;
}

static int
do_rm (PedDevice** dev)
{
	PedDisk*		disk;
	PedPartition*		part = NULL;

	disk = ped_disk_new (*dev);
	if (!disk)
		goto error;

	if (!command_line_get_partition (_("Partition number?"), disk, &part))
		goto error_destroy_disk;
	if (!_partition_warn_busy (part))
		goto error_destroy_disk;

	ped_disk_delete_partition (disk, part);
	ped_disk_commit (disk);
	ped_disk_destroy (disk);
	return 1;

error_destroy_disk:
	ped_disk_destroy (disk);
error:
	return 0;
}

static int
do_select (PedDevice** dev)
{
	PedDevice*	new_dev = *dev;

	if (!command_line_get_device (_("New device?"), &new_dev))
		return 0;
	if (!ped_device_open (new_dev))
		return 0;

	ped_device_close (*dev);
	*dev = new_dev;
	print_using_dev (*dev);
	return 1;
}

static int
do_set (PedDevice** dev)
{
	PedDisk*		disk;
	PedPartition*		part = NULL;
	PedPartitionFlag	flag;
	int			state;

	disk = ped_disk_new (*dev);
	if (!disk)
		goto error;

	if (!command_line_get_partition (_("Partition number?"), disk, &part))
		goto error_destroy_disk;
	if (!command_line_get_part_flag (_("Flag to change?"), part, &flag))
		goto error_destroy_disk;
	state = ped_partition_get_flag (part, flag);
	if (!command_line_get_state (_("New state?"), &state))
		goto error_destroy_disk;

	if (!ped_partition_set_flag (part, flag, state))
		goto error_destroy_disk;
	if (!ped_disk_commit (disk))
		goto error_destroy_disk;
	ped_disk_destroy (disk);
	return 1;

error_destroy_disk:
	ped_disk_destroy (disk);
error:
	return 0;
}

static int
do_unit (PedDevice** dev)
{
	PedUnit unit = ped_unit_get_default ();
	if (!command_line_get_unit (_("Unit?"), &unit))
		return 0;
	ped_unit_set_default (unit);
	return 1;
}

static void
_init_messages ()
{
	StrList*		list;
	int			first;
	PedFileSystemType*	fs_type;
	PedDiskType*		disk_type;
	PedPartitionFlag	part_flag;
	PedUnit			unit;

/* flags */
	first = 1;
	list = str_list_create (_(flag_msg_start), NULL);
	for (part_flag = ped_partition_flag_next (0); part_flag;
	     		part_flag = ped_partition_flag_next (part_flag)) {
		if (first)
			first = 0;
		else
			str_list_append (list, ", ");
		str_list_append (list,
				 _(ped_partition_flag_get_name (part_flag)));
	}
	str_list_append (list, "\n");

	flag_msg = str_list_convert (list);
	str_list_destroy (list);

/* units */
	first = 1;
	list = str_list_create (_(unit_msg_start), NULL);
	for (unit = PED_UNIT_FIRST; unit <= PED_UNIT_LAST; unit++) {
		if (first)
			first = 0;
		else
			str_list_append (list, ", ");
		str_list_append (list, ped_unit_get_name (unit));
	}
	str_list_append (list, "\n");

	unit_msg = str_list_convert (list);
	str_list_destroy (list);

/* disk type */
	list = str_list_create (_(label_type_msg_start), NULL);

	first = 1;
	for (disk_type = ped_disk_type_get_next (NULL);
	     disk_type; disk_type = ped_disk_type_get_next (disk_type)) {
		if (disk_type->ops->write == NULL)
			continue;

		if (first)
			first = 0;
		else
			str_list_append (list, ", ");
		str_list_append (list, disk_type->name);
	}
	str_list_append (list, "\n");

	label_type_msg = str_list_convert (list);
	str_list_destroy (list);

/* mkfs - file system types */
	list = str_list_create (_(fs_type_msg_start), NULL);

	first = 1;
	for (fs_type = ped_file_system_type_get_next (NULL);
	     fs_type; fs_type = ped_file_system_type_get_next (fs_type)) {
		if (fs_type->ops->create == NULL)
			continue;

		if (first)
			first = 0;
		else
			str_list_append (list, ", ");
		str_list_append (list, fs_type->name);
	}
	str_list_append (list, "\n");

	mkfs_fs_type_msg = str_list_convert (list);
	str_list_destroy (list);

/* mkpart - file system types */
	list = str_list_create (_(fs_type_msg_start), NULL);

	first = 1;
	for (fs_type = ped_file_system_type_get_next (NULL);
	     fs_type; fs_type = ped_file_system_type_get_next (fs_type)) {
		if (first)
			first = 0;
		else
			str_list_append (list, ", ");
		str_list_append (list, fs_type->name);
	}
	str_list_append (list, "\n");

	mkpart_fs_type_msg = str_list_convert (list);
	str_list_destroy (list);

/* resize - file system types */
	list = str_list_create (_(resize_msg_start), NULL);

	first = 1;
	for (fs_type = ped_file_system_type_get_next (NULL);
	     fs_type; fs_type = ped_file_system_type_get_next (fs_type)) {
		if (fs_type->ops->resize == NULL)
			continue;

		if (first)
			first = 0;
		else
			str_list_append (list, ", ");
		str_list_append (list, fs_type->name);
	}
	str_list_append (list, "\n");

	resize_fs_type_msg = str_list_convert (list);
	str_list_destroy (list);
}

static void
_done_messages ()
{
	free (flag_msg);
	free (mkfs_fs_type_msg);
	free (mkpart_fs_type_msg);
	free (resize_fs_type_msg);
	free (label_type_msg);
}

static void
_init_commands ()
{
	command_register (commands, command_create (
		str_list_create_unique ("check", _("check"), NULL),
		do_check,
		str_list_create (
_("check NUMBER                  do a simple check on the file system"),
NULL),
		str_list_create (_(number_msg), NULL)));

	command_register (commands, command_create (
		str_list_create_unique ("cp", _("cp"), NULL),
		do_cp,
		str_list_create (
_("cp [FROM-DEVICE] FROM-NUMBER TO-NUMBER   copy file system to another "
  "partition"),
NULL),
		str_list_create (_(number_msg), _(device_msg), NULL)));

	command_register (commands, command_create (
		str_list_create_unique ("help", _("help"), NULL),
		do_help,
		str_list_create (
_("help [COMMAND]                prints general help, or help on COMMAND"),
NULL),
		NULL));

	command_register (commands, command_create (
		str_list_create_unique ("mklabel", _("mklabel"), NULL),
		do_mklabel,
		str_list_create (
_("mklabel LABEL-TYPE            create a new disklabel (partition table)"),
NULL),
		str_list_create (label_type_msg, NULL)));

	command_register (commands, command_create (
		str_list_create_unique ("mkfs", _("mkfs"), NULL),
		do_mkfs,
		str_list_create (
_("mkfs NUMBER FS-TYPE           make a FS-TYPE file system on partititon "
  "NUMBER"),
NULL),
		str_list_create (_(number_msg), _(mkfs_fs_type_msg), NULL)));

	command_register (commands, command_create (
		str_list_create_unique ("mkpart", _("mkpart"), NULL),
		do_mkpart,
		str_list_create (
_("mkpart PART-TYPE [FS-TYPE] START END     make a partition"),
NULL),
		str_list_create (_(part_type_msg),
				 _(mkpart_fs_type_msg),
				 _(start_end_msg),
				 "\n",
_("mkpart makes a partition without creating a new file system on the "
  "partition.  FS-TYPE may be specified to set an appropriate partition ID.\n"),
NULL)));

	command_register (commands, command_create (
		str_list_create_unique ("mkpartfs", _("mkpartfs"), NULL),
		do_mkpartfs,
		str_list_create (
_("mkpartfs PART-TYPE FS-TYPE START END     make a partition with a "
  "file system"),
NULL),
		str_list_create (_(part_type_msg), _(start_end_msg), NULL)));

	command_register (commands, command_create (
		str_list_create_unique ("move", _("move"), NULL),
		do_move,
		str_list_create (
_("move NUMBER START END         move partition NUMBER"),
NULL),
		str_list_create (_(number_msg), _(start_end_msg), NULL)));

	command_register (commands, command_create (
		str_list_create_unique ("name", _("name"), NULL),
		do_name,
		str_list_create (
_("name NUMBER NAME              name partition NUMBER as NAME"),
NULL),
		str_list_create (_(number_msg), _(name_msg), NULL)));

	command_register (commands, command_create (
		str_list_create_unique ("print", _("print"), NULL),
		do_print,
		str_list_create (
_("print [NUMBER]                display the partition table, or a partition"),
NULL),
		str_list_create (
_("Without arguments, print displays the entire partition table.  If a\n"
  "partition number is given, then more detailed information is displayed\n"
  "about that partition.\n"),
NULL)));
	
	command_register (commands, command_create (
		str_list_create_unique ("quit", _("quit"), NULL),
		do_quit,
		str_list_create (
_("quit                          exit program"),
NULL),
		NULL));

	command_register (commands, command_create (
		str_list_create_unique ("rescue", _("rescue"), NULL),
		do_rescue,
		str_list_create (
_("rescue START END              rescue a lost partition near START and END"),
NULL),
		str_list_create (_(start_end_msg), NULL)));

	command_register (commands, command_create (
		str_list_create_unique ("resize", _("resize"), NULL),
		do_resize,
		str_list_create (
_("resize NUMBER START END       resize partition NUMBER and its file system"),
NULL),
		str_list_create (_(number_msg),
				 _(start_end_msg),
				 _(resize_fs_type_msg), NULL)));

	command_register (commands, command_create (
		str_list_create_unique ("rm", _("rm"), NULL),
		do_rm,
		str_list_create (
_("rm NUMBER                     delete partition NUMBER"),
NULL),
		str_list_create (_(number_msg), NULL)));

	command_register (commands, command_create (
		str_list_create_unique ("select", _("select"), NULL),
		do_select,
		str_list_create (
_("select DEVICE                 choose the device to edit"),
NULL),
		str_list_create (_(device_msg), NULL)));

	command_register (commands, command_create (
		str_list_create_unique ("set", _("set"), NULL),
		do_set,
		str_list_create (
_("set NUMBER FLAG STATE         change a flag on partition NUMBER"),
NULL),
		str_list_create (_(number_msg), flag_msg, _(state_msg), NULL)));

	command_register (commands, command_create (
		str_list_create_unique ("unit", _("unit"), NULL),
		do_unit,
		str_list_create (
_("unit UNIT                     set the default unit to UNIT"),
NULL),
		str_list_create (unit_msg, NULL)));
}

static void
_done_commands ()
{
	Command**	walk;

	for (walk = commands; *walk; walk++) {
		command_destroy (*walk);
		*walk = NULL;
	}
}

static void
_init_i18n ()
{
/* intialize i18n */
#ifdef ENABLE_NLS
	setlocale(LC_ALL, "");
	bindtextdomain(PACKAGE, LOCALEDIR);
	textdomain(PACKAGE);
#endif /* ENABLE_NLS */
}

void
_version ()
{
	printf (prog_name);
	exit (0);
}

static int
_parse_options (int* argc_ptr, char*** argv_ptr)
{
	int	opt;

	while (1)
	{
#ifdef HAVE_GETOPT_H
		opt = getopt_long (*argc_ptr, *argv_ptr, "hisv",
				   options, NULL);
#else
		opt = getopt (*argc_ptr, *argv_ptr, "hisv");
#endif
		if (opt == -1)
			break;

		switch (opt) {
			case 'h': help_msg (); break;
			case 'i': opt_script_mode = 0; break;
			case 's': opt_script_mode = 1; break;
			case 'v': _version (); break;
		}
	}

	*argc_ptr -= optind;
	*argv_ptr += optind;
	return 1;

error:
	return 0;
}

static PedDevice*
_choose_device (int* argc_ptr, char*** argv_ptr)
{
	PedDevice*	dev;

	/* specified on comand line? */
	if (*argc_ptr) {
		dev = ped_device_get ((*argv_ptr) [0]);
		if (!dev)
			return NULL;
		(*argc_ptr)--;
		(*argv_ptr)++;
	} else {
	retry:
		ped_device_probe_all ();
		dev = ped_device_get_next (NULL);
		if (!dev) {
			if (ped_exception_throw (PED_EXCEPTION_ERROR,
				PED_EXCEPTION_RETRY_CANCEL,
				_("No device found"))
					== PED_EXCEPTION_RETRY)
				goto retry;
			else
				return NULL;
		}
	}

	if (!ped_device_open (dev))
		return NULL;
	return dev;	
}

static PedDevice*
_init (int* argc_ptr, char*** argv_ptr)
{
	PedDevice*	dev;

#ifdef ENABLE_MTRACE
	mtrace();
#endif

	_init_i18n ();
	if (!init_ui ())
		goto error;
	_init_messages ();
	_init_commands ();

	if (!_parse_options (argc_ptr, argv_ptr))
		goto error_done_commands;
	dev = _choose_device (argc_ptr, argv_ptr);
	if (!dev)
		goto error_done_commands;

	timer = ped_timer_new (_timer_handler, &timer_context);
	if (!timer)
		goto error_done_commands;
	timer_context.last_update = 0;

	return dev;

error_done_commands:
	_done_commands ();
	_done_messages ();
error_done_ui:
	done_ui ();
error:
	return NULL;
}

static void
_done (PedDevice* dev)
{
	if (dev->boot_dirty && dev->type != PED_DEVICE_FILE) {
		ped_exception_throw (
			PED_EXCEPTION_WARNING,
			PED_EXCEPTION_OK,
		_("You should reinstall your boot loader before "
		  "rebooting.  Read section 4 of the Parted User "
		  "documentation for more information."));
	}
	if (dev->type != PED_DEVICE_FILE && !opt_script_mode) {
		ped_exception_throw (
			PED_EXCEPTION_INFORMATION, PED_EXCEPTION_OK,
			_("Don't forget to update /etc/fstab, if "
			  "necessary.\n"));
	}

	ped_device_close (dev);

	ped_timer_destroy (timer);
	_done_commands ();
	_done_messages ();
	done_ui();
}

int
main (int argc, char** argv)
{
	PedDevice*	dev;
	int		status;

	dev = _init (&argc, &argv);
	if (!dev)
		return 1;

	if (argc || opt_script_mode)
		status = non_interactive_mode (&dev, commands, argc, argv);
	else
		status = interactive_mode (&dev, commands);

	_done (dev);

	return !status;
}

