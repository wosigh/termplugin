/********************************************************/
/* Gsh Copyright (C) 1999 Vaughn Cato                   */
/* See the file "LICENSE", included with this software, */
/* for license information.                             */
/********************************************************/
#include "termstate.h"

#include <stdio.h>
#include <assert.h>
#include <ctype.h>
#include <stdlib.h>

#define ELEMENTS(x) (sizeof(x)/sizeof*(x))


/* evaluate command with no arguments */
static int EvalCmd(TermState *state,TermOp_t op)
{
	return state->eval_procs.void_proc(state->eval_data,op);
}

/* evaluate command with one int argument */
static int EvalCmdInt(TermState *state,TermOp_t op,int arg1)
{
	return state->eval_procs.int_proc(state->eval_data,op,arg1);
}

/* evaluate command with two int arguments */
static int EvalCmd2Int(TermState *state,TermOp_t op,int arg1,int arg2)
{
	return
	state->eval_procs.int_int_proc(state->eval_data,op,arg1,arg2);
}

/* evaluate command with one string argument*/
static int EvalCmdString(TermState *state,TermOp_t op,const char *arg)
{
	return state->eval_procs.string_proc(state->eval_data,op,arg);
}


static int TermState_PrevBufPos(TermState *state,int pos)
{
	return (pos+ELEMENTS(state->buffer)-1) % ELEMENTS(state->buffer);
}

static void TermState_PushAlternative(TermState *state,TermStateProc *proc)
{
	assert(state->alt_stack_pos<(int)ELEMENTS(state->bufpos_stack));
	state->bufpos_stack[state->alt_stack_pos] =
		TermState_PrevBufPos(state,state->bufpos);
	assert(state->alt_stack_pos<(int)ELEMENTS(state->alt_stack));
	state->alt_stack[state->alt_stack_pos] = proc;
	assert(state->alt_stack_pos<(int)ELEMENTS(state->cont_stack_pos_stack));
	state->cont_stack_pos_stack[state->alt_stack_pos] = state->cont_stack_pos;
	++state->alt_stack_pos;
}

static void TermState_Fail(TermState *state)
{
	assert(state->alt_stack_pos>0);
	--state->alt_stack_pos;
	assert(state->alt_stack_pos<(int)ELEMENTS(state->alt_stack));
	state->proc = state->alt_stack[state->alt_stack_pos];
	assert(state->alt_stack_pos<(int)ELEMENTS(state->bufpos_stack));
	state->bufpos = state->bufpos_stack[state->alt_stack_pos];
	assert(state->alt_stack_pos<(int)ELEMENTS(state->cont_stack_pos_stack));
	state->cont_stack_pos = state->cont_stack_pos_stack[state->alt_stack_pos];
}

static void TermState_PushContinuation(TermState *state,TermStateProc *proc)
{
	assert(state->cont_stack_pos>=0);
	assert(state->cont_stack_pos<(int)ELEMENTS(state->cont_stack));
	state->cont_stack[state->cont_stack_pos++] = proc;
}

static void TermState_Continue(TermState *state)
{
	assert(state->cont_stack_pos>0);
	--state->cont_stack_pos;
	assert(state->cont_stack_pos<(int)ELEMENTS(state->cont_stack));
	state->proc = state->cont_stack[state->cont_stack_pos];
}

static int TermState_IntRest(TermState *state,int c)
{
	assert(state->alt_stack_pos>0);
	if (isdigit(c)) {
		assert(state->int_count>=0);
		assert(state->int_count<=(int)ELEMENTS(state->int_params));
		{
			int *current_int = &state->int_params[state->int_count-1];
			*current_int *= 10;
			*current_int += c-'0';
			state->proc = TermState_IntRest;
		}
	}
	else {
		state->bufpos = TermState_PrevBufPos(state,state->bufpos);
		TermState_Continue(state);
	}
	return 1;
}

static int TermState_Int(TermState *state,int c)
{
	if (state->int_count<(int)ELEMENTS(state->int_params) && isdigit(c)) {
		state->int_params[state->int_count] = c-'0';
		++state->int_count;
		state->proc = TermState_IntRest;
	}
	else {
		return 0;
	}
	return 1;
}


static int StateCode(int c)
{
	int value = -1;

	switch (c) {
	case 'l':  /* low */
		value = 0;
		break;
	case 'h': /* high */
		value = 1;
		break;
	case 's': /* save */
		value = 2;
		break;
	case 'r': /* restore */
		value = 3;
		break;
	case 't': /* toggle */
		value = 4;
		break;
	}
	return value;
}

