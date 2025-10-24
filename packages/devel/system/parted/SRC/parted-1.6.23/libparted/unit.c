/*
    libparted - a library for manipulating disk partitions
    Copyright (C) 2005 Free Software Foundation, Inc.

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

#include <parted/parted.h>
#include <parted/debug.h>

#include <ctype.h>
#include <stdio.h>
#include <string.h>

#define N_(String) String
#if ENABLE_NLS
#  include <libintl.h>
#  define _(String) dgettext (PACKAGE, String)
#else
#  define _(String) (String)
#endif /* ENABLE_NLS */


static PedUnit default_unit = PED_UNIT_COMPACT;
static const char* unit_names[] = {
	"s",
	"B",
	"kB",
	"MB",
	"GB",
	"TB",
	"compact",
	"cyl",
	"chs",
	"%"
};


/**
 * Set the default unit used by subsequent calls to the PedUnit API.
 * In particular, this affects how locations inside error messages
 * (exceptions) are displayed.
 */
void
ped_unit_set_default (PedUnit unit)
{
	default_unit = unit;
}


/**
 * Get the current default unit.
 */
PedUnit
ped_unit_get_default ()
{
	return default_unit;
}

/**
 * Returns the byte size of a given unit.
 */
long long
ped_unit_get_size (PedDevice* dev, PedUnit unit)
{
	PedSector cyl_size = dev->bios_geom.heads * dev->bios_geom.sectors;

	switch (unit) {
		case PED_UNIT_SECTOR:	return PED_SECTOR_SIZE;
		case PED_UNIT_BYTE:	return 1;
		case PED_UNIT_KILOBYTE:	return PED_KILOBYTE_SIZE;
		case PED_UNIT_MEGABYTE:	return PED_MEGABYTE_SIZE;
		case PED_UNIT_GIGABYTE:	return PED_GIGABYTE_SIZE;
		case PED_UNIT_TERABYTE:	return PED_TERABYTE_SIZE;
		case PED_UNIT_CYLINDER:	return cyl_size * PED_SECTOR_SIZE;
		case PED_UNIT_CHS:	return PED_SECTOR_SIZE;

		case PED_UNIT_PERCENT:
			return dev->length * PED_SECTOR_SIZE / 100;

		case PED_UNIT_COMPACT:
			ped_exception_throw (
				PED_EXCEPTION_ERROR, PED_EXCEPTION_CANCEL,
				_("Cannot get unit size for special unit "
				  "'COMPACT'."));
			return 0;
	}

	/* never reached */
	PED_ASSERT(0, return 0);
	return 0;
}

/**
 * Returns a textual (non-internationalized) representation of a unit.
 */
const char*
ped_unit_get_name (PedUnit unit)
{
	return unit_names[unit];
}

/*
 * Returns a unit based on its textual representation.
 * For example, ped_unit_get_by_name("Mb") returns PED_UNIT_MEGABYTE.
 */
PedUnit
ped_unit_get_by_name (const char* unit_name)
{
	PedUnit unit;
	for (unit = PED_UNIT_FIRST; unit <= PED_UNIT_LAST; unit++) {
		if (!strcasecmp (unit_names[unit], unit_name))
			return unit;
	}
	return -1;
}

/**
 * Returns a string that describes the location "sector" on device "dev".
 * The string is described with the default unit, which is set
 * by ped_unit_set_default().
 * The returned string must be freed with ped_free().
 */
char*
ped_unit_format (PedDevice* dev, PedSector sector)
{
	PED_ASSERT (dev != NULL, return NULL);
	return ped_unit_format_custom (dev, sector, default_unit);
}

static char*
ped_strdup (const char *str)
{
	char *result;
	result = ped_malloc (strlen (str) + 1);
	if (!result)
		return NULL;
	strcpy (result, str);
	return result;
}

/**
 * Returns a string that describes the location "sector" on device "dev".
 * The string is described with the desired unit.
 * The returned string must be freed with ped_free().
 */
