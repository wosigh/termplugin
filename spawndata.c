/********************************************************/
/* Gsh Copyright (C) 1999 Vaughn Cato                   */
/* See the file "LICENSE", included with this software, */
/* for license information.                             */
/********************************************************/
#include "spawndata.h"

#include <assert.h>
#include <stdlib.h>
#include <stddef.h>

static SpawnData *first_spawn_data = 0;

#if 0
struct TermCommandMember_s term_command_members[] = {
		{"home_cursor",offsetof(TermCommands,home_cursor)},
		{"clear_below",offsetof(TermCommands,clear_below)},
		{"clear_above",offsetof(TermCommands,clear_above)},
		{"clear_to_right",offsetof(TermCommands,clear_to_right)},
		{"clear_to_left",offsetof(TermCommands,clear_to_left)},
		{"clear_screen",offsetof(TermCommands,clear_screen)},
		{"clear_line",offsetof(TermCommands,clear_line)},
		{"norm_attr",offsetof(TermCommands,norm_attr)},
		{"bold_attr",offsetof(TermCommands,bold_attr)},
		{"dim_attr",offsetof(TermCommands,dim_attr)},
		{"italic_attr",offsetof(TermCommands,italic_attr)},
		{"underline_attr",offsetof(TermCommands,underline_attr)},
		{"blink_attr",offsetof(TermCommands,blink_attr)},
		{"inverse_attr",offsetof(TermCommands,inverse_attr)},
		{"fgcolor_attr",offsetof(TermCommands,fgcolor_attr)},
		{"bgcolor_attr",offsetof(TermCommands,bgcolor_attr)},
		{"save_cursor",offsetof(TermCommands,save_cursor)},
		{"restore_cursor",offsetof(TermCommands,restore_cursor)},
		{"set_tab",offsetof(TermCommands,set_tab)},
		{"clear_tabs",offsetof(TermCommands,clear_tabs)},
		{"bell",offsetof(TermCommands,bell)},
		{"backspace",offsetof(TermCommands,backspace)},
		{"tab",offsetof(TermCommands,tab)},
		{"carriage_return",offsetof(TermCommands,carriage_return)},
		{"text",offsetof(TermCommands,text)},
		{"unexpected",offsetof(TermCommands,unexpected)},
		{"app_cursor_keys",offsetof(TermCommands,app_cursor_keys)},
		{"autowrap",offsetof(TermCommands,autowrap)},
		{"set_screen",offsetof(TermCommands,set_screen)},
		{"cursor_visible",offsetof(TermCommands,cursor_visible)},
		{"mouse_reporting",offsetof(TermCommands,mouse_reporting)},
		{"mouse_highlighting",offsetof(TermCommands,mouse_highlighting)},
		{"set_row",offsetof(TermCommands,set_row)},
		{"set_col",offsetof(TermCommands,set_col)},
		{"insert_line",offsetof(TermCommands,insert_line)},
		{"delete_line",offsetof(TermCommands,delete_line)},
		{"insert_char",offsetof(TermCommands,insert_char)},
		{"delete_char",offsetof(TermCommands,delete_char)},
		{"erase_char",offsetof(TermCommands,erase_char)},
		{"move_up",offsetof(TermCommands,move_up)},
		{"move_down",offsetof(TermCommands,move_down)},
		{"cursor_up",offsetof(TermCommands,cursor_up)},
		{"cursor_down",offsetof(TermCommands,cursor_down)},
		{"cursor_right",offsetof(TermCommands,cursor_right)},
		{"cursor_left",offsetof(TermCommands,cursor_left)},
		{"app_keypad",offsetof(TermCommands,app_keypad)},
		{"insert_mode",offsetof(TermCommands,insert_mode)},
		{"set_cursor",offsetof(TermCommands,set_cursor)},
		{"set_scroll_region",offsetof(TermCommands,set_scroll_region)},
		{"set_font_style",offsetof(TermCommands,set_font_style)},
		{"set_charset",offsetof(TermCommands,set_charset)},
		{"buffer_empty",offsetof(TermCommands,buffer_empty)},
		{"set_title",offsetof(TermCommands,set_title)},
		{"eof",offsetof(TermCommands,eof)},
		{"req_attr",offsetof(TermCommands,req_attr)},
		{NULL,0}
};
#endif

SpawnData *NewSpawnData(CommandHandlers *cmd_handlers_ptr,void *client_data)
{
	SpawnData *new_spawn_data = (SpawnData *)malloc(sizeof(SpawnData));
	new_spawn_data->output_buffer = 0;
	new_spawn_data->output_buffer_size = 0;
	new_spawn_data->master_fd = -1;
	new_spawn_data->reading_master = 0;
	new_spawn_data->slave_fd = -1;
	new_spawn_data->window_size.ws_row = 25;
	new_spawn_data->window_size.ws_col = 80;
	new_spawn_data->window_size.ws_xpixel = 0;
	new_spawn_data->window_size.ws_ypixel = 0;
	new_spawn_data->slave_name[0] = '\0';
	new_spawn_data->child_running = 0;
	new_spawn_data->child_exit_handled = 1;
	new_spawn_data->child_pid = -1;
	new_spawn_data->output_enabled = 1;
	new_spawn_data->exit_signal = -1;
	new_spawn_data->exit_code = -1;
	new_spawn_data->exit_reported = 0;
	{
		new_spawn_data->default_data = 0;
	}
	TermState_Init(&new_spawn_data->term_state,cmd_handlers_ptr,client_data);
#if 0
{
	static TermCommands null_term_commands;
	new_spawn_data->term_commands = null_term_commands;
}
#endif

new_spawn_data->next = first_spawn_data;
first_spawn_data = new_spawn_data;

return new_spawn_data;
}

void DeleteSpawnData(SpawnData *spawn_data)
{
	assert(spawn_data);
	assert(spawn_data->child_pid==-1);
	assert(!spawn_data->child_running);
	assert(spawn_data->master_fd==-1);
	assert(spawn_data->slave_fd==-1);
	assert(spawn_data->child_exit_handled);
	assert(!spawn_data->reading_master);

	if (spawn_data->output_buffer) {
		free(spawn_data->output_buffer);
		spawn_data->output_buffer = 0;
		spawn_data->output_buffer_size = 0;
	}
#if 0
{
	int i = 0;
	for (;term_command_members[i].name;++i) {
		int offset = term_command_members[i].offset;
		char **mp = (char **)((char *)&spawn_data->term_commands+offset);
		if (*mp) {
			free(*mp);
		}
	}
}
#endif
{
	SpawnData **next_spawn_data_ptr = &first_spawn_data;

	while (*next_spawn_data_ptr) {
		if (*next_spawn_data_ptr==spawn_data) {
			*next_spawn_data_ptr = spawn_data->next;
			free(spawn_data);
			return;
		}
		next_spawn_data_ptr = &(*next_spawn_data_ptr)->next;
	}
	assert(0);
}
}

int SpawnId(SpawnData *spawn_data)
{
	assert(spawn_data->master_fd>=0);
	return spawn_data->master_fd;
}

SpawnData *FirstSpawnData(void)
{
	return first_spawn_data;
}

SpawnData *SpawnWithId(int spawn_id)
{
	SpawnData *sd = first_spawn_data;

	for (;sd;sd=sd->next) {
		if (SpawnId(sd)==spawn_id) {
			break;
		}
	}
	return sd;
}