static int TermState_XTermString(TermState *state,int c)
{
	if (c==7) {
		int result;

		assert(state->string_param_len<state->string_param_size);
		state->string_param[state->string_param_len] = '\0';
		result = EvalCmdString(state,TERM_SET_TITLE,state->string_param);
		free(state->string_param);
		state->string_param = 0;
		return result;
	}
	if (state->string_param_len+1>=state->string_param_size) {
		state->string_param_size *= 2;
		state->string_param =
			(char *)realloc(state->string_param,state->string_param_size);
	}
	assert(state->string_param_len<state->string_param_size);
	state->string_param[state->string_param_len++] = c;
	state->proc = TermState_XTermString;
	return 1;
}

static int TermState_XTermInt(TermState *state,int c)
{
	if (c==';') {
		state->proc = TermState_XTermString;
		assert(!state->string_param);
		state->string_param_size = 256;
		state->string_param = (char *)malloc(state->string_param_size);
		state->string_param_len = 0;
		return 1;
	}
	return 0;
}

static int TermState_XTerm(TermState *state,int c)
{
	switch (c) {
	case '0':
		state->proc = TermState_XTermInt;
		return 1;
	}
	return 0;
}

static int TermState_CSIInt(TermState *state,int c);

static int TermState_CSI2(TermState *state,int c)
{
	TermState_PushAlternative(state,TermState_CSIInt);
	TermState_PushContinuation(state,TermState_CSIInt);
	return TermState_Int(state,c);
}

