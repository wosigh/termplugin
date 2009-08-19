/********************************************************/
/* Gsh Copyright (C) 1999 Vaughn Cato                   */
/* See the file "LICENSE", included with this software, */
/* for license information.                             */
/********************************************************/
#ifndef TERMSTATE_H_
#define TERMSTATE_H_

#define TERM_VT100_ANS "\033[?1;2c"

typedef struct TermState_s TermState;
typedef int (TermStateProc)(TermState *,int);

typedef enum {
	TERM_INVALID_OP,
	TERM_APP_CURSOR_KEYS,
	TERM_AUTOWRAP,
	TERM_SET_SCREEN,
	TERM_CURSOR_VISIBLE,
	TERM_MOUSE_REPORTING,
	TERM_MOUSE_HIGHLIGHTING,
	TERM_HOME_CURSOR,
	TERM_SET_ROW,
	TERM_SET_COL,
	TERM_SET_CURSOR,
	TERM_CLEAR_BELOW,
	TERM_CLEAR_ABOVE,
	TERM_CLEAR_TO_RIGHT,
	TERM_CLEAR_TO_LEFT,
	TERM_CLEAR_SCREEN,
	TERM_CLEAR_LINE,
	TERM_INSERT_LINE,
	TERM_DELETE_LINE,
	TERM_INSERT_CHAR,
	TERM_ERASE_CHAR,
	TERM_DELETE_CHAR,
	TERM_NORM_ATTR,
	TERM_BOLD_ATTR,
	TERM_DIM_ATTR,
	TERM_ITALIC_ATTR,
	TERM_UNDERLINE_ATTR,
	TERM_BLINK_ATTR,
	TERM_INVERSE_ATTR,
	TERM_FGCOLOR_ATTR,
	TERM_BGCOLOR_ATTR,
	TERM_APP_KEYPAD,
	TERM_SAVE_CURSOR,
	TERM_RESTORE_CURSOR,
	TERM_SET_TAB,
	TERM_CLEAR_TABS,
	TERM_TEXT,
	TERM_UNEXPECTED,
	TERM_BELL,
	TERM_BACKSPACE,
	TERM_TAB,
	TERM_MOVE_UP,     /* Move cursor up, scroll screen down if at the top */
	TERM_MOVE_DOWN,   /* Move cursor down, scroll screen up if at the bottom */
	TERM_CURSOR_UP,   /* Move cursor up, don't scroll */
	TERM_CURSOR_DOWN, /* Move cursor down, don't scroll */
	TERM_CURSOR_RIGHT,
	TERM_CURSOR_LEFT,
	TERM_CARRIAGE_RETURN,
	TERM_STANDARD_CHARSET,
	TERM_ALT_CHARSET,
	TERM_SET_SCROLL_REGION,
	TERM_SET_FONT_STYLE,
	TERM_PRIMARY_FONT,
	TERM_ALTERNATE_FONT1,
	TERM_ALTERNATE_FONT2,
	TERM_INSERT_MODE,
	TERM_SET_TITLE,
	TERM_REQ_ATTR,
	/* Request terminal attributes.  Send back TERM_VT100_ANS */
	TERM_BUFFER_EMPTY,
	TERM_EOF,
	TERM_RESET,
} TermOp_t;

typedef enum {
	TERM_DEC_SPECIAL_STYLE,
	TERM_UK_STYLE,
	TERM_USASCII_STYLE,
	TERM_MULTINATIONAL_STYLE,
	TERM_FINNISH_STYLE,
	TERM_GERMAN_STYLE
} TermFontStyle_t;

typedef struct CommandHandlers_s CommandHandlers;

struct CommandHandlers_s {
	int (*void_proc)(void *,TermOp_t op);
	int (*int_proc)(void *,TermOp_t op,int arg1);
	int (*int_int_proc)(void *,TermOp_t op,int arg1,int arg2);
	int (*string_proc)(void *,TermOp_t op,const char *arg);
	void (*text_proc)(void *,const char *,int);
	void (*unexpected_proc)(void *,int);
};

struct TermState_s {
	void *eval_data;
	CommandHandlers eval_procs;
	TermStateProc *proc;
	int bufpos;
	int bufend;
	int alt_stack_pos;
	int bufpos_stack[10];
	int int_params[10];
	char priv;
	int int_count;
	unsigned char buffer[32];
	TermStateProc *alt_stack[10];
	int cont_stack_pos_stack[10];
	int cont_stack_pos;
	TermStateProc *cont_stack[10];
	unsigned char outbuf[256];
	int outbufpos;
	char *string_param;
	int string_param_len;
	int string_param_size;
};

extern void
TermState_Init(TermState *state,CommandHandlers *eval_procs,void *eval_data);
extern void TermState_AddChars(TermState *state,const char *buffer,int count);
extern void TermState_EOF(TermState *state);
extern void TermState_BufferEmpty(TermState *state);
extern const char *TermState_OpName(TermOp_t op);

#endif /* TERMSTATE_H_ */
