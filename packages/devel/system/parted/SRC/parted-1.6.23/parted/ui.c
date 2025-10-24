/*
    parted - a frontend to libparted
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

#include <parted/parted.h>
#include <parted/debug.h>

#include <ctype.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <setjmp.h>

#include "../config.h"
#include "command.h"
#include "strlist.h"
#include "ui.h"

#define N_(String) String
#if ENABLE_NLS
#  include <libintl.h>
#  include <locale.h>
#  define _(String) dgettext (PACKAGE, String)
#else
#  define _(String) (String)
#endif /* ENABLE_NLS */

#ifdef HAVE_LIBREADLINE

#ifdef HAVE_TERMCAP_H
#include <termcap.h>
#else
extern int tgetnum (char* key);
#endif

#include <readline/readline.h>
#include <readline/history.h>

#ifndef HAVE_RL_COMPLETION_MATCHES
#define rl_completion_matches completion_matches
#endif

#ifndef rl_compentry_func_t
#define rl_compentry_func_t void
#endif

#endif /* HAVE_LIBREADLINE */

char* prog_name = "GNU Parted " VERSION "\n";

static char* banner_msg = N_(
"Copyright (C) 1998 - 2005 Free Software Foundation, Inc.\n"
"This program is free software, covered by the GNU General Public License.\n"
"\n"
"This program is distributed in the hope that it will be useful, "
"but WITHOUT ANY WARRANTY; without even the implied warranty of "
"MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the "
"GNU General Public License for more details.\n\n");

static char* usage_msg = N_(
"Usage: parted [OPTION]... [DEVICE [COMMAND [PARAMETERS]...]...]\n"
"Apply COMMANDs with PARAMETERS to DEVICE.  If no COMMAND(s) are given, runs in\n"
"interactive mode.\n");

static char* bug_msg = N_(
"You found a bug in GNU Parted.  Please email a bug report to bug-parted@gnu.org "
"containing the version (%s), and the following message:\n");

#define MAX_WORDS	1024

static StrList*		command_line;
static Command**	commands;
static StrList*		ex_opt_str [64];
static StrList*		on_list;
static StrList*		off_list;
static StrList*		on_off_list;
static StrList*		fs_type_list;
static StrList*		disk_type_list;

static struct {
	const StrList*	possibilities;
	const StrList*	cur_pos;
	int		in_readline;
	sigjmp_buf	jmp_state;
} readline_state;

static PedExceptionOption	current_exception_opt = 0;

int
screen_width ()
{
	int	width = 0;

	if (opt_script_mode)
		return 32768;	/* no wrapping ;) */

/* HACK: don't specify termcap separately - it'll annoy the users. */
#ifdef HAVE_LIBREADLINE
	width = tgetnum ("co");
#endif

	if (width <= 0)
		width = 80;

	return width;
}

void
wipe_line ()
{
	if (opt_script_mode)
		return;

	/* yuck */
	printf (
"\r                                                                          \r"
	);
}


#ifdef HAVE_LIBREADLINE
/* returns matching commands for text */
static char*
command_generator (char* text, int state)
{
	if (!state)
		readline_state.cur_pos = readline_state.possibilities;

	while (readline_state.cur_pos) {
		const StrList*	cur = readline_state.cur_pos;
		readline_state.cur_pos = cur->next;
		if (str_list_match_node (cur, text))
			return str_list_convert_node (cur);
	}

	return NULL;
}

/* completion function for readline() */
char**
complete_function (char* text, int start, int end)
{
	return rl_completion_matches (text,
				      (rl_compentry_func_t*) command_generator);
}

static void
_add_history_unique (const char* line)
{
	HIST_ENTRY*	last_entry = current_history ();
	if (!strlen (line))
		return;
	if (!last_entry || strcmp (last_entry->line, line))
		add_history ((char*) line);
}
#endif /* HAVE_LIBREADLINE */

static void
interrupt_handler (int signum)
{
	int	in_readline = readline_state.in_readline;

	readline_state.in_readline = 0;

	signal (signum, &interrupt_handler);

	if (in_readline) {
		printf ("\n");
		siglongjmp (readline_state.jmp_state, 1);
	}
}