static int TermState_CSIInt(TermState *state,int c)
{
	TermOp_t cmd = TERM_INVALID_OP;

	switch (c) {
	case 'H':
	{
		if (state->int_count==0) {
			return EvalCmd(state,TERM_HOME_CURSOR);
		}
		if (state->int_count==1) {
			return EvalCmdInt(state,TERM_SET_ROW,state->int_params[0]);
		}
		if (state->int_count==2) {
			return
			EvalCmd2Int(
					state,
					TERM_SET_CURSOR,
					state->int_params[0],
					state->int_params[1]
			);
		}
		return 0;
	}
	case 'G':
	{
		if (state->int_count==1) {
			return EvalCmdInt(state,TERM_SET_COL,state->int_params[0]);
		}
		return 0;
	}
	case 'd':
	{
		if (state->int_count==1) {
			return EvalCmdInt(state,TERM_SET_ROW,state->int_params[0]);
		}
		return 0;
	}
	case 'r':
	{
		if (state->int_count==2) {
			return
			EvalCmd2Int(
					state,
					TERM_SET_SCROLL_REGION,
					state->int_params[0],
					state->int_params[1]
			);
		}
		return 0;
	}
	case 'g':
	{
		if (state->int_count==1) {
			switch (state->int_params[0]) {
			case 0:
				return EvalCmdInt(state,TERM_SET_TAB,0);
				break;
			case 3:
				return EvalCmd(state,TERM_CLEAR_TABS);
				break;
			}
		}
		return 0;
	}
	case 'J':
	{
		if (state->int_count<=1) {
			int param = 0;
			if (state->int_count==1) {
				param = state->int_params[0];
			}
			switch (param) {
			case 0: cmd = TERM_CLEAR_BELOW; break;
			case 1: cmd = TERM_CLEAR_ABOVE; break;
			case 2: cmd = TERM_CLEAR_SCREEN; break;
			}
			if (cmd!=TERM_INVALID_OP) {
				return EvalCmd(state,cmd);
			}
		}
		return 0;
	}
	case 'K':
	{
		if (state->int_count<=1) {
			int param = 0;
			if (state->int_count==1) {
				param = state->int_params[0];
			}
			switch (param) {
			case 0: cmd = TERM_CLEAR_TO_RIGHT; break;
			case 1: cmd = TERM_CLEAR_TO_LEFT; break;
			case 2: cmd = TERM_CLEAR_LINE; break;
			}
			if (cmd!=TERM_INVALID_OP) {
				return EvalCmd(state,cmd);
			}
		}
		return 0;
	}
	case 'm':
	{
		int i = 0;
		for (;i<state->int_count;++i) {
			int result = 0;

			int code = state->int_params[i];
			if (code>=30 && code<40) {
				result = EvalCmdInt(state,TERM_FGCOLOR_ATTR,code-30);
			}
			else if (code>=300 && code <400) {
				result = EvalCmdInt(state,TERM_FGCOLOR_ATTR,code-300);
			}
			else if (code>=40 && code<50) {
				result = EvalCmdInt(state,TERM_BGCOLOR_ATTR,code-40);
			}
			else if (code>=400 && code <500) {
				result = EvalCmdInt(state,TERM_FGCOLOR_ATTR,code-400);
			}
			else if (code==10) {
				result = EvalCmd(state,TERM_PRIMARY_FONT);
			}
			else if (code==11) {
				result = EvalCmd(state,TERM_ALTERNATE_FONT1);
			}
			else if (code==12) {
				result = EvalCmd(state,TERM_ALTERNATE_FONT2);
			}
			else {
				int set = 1;

				if (code>=20 && code<30) {
					code -= 20;
					set = 0;
				}
				switch (code) {
				case 0:
					result = EvalCmd(state,TERM_NORM_ATTR);
					break;
				case 1:
					result = EvalCmdInt(state,TERM_BOLD_ATTR,set);
					break;
				case 2:
					result = EvalCmdInt(state,TERM_DIM_ATTR,set);
					break;
				case 3:
					result = EvalCmdInt(state,TERM_ITALIC_ATTR,set);
					break;
				case 4:
					result = EvalCmdInt(state,TERM_UNDERLINE_ATTR,set);
					break;
				case 5:
					result = EvalCmdInt(state,TERM_BLINK_ATTR,set);
					break;
				case 7:
					result = EvalCmdInt(state,TERM_INVERSE_ATTR,set);
					break;
				}
			}
			/* If any of them fail, consider the whole sequence */
			/* to have failed. */
			if (!result) {
				return 0;
			}
		}
		if (i==0) { /* No parameters */
			return EvalCmd(state,TERM_NORM_ATTR);
		}
		return 1;
	}
	case ';':
	{
		state->proc = TermState_CSI2;
		/* TermState_PushContinuation(state,TermState_CSIInt); */
		/* state->proc = TermState_Int; */
		return 1;
	}
	case 'c':
	{
		if (state->int_count==0 ||
				(state->int_count==1 && state->int_params[0]==0)) {
			return EvalCmd(state,TERM_REQ_ATTR);
		}
		return 0;
	}
	}
	{
		if (state->int_count<=1) {
			int count = 1;
			if (state->int_count==1) {
				count = state->int_params[0];
			}
			switch (c) {
			case 'A': cmd = TERM_CURSOR_UP; break;
			case 'B': cmd = TERM_CURSOR_DOWN; break;
			case 'C': cmd = TERM_CURSOR_RIGHT; break;
			case 'D': cmd = TERM_CURSOR_LEFT; break;
			case 'L': cmd = TERM_INSERT_LINE; break;
			case 'M': cmd = TERM_DELETE_LINE; break;
			case '@': cmd = TERM_INSERT_CHAR; break;
			case 'X': cmd = TERM_ERASE_CHAR; break;
			case 'P': cmd = TERM_DELETE_CHAR; break;
			}
			if (cmd!=TERM_INVALID_OP) {
				return EvalCmdInt(state,cmd,count);
			}
		}
	}
	{
		int state_code = StateCode(c);

		if (state_code!=-1) {
			if (state->int_count==1) {
				if (state->priv==0) {
					switch (state->int_params[0]) {
					case 4:
						return EvalCmdInt(state,TERM_INSERT_MODE,state_code);
					default:
						fprintf(
								stderr,
								"TermState_CSIInt: <ESC>[%d%c\n",
								state->int_params[0],c
						);
						return 1;
					}
				}
				else {
					int value = StateCode(c);

					if (value==-1)
						return 0;

					switch (state->int_params[0]) {
					case 1:
						return EvalCmdInt(state,TERM_APP_CURSOR_KEYS,value);
					case 7:
						return EvalCmdInt(state,TERM_AUTOWRAP,value);
					case 25:
						return EvalCmdInt(state,TERM_CURSOR_VISIBLE,value);
					case 47:
						return EvalCmdInt(state,TERM_SET_SCREEN,value);
					case 1000:
						return EvalCmdInt(state,TERM_MOUSE_REPORTING,value);
					case 1001:
						return EvalCmdInt(state,TERM_MOUSE_HIGHLIGHTING,value);
					default:
						fprintf(
								stderr,
								"TermState_CSIQMInt: <ESC>[%c%d%c\n",
								state->priv,
								state->int_params[0],
								c
						);
						return 1;
					}
				}
				return 0;
			}
		}
	}

	return 0;
}