char*
ped_unit_format_custom (PedDevice* dev, PedSector sector, PedUnit unit)
{
	char buf[100];
	char* result;

	/* CHS has a special comma-separated format. */
	if (unit == PED_UNIT_CHS) {
		PedCHSGeometry *chs = &dev->bios_geom;
		snprintf (buf, 100, "%lld,%lld,%lld",
			  sector / chs->sectors / chs->heads,
			  (sector / chs->sectors) % chs->heads,
			  sector % chs->sectors);
		return ped_strdup (buf);
	}

	/* Cylinders should be rounded down. */
	if (unit == PED_UNIT_CYLINDER) {
		snprintf (buf, 100, "%lldcyl",
			  sector * PED_SECTOR_SIZE
				  / ped_unit_get_size (dev, unit));
		return ped_strdup (buf);
	}

	if (unit == PED_UNIT_COMPACT) {
		if (sector >= 10LL * PED_TERABYTE_SIZE / PED_SECTOR_SIZE)
			unit = PED_UNIT_TERABYTE;
		else if (sector >= 10LL * PED_GIGABYTE_SIZE / PED_SECTOR_SIZE)
			unit = PED_UNIT_GIGABYTE;
		else if (sector >= 10LL * PED_MEGABYTE_SIZE / PED_SECTOR_SIZE)
			unit = PED_UNIT_MEGABYTE;
		else
			unit = PED_UNIT_KILOBYTE;
	}

	snprintf (buf, 100, "%lld%s",
		  ped_div_round_to_nearest (
			  sector * PED_SECTOR_SIZE,
			  ped_unit_get_size (dev, unit)),
		  ped_unit_get_name (unit));
	return ped_strdup (buf);
}

/**
 * If str contains a valid description of a location on dev, then *sector
 * is modified to describe the location.  If no units are specified, then
 * the default unit is assumed.  This function returns 1 if str is
 * a valid location description, 0 otherwise.
 */
int
ped_unit_parse (const char* str, PedDevice* dev, PedSector *sector,
		PedGeometry** range)
{
	return ped_unit_parse_custom (str, dev, default_unit, sector, range);
}

/* Inefficiently removes all spaces from a string, in-place. */
static void
strip_string (char* str)
{
	int i;

	for (i = 0; str[i] != 0; i++) {
		if (isspace (str[i])) {
			int j;
			for (j = i + 1; str[j] != 0; j++)
				str[j - 1] = str[j];
		}
	}
}


/* Find non-number suffix.  Eg: find_suffix("32Mb") returns a pointer to
 * "Mb". */
static char*
find_suffix (char* str)
{
	while (str[0] != 0 && (isdigit (str[0]) || strchr(",.-", str[0])))
		str++;
	return str;
}

static void
remove_punct (char* str)
{
	int i = 0;

	for (i = 0; str[i]; i++) {
		if (ispunct (str[i]))
			str[i] = ' ';
	}
}

static int
is_chs (const char* str)
{
	int punct_count = 0;
	int i = 0;

	for (i = 0; str[i]; i++)
		punct_count += ispunct (str[i]) != 0;
	return punct_count == 2;
}

static int
parse_chs (const char* str, PedDevice* dev, PedSector* sector,
		PedGeometry** range)
{
	PedSector cyl_size = dev->bios_geom.heads * dev->bios_geom.sectors;
	char* copy = ped_strdup (str);
	PedCHSGeometry chs;

	copy = ped_strdup (str);
	if (!copy)
		return 0;
	strip_string (copy);
	remove_punct (copy);

	if (sscanf (copy, "%d %d %d",
		    &chs.cylinders, &chs.heads, &chs.sectors) != 3) {
		ped_exception_throw (
				PED_EXCEPTION_ERROR, PED_EXCEPTION_CANCEL,
				_("\"%s\" has invalid syntax for locations."),
				copy);
		goto error_free_copy;
	}

	if (chs.heads >= dev->bios_geom.heads) {
		ped_exception_throw (
				PED_EXCEPTION_ERROR, PED_EXCEPTION_CANCEL,
				_("The maximum head value is %d."),
				dev->bios_geom.heads - 1);
		goto error_free_copy;
	}
	if (chs.sectors >= dev->bios_geom.sectors) {
		ped_exception_throw (
				PED_EXCEPTION_ERROR, PED_EXCEPTION_CANCEL,
				_("The maximum sector value is %d."),
				dev->bios_geom.sectors - 1);
		goto error_free_copy;
	}

	*sector = 1LL * chs.cylinders * cyl_size
		+ chs.heads * dev->bios_geom.sectors
		+ chs.sectors;

	if (*sector >= dev->length) {
		ped_exception_throw (
				PED_EXCEPTION_ERROR, PED_EXCEPTION_CANCEL,
				_("The location %s is outside of the "
				  "device %s."),
				str, dev->path);
		goto error_free_copy;
	}
	*range = ped_geometry_new (dev, *sector, 1);
	ped_free (copy);
	return 1;

error_free_copy:
	ped_free (copy);
error:
	return 0;
}