static char*
_readline (const char* prompt, const StrList* possibilities)
{
	char*		line;

	readline_state.possibilities = possibilities;
	readline_state.cur_pos = NULL;
	readline_state.in_readline = 1;

	if (sigsetjmp (readline_state.jmp_state, 1))
		return NULL;

	wipe_line ();
#ifdef HAVE_LIBREADLINE
	if (!opt_script_mode) {
		/* XXX: why isn't prompt const? */
		line = readline ((char*) prompt);
		if (line)
			_add_history_unique (line);
	} else
#endif
	{
		printf ("%s", prompt);
		fflush (stdout);
		line = (char*) malloc (256);
		if (fgets (line, 256, stdin) && strcmp (line, "") != 0) {
			line [strlen (line) - 1] = 0;	/* kill trailing CR */
		} else {
			free (line);
			line = NULL;
		}
	}

	readline_state.in_readline = 0;
	return line;
}

static PedExceptionOption
option_get_next (PedExceptionOption options, PedExceptionOption current)
{
	PedExceptionOption	i;

	if (current == 0)
		i = PED_EXCEPTION_OPTION_FIRST;
	else
		i = current * 2;

	for (; i <= options; i *= 2) {
		if (options & i)
			return i;
	}

	return 0;
}

static void
_print_exception_text (PedException* ex)
{
	StrList*		text;

	wipe_line ();

	if (ex->type == PED_EXCEPTION_BUG) {
		printf (bug_msg, VERSION);
		text = str_list_create ("\n", ex->message, "\n", NULL);
	} else {
		text = str_list_create (
			   _(ped_exception_get_type_string (ex->type)),
			   ": ", ex->message, "\n", NULL);
	}

	str_list_print_wrap (text, screen_width (), 0, 0);
	str_list_destroy (text);
}

static PedExceptionOption
exception_handler (PedException* ex)
{
	PedExceptionOption	opt;

	_print_exception_text (ex);

	/* only one choice?  Take it ;-) */
	opt = option_get_next (ex->options, 0);
	if (!option_get_next (ex->options, opt))
		return opt;

	/* script-mode: don't handle the exception */
	if (opt_script_mode)
		return PED_EXCEPTION_UNHANDLED;

	do {
		opt = command_line_get_ex_opt ("", ex->options);
	} while (opt == PED_EXCEPTION_UNHANDLED && isatty (0));
	return opt;
}


void
command_line_push_word (const char* word)
{
	command_line = str_list_append (command_line, word);
}

char*
command_line_pop_word ()
{
	char*		result;
	StrList*	next;

	PED_ASSERT (command_line != NULL, return NULL);

	result = str_list_convert_node (command_line);
	next = command_line->next;

	str_list_destroy_node (command_line);
	command_line = next;
	return result;
}

void
command_line_flush ()
{
	str_list_destroy (command_line);
	command_line = NULL;
}

char*
command_line_peek_word ()
{
	if (command_line)
		return str_list_convert_node (command_line);
	else
		return NULL;
}

int
command_line_get_word_count ()
{
	return str_list_length (command_line);
}

static int
_str_is_spaces (const char* str)
{
	while (isspace (*str)) str++;
	return *str == 0;
}

/* "multi_word mode" is the "normal" mode... many words can be typed,
 * delimited by spaces, etc.
 * 	In single-word mode, only one word is parsed per line.
 * Leading and trailing spaces are removed.  For example: " a b c "
 * is a single word "a b c".  The motivation for this mode is partition
 * names, etc.  In single-word mode, the empty string is a word.
 * (but not in multi-word mode).
 */
void
command_line_push_line (const char* line, int multi_word)
{
	int	quoted = 0;
	char	quote_char = 0;
	char	this_word [256];
	int	i;

	do {
		while (*line == ' ')
			line++;

		i = 0;
		for (; *line; line++) {
			if (*line == ' ' && !quoted) {
				if (multi_word)
					break;

			/* single word: check for trailing spaces + eol */
				if (_str_is_spaces (line))
					break;
			}

			if (!quoted && strchr ("'\"", *line)) {
				quoted = 1;
				quote_char = *line;
				continue;
			}
			if (quoted && *line == quote_char) {
				quoted = 0;
				continue;
			}

			/* hack: escape characters */
			if (quoted && line[0] == '\\' && line[1])
				line++;

			this_word [i++] = *line;
		}
		if (i || !multi_word) {
			this_word [i] = 0;
			command_line_push_word (this_word);
		}
	} while (*line && multi_word);
}

static char*
realloc_and_cat (char* str, const char* append)
{
	int	length = strlen (str) + strlen (append) + 1;
	char*	new_str = realloc (str, length);
	strcat (new_str, append);
	return new_str;
}