static int TermState_CSI(TermState *state,int c)
{
	if (c >='<' && c <='?') {
		state->priv = c;
		state->proc = TermState_CSI2;
		return 1;
	}
	else {
		state->priv = 0;
		return TermState_CSI2(state,c);
	}
}

static int TermState_Charset(TermState *state,int c)
{
	int style;

	switch (c) {
	case '0': style = TERM_DEC_SPECIAL_STYLE; break;
	case 'A': style = TERM_UK_STYLE; break;
	case 'B': style = TERM_USASCII_STYLE; break;
	case '<': style = TERM_MULTINATIONAL_STYLE; break;
	case '5':
	case 'C': style = TERM_FINNISH_STYLE; break;
	case 'K': style = TERM_GERMAN_STYLE; break;
	default:
		return 1; /* Don't know it, but that's ok */
	}
	{
		int charset = state->int_params[0];

		return EvalCmd2Int(state,TERM_SET_FONT_STYLE,charset,style);
	}
}

static int TermState_Esc(TermState *state,int c)
{
	int result = 1;

	if (c=='[') {
		state->proc = TermState_CSI;
	}
	else if (c==']') {
		state->proc = TermState_XTerm;
	}
	else if (c=='=') {
		result = EvalCmdInt(state,TERM_APP_KEYPAD,1);
	}
	else if (c=='>') {
		result = EvalCmdInt(state,TERM_APP_KEYPAD,0);
	}
	else if (c=='7') {
		result = EvalCmd(state,TERM_SAVE_CURSOR);
	}
	else if (c=='8') {
		result = EvalCmd(state,TERM_RESTORE_CURSOR);
	}
	else if (c=='H') {
		result = EvalCmdInt(state,TERM_SET_TAB,1);
	}
	else if (c=='M') {
		result = EvalCmd(state,TERM_MOVE_UP);
	}
	else if (c=='c') {
		result = EvalCmd(state,TERM_RESET);
	}
	else if (c=='(') {
		state->int_params[0] = 0;
		state->proc = TermState_Charset;
	}
	else if (c==')') {
		state->int_params[0] = 1;
		state->proc = TermState_Charset;
	}
	else if (c=='*') {
		state->int_params[0] = 2;
		state->proc = TermState_Charset;
	}
	else if (c=='+') {
		state->int_params[0] = 3;
		state->proc = TermState_Charset;
	}
	else {
		return 0;
	}
	return result;
}

static int TermState_Unexpected(TermState *state,int c)
{
	(*state->eval_procs.unexpected_proc)(state->eval_data,c);
	return 1;
}

static int TermState_TopLevel(TermState *state,int c);

static int TermState_Reset(TermState *state,int c)
{
	assert(state->cont_stack_pos==0);
	state->int_count = 0;
	state->alt_stack_pos = 0;
	if (state->string_param) {
		free(state->string_param);
		state->string_param = 0;
	}
	TermState_PushAlternative(state,TermState_Unexpected);
	TermState_PushContinuation(state,TermState_Reset);
	return TermState_TopLevel(state,c);
}

static void TermState_Flush(TermState *state)
{
	if (state->outbufpos>0) {
		state->eval_procs.text_proc(
				state->eval_data,(char *)state->outbuf,state->outbufpos
		);
		state->outbufpos = 0;
	}
}

static int TermState_TopLevel(TermState *state,int c)
{
	if (c>=0x20) {
		/* This is usually handled by TermState_AddChar directly, but if */
		/* we failed to recognize a control sequence, we can end up back here */
		/* with normal characters */
		if (state->outbufpos>=(int)ELEMENTS(state->outbuf))
			TermState_Flush(state);
		state->outbuf[state->outbufpos++] = c;
		return 1;
	}

	TermState_Flush(state);

	{
		int result = 1;

		switch (c) {
		case 7:
			result = EvalCmd(state,TERM_BELL);
			break;
		case 8:
			result = EvalCmd(state,TERM_BACKSPACE);
			break;
		case 9:
			result = EvalCmd(state,TERM_TAB);
			break;
		case 0xa:
			result = EvalCmd(state,TERM_MOVE_DOWN);
			break;
		case 0xd:
			result = EvalCmd(state,TERM_CARRIAGE_RETURN);
			break;
		case 0xe:
			result = EvalCmdInt(state,TERM_ALT_CHARSET,1);
			break;
		case 0xf:
			result = EvalCmdInt(state,TERM_ALT_CHARSET,0);
			break;
		case 0x1b:
			TermState_PushAlternative(state,TermState_Unexpected);
			state->proc = TermState_Esc;
			break;
		default:
			result = 0;
		}
		if (!result) {
			TermState_Unexpected(state,c);
			result = 1;
		}
		return result;
	}
}

