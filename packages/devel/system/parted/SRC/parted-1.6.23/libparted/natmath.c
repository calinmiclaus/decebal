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

#include <stdlib.h>
#include <parted/parted.h>
#include <parted/debug.h>
#include <parted/natmath.h>

/* Arrrghhh!  Why doesn't C have tuples? */
typedef struct {
	PedSector	gcd;		/* "converges" to the gcd */
	PedSector	x;
	PedSector	y;
} EuclidTriple;

static const PedAlignment _any = {
	offset:		0,
	grain_size:	1
};

const PedAlignment* ped_alignment_any = &_any;
const PedAlignment* ped_alignment_none = NULL;

/* This function returns "a mod b", the way C should have done it!
 * Mathematicians prefer -3 mod 4 to be 1.  Reason: division by N
 * is all about adding or subtracting N, and we like our remainders
 * to be between 0 and N - 1.
 */
PedSector
abs_mod (PedSector a, PedSector b)
{
	if (a < 0)
		return a % b + b;
	else
		return a % b;
}

/* Rounds a number down to the closest number that is a multiple of
 * grain_size.
 */
PedSector
ped_round_down_to (PedSector sector, PedSector grain_size)
{
	return sector - abs_mod (sector, grain_size);
}

/* Rounds a number up to the closest number that is a multiple of
 * grain_size.
 */
PedSector
ped_round_up_to (PedSector sector, PedSector grain_size)
{
	if (sector % grain_size)
		return ped_round_down_to (sector, grain_size) + grain_size;
	else
		return sector;
}

/* Rounds a number to the closest number that is a multiple of grain_size. */
PedSector
ped_round_to_nearest (PedSector sector, PedSector grain_size)
{
	if (sector % grain_size > grain_size/2)
		return ped_round_up_to (sector, grain_size);
	else
		return ped_round_down_to (sector, grain_size);
}

/* This function returns the largest number that divides both a and b.
 * It uses the ancient Euclidean algorithm.
 */
PedSector
ped_greatest_common_divisor (PedSector a, PedSector b)
{
	PED_ASSERT (a >= 0, return 0);
	PED_ASSERT (b >= 0, return 0);

	/* Put the arguments in the "right" format.  (Recursive calls made by
	 * this function are always in the right format.)
	 */
	if (b > a)
		return ped_greatest_common_divisor (b, a);

	if (b)
		return ped_greatest_common_divisor (b, a % b);
	else
		return a;
}

int
ped_alignment_init (PedAlignment* align, PedSector offset, PedSector grain_size)
{
	PED_ASSERT (align != NULL, return 0);

	if (grain_size < 0)
		return 0;

	if (grain_size)
		align->offset = abs_mod (offset, grain_size);
	else
		align->offset = offset;
	align->grain_size = grain_size;

	return 1;
}

PedAlignment*
ped_alignment_new (PedSector offset, PedSector grain_size)
{
	PedAlignment*	align;

	align = (PedAlignment*) ped_malloc (sizeof (PedAlignment));
	if (!align)
		goto error;

	if (!ped_alignment_init (align, offset, grain_size))
		goto error_free_align;

	return align;

error_free_align:
	ped_free (align);
error:
	return NULL;
}

void
ped_alignment_destroy (PedAlignment* align)
{
	if (align)
		ped_free (align);
}

PedAlignment*
ped_alignment_duplicate (const PedAlignment* align)
{
	if (!align)
		return NULL;
	return ped_alignment_new (align->offset, align->grain_size);
}

/* the extended Euclid algorithm.
 *
 * input:
 * 	a and b, a > b
 *
 * output:
 * 	gcd, x and y, such that:
 *
 * 	gcd = greatest common divisor of a and b
 * 	gcd = x*a + y*b
 */
EuclidTriple
extended_euclid (int a, int b)
{
	EuclidTriple	result;
	EuclidTriple	tmp;

	if (b == 0) {
		result.gcd = a;
		result.x = 1;
		result.y = 0;
		return result;
	}

	tmp = extended_euclid (b, a % b);
	result.gcd = tmp.gcd;
	result.x = tmp.y;
	result.y = tmp.x - (a/b) * tmp.y;
	return result;
}

/* This function computes a PedAlignment object that describes the
 * intersection of two alignments.  That is, a sector satisfies the
 * new alignment object if and only if it satisfies both of the original
 * ones.  (See ped_alignment_is_aligned() for the meaning of "satisfies")
 * 
 * Apart from the trivial cases (where one or both of the alignment objects
 * constraints have no sectors that satisfy them), this is what we're trying to
 * do:
 *  - two input constraints: a and b.
 *  - the new grain_size is going to be the lowest common multiple of
 *  a->grain_size and b->grain_size
 *  - hard part - solve the simulataneous equations, for offset, where offset,
 *  X and Y are variables.  (Note: offset can be obtained from either X or Y,
 *  by substituing into either equation)
 *
 *  	offset = a->offset + X * a->grain_size		(1)
 *  	offset = b->offset + Y * b->grain_size		(2)
 * 
 * or, abbreviated:
 *
 *  	o = Ao + X*Ag		(1)
 *  	o = Bo + Y*Bg		(2)
 * 
 *  =>	Ao + X*Ag    = Bo + Y*Bg     (1) = (2)
 *  	X*Ag - Y*Bg  = Bo - Ao  (3)
 *
 * As it turns out, there only exists a solution if (Bo - Ao) is a multiple
 * of the GCD of Ag and Bg.  Reason: all linear combinations of Ag and Bg are
 * multiples of the GCD.
 *
 * Proof:
 *
 *	A * Ag + B * Bg
 *	= A * (a * gcd) + B * (b * gcd)
 *	= gcd * (A * a + B * b)
 *
 * gcd is a factor of the linear combination.  QED
 * 
 * Anyway, a*Ag + b*Bg = gcd can be solved (for a, b and gcd) with Euclid's
 * extended algorithm.  Then, we just multiply through by (Bo - Ao) / gcd to
 * get (3).
 *
 * i.e.
 *
 * 	A * Ag + B * Bg				= gcd
 * 	A*(Bo-Ao)/gcd * Ag + B(Bo-Ao)/gcd * Bg	= gcd * (Bo-Ao)/gcd
 * 	X*Ag - Y*Bg				= Bo - Ao		(3)
 *
 * 	X = A*(Bo-Ao)/gcd
 * 	Y = - B*(Bo-Ao)/gcd
 *
 * then:
 *  	o = Ao + X*Ag			(1)
 *	  = Ao + A*(Bo-Ao)/gcd*Ag
 *  	o = Bo + Y*Bg			(2)
 *	  = Bo - B*(Bo-Ao)/gcd*Ag
 *
 * Thanks go to Nathan Hurst (njh@hawthorn.csse.monash.edu.au) for figuring
 * this algorithm out :-)
 */