static char*
_construct_prompt (const char* head, const char* def,
		   const StrList* possibilities)
{
	char*	prompt = strdup (head);

	if (def && possibilities)
		PED_ASSERT (str_list_match_any (possibilities, def),
			    return NULL);

	if (possibilities && str_list_length (possibilities) < 8) {
		const StrList*	walk;
		if (strlen (prompt))
			prompt = realloc_and_cat (prompt, "  ");

		for (walk = possibilities; walk; walk = walk->next) {
			if (walk != possibilities)
				prompt = realloc_and_cat (prompt, "/");

			if (def && str_list_match_node (walk, def) == 2) {
				prompt = realloc_and_cat (prompt, "[");
				prompt = realloc_and_cat (prompt, def);
				prompt = realloc_and_cat (prompt, "]");
			} else {
				char*	text = str_list_convert_node (walk);
				prompt = realloc_and_cat (prompt, text);
				free (text);
			}
		}
		prompt = realloc_and_cat (prompt, "? ");
	} else if (def) {
		if (strlen (prompt))
			prompt = realloc_and_cat (prompt, "  ");
		prompt = realloc_and_cat (prompt, "[");
		prompt = realloc_and_cat (prompt, def);
		prompt = realloc_and_cat (prompt, "]? ");
	} else {
		if (strlen (prompt))
			prompt = realloc_and_cat (prompt, " ");
	}

	return prompt;
}

void
command_line_prompt_words (const char* prompt, const char* def,
			   const StrList* possibilities, int multi_word)
{
	char*	line;
	char*	real_prompt;
	char*	_def = (char*) def;
	int	_def_needs_free = 0;

	if (!def && str_list_length (possibilities) == 1) {
		_def = str_list_convert_node (possibilities);
		_def_needs_free = 1;
	}

	if (opt_script_mode) {
		if (_def)
			command_line_push_line (_def, 0);
		return;
	}

	do {
		real_prompt = _construct_prompt (prompt, _def, possibilities);
		line = _readline (real_prompt, possibilities);
		free (real_prompt);
		if (!line)
			break;

		if (!strlen (line)) {
			if (_def)
				command_line_push_line (_def, 0);
		} else {
			command_line_push_line (line, multi_word);
		}
		free (line);
	} while (!command_line_get_word_count () && !_def);

	if (_def_needs_free)
		free (_def);
}

char*
command_line_get_word (const char* prompt, const char* def,
		       const StrList* possibilities, int multi_word)
{
	do {
		if (command_line_get_word_count ()) {
			char*		result = command_line_pop_word ();
			StrList*	result_node;

			if (!possibilities)
				return result;
			result_node = str_list_match (possibilities, result);
			free (result);
			if (result_node)
				return str_list_convert_node (result_node);
			command_line_flush ();
		}

		command_line_prompt_words (prompt, def, possibilities,
					   multi_word);
	} while (command_line_get_word_count ());

	return NULL;
}

int
command_line_get_integer (const char* prompt, int* value)
{
	char	def_str [10];
	char*	input;
	int	valid;

	snprintf (def_str, 10, "%d", *value);
	input = command_line_get_word (prompt, *value ? def_str : NULL,
				       NULL, 1);
	if (!input)
		return 0;
	valid = sscanf (input, "%d", value);
	free (input);
	return valid;
}

int
command_line_get_sector (const char* prompt, PedDevice* dev, PedSector* value,
			 PedGeometry** range)
{
	char*	def_str;
	char*	input;
	int	valid;

	def_str = ped_unit_format (dev, *value);
	input = command_line_get_word (prompt, *value ? def_str : NULL,
				       NULL, 1);

	/* def_str might have rounded *value a little bit.  If the user picked
	 * the default, make sure the selected sector is identical to the
	 * default.
	 */
	if (input && *value && !strcmp (input, def_str)) {
		ped_free (def_str);
		return 1;
	}

	ped_free (def_str);
	if (!input)
		return 0;

	valid = ped_unit_parse (input, dev, value, range);

	free (input);
	return valid;
}

int
command_line_get_state (const char* prompt, int* value)
{
	char*	def_word;
	char*	input;

	if (*value)
		def_word = str_list_convert_node (on_list);
	else
		def_word = str_list_convert_node (off_list);
	input = command_line_get_word (prompt, def_word, on_off_list, 1);
	free (def_word);
	if (!input)
		return 0;
	if (str_list_match_any (on_list, input))
		*value = 1;
	else
		*value = 0;
	free (input);
	return 1;
}

