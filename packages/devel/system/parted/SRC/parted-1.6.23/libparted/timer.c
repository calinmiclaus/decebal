/*
    libparted - a library for manipulating disk partitions
    Copyright (C) 2001 Free Software Foundation, Inc.

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

#define PED_TIMER_START_DELAY	2

typedef struct {
	PedTimer*	parent;
	float		nest_frac;
	float		start_frac;
} NestedContext;

PedTimer*
ped_timer_new (PedTimerHandler* handler, void* context)
{
	PedTimer*	timer;

	PED_ASSERT (handler != NULL, return NULL);

	timer = (PedTimer*) ped_malloc (sizeof (PedTimer));
	if (!timer)
		return NULL;

	timer->handler = handler;
	timer->context = context;
	ped_timer_reset (timer);
	return timer;
}

void
ped_timer_destroy (PedTimer* timer)
{
	if (!timer)
		return;

	ped_free (timer);
}

/* This function is used by ped_timer_new_nested() as the timer->handler
 * function.
 */
static void
_nest_handler (PedTimer* timer, void* context)
{
	NestedContext*	ncontext = (NestedContext*) context;

	ped_timer_update (
		ncontext->parent,
		ncontext->start_frac + ncontext->nest_frac * timer->frac);
}

/* This function creates a "nested" timer that describes the progress
 * of a subtask.  Updates to the progress of the subtask are propagated
 * back through to the parent task's timer.
 */
PedTimer*
ped_timer_new_nested (PedTimer* parent, float nest_frac)
{
	NestedContext*	context;

	if (!parent)
		return NULL;

	PED_ASSERT (nest_frac >= 0.0, return NULL);
	PED_ASSERT (nest_frac <= 1.0, return NULL);

	context = (NestedContext*) ped_malloc (sizeof (NestedContext));
	if (!context)
		return NULL;
	context->parent = parent;
	context->nest_frac = nest_frac;
	context->start_frac = parent->frac;

	return ped_timer_new (_nest_handler, context);
}

void
ped_timer_destroy_nested (PedTimer* timer)
{
	if (!timer)
		return;

	ped_free (timer->context);
	ped_timer_destroy (timer);
}

/* This function calls the update handler, making sure that it has
 * the latest time.
 */
void
ped_timer_touch (PedTimer* timer)
{
	if (!timer)
	       return;

	timer->now = time (NULL);
	if (timer->now > timer->predicted_end)
		timer->predicted_end = timer->now;

	timer->handler (timer, timer->context);
}

/* This function sets the timer into a "start of task" position. */
void
ped_timer_reset (PedTimer* timer)
{
	if (!timer)
	       return;

	timer->start = timer->now = timer->predicted_end = time (NULL);
	timer->state_name = NULL;
	timer->frac = 0;

	ped_timer_touch (timer);
}

/* This function tells a timer what fraction of the task has been
 * completed.
 */
void
ped_timer_update (PedTimer* timer, float frac)
{
	if (!timer)
	       return;

	timer->now = time (NULL);
	timer->frac = frac;

	if (frac)
		timer->predicted_end
			= timer->start
			  + (long) ((timer->now - timer->start) / frac);

	ped_timer_touch (timer);
}

/* This function changes the description of the current task that the
 * timer describes.
 */
void
ped_timer_set_state_name (PedTimer* timer, const char* state_name)
{
	if (!timer)
	       return;

	timer->state_name = state_name;
	ped_timer_touch (timer);
}