void
TermState_Init(
		TermState *state,CommandHandlers *cmd_handlers_ptr,void *client_data
)
{
	static TermState default_state;
	*state = default_state;
	state->proc = TermState_Reset;
	state->eval_procs = *cmd_handlers_ptr;
	state->eval_data = client_data;
	state->outbufpos = 0;
	state->string_param = 0;
	assert(state->proc);
}

void TermState_EOF(TermState *state)
{
	if (state->proc!=TermState_Reset) {
		TermState_Fail(state);
	}
	TermState_Flush(state);
	EvalCmd(state,TERM_EOF);
}

void TermState_BufferEmpty(TermState *state)
{
	TermState_Flush(state);
	EvalCmd(state,TERM_BUFFER_EMPTY);
}

#if 0
//	http://ascii-table.com/ansi-escape-sequences-vt-100.php
static void debug_AddChars(TermState *state,const char *chars,int count)
	{
	fprintf(stderr, "debug_AddChars: count:%d, state->int_count:%d, state->int_params:%d chars:[", count, state->int_count, state->int_params[0]);
	for(; count > 0; count--, chars++)
		{
		int ch = *chars;
		if(ch == '<' || ch == '>')
			fprintf(stderr, "%c%c", ch, ch);
		else if(ch >= 32 && ch <= 127)
			fprintf(stderr, "%c", ch);
		else
			fprintf(stderr, "<%X>", ch);
		}
	fprintf(stderr, "]\n");
	}
#endif

void TermState_AddChars(TermState *state,const char *chars,int count)
{
	int charnum = 0;

	//debug_AddChars(state, chars, count);
	for (;charnum<count;++charnum) {
		int c = (unsigned char)chars[charnum];
		assert(state->bufpos>=0 && state->bufpos<(int)ELEMENTS(state->buffer));
		assert(state->proc);

		if (state->proc==TermState_Reset && c>=0x20) {
			if (state->outbufpos>=(int)ELEMENTS(state->outbuf))
				TermState_Flush(state);
			state->outbuf[state->outbufpos++] = c;
		}
		else {
			state->buffer[state->bufpos] = c;
			state->bufend = (state->bufpos+1) % ELEMENTS(state->buffer);
			while (state->bufpos!=state->bufend) {
				TermStateProc *proc = state->proc;
				unsigned char c = state->buffer[state->bufpos];

				state->bufpos = (state->bufpos+1) % ELEMENTS(state->buffer);
				assert(state->bufpos>=0);
				assert(proc);
				state->proc = 0;
				if (!proc(state,c)) {
					assert(state->alt_stack_pos>0);
					TermState_Fail(state);
				}
				else {
					if (!state->proc) {
						TermState_Continue(state);
					}
				}
				assert(state->proc);
			}
			assert(state->proc);
		}
	}
}