int
command_line_get_device (const char* prompt, PedDevice** value)
{
	char*		def_dev_name = *value ? (*value)->path : NULL;
	char*		dev_name;
	PedDevice*	dev;

	dev_name = command_line_get_word (prompt, def_dev_name, NULL, 1);
	if (!dev_name)
		return 0;
	dev = ped_device_get (dev_name);
	free (dev_name);
	if (!dev)
		return 0;
	*value = dev;
	return 1;
}

int
command_line_get_disk (const char* prompt, PedDisk** value)
{
	PedDevice*	dev = *value ? (*value)->dev : NULL;
	if (!command_line_get_device (prompt, &dev))
		return 0;
	if (dev != (*value)->dev) {
		PedDisk* new_disk = ped_disk_new (dev);
		if (!new_disk)
			return 0;
		*value = new_disk;
	}
	return 1;
}

int
command_line_get_partition (const char* prompt, PedDisk* disk,
			    PedPartition** value)
{
	PedPartition*	part;
	int		num = (*value) ? (*value)->num : 0;

	if (!command_line_get_integer (prompt, &num)) {
		ped_exception_throw (PED_EXCEPTION_ERROR, PED_EXCEPTION_CANCEL,
				     _("Expecting a partition number."));
		return 0;
	}
	part = ped_disk_get_partition (disk, num);
	if (!part) {
		ped_exception_throw (PED_EXCEPTION_ERROR, PED_EXCEPTION_CANCEL,
				     _("Partition doesn't exist."));
		return 0;
	}
	*value = part;
	return 1;
}

int
command_line_get_fs_type (const char* prompt, const PedFileSystemType*(* value))
{
	char*			fs_type_name;
	PedFileSystemType*	fs_type;

	fs_type_name = command_line_get_word (prompt,
					      *value ? (*value)->name : NULL,
		       			      fs_type_list, 1);
	if (!fs_type_name) {
		ped_exception_throw (PED_EXCEPTION_ERROR, PED_EXCEPTION_CANCEL,
				     _("Expecting a file system type."));
		return 0;
	}
	fs_type = ped_file_system_type_get (fs_type_name);
	if (!fs_type) {
		ped_exception_throw (PED_EXCEPTION_ERROR, PED_EXCEPTION_CANCEL,
				     _("Unknown file system type \"%s\"."),
				     fs_type_name);
		return 0;
	}
	free (fs_type_name);
	*value = fs_type;
	return 1;
}

int
command_line_get_disk_type (const char* prompt, const PedDiskType*(* value))
{
	char*		disk_type_name;
	PedDiskType*	disk_type;

	disk_type_name = command_line_get_word (prompt,
						*value ? (*value)->name : NULL,
						disk_type_list, 1);
	if (!disk_type_name) {
		ped_exception_throw (PED_EXCEPTION_ERROR, PED_EXCEPTION_CANCEL,
				     _("Expecting a disk label type."));
		return 0;
	}
	*value = ped_disk_type_get (disk_type_name);
	free (disk_type_name);
	PED_ASSERT (*value != NULL, return 0);
	return 1;
}

int
command_line_get_part_flag (const char* prompt, const PedPartition* part,
			    PedPartitionFlag* flag)
{
	StrList*		opts = NULL;
	PedPartitionFlag	walk = 0;
	char*			flag_name;

	while ( (walk = ped_partition_flag_next (walk)) ) {
		if (ped_partition_is_flag_available (part, walk)) {
			const char*	walk_name;

			walk_name = ped_partition_flag_get_name (walk);
			opts = str_list_append (opts, walk_name);
			opts = str_list_append_unique (opts, _(walk_name));
		}
	}

	flag_name = command_line_get_word (prompt, NULL, opts, 1);
	str_list_destroy (opts);

	if (flag_name) {
		*flag = ped_partition_flag_get_by_name (flag_name);
		ped_free (flag_name);
		return 1;
	} else {
		return 0;
	}
}

static int
_can_create_primary (const PedDisk* disk)
{
	int	i;

	for (i = 1; i <= ped_disk_get_max_primary_partition_count (disk); i++) {
		if (!ped_disk_get_partition (disk, i))
			return 1;
	}

	return 0;
}

