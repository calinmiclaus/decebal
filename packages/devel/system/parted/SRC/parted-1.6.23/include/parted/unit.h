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

#ifndef PED_UNIT_H_INCLUDED
#define PED_UNIT_H_INCLUDED

#include <parted/device.h>

#include <stdarg.h>
#include <stdio.h>

#define PED_SECTOR_SIZE   512LL
#define PED_KILOBYTE_SIZE 1000LL
#define PED_MEGABYTE_SIZE 1000000LL
#define PED_GIGABYTE_SIZE 1000000000LL
#define PED_TERABYTE_SIZE 1000000000000LL


typedef enum {
	PED_UNIT_SECTOR,
	PED_UNIT_BYTE,
	PED_UNIT_KILOBYTE,
	PED_UNIT_MEGABYTE,
	PED_UNIT_GIGABYTE,
	PED_UNIT_TERABYTE,
	PED_UNIT_COMPACT,
	PED_UNIT_CYLINDER,
	PED_UNIT_CHS,
	PED_UNIT_PERCENT
} PedUnit;

#define PED_UNIT_FIRST PED_UNIT_SECTOR
#define PED_UNIT_LAST PED_UNIT_PERCENT

extern long long ped_unit_get_size (PedDevice* dev, PedUnit unit);
extern const char* ped_unit_get_name (PedUnit unit);
extern PedUnit ped_unit_get_by_name (const char* unit_name);

extern void ped_unit_set_default (PedUnit unit);
extern PedUnit ped_unit_get_default ();

extern char* ped_unit_format (PedDevice* dev, PedSector sector);
extern char* ped_unit_format_custom (PedDevice* dev, PedSector sector,
				     PedUnit unit);

extern int ped_unit_parse (const char* str, PedDevice* dev, PedSector* sector,
			   PedGeometry** range);
extern int ped_unit_parse_custom (const char* str, PedDevice* dev,
				  PedUnit unit, PedSector* sector,
				  PedGeometry** range);

#endif /* PED_UNIT_H_INCLUDED */