PedAlignment*
ped_alignment_intersect (const PedAlignment* a, const PedAlignment* b)
{
	PedSector	new_grain_size;
	PedSector	new_offset;
	PedSector	delta_on_gcd;
	EuclidTriple	gcd_factors;

	if (!a || !b)
		return NULL;

	if (a->grain_size < b->grain_size) {
		const PedAlignment*	tmp;
	        tmp = a; a = b; b = tmp;
	}

	/* weird/trivial case: where the solution space for "a" or "b" is
	 * either empty or contains has exactly one solution
	 */
	if (a->grain_size == 0 && b->grain_size == 0) {
		if (a->offset == b->offset)
			return ped_alignment_duplicate (a);
		else
			return NULL;
	}

	/* general case */
	gcd_factors = extended_euclid (a->grain_size, b->grain_size);

	delta_on_gcd = (b->offset - a->offset) / gcd_factors.gcd;
	new_offset = a->offset + gcd_factors.x * delta_on_gcd * a->grain_size;
	new_grain_size = a->grain_size * b->grain_size / gcd_factors.gcd;

	/* inconsistency => no solution */
	if (new_offset
	    != b->offset - gcd_factors.y * delta_on_gcd * b->grain_size)
		return NULL;

	return ped_alignment_new (new_offset, new_grain_size);
}

/* This function returns the sector closest to "sector" that lies inside
 * geom and satisfies the alignment constraint.
 */
static PedSector
_closest_inside_geometry (const PedAlignment* align, const PedGeometry* geom,
			  PedSector sector)
{
	PED_ASSERT (align != NULL, return -1);

	if (!align->grain_size) {
		if (ped_alignment_is_aligned (align, geom, sector)
		    && (!geom || ped_geometry_test_sector_inside (geom,
				    				  sector)))
			return sector;
		else
			return -1;
	}

	if (sector < geom->start)
		sector += ped_round_up_to (geom->start - sector,
					   align->grain_size);
	if (sector > geom->end)
		sector -= ped_round_up_to (sector - geom->end,
					   align->grain_size);

	if (!ped_geometry_test_sector_inside (geom, sector))
		return -1;
	return sector;
}

/* This function returns the closest sector to "sector" that lies inside
 * geom that satisfies the given alignment constraint.  It prefers
 * sectors that are beyond "sector", but does not guarantee that this.
 */
PedSector
ped_alignment_align_up (const PedAlignment* align, const PedGeometry* geom,
			PedSector sector)
{
	PedSector	result;

	PED_ASSERT (align != NULL, return -1);

	if (!align->grain_size)
		result = align->offset;
	else
		result = ped_round_up_to (sector - align->offset,
			       		  align->grain_size)
			 + align->offset;

	if (geom)
		result = _closest_inside_geometry (align, geom, result);
	return result;
}

/* This function returns the closest sector to "sector" that lies inside
 * geom that satisfies the given alignment constraint.  It prefers
 * sectors that are before "sector", but does not guarantee that this.
 */
PedSector
ped_alignment_align_down (const PedAlignment* align, const PedGeometry* geom,
			  PedSector sector)
{
	PedSector	result;

	PED_ASSERT (align != NULL, return -1);

	if (!align->grain_size)
		result = align->offset;
	else
		result = ped_round_down_to (sector - align->offset,
			      		    align->grain_size)
			 + align->offset;

	if (geom)
		result = _closest_inside_geometry (align, geom, result);
	return result;
}

/* Returns either a or b, depending on which is closest to "sector". */
static PedSector
closest (PedSector sector, PedSector a, PedSector b)
{
	if (a == -1)
		return b;
	if (b == -1)
		return a;

	if (abs (sector - a) < abs (sector - b))
		return a;
	else
		return b;
}

/* Returns the sector that is closest to "sector", satisfies the alignment
 * constraint and lies inside geom.
 */
PedSector
ped_alignment_align_nearest (const PedAlignment* align, const PedGeometry* geom,
			     PedSector sector)
{
	PED_ASSERT (align != NULL, return -1);

	return closest (sector, ped_alignment_align_up (align, geom, sector),
			ped_alignment_align_down (align, geom, sector));
}

/* Returns 1 if sector satisfies the alignment constraint and lies inside
 * geom.
 */
int
ped_alignment_is_aligned (const PedAlignment* align, const PedGeometry* geom,
			  PedSector sector)
{
	if (!align)
		return 0;

	if (geom && !ped_geometry_test_sector_inside (geom, sector))
		return 0;

	if (align->grain_size)
		return (sector - align->offset) % align->grain_size == 0;
	else
		return sector == align->offset;
}