static int
_can_create_extended (const PedDisk* disk)
{
	if (!_can_create_primary (disk))
		return 0;
	if (!ped_disk_type_check_feature (disk->type, PED_DISK_TYPE_EXTENDED))
		return 0;
	if (ped_disk_extended_partition (disk))
		return 0;
	return 1;
}

static int
_can_create_logical (const PedDisk* disk)
{
	if (!ped_disk_type_check_feature (disk->type, PED_DISK_TYPE_EXTENDED))
		return 0;
	return ped_disk_extended_partition (disk) != 0;
}

int
command_line_get_part_type (const char* prompt, const PedDisk* disk,
	       		    PedPartitionType* type)
{
	StrList*	opts = NULL;
	char*		type_name;

	if (_can_create_primary (disk)) {
		opts = str_list_append_unique (opts, "primary");
		opts = str_list_append_unique (opts, _("primary"));
	}
	if (_can_create_extended (disk)) {
		opts = str_list_append_unique (opts, "extended");
		opts = str_list_append_unique (opts, _("extended"));
	}
	if (_can_create_logical (disk)) {
		opts = str_list_append_unique (opts, "logical");
		opts = str_list_append_unique (opts, _("logical"));
	}
	if (!opts) {
		ped_exception_throw (
			PED_EXCEPTION_ERROR, PED_EXCEPTION_CANCEL,
			_("Can't create any more partitions."));
		return 0;
	}

	type_name = command_line_get_word (prompt, NULL, opts, 1);
	str_list_destroy (opts);

	if (!type_name) {
		ped_exception_throw (
			PED_EXCEPTION_ERROR, PED_EXCEPTION_CANCEL,
			_("Expecting a partition type."));
		return 0;
	}

	if (!strcmp (type_name, "primary")
			|| !strcmp (type_name, _("primary"))) {
		*type = 0;
	}
	if (!strcmp (type_name, "extended")
			|| !strcmp (type_name, _("extended"))) {
		*type = PED_PARTITION_EXTENDED;
	}
	if (!strcmp (type_name, "logical")
			|| !strcmp (type_name, _("logical"))) {
		*type = PED_PARTITION_LOGICAL;
	}

	free (type_name);
	return 1;
}

PedExceptionOption
command_line_get_ex_opt (const char* prompt, PedExceptionOption options)
{
	StrList*		options_strlist = NULL;
	PedExceptionOption	opt;
	char*			opt_name;

	for (opt = option_get_next (options, 0); opt; 
	     opt = option_get_next (options, opt)) {
		options_strlist = str_list_append_unique (options_strlist,
				     _(ped_exception_get_option_string (opt)));
		options_strlist = str_list_append_unique (options_strlist,
				     ped_exception_get_option_string (opt));
	}

	opt_name = command_line_get_word (prompt, NULL, options_strlist, 1);
	if (!opt_name)
		return PED_EXCEPTION_UNHANDLED;
	str_list_destroy (options_strlist);

	opt = PED_EXCEPTION_OPTION_FIRST;
	while (1) {
		opt = option_get_next (options, opt);
		if (strcmp (opt_name,
			    ped_exception_get_option_string (opt)) == 0)
			break;
		if (strcmp (opt_name,
			    _(ped_exception_get_option_string (opt))) == 0)
			break;
	}
	free (opt_name);
	return opt;
}

int
command_line_get_unit (const char* prompt, PedUnit* unit)
{
	StrList*	opts = NULL;
	PedUnit		walk;
	char*		unit_name;
	const char*	default_unit_name;

	for (walk = PED_UNIT_FIRST; walk <= PED_UNIT_LAST; walk++)
		opts = str_list_append (opts, ped_unit_get_name (walk));

	default_unit_name = ped_unit_get_name (ped_unit_get_default ());
	unit_name = command_line_get_word (prompt, default_unit_name, opts, 1);
	str_list_destroy (opts);

	if (unit_name) {
		*unit = ped_unit_get_by_name (unit_name);
		free (unit_name);
		return 1;
	} else {
		return 0;
	}
}

int
command_line_is_integer ()
{
	char*	word;
	int	is_integer;
	int	scratch;

	word = command_line_peek_word ();
	if (!word)
		return 0;

	is_integer = sscanf (word, "%d", &scratch);
	free (word);
	return is_integer;
}

