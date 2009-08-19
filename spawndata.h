/********************************************************/
/* Gsh Copyright (C) 1999 Vaughn Cato                   */
/* See the file "LICENSE", included with this software, */
/* for license information.                             */
/********************************************************/
#ifndef SPAWNDATA_H_
#define SPAWNDATA_H_

#include <sys/ioctl.h>
#include <termios.h>
#include "termstate.h"


#if 0
typedef struct TermCommands_s TermCommands;

struct TermCommands_s {
	/* Commands with no parameters */
	char *home_cursor;
	char *clear_below;
	char *clear_above;
	char *clear_to_right;
	char *clear_to_left;
	char *clear_screen;
	char *clear_line;
	char *norm_attr;
	char *save_cursor;
	char *restore_cursor;
	char *set_tab;
	char *clear_tabs;
	char *bell;
	char *backspace;
	char *tab;
	char *carriage_return;
	/* Commands with one integer parameter */
	char *text;
	char *unexpected;
	char *bold_attr;
	char *dim_attr;
	char *italic_attr;
	char *underline_attr;
	char *blink_attr;
	char *inverse_attr;
	char *fgcolor_attr;
	char *bgcolor_attr;
	char *app_cursor_keys;
	char *autowrap;
	char *set_screen;
	char *cursor_visible;
	char *mouse_reporting;
	char *mouse_highlighting;
	char *set_row;
	char *set_col;
	char *insert_line;
	char *delete_line;
	char *insert_char;
	char *delete_char;
	char *erase_char;
	char *move_up;
	char *move_down;
	char *cursor_up;
	char *cursor_down;
	char *cursor_right;
	char *cursor_left;
	char *app_keypad;
	char *insert_mode;
	/* Commands with two integer arguments */
	char *set_cursor;
	char *set_scroll_region;
	char *set_font_style;
	char *set_charset;
	char *set_title;
	char *req_attr;
	char *buffer_empty;
	char *eof;
};
#endif

#if 0
extern struct TermCommandMember_s {
	const char *name;
	int offset;
} term_command_members[];
#endif

typedef struct SpawnData_s SpawnData;

struct SpawnData_s {
	char *output_buffer;
	int output_buffer_size;
	void *default_data;
	int master_fd;
	int reading_master;
	int slave_fd;
	struct winsize window_size;
	char slave_name[64];
	int child_running;
	int child_exit_handled;
	int child_pid;
	int output_enabled;
	int exit_signal;
	int exit_code;
	int exit_reported;
	SpawnData *next;
	TermState term_state;
#if 0
	TermCommands term_commands;
#endif
};

/* All eval procs return false if they didn't handle the operation.  */
/* text_proc and unexpected_proc must be able to handle normal text and */
/* unexpected characters respectively. */
extern SpawnData *NewSpawnData(CommandHandlers *cmd_handlers_ptr,void *);
extern void DeleteSpawnData(SpawnData *spawn_data);
extern int SpawnId(SpawnData *spawn_data);
extern SpawnData *FirstSpawnData(void);
extern SpawnData *SpawnWithId(int spawn_id);

#endif /* SPAWNDATA_H_ */