static PedSector
clip (PedDevice* dev, PedSector sector)
{
	if (sector < 0)
		return 0;
	if (sector > dev->length - 1)
		return dev->length - 1;
	return sector;
}

static PedGeometry*
geometry_from_centre_radius (PedDevice* dev, PedSector sector, PedSector radius)
{
	PedSector start = clip (dev, sector - radius);
	PedSector end = clip (dev, sector + radius);
	if (sector - end > radius || start - sector > radius)
		return NULL;
	return ped_geometry_new (dev, start, end - start + 1);
}

static PedUnit
parse_unit_suffix (const char* suffix, PedUnit suggested_unit)
{
	if (strlen (suffix) != 0) {
		switch (tolower (suffix[0])) {
			case 's': return PED_UNIT_SECTOR;
			case 'b': return PED_UNIT_BYTE;
			case 'k': return PED_UNIT_KILOBYTE;
			case 'm': return PED_UNIT_MEGABYTE;
			case 'g': return PED_UNIT_GIGABYTE; 
			case 't': return PED_UNIT_TERABYTE;
			case 'c': return PED_UNIT_CYLINDER; 
			case '%': return PED_UNIT_PERCENT;
		}
	}

	if (suggested_unit == PED_UNIT_COMPACT) {
		if (default_unit == PED_UNIT_COMPACT)
			return PED_UNIT_MEGABYTE;
		else
			return default_unit;
	}

	return suggested_unit;
}

/**
 * If str contains a valid description of a location on dev, then *sector
 * is modified to describe the location.  If no units are specified, then
 * the desired unit is assumed.  This function returns 1 if str is
 * a valid location description, 0 otherwise.
 */
int
ped_unit_parse_custom (const char* str, PedDevice* dev, PedUnit unit,
		       PedSector* sector, PedGeometry** range)
{
	char*     copy;
	char*     suffix;
	double    num;
	long long unit_size;
	PedSector radius;

	if (is_chs (str))
		return parse_chs (str, dev, sector, range);

	copy = ped_strdup (str);
	if (!copy)
		return 0;
	strip_string (copy);

	suffix = find_suffix (copy);
	unit = parse_unit_suffix (suffix, unit);
	suffix[0] = 0;

	if (sscanf (copy, "%lf", &num) != 1) {
		ped_exception_throw (
				PED_EXCEPTION_ERROR,
				PED_EXCEPTION_CANCEL,
				_("Invalid number."));
		goto error_free_copy;
	}

	unit_size = ped_unit_get_size (dev, unit);
	radius = ped_div_round_up (unit_size, PED_SECTOR_SIZE) - 1;
	if (radius < 0)
		radius = 0;

	*sector = num * unit_size / PED_SECTOR_SIZE;
	/* negative numbers count from the end */
	if (copy[0] == '-')
		*sector += dev->length;
	*range = geometry_from_centre_radius (dev, *sector, radius);
	if (!*range) {
		ped_exception_throw (
			PED_EXCEPTION_ERROR, PED_EXCEPTION_CANCEL,
			_("The location %s is outside of the device %s."),
			str, dev->path);
		goto error_free_copy;
	}
	*sector = clip (dev, *sector);

	ped_free (copy);
	return 1;

error_destroy_range:
	ped_geometry_destroy (*range);
error_free_copy:
	ped_free (copy);
error:
	*sector = 0;
	*range = NULL;
	return 0;
}