static int
init_ex_opt_str ()
{
	int			i;
	PedExceptionOption	opt;

	for (i = 0; (1 << i) <= PED_EXCEPTION_OPTION_LAST; i++) {
		opt = (1 << i);
		ex_opt_str [i]
			= str_list_create (
				ped_exception_get_option_string (opt),
				_(ped_exception_get_option_string (opt)),
				NULL);
		if (!ex_opt_str [i])
			return 0;
	}

	ex_opt_str [i] = NULL;
	return 1;
}

static void
done_ex_opt_str ()
{
	int	i;

	for (i=0; ex_opt_str [i]; i++)
		str_list_destroy (ex_opt_str [i]);
}

static int
init_state_str ()
{
	on_list = str_list_create_unique (_("on"), "on", NULL);
	off_list = str_list_create_unique (_("off"), "off", NULL);
	on_off_list = str_list_join (str_list_duplicate (on_list),
				     str_list_duplicate (off_list));
	return 1;
}

static void
done_state_str ()
{
	str_list_destroy (on_list);
	str_list_destroy (off_list);
	str_list_destroy (on_off_list);
}

static int
init_fs_type_str ()
{
	PedFileSystemType*	walk;

	fs_type_list = NULL;

	for (walk = ped_file_system_type_get_next (NULL); walk;
	     walk = ped_file_system_type_get_next (walk))
	{
		fs_type_list = str_list_insert (fs_type_list, walk->name);
		if (!fs_type_list)
			return 0;
	}

	return 1;
}

static int
init_disk_type_str ()
{
	PedDiskType*	walk;

	disk_type_list = NULL;

	for (walk = ped_disk_type_get_next (NULL); walk;
	     walk = ped_disk_type_get_next (walk))
	{
		disk_type_list = str_list_insert (disk_type_list, walk->name);
		if (!disk_type_list)
			return 0;
	}

	return 1;
}

int
init_ui ()
{
	opt_script_mode = !isatty (0);

	if (!init_ex_opt_str ()
	    || !init_state_str ()
	    || !init_fs_type_str ()
	    || !init_disk_type_str ())
		return 0;
	ped_exception_set_handler (exception_handler);

#ifdef HAVE_LIBREADLINE
	rl_initialize ();
	rl_attempted_completion_function = (CPPFunction*) complete_function;
	readline_state.in_readline = 0;
#endif

	signal (SIGINT, &interrupt_handler);

	return 1;
}

void
done_ui ()
{
	ped_exception_set_handler (NULL);
	done_ex_opt_str ();
	done_state_str ();
	str_list_destroy (fs_type_list);
	str_list_destroy (disk_type_list);
}

void
help_msg ()
{
	printf (_(usage_msg));

	printf ("\n%s\n", _("OPTIONs:"));
	print_options_help ();

	printf ("\n%s\n", _("COMMANDs:"));
	print_commands_help ();
	exit (0);
}

void
print_using_dev (PedDevice* dev)
{
	printf (_("Using %s\n"), dev->path);
}

int
interactive_mode (PedDevice** dev, Command* cmd_list[])
{
	char*		line;
	StrList*	list;
	StrList*	command_names = command_get_names (cmd_list);

	commands = cmd_list;	/* FIXME yucky, nasty, evil hack */

	printf ("%s", prog_name);

	list = str_list_create (_(banner_msg), NULL);
	str_list_print_wrap (list, screen_width (), 0, 0);
	str_list_destroy (list);

	print_using_dev (*dev);

	while (1) {
		char*		word;
		Command*	cmd;

		while (!command_line_get_word_count ()) {
			if (feof (stdin)) {
				printf ("\n");
				return 1;
			}
			command_line_prompt_words ("(parted)", NULL,
						   command_names, 1);
		}

		word = command_line_pop_word ();
		if (word) {
			cmd = command_get (commands, word);
			free (word);
			if (cmd) {
				if (!command_run (cmd, dev))
					command_line_flush ();
			} else {
				print_commands_help ();
			}
		}
	}

	return 1;
}


int
non_interactive_mode (PedDevice** dev, Command* cmd_list[],
		      int argc, char* argv[])
{
	int		i;
	Command*	cmd;

	commands = cmd_list;	/* FIXME yucky, nasty, evil hack */

	for (i = 0; i < argc; i++)
		command_line_push_line (argv [i], 1);

	while (command_line_get_word_count ()) {
		char*		word;

		word = command_line_pop_word ();
		if (!word)
			break;
		cmd = command_get (commands, word);
		if (!cmd) {
			help_msg ();
			goto error;
		}
		if (!command_run (cmd, dev))
			goto error;
	}
	return 1;

error:
	return 0;
}

