/*
    libparted - a library for manipulating disk partitions
    Copyright (C) 2004 Free Software Foundation, Inc.

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

#include <stdlib.h>
#include <string.h>
#include <parted/parted.h>
#include <parted/endian.h>
#include <parted/debug.h>
#include <stdint.h>

#if ENABLE_NLS
#  include <libintl.h>
#  define _(String) dgettext (PACKAGE, String)
#else
#  define _(String) (String)
#endif /* ENABLE_NLS */

#include "hfs.h"

#include "probe.h"

/* Probe an HFS volume, detecting it even if
it is in fact a wrapper to an HFS+ volume */
/* Used by hfsplus_probe and hfs_probe */
PedGeometry*
hfs_and_wrapper_probe (PedGeometry* geom)
{
	uint8_t		buf[PED_SECTOR_SIZE];
	HfsMasterDirectoryBlock	*mdb;
	PedGeometry*	geom_ret;
	PedSector	search, max;

	PED_ASSERT(geom != NULL, return NULL);

	mdb = (HfsMasterDirectoryBlock *) buf;

	/* is 5 an intelligent value ? */
	if ((geom->length < 5)
	    || (!ped_geometry_read (geom, buf, 2, 1))
	    || (PED_BE16_TO_CPU (mdb->signature) != HFS_SIGNATURE) )
		return NULL;

	search = ((PedSector) PED_BE16_TO_CPU (mdb->start_block)
		  + ((PedSector) PED_BE16_TO_CPU (mdb->total_blocks)
		     * (PED_BE32_TO_CPU (mdb->block_size) / PED_SECTOR_SIZE )));
	max = search + (PED_BE32_TO_CPU (mdb->block_size) / PED_SECTOR_SIZE);
	if (!(geom_ret = ped_geometry_new (geom->dev, geom->start, search + 2)))
		return NULL;

	for (; search < max; search++) {
		if (!ped_geometry_set (geom_ret, geom_ret->start, search + 2)
		    || !ped_geometry_read (geom_ret, buf, search, 1))
			break;
		if (PED_BE16_TO_CPU (mdb->signature) == HFS_SIGNATURE)
			return geom_ret;
	}

	ped_geometry_destroy (geom_ret);
	return NULL;
}

PedGeometry*
hfsplus_probe (PedGeometry* geom)
{
	PedGeometry*	geom_ret;
	uint8_t		buf[PED_SECTOR_SIZE];

	PED_ASSERT(geom != NULL, return NULL);

	if ((geom_ret = hfs_and_wrapper_probe(geom))) {
		/* HFS+ is embedded in an HFS volume ? */
		HfsMasterDirectoryBlock	*mdb;
		mdb = (HfsMasterDirectoryBlock *) buf;

		if (!ped_geometry_read (geom, buf, 2, 1)
		    || (PED_BE16_TO_CPU (mdb->old_new.embedded.signature)
			!= HFSP_SIGNATURE)) {
			ped_geometry_destroy (geom_ret);
			return NULL;
		} else
			return geom_ret;
	} else {
		/* This is a standalone HFS+ volume ? */
		PedSector	search, max;
		HfsPVolumeHeader *vh;
		vh = (HfsPVolumeHeader *) buf;

		if ((geom->length < 5)
		    || !ped_geometry_read (geom, buf, 2, 1)
		    || (PED_BE16_TO_CPU (vh->signature) != HFSP_SIGNATURE))
			return NULL;

		/* Correct range is indeed [ blocks*sz-2;(blocs+1)*sz-2 ( */
		/* But previous versions of my implementation used to */
		/* assume range is [(blocks-1)*sz-1;(blocks*sz) ( */
		/* (blocks-1)*sz-1 has to be scanned last, because */
		/* it can belong to a regular file */
		max = ((PedSector) PED_BE32_TO_CPU (vh->total_blocks) + 1)
		      * ( PED_BE32_TO_CPU (vh->block_size) / PED_SECTOR_SIZE )
		      - 2;
		search = max - 2 * ( PED_BE32_TO_CPU (vh->block_size)
				     / PED_SECTOR_SIZE ) + 2;
		if (!(geom_ret = ped_geometry_new (geom->dev, geom->start,
						   search + 2)))
			return NULL;

		for (; search < max; search++) {
			if (!ped_geometry_set (geom_ret, geom_ret->start,
					       search + 2)
			    || !ped_geometry_read (geom_ret, buf, search, 1))
				break;
			if (PED_BE16_TO_CPU (vh->signature) == HFSP_SIGNATURE)
				return geom_ret;
		}
		search = ((PedSector) PED_BE32_TO_CPU (vh->total_blocks) - 1)
		      * ( PED_BE32_TO_CPU (vh->block_size) / PED_SECTOR_SIZE )
		      - 1;
		if (!ped_geometry_set (geom_ret, geom_ret->start,
					       search + 2)
		    || !ped_geometry_read (geom_ret, buf, search, 1)
		    || PED_BE16_TO_CPU (vh->signature) != HFSP_SIGNATURE) {
		    	ped_geometry_destroy (geom_ret);
			return NULL;
		} else
			return geom_ret;
	}
}

PedGeometry*
hfs_probe (PedGeometry* geom)
{
	PedGeometry*	geom_base;
	PedGeometry*	geom_plus = NULL;
	
	PED_ASSERT(geom != NULL, return NULL);
	
	if ((geom_base = hfs_and_wrapper_probe(geom))
	    && (!(geom_plus = hfsplus_probe(geom_base))))
		return geom_base;
	else {
		if (geom_base) ped_geometry_destroy (geom_base);
		if (geom_plus) ped_geometry_destroy (geom_plus);
		return NULL;
	}
}