const char *TermState_OpName(TermOp_t op)
{
	static struct {
		TermOp_t op;
		const char *name;
	} op_names[] = {
			{TERM_INVALID_OP,"TERM_INVALID_OP"},
			{TERM_APP_CURSOR_KEYS,"TERM_APP_CURSOR_KEYS"},
			{TERM_AUTOWRAP,"TERM_AUTOWRAP"},
			{TERM_SET_SCREEN,"TERM_SET_SCREEN"},
			{TERM_CURSOR_VISIBLE,"TERM_CURSOR_VISIBLE"},
			{TERM_MOUSE_REPORTING,"TERM_MOUSE_REPORTING"},
			{TERM_MOUSE_HIGHLIGHTING,"TERM_MOUSE_HIGHLIGHTING"},
			{TERM_HOME_CURSOR,"TERM_HOME_CURSOR"},
			{TERM_SET_ROW,"TERM_SET_ROW"},
			{TERM_SET_COL,"TERM_SET_COL"},
			{TERM_SET_CURSOR,"TERM_SET_CURSOR"},
			{TERM_CLEAR_BELOW,"TERM_CLEAR_BELOW"},
			{TERM_CLEAR_ABOVE,"TERM_CLEAR_ABOVE"},
			{TERM_CLEAR_TO_RIGHT,"TERM_CLEAR_TO_RIGHT"},
			{TERM_CLEAR_TO_LEFT,"TERM_CLEAR_TO_LEFT"},
			{TERM_CLEAR_SCREEN,"TERM_CLEAR_SCREEN"},
			{TERM_CLEAR_LINE,"TERM_CLEAR_LINE"},
			{TERM_INSERT_LINE,"TERM_INSERT_LINE"},
			{TERM_DELETE_LINE,"TERM_DELETE_LINE"},
			{TERM_INSERT_CHAR,"TERM_INSERT_CHAR"},
			{TERM_ERASE_CHAR,"TERM_ERASE_CHAR"},
			{TERM_DELETE_CHAR,"TERM_DELETE_CHAR"},
			{TERM_NORM_ATTR,"TERM_NORM_ATTR"},
			{TERM_BOLD_ATTR,"TERM_BOLD_ATTR"},
			{TERM_DIM_ATTR,"TERM_DIM_ATTR"},
			{TERM_ITALIC_ATTR,"TERM_ITALIC_ATTR"},
			{TERM_UNDERLINE_ATTR,"TERM_UNDERLINE_ATTR"},
			{TERM_BLINK_ATTR,"TERM_BLINK_ATTR"},
			{TERM_INVERSE_ATTR,"TERM_INVERSE_ATTR"},
			{TERM_FGCOLOR_ATTR,"TERM_FGCOLOR_ATTR"},
			{TERM_BGCOLOR_ATTR,"TERM_BGCOLOR_ATTR"},
			{TERM_APP_KEYPAD,"TERM_APP_KEYPAD"},
			{TERM_SAVE_CURSOR,"TERM_SAVE_CURSOR"},
			{TERM_RESTORE_CURSOR,"TERM_RESTORE_CURSOR"},
			{TERM_SET_TAB,"TERM_SET_TAB"},
			{TERM_CLEAR_TABS,"TERM_CLEAR_TABS"},
			{TERM_TEXT,"TERM_TEXT"},
			{TERM_UNEXPECTED,"TERM_UNEXPECTED"},
			{TERM_BELL,"TERM_BELL"},
			{TERM_BACKSPACE,"TERM_BACKSPACE"},
			{TERM_TAB,"TERM_TAB"},
			{TERM_MOVE_UP,"TERM_MOVE_UP"},
			{TERM_MOVE_DOWN,"TERM_MOVE_DOWN"},
			{TERM_CURSOR_UP,"TERM_CURSOR_UP"},
			{TERM_CURSOR_DOWN,"TERM_CURSOR_DOWN"},
			{TERM_CURSOR_RIGHT,"TERM_CURSOR_RIGHT"},
			{TERM_CURSOR_LEFT,"TERM_CURSOR_LEFT"},
			{TERM_CARRIAGE_RETURN,"TERM_CARRIAGE_RETURN"},
			{TERM_STANDARD_CHARSET,"TERM_STANDARD_CHARSET"},
			{TERM_ALT_CHARSET,"TERM_ALT_CHARSET"},
			{TERM_SET_SCROLL_REGION,"TERM_SET_SCROLL_REGION"},
			{TERM_SET_FONT_STYLE,"TERM_SET_FONT_STYLE"},
			{TERM_PRIMARY_FONT,"TERM_PRIMARY_FONT"},
			{TERM_ALTERNATE_FONT1,"TERM_ALTERNATE_FONT1"},
			{TERM_ALTERNATE_FONT2,"TERM_ALTERNATE_FONT2"},
			{TERM_INSERT_MODE,"TERM_INSERT_MODE"},
			{TERM_SET_TITLE,"TERM_SET_TITLE"},
			{TERM_REQ_ATTR, "TERM_REQ_ATTR"},
			{TERM_BUFFER_EMPTY,"TERM_BUFFER_EMPTY"},
			{TERM_EOF,"TERM_EOF"},
			{TERM_RESET,"TERM_RESET"},
			{TERM_INVALID_OP,0},
	};


	int i = 0;

	for (;op_names[i].name;++i) {
		if (op_names[i].op==op) {
			return op_names[i].name;
		}
	}
	assert(0);
	return 0;
}
