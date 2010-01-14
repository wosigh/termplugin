
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Josh Aas <josh@mozilla.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include <sys/time.h>

#include "termplugin.h"
#include "nppalmdefs.h"
#include "pixelbuffer.hpp"
#include "charbuffer.hpp"
#include "spawn.h"
#include "keyman.hpp"
#include "screen.hpp"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <pthread.h>
#include <algorithm>
#include "common.h"
#include "api.h"
using namespace std;

#define PLUGIN_NAME        "Term Plug-in"
#define PLUGIN_DESCRIPTION PLUGIN_NAME " (Mozilla SDK)"
#define PLUGIN_VERSION     "1.0.0.0"
#define PLUGIN_MIMEDESCRIPTION "application/x-webosinternals-termplugin:trm:Term plugin"

NPNetscapeFuncs NPNFuncs;

typedef struct
{
	const char *key;
	int id;
	int minArgs;
	const char *argsDesc;
}SUPPORTED_OBJECTS;


double CurrentTime()
{
	struct timeval tv;
	gettimeofday(&tv, 0);
	return tv.tv_sec + tv.tv_usec/1000000.0;
}

#ifdef DEBUG_OUTPUT
static void logTimeStamp(FILE *out, int line)
{
	struct tm *t;
	time_t now;

	if(!out)
		return;
	time(&now);
	t = localtime(&now);

	my_fprintf(out, "%.4d-%.2d-%.2d %.2d:%.2d:%.2d	%d	term: ",
			t->tm_year + 1900,
			t->tm_mon + 1,
			t->tm_mday,
			t->tm_hour,
			t->tm_min,
			t->tm_sec, line);
}
#endif
#define debugLogLine(x)								debugLog(__LINE__, x)
#if 0
static void debugLogArgv(int line, int argc, char **argn, char **argv, const char *msg)
{
	int j;
	const char *prefix = "args =";
#if !defined(__STDERR_NO_GOOD__)
	FILE *out = stderr;
#else
	FILE *out = fopen("/tmp/termPluginOutput.log", "a");
	if(!out)
		return;
#endif
	logTimeStamp(out, line);
	my_fprintf(out, "%s(", msg);
	for(j = 0; argc > 0; argc--, argv++, argn++, j++, prefix = ",")
		my_fprintf(out, "%s %d:%s[%s]", prefix, j, *argn, *argv);
	my_fprintf(out, ")\n");
#if defined(__STDERR_NO_GOOD__)
	if(out != stderr)
		fclose(out);
#endif
}
#endif


#include <stdarg.h>
static void debugLog(int line, const char *format, ...)
{
#ifdef DEBUG_OUTPUT
#if !defined(__STDERR_NO_GOOD__)
	FILE *out = stderr;
#else
	FILE *out = fopen("/tmp/termPluginOutput.log", "a");
	if(!out)
		return;
#endif
	logTimeStamp(out, line);
	va_list(arglist);
	va_start(arglist, format);
	vfprintf(out, format, arglist);
	my_fprintf(out, "\n");
#if defined(__STDERR_NO_GOOD__)
	if(out != stderr)
		fclose(out);
#endif
#endif
}

static const char *NPVariantTypeStr(NPVariantType type)
	{
	switch(type)
		{
		case NPVariantType_Void:		return "Void";
		case NPVariantType_Null:		return "Null";
		case NPVariantType_Bool:		return "Bool";
		case NPVariantType_Int32:		return "Int32";
		case NPVariantType_Double:		return "Double";
		case NPVariantType_String:		return "String";
		case NPVariantType_Object:		return "Object";
		}
	return "unknown";
	}

#if 0
void debugLogVariant(int line, NPVariant *var, bool ok, const char *msg)
{
	if(ok)
	{
		if(NPVARIANT_IS_INT32(*var))
			debugLog(line, "%s = %d val:%d", msg, ok, NPVARIANT_TO_INT32(*var));
		else if(NPVARIANT_IS_DOUBLE(*var))
			debugLog(line, "%s = %d val:%f", msg, ok, NPVARIANT_TO_DOUBLE(*var));
		else if(NPVARIANT_IS_BOOLEAN(*var))
			debugLog(line, "%s = %d val:%s", msg, ok, NPVARIANT_TO_BOOLEAN(*var) ? "true" : "false");
		else if(NPVARIANT_IS_STRING(*var))
			debugLog(line, "%s = %d val:[%s]", msg, ok, NPVARIANT_TO_STRING(*var).UTF8Characters);
		else if(NPVARIANT_IS_OBJECT(*var))
			debugLog(line, "%s = %d val:%p", msg, ok, NPVARIANT_TO_OBJECT(*var));
		else
			debugLog(line, "%s = %d type:%s", msg, ok, NPVariantTypeStr(var->type));
	}
	else
		debugLog(line, "%s = %d", msg, ok);
}
#endif

struct MyObject : NPObject, KeyManager::Client {
	NPP instance;
	KeyManager key_manager;
	int event_x;
	int event_y;
	int terminalHeight;
	Screen screen;
	SpawnData *spawn_data;
	pthread_t select_thread;
	bool select_thread_started;
	pthread_mutex_t draw_mutex;
	pthread_cond_t drawn_cond;
	NPObject *scroller;
	NPObject *keyStatesParentObj;
	bool showKeyStates;
	bool drawEnabled;

	MyObject(NPP i);
	~MyObject();

	void checkSpawn();

	void sendChars(const char *buf,int len)
	{
#ifdef DEBUG_OUTPUT
		{
			my_fprintf(stderr,"Sending");
			int i = 0;
			for (;i<len;++i) {
				my_fprintf(stderr," %d",buf[i]);
			}
			my_fprintf(stderr,"\n");
		}
#endif
		Spawn_Send(spawn_data,buf,len);
	}

	static CommandHandlers term_handlers;

	static int _termVoidOp(void *obj,TermOp_t op)
	{
		((MyObject *)obj)->_termVoid(op);
		return 1;
	}

	static int _termIntOp(void *obj,TermOp_t op,int arg1)
	{
		((MyObject *)obj)->_termInt(op,arg1);
		return 1;
	}

	static int _termIntIntOp(void *obj,TermOp_t op,int arg1,int arg2)
	{
		((MyObject *)obj)->_termIntInt(op,arg1,arg2);
		return 1;
	}

	static int _termStringOp(void *,TermOp_t op,const char *arg)
	{
		my_fprintf(stderr,"term: string op\n");
		return 1;
	}

	static void _termTextOp(void *obj_ptr,const char *buf,int n)
	{
		MyObject *myobj = (MyObject *)obj_ptr;
		myobj->_termText(buf,n);
		my_fprintf(stderr, "myobj->scroller:%p\n", myobj->scroller);
		if(myobj->scroller)
			scrollBottom(myobj->instance, myobj->scroller);
	}

	static void _termUnexpected(void *,int c)
	{
		my_fprintf(stderr,"term: unexpected:%d\n", c);
	}

	void _termVoid(TermOp_t op);
	void _termInt(TermOp_t op,int arg1);
	void _termIntInt(TermOp_t op,int arg1,int arg2);
	void _termText(const char *buf,int n);
	void startSelectThread();
	void stopSelectThread();
	static void *selectThread(void *);
	void runSelect();
	static void masterReady(void *obj_ptr);
	void start(char *user);
};

MyObject::MyObject(NPP i)
: instance(i),
  key_manager(this),
  event_x(0),
  event_y(0),
  spawn_data(0),
  select_thread_started(false),
  scroller(0),
  keyStatesParentObj(0),
  showKeyStates(true),
  drawEnabled(true)
{
	term_handlers.void_proc = MyObject::_termVoidOp;
	term_handlers.int_proc = MyObject::_termIntOp;
	term_handlers.int_int_proc = MyObject::_termIntIntOp;
	term_handlers.string_proc = MyObject::_termStringOp;
	term_handlers.text_proc = MyObject::_termTextOp;
	term_handlers.unexpected_proc = MyObject::_termUnexpected;
}

MyObject::~MyObject()
{
	if (select_thread_started) {
		stopSelectThread();
	}
	Spawn_Kill(spawn_data,"HUP");
	Spawn_CheckChildExit(spawn_data,1);
	Spawn_Delete(spawn_data);
	spawn_data = 0;
	if(scroller)
		NPNFuncs.releaseobject(scroller);
	if(keyStatesParentObj)
		NPNFuncs.releaseobject(keyStatesParentObj);
}

void MyObject::start(char *user)
{
	spawn_data = Spawn_Open(&term_handlers,this);
	if (spawn_data) {
		const char *argv[] = {"/bin/login","-f",user, 0};
		Spawn_Exec(spawn_data,(char **)argv);
		startSelectThread();
	}
	free(user);
}

void MyObject::masterReady(void *obj_ptr)
{
	my_fprintf(stderr,"term: masterReady: forcing redraw\n");
	NPN_ForceRedraw(((MyObject *)obj_ptr)->instance);
}

void MyObject::runSelect()
{
	my_fprintf(stderr,"In select thread.\n");
	for (;;) {
		Spawn_WaitMaster(spawn_data);
#if 0
		my_fprintf(stderr,"term: calling masterReady\n");
		my_fprintf(stderr,"term: func=%p\n",NPNFuncs.pluginthreadasynccall);
		my_fprintf(stderr,"term: instance=%p\n",instance);
		NPNFuncs.pluginthreadasynccall(instance,MyObject::masterReady,this);
#else
		pthread_mutex_lock(&draw_mutex);
		fprintf(stderr,"Forcing redraw\n");
		NPN_ForceRedraw(instance);
		fprintf(stderr,"Waiting for redraw\n");
		pthread_cond_wait(&drawn_cond,&draw_mutex);
		fprintf(stderr,"Done redraw\n");
		pthread_mutex_unlock(&draw_mutex);
#endif
	}
}

void *MyObject::selectThread(void *obj_ptr)
{
	((MyObject *)obj_ptr)->runSelect();
	return 0;
}

void MyObject::startSelectThread()
{
	pthread_mutex_init(&draw_mutex,NULL);
	pthread_cond_init(&drawn_cond,NULL);
	int rc = pthread_create(&select_thread,NULL,MyObject::selectThread,this);

	if (rc!=0) {
		my_fprintf(stderr,"Failed to create select thread.\n");
	}
	else {
		my_fprintf(stderr,"Created select thread\n");
		select_thread_started = true;
	}
}


void MyObject::stopSelectThread()
{
	pthread_cancel(select_thread);
	void *thread_return = 0;
	pthread_join(select_thread,&thread_return);
	my_fprintf(stderr,"Select thread terminated\n");
	select_thread_started = false;
}


void MyObject::_termText(const char *buf,int n)
{
	Screen::CharBuf &char_buf = screen.charBuf();

#ifdef DEBUG_OUTPUT
	{
		my_fprintf(stderr,"_termText: '");
		int i = 0;
		for (;i!=n;++i) {
			my_fprintf(stderr,"%c",buf[i]);
		}
		my_fprintf(stderr,"'\n");
	}
#endif
	int i = 0;
	for (;i!=n;++i) {
		char_buf.put(buf[i]);
		char_buf.advanceCursor();
		//my_fprintf(stderr,"cursor at %d,%d\n",char_buf.cursor().row(),char_buf.cursor().col());
	}
}


void MyObject::_termVoid(TermOp_t op)
{
	Screen::CharBuf &char_buf = screen.charBuf();

	my_fprintf(stderr,"_termVoid: %s\n",TermState_OpName(op));
	switch (op) {
	case TERM_HOME_CURSOR:
		char_buf.homeCursor();
		break;
	case TERM_CLEAR_BELOW:
		char_buf.clearToRight();
		char_buf.clearBelow();
		break;
	case TERM_CLEAR_ABOVE:
		break;
	case TERM_CLEAR_TO_RIGHT:
		char_buf.clearToRight();
		break;
	case TERM_CLEAR_TO_LEFT:
		break;
	case TERM_CLEAR_SCREEN:
		break;
	case TERM_CLEAR_LINE:
		break;
	case TERM_NORM_ATTR:
		char_buf.resetAttr();
		break;
	case TERM_RESET:
		char_buf.reset();
		break;
	case TERM_SAVE_CURSOR:
		break;
	case TERM_RESTORE_CURSOR:
		break;
	case TERM_BELL:
		break;
	case TERM_BACKSPACE:
		char_buf.backSpaceCursor();
		break;
	case TERM_TAB:
		break;
	case TERM_CLEAR_TABS:
		break;
	case TERM_CARRIAGE_RETURN:
		char_buf.carriageReturn();
		break;
	case TERM_MOVE_UP:
		break;
	case TERM_MOVE_DOWN:
		char_buf.moveCursorDown();
		break;
	case TERM_EOF:
		break;
	case TERM_BUFFER_EMPTY:
		break;
	case TERM_REQ_ATTR:
		break;
	case TERM_PRIMARY_FONT:
		break;
	case TERM_ALTERNATE_FONT1:
		break;
	case TERM_ALTERNATE_FONT2:
		break;
	default:
		assert(0);
	}
}


void MyObject::_termInt(TermOp_t op,int arg1)
{
	Screen::CharBuf &char_buf = screen.charBuf();

	my_fprintf(stderr,"_termInt: %s %d\n",TermState_OpName(op),arg1);
	switch (op) {
	case TERM_CURSOR_UP:
		char_buf.moveCursorUp(arg1);
		break;
	case TERM_CURSOR_DOWN:
		char_buf.moveCursorDown(arg1);
		break;
	case TERM_CURSOR_RIGHT:
		char_buf.moveCursorRight(arg1);
		break;
	case TERM_CURSOR_LEFT:
		char_buf.moveCursorLeft(arg1);
		break;
	case TERM_INSERT_LINE:
		break;
	case TERM_DELETE_LINE:
		break;
	case TERM_INSERT_CHAR:
		break;
	case TERM_ERASE_CHAR:
		break;
	case TERM_DELETE_CHAR:
		break;
	case TERM_FGCOLOR_ATTR:
		char_buf.setFgColor(arg1);
		break;
	case TERM_BGCOLOR_ATTR:
		char_buf.setBgColor(arg1);
		break;
	case TERM_BOLD_ATTR:
		char_buf.setFlag(eBold, arg1);
		break;
	case TERM_DIM_ATTR:
		char_buf.setFlag(eDim, arg1);
		break;
	case TERM_ITALIC_ATTR:
		char_buf.setFlag(eItalics, arg1);
		break;
	case TERM_UNDERLINE_ATTR:
		char_buf.setFlag(eUnderline, arg1);
		break;
	case TERM_BLINK_ATTR:
		char_buf.setFlag(eBlink, arg1);
		break;
	case TERM_INVERSE_ATTR:
		char_buf.setFlag(eInverse, arg1);
		break;
	case TERM_APP_CURSOR_KEYS:
		key_manager.setAppCursorKeys(arg1);
		break;
	default:
		break;
	}
}


void MyObject::_termIntInt(TermOp_t op,int arg1,int arg2)
{
	Screen::CharBuf &char_buf = screen.charBuf();

	my_fprintf(stderr,"_termIntInt: %s %d %d\n",TermState_OpName(op),arg1,arg2);

	switch (op) {
	case TERM_SET_CURSOR:
		char_buf.setCursor(arg1,arg2);
		break;
	case TERM_SET_SCROLL_REGION:
		char_buf.setScrollRegion(arg1,arg2);
		break;
	default:
		break;
	}
}


CommandHandlers MyObject::term_handlers;

void MyObject::checkSpawn()
{
	if (spawn_data) {
		if (!Spawn_ReadMaster(spawn_data)) {
			spawn_data = 0;
		}
	}
}

struct InstanceData {
	//  NPP npp;
	//  NPWindow window;
	MyObject *object;
	InstanceData() : object(0)
	{
	}
};

void logmsgv(const char* desc, NPPVariable variable)
{
	my_fprintf(stderr, "term: %s - %s (#%i)\n", desc, varname(variable), variable);
}

static void
fillPluginFunctionTable(NPPluginFuncs* pFuncs)
{
	pFuncs->version = 11;
	pFuncs->size = sizeof(*pFuncs);
	pFuncs->newp = NPP_New;
	pFuncs->destroy = NPP_Destroy;
	pFuncs->setwindow = NPP_SetWindow;
	pFuncs->newstream = NPP_NewStream;
	pFuncs->destroystream = NPP_DestroyStream;
	pFuncs->asfile = NPP_StreamAsFile;
	pFuncs->writeready = NPP_WriteReady;
	pFuncs->write = NPP_Write;
	pFuncs->print = NPP_Print;
	pFuncs->event = NPP_HandleEvent;
	pFuncs->urlnotify = NPP_URLNotify;
	pFuncs->getvalue = NPP_GetValue;
	pFuncs->setvalue = NPP_SetValue;
}

NP_EXPORT(NPError)
NP_Initialize(NPNetscapeFuncs* bFuncs, NPPluginFuncs* pFuncs)
{
	my_fprintf(stderr,"term: NP_Initialize\n");
	my_fprintf(stderr,"term: bFuncs->size=%d\n",bFuncs->size);
	my_fprintf(stderr,"term: sizeof(NPNetscapeFuncs)=%d\n",sizeof(NPNetscapeFuncs));

	NPNFuncs = *bFuncs;

	fillPluginFunctionTable(pFuncs);

	return NPERR_NO_ERROR;
}

NP_EXPORT(char*)
NP_GetPluginVersion()
{
	my_fprintf(stderr, "term: NP_GetPluginVersion\n");
	return (char *)PLUGIN_VERSION;
}

NP_EXPORT(char*)
NP_GetMIMEDescription()
{
	my_fprintf(stderr,"term: NP_GetMIMEDescription\n");
	return (char *)PLUGIN_MIMEDESCRIPTION;
}

NPObject *NPN_CreateObject(NPP npp, NPClass *aClass)
{
	return NPNFuncs.createobject(npp, aClass);
}

void NPN_ForceRedraw(NPP npp)
{
	return NPNFuncs.forceredraw(npp);
}

bool NPN_IdentifierIsString(NPIdentifier identifier)
{
	return NPNFuncs.identifierisstring(identifier);
}

NPUTF8 *NPN_UTF8FromIdentifier(NPIdentifier identifier)
{
	return NPNFuncs.utf8fromidentifier(identifier);
}

static NPObject *Object_Allocate(NPP npp,NPClass *aClass)
{
        MyObject *obj = new MyObject(npp);
//	return new MyObject(npp);
	my_fprintf(stderr,"term: created object at at %p\n",&obj);
	return obj;
}

static void Object_Deallocate(NPObject *npobj)
{
	my_fprintf(stderr,"term: In Object_Deallocate\n");
	my_fprintf(stderr,"term: was asked to delete object at %p\n",&npobj);
	delete (MyObject *)npobj;
	my_fprintf(stderr,"term: exiting Object_Deallocate\n");
}

static bool
Object_Construct(
		NPObject *npobj,
		const NPVariant *args,
		uint32_t argCount,
		NPVariant *result
)
{
	my_fprintf(stderr,"In Object_Construct\n");
	return false;
}

static const SUPPORTED_OBJECTS *ContainsName(const char *callerName, const SUPPORTED_OBJECTS *list, NPIdentifier name)
{
	my_fprintf(stderr,"%22s: ", callerName);
	bool is_string = NPN_IdentifierIsString(name);
	if (!is_string) {
		fprintf(stderr,"-- Identifier is not a string.\n");
		return NULL;
	}
	const char *name_str = NPN_UTF8FromIdentifier(name);
	my_fprintf(stderr, " Identifier: %s",name_str);
	for (const SUPPORTED_OBJECTS *me = list; me->key; me++)
	{
		if(strcmp(name_str, me->key))
			continue;
		if(me->minArgs >= 0)
			my_fprintf(stderr, "(%s)\n", me->argsDesc);
		else
			my_fprintf(stderr, "\n");
		return me;
	}
	my_fprintf(stderr, " -- NOT found\n");
	return NULL;
}


#ifdef DEBUG_OUTPUT
static void PrintValue(const NPVariant *arg)
{
	if (NPVARIANT_IS_VOID(*arg)) {
		my_fprintf(stderr,"void");
	}
	else if (NPVARIANT_IS_NULL(*arg)) {
		my_fprintf(stderr,"null");
	}
	else if (NPVARIANT_IS_BOOLEAN(*arg)) {
		if (NPVARIANT_TO_BOOLEAN(*arg)) {
			my_fprintf(stderr,"true");
		}
		else {
			my_fprintf(stderr,"false");
		}
	}
	else if (NPVARIANT_IS_INT32(*arg)) {
		my_fprintf(stderr,"%d",NPVARIANT_TO_INT32(*arg));
	}
	else if (NPVARIANT_IS_DOUBLE(*arg)) {
		my_fprintf(stderr,"%g",NPVARIANT_TO_DOUBLE(*arg));
	}
	else if (NPVARIANT_IS_STRING(*arg)) {
		NPString value = NPVARIANT_TO_STRING(*arg);
		const NPUTF8 *chars = value.UTF8Characters;
		uint32_t len = value.UTF8Length;
		my_fprintf(stderr,"\"");
		{
			uint32_t i = 0;
			for (;i!=len;++i) {
				my_fprintf(stderr,"%c",chars[i]);
			}
		}
		my_fprintf(stderr,"\"");
	}
	else if (NPVARIANT_IS_OBJECT(*arg)) {
		my_fprintf(stderr,"object");
	}
	else {
		my_fprintf(stderr,"unknown");
	}
}
#endif


static void PrintArgs(const NPVariant *args,int argCount)
{
#ifdef DEBUG_OUTPUT
	my_fprintf(stderr,"(");
	int argnum = 0;
	for (;argnum!=argCount;++argnum) {
		if (argnum!=0) {
			my_fprintf(stderr,",");
		}
		PrintValue(&args[argnum]);
	}
	my_fprintf(stderr,")");
#endif
}

static char* StringValue(const NPVariant *arg)
{
	if (NPVARIANT_IS_STRING(*arg)) {
		NPString value = NPVARIANT_TO_STRING(*arg);
		char* shell = (char*)calloc(value.UTF8Length + 1,sizeof(char));
		memcpy(shell, value.UTF8Characters, value.UTF8Length);
		return shell;
	}
	return NULL;
}


int IntValue(const NPVariant *value)
{
	if (NPVARIANT_IS_INT32(*value)) {
		int32_t value_int32 = NPVARIANT_TO_INT32(*value);
		return value_int32;
	}
	else if (NPVARIANT_IS_DOUBLE(*value)) {
		double value_double = NPVARIANT_TO_DOUBLE(*value);
		return (int)value_double;
	}
	else if (NPVARIANT_IS_BOOLEAN(*value)) {
		bool value_bool = NPVARIANT_TO_BOOLEAN(*value);
		return (int)value_bool;
	}
	else if (NPVARIANT_IS_STRING(*value)) {
		char *value_chars = StringValue(value);
		my_fprintf(stderr,"String is %s\n",value_chars);
		int ret = atoi(value_chars);
		free(value_chars);
		return ret;
	}
	else {
		my_fprintf(stderr,"Expecting int, but got type:%s\n",NPVariantTypeStr(value->type));
		return 0;
	}
}

#if 0
static bool BoolValue(const NPVariant *value)
{
	if (NPVARIANT_IS_BOOLEAN(*value)) {
		bool value_bool = NPVARIANT_TO_BOOLEAN(*value);
		return value_bool;
	}
	else {
		return IntValue(value)!=0;
	}
}
#endif

typedef enum
{
	//eMethod_sendEnter,
	eMethod_setTerminalHeight,
	eMethod_setFont,
	eMethod_setColors,
	eMethod_sendTap,
	eMethod_initialize,
	eMethod_start,
	eMethod_redraw,
	/*
  eMethod_sendMouseDown,
  eMethod_sendMouseUp,
  eMethod_sendKeyUp,
  eMethod_sendKeyPress,
  eMethod_sendGestureStart,
  eMethod_sendGestureEnd,
  eMethod_sendGestureChange,
  eMethod_test,
	 */
}eMethods;
const static SUPPORTED_OBJECTS supported_methods[] =
{
		//{ "sendEnter",               eMethod_sendEnter,                0,  "" },
		{ "setTerminalHeight",       eMethod_setTerminalHeight,        1,  "height" },
		{ "setFont",                 eMethod_setFont,                  2,  "width, height" },
		{ "setColors",               eMethod_setColors,                2,  "fgColor, bgColor" },
		{ "sendTap",                 eMethod_sendTap,                  2,  "x, y" },
		{ "initialize",              eMethod_initialize,               0,  "" },		//	looks like it wont start w/o this method!!!
		{ "start",			     	 eMethod_start,			           1,  "user" },
		{ "redraw",		             eMethod_redraw,	               0,  "" },
		/*
    { "sendMouseDown",           eMethod_sendMouseDown,            0,  "" },
    { "sendMouseUp",             eMethod_sendMouseUp,              0,  "" },
    { "sendKeyUp",               eMethod_sendKeyUp,                0,  "" },
    { "sendKeyPress",            eMethod_sendKeyPress,             0,  "" },
    { "sendGestureStart",        eMethod_sendGestureStart,         0,  "" },
    { "sendGestureEnd",          eMethod_sendGestureEnd,           0,  "" },
    { "sendGestureChange",       eMethod_sendGestureChange,        0,  "" },
    { "test",                    eMethod_test,                     0,  "" },
		 */
		{ 0, 0, 0, 0 }
};

static bool Object_HasMethod(NPObject *npobj,NPIdentifier name)
{
	return ContainsName("Object_HasMethod", supported_methods, name);
}

static bool
Object_Invoke(
		NPObject *npobj,
		NPIdentifier name,
		const NPVariant *args,
		uint32_t argCount,
		NPVariant *result
)
{
	const SUPPORTED_OBJECTS *method = ContainsName("Object_Invoke", supported_methods, name);
	if (!method)
		return false;
	my_fprintf(stderr,"  Object_Invoke: %s", method->key);
	PrintArgs(args,argCount);
	my_fprintf(stderr,"\n");
	if((int)argCount < method->minArgs)
	{
		debugLog(__LINE__, "   wrong number of args for %s(%s), [requires:%d, found:%d]", method->key, method->argsDesc, method->minArgs, argCount);
		return false;
	}
	MyObject *myobj_ptr = (MyObject *)npobj;
	assert(myobj_ptr);
	MyObject &myobj = *myobj_ptr;
	switch(method->id)
	{
	case eMethod_sendTap:
		myobj.event_x = IntValue(&args[0]);
		myobj.event_y = IntValue(&args[1]);
		NPN_ForceRedraw(myobj.instance);
		return true;
	case eMethod_setFont:
	{
		int width = IntValue(&args[0]);
		int height = IntValue(&args[1]);
		myobj.screen.setFont(width,height);
		FontInfo &font = myobj.screen.font();
		INT32_TO_NPVARIANT(font.char_height(), (*result));	// tell caller the height of the font
		my_fprintf(stderr,"  %s(%dx%d) result:%dx%d, returning:%d\n",method->key, width, height, font.char_width(), font.char_height(), font.char_height());
		return true;
	}
	case eMethod_setColors:
	{
		int fgColor = IntValue(&args[0]);
		int bgColor = IntValue(&args[1]);
		myobj.screen.charBuf().setDefaultColors(fgColor, bgColor);
		//my_fprintf(stderr,"  %s(%d, %d)\n",method->key, fgColor, bgColor);
		//NPN_ForceRedraw(myobj.instance);
		return true;
	}
	case eMethod_setTerminalHeight:
	{
		int terminalHeight = IntValue(&args[0]);
		//my_fprintf(stderr,"  setTerminalHeight(%d)\n",terminalHeight);
		myobj.screen.setVisibleHeightPixels(terminalHeight);
		//my_fprintf(stderr,"  setTerminalHeight(%d) FINISHED, visibleHeight:%d, visibleWidth:%d, charHeight:%d\n",terminalHeight, myobj.screen.charBuf().visibleHeight(), myobj.screen.charBuf().visibleWidth(), myobj.screen.font().char_height());
		//NPN_ForceRedraw(myobj.instance);
		return true;
	}
	case eMethod_start:
	{
		myobj.start(StringValue(&args[0]));
		return true;
	}
	case eMethod_redraw:
	{
		NPN_ForceRedraw(myobj.instance);
		return true;
	}
	//case eMethod_sendEnter:
	//	myobj.key_manager.keyPressed(13,false,false,false,false);
	//	return true;
	}

	return false;
}

typedef enum
{
	eProperty_event_x,
	eProperty_event_y,
	eProperty_scroller,
	eProperty_keyStatesParentObj,
	eProperty_showKeyStates,
	eProperty_drawEnabled,
	eProperty_test_property,
}eProperties;
const static SUPPORTED_OBJECTS supported_properties[] =
{
		{ "event_x",                 eProperty_event_x,                -1,  "" },
		{ "event_y",                 eProperty_event_y,                -1,  "" },
		{ "scroller",                eProperty_scroller,               -1,  "" },
		{ "keyStatesParentObj",      eProperty_keyStatesParentObj,     -1,  "" },
		{ "showKeyStates",           eProperty_showKeyStates,          -1,  "" },
		{ "drawEnabled",             eProperty_drawEnabled,            -1,  "" },
		{ "test_property",           eProperty_test_property,          -1,  "" },
		{ 0, 0, 0, 0 }
};
static bool Object_HasProperty(NPObject *npobj, NPIdentifier name)
{
	return ContainsName("Object_HasProperty", supported_properties, name) ? true : false;
}

static void saveObj(const NPVariant *value, NPObject **obj, const char *desc)
	{
	if(!obj || !value)
		return;
	if(NPVARIANT_IS_OBJECT(*value))
		{
		if(*obj)
			NPNFuncs.releaseobject(*obj);		//	get rid of old OBJ
		*obj = NPVARIANT_TO_OBJECT(*value);
		my_fprintf(stderr, "setting %s:%p\n", desc, *obj);
		if(*obj)
			{
			*obj = NPNFuncs.retainobject(*obj);
			my_fprintf(stderr, " saving %s:%p\n", desc, *obj);
			}
		}
	else if(
			NPVARIANT_IS_VOID(*value) ||
			NPVARIANT_IS_NULL(*value) ||
			(NPVARIANT_IS_INT32(*value) && NPVARIANT_TO_INT32(*value) == 0) ||
			(NPVARIANT_IS_DOUBLE(*value) && NPVARIANT_TO_DOUBLE(*value) == 0)
			)
		{
		my_fprintf(stderr, "clearing %s (old:%p)\n", desc, *obj);
		if(*obj)
			NPNFuncs.releaseobject(*obj);		//	get rid of old OBJ
		*obj = 0;
		}
	else
		my_fprintf(stderr, "%s 0x%X, type:%s\n", desc, NPVARIANT_TO_INT32(*value), NPVariantTypeStr(value->type));
	}

static bool
Object_SetProperty(
		NPObject *npobj,
		NPIdentifier name,
		const NPVariant *value
)
{
	const SUPPORTED_OBJECTS *property = ContainsName("Object_SetProperty", supported_properties, name);
	if (property)
	{
		MyObject *myobj = (MyObject *)npobj;
		switch(property->id)
		{
		case eProperty_event_x:
			myobj->event_x = IntValue(value);
			my_fprintf(stderr,"  event_x set to %d\n",myobj->event_x);
			break;
		case eProperty_event_y:
			myobj->event_y = IntValue(value);
			my_fprintf(stderr,"  event_y set to %d\n",myobj->event_y);
			break;
		case eProperty_showKeyStates:
			myobj->showKeyStates = IntValue(value);
			break;
		case eProperty_drawEnabled:
			myobj->drawEnabled = IntValue(value);
			if(myobj->drawEnabled)
				NPN_ForceRedraw(myobj->instance);		//	cause a redraw now
			break;
		case eProperty_keyStatesParentObj:
			saveObj(value, &myobj->keyStatesParentObj, "keyStatesParentObj");
			//my_fprintf(stderr, "        keyStatesParentObj:%p\n", myobj->keyStatesParentObj);
			break;
		case eProperty_scroller:
			saveObj(value, &myobj->scroller, "scroller");
			//my_fprintf(stderr, "        scroller:%p\n", myobj->scroller);
			break;
		case eProperty_test_property:
		{
			int command = IntValue(value);
			my_fprintf(stderr,"  command=%d\n",command);
			if (command==2) {
				NPN_ForceRedraw(myobj->instance);
			}
			break;
		}
		default:
			my_fprintf(stderr,"Invalid property\n");
			break;
		}
	}
	return false;
}

static NPObject *GetScriptableObject(NPP instance)
{
	my_fprintf(stderr,"GetScriptableObject: instance=%p\n",instance);
	InstanceData *data = (InstanceData *)instance->pdata;
	if (!data->object) {
		static NPClass object_class;
		object_class.structVersion = NP_CLASS_STRUCT_VERSION;
		object_class.allocate = Object_Allocate;
		object_class.deallocate = Object_Deallocate;
		object_class.invalidate = 0;
		object_class.hasMethod = Object_HasMethod;
		object_class.invoke = Object_Invoke;
		object_class.invokeDefault = 0;
		object_class.hasProperty = Object_HasProperty;
		object_class.getProperty = 0;
		object_class.setProperty = Object_SetProperty;
		object_class.removeProperty = 0;
		object_class.enumerate = 0;
		object_class.construct = Object_Construct;

		data->object =
			(MyObject *)NPN_CreateObject(
					instance,
					&object_class
			);
	}

	return data->object;
}

NP_EXPORT(NPError)
NP_GetValue(NPP instance, NPPVariable aVariable, void* aValue) {

	switch (aVariable) {
	case NPPVpluginNameString:
		*((const char**)aValue) = PLUGIN_NAME;
		break;
	case NPPVpluginDescriptionString:
		*((const char**)aValue) = PLUGIN_DESCRIPTION;
		break;
	default:
		return NPERR_INVALID_PARAM;
		break;
	}
	return NPERR_NO_ERROR;
}

NP_EXPORT(NPError)
NP_Shutdown()
{
	my_fprintf(stderr,"term: NP_Shutdown\n");
	return NPERR_NO_ERROR;
}

NPError
NPP_New(NPMIMEType pluginType, NPP instance, uint16_t mode, int16_t argc, char* argn[], char* argv[], NPSavedData* saved) {
	instance->pdata = new InstanceData;
	my_fprintf(stderr,"term: NP_New\n");
	return NPERR_NO_ERROR;
}

NPError
NPP_Destroy(NPP instance, NPSavedData** save) {
	my_fprintf(stderr,"term: NPP_Destroy\n");
	InstanceData* instanceData = (InstanceData*)(instance->pdata);
	if (instanceData) {
		delete instanceData;
	}
	return NPERR_NO_ERROR;
}


NPError
NPP_SetWindow(NPP instance, NPWindow* window) {
	if (!window) {
		// Destroying window
		debugLog(__LINE__, "NPP_SetWindow(window:%p)", window);
		return NPERR_NO_ERROR;
	}
	if (!window->window) {
		debugLog(__LINE__, "NPP_SetWindow(window->window:%p)", window->window);
		return NPERR_NO_ERROR;
	}
#if 0
	debugLog(__LINE__, "NPP_SetWindow(x:%d, y:%d, width:%d, height:%d, type:%d)", window->x, window->y, window->width, window->height, window->type);
#endif

	InstanceData *data = (InstanceData *)instance->pdata;
	MyObject *myobj_ptr = data->object;
	if (!myobj_ptr) {
		return NPERR_NO_ERROR;
	}
	Screen &screen = myobj_ptr->screen;
	if (!screen.setSizePixels(window->width, window->height)) {
		return NPERR_NO_ERROR;
	}
	CharBuffer &char_buf = screen.charBuf();
	my_fprintf(stderr,"term: visibile size changed: %dx%d\n", char_buf.visibleWidth(), char_buf.visibleHeight());
        if (myobj_ptr->spawn_data) {
                Spawn_Stty(
                        myobj_ptr->spawn_data,
                        char_buf.visibleHeight(),
                        char_buf.visibleWidth()
                );
        }
	return NPERR_NO_ERROR;
}


NPError
NPP_NewStream(NPP instance, NPMIMEType type, NPStream* stream, NPBool seekable, uint16_t* stype) {
	my_fprintf(stderr,"term: NPP_NewStream\n");
	return NPERR_GENERIC_ERROR;
}


NPError
NPP_DestroyStream(NPP instance, NPStream* stream, NPReason reason) {
	my_fprintf(stderr,"term: NPP_DestroyStream\n");
	return NPERR_GENERIC_ERROR;
}

int32_t
NPP_WriteReady(NPP instance, NPStream* stream) {
	my_fprintf(stderr,"term: NPP_WriteReady\n");
	return 0;
}

int32_t
NPP_Write(NPP instance, NPStream* stream, int32_t offset, int32_t len, void* buffer) {
	my_fprintf(stderr,"term: NPP_Write\n");
	return 0;
}

void
NPP_StreamAsFile(NPP instance, NPStream* stream, const char* fname) {
	my_fprintf(stderr,"term: NPP_StreamAsFile\n");
}

void
NPP_Print(NPP instance, NPPrint* platformPrint) {
	my_fprintf(stderr,"term: NPP_Print\n");
}


void HandlePenMoveEvent(NpPalmPenEvent *penEvent,NPP instance)
{
	my_fprintf(stderr,"HandlePenMoveEvent: x=%d, y=%d\n",penEvent->xCoord,penEvent->yCoord);
}


void HandleKeyEvent(NpPalmKeyEvent *keyEvent,NPP instance)
{
	int key_code = keyEvent->rawkeyCode;
	int modifiers = keyEvent->rawModifier;

	my_fprintf(
			stderr,
			"term: keyEvent key=%d, modifiers=%d\n",
			key_code,
			modifiers
	);
#if 1
	InstanceData *data = (InstanceData *)instance->pdata;
	MyObject *myobj_ptr = data->object;
	if (!myobj_ptr) {
		return;
	}
	MyObject &myobj = *myobj_ptr;

	bool shift_pressed = (modifiers & 128)!=0;
	bool ctrl_pressed = (modifiers & 64)!=0;
	bool alt_pressed = false;
	bool meta_pressed = (modifiers & 32)!=0;

	bool key_state_changed = myobj.key_manager.keyPressed(
			key_code,shift_pressed,alt_pressed,ctrl_pressed,meta_pressed
	);
	//my_fprintf(stderr,"  keyEvent changed:%s\n", key_state_changed ? "true" : "false");
	if(key_state_changed)
		{
	#ifdef USE_MOJO_FOR_KEYSTATE
		//this.termplugin.keyStatesParentObj = this;
		//this.keyStatesChanged(gesture, red, sym, shift)
		if(myobj.keyStatesParentObj)
			{
			NPVariant args[4], result;
			INT32_TO_NPVARIANT(myobj.key_manager.getGestureHoldSstate(), args[0]);
			INT32_TO_NPVARIANT(myobj.key_manager.getRedState(), args[1]);
			INT32_TO_NPVARIANT(myobj.key_manager.getSymState(), args[2]);
			INT32_TO_NPVARIANT(myobj.key_manager.getShiftState(), args[3]);
			invokeObj(myobj.instance, myobj.keyStatesParentObj, "keyStatesChanged", args, 4, &result);
			}
	#endif
		if(myobj.showKeyStates)
			NPN_ForceRedraw(myobj.instance);
		}
#endif
}

//#ifndef USE_MOJO_FOR_KEYSTATE
int write_key_state(PixelBuffer &pbuf, int x, PixelBuffer::Pixel foreground, PixelBuffer::Pixel background, const char *desc, int state)
{
	if(state)
	{
		if(state < 2 && *desc == '*')
			desc++;		//	skip the '*', just show key-desc
		x = pbuf.writeStrAbsoluteXY(x, 0, desc, foreground, background);
	}
	return x;
}
//#endif

void HandleDrawEvent(NpPalmDrawEvent *drawEvent,NPP instance)
{
	my_fprintf(stderr,"term: drawEvent sl: %i sr: %i st: %i sb: %i dl: %i dr: %i dt: %i db: %i drb:%i\n",
			drawEvent->srcLeft,
			drawEvent->srcRight,
			drawEvent->srcTop,
			drawEvent->srcBottom,
			drawEvent->dstLeft,
			drawEvent->dstRight,
			drawEvent->dstTop,
			drawEvent->dstBottom,
			drawEvent->dstRowBytes
	);
	double start_time = CurrentTime();
	unsigned int *screenBuffer = (unsigned int*) drawEvent->dstBuffer;
	InstanceData *data = (InstanceData *)instance->pdata;
	MyObject *myobj_ptr = data->object;
	if (!myobj_ptr) {
		return;
	}
	MyObject &myobj = *myobj_ptr;
	if(!myobj.drawEnabled) {
		my_fprintf(stderr, "Drawing disabled!\n");
		return;
	}
	/*
	if(myobj.screen.charBuf().visibleHeight() <= 0) {
		my_fprintf(stderr, "Visible Height is too small!\n");
		return;
	}
	*/
	pthread_mutex_lock(&myobj.draw_mutex);
	myobj.checkSpawn();

	int dst_width = drawEvent->dstRight-drawEvent->dstLeft;
	int dst_height = drawEvent->dstBottom-drawEvent->dstTop;

#if 0
	const int screenWidth	= drawEvent->dstRowBytes / 4;
	int left	= max(0, drawEvent->dstLeft);
	int right	= min(screenWidth, drawEvent->dstRight);
	int top		= max(0, drawEvent->srcTop);
	int bottom	= drawEvent->srcBottom;
	debugLog(__LINE__, "HandleDrawEvent(sl:%i sr:%i st:%i sb:%i dl:%i dr:%i dt:%i db:%i drb:%i [top:%d left:%d bottom:%d right:%d screenWidth:%d] buf:%p myobj:%p)",
			drawEvent->srcLeft,
			drawEvent->srcRight,
			drawEvent->srcTop,
			drawEvent->srcBottom,
			drawEvent->dstLeft,
			drawEvent->dstRight,
			drawEvent->dstTop,
			drawEvent->dstBottom,
			drawEvent->dstRowBytes,
			top,
			left,
			bottom,
			right,
			screenWidth,
			screenBuffer,
			&myobj);
#endif

	FontInfo &font = myobj.screen.font();
	//debugLog(__LINE__, "font:%p", font);
	PixelBuffer pbuf(font, screenBuffer,dst_width,dst_height,drawEvent->dstRowBytes,drawEvent->srcTop);
	//debugLog(__LINE__, "pbuf:%p", &pbuf);
	//my_fprintf(stderr," ... Drawing before clear ... time: %g\n",CurrentTime()-start_time);
	//pbuf.clear(PixelBuffer::pixel(0,0,0));
	//my_fprintf(stderr," ... Drawing after clear ... time: %g\n",CurrentTime()-start_time);

	{
		Screen::CharBuf &char_buf = myobj.screen.charBuf();
		//const int width = char_buf.width();
		const int height = char_buf.height();
#if 0
		const int char_height = font.char_height();
		const int char_width = font.char_width();
		const int topRow = font.getLine(top);
		int row = topRow, y = top - (topRow * line_height);
		for (;row < height && y < bottom;++row, y += line_height) {
			int col = 0, x;
			for (x = left; col < width && x < right;++col, x += char_width) {
				pbuf.writeCharXY(x,y,char_buf.cell(row,col));
			}
		}
#else
		int begin_row = max(pbuf.topRow(),0);
		int end_row = min(pbuf.bottomRow(),height);
		//my_fprintf(stderr, "begin_row:%d, end_row:%d, height:%d\n", begin_row, end_row, height);
		for (int row = begin_row;row < end_row;++row) {
			//assert(row < height);
			pbuf.writeRow(row, char_buf);
		}

		//my_fprintf(stderr, "char_buf.currentRow:%d, height:%d\n", char_buf.currentRow(), height);
		assert(char_buf.currentRow()<height);
		pbuf.invertChar(char_buf.currentRow(),char_buf.currentCol());
#endif
//#ifndef USE_MOJO_FOR_KEYSTATE
		if(myobj.showKeyStates)	{
			//my_fprintf(stderr," ... Drawing Keystate ... time: %g\n",CurrentTime()-start_time);
			const int char_width = font.char_width();
			int x = char_width;
			PixelBuffer::Pixel foreground = colorRed;
			PixelBuffer::Pixel background = colorYellow;
			x = write_key_state(pbuf, x, foreground, background, "*Gesture ", myobj.key_manager.getGestureHoldSstate());
			x = write_key_state(pbuf, x, foreground, background, "*Orange ", myobj.key_manager.getRedState());
			x = write_key_state(pbuf, x, foreground, background, "*Sym ", myobj.key_manager.getSymState());
			x = write_key_state(pbuf, x, foreground, background, "*Shift ", myobj.key_manager.getShiftState());
			if(x != char_width)
			{
				pbuf.writeCharAbsoluteXY(0, 0, ' ', foreground, background);
				//int y = font.char_height(), pixelsPerLine = (drawEvent->dstRowBytes / 4);
				//unsigned int *o = screenBuffer + (y * pixelsPerLine);
				//for(x = 0; x < pixelsPerLine; x++, o++)
				//	*o = colorBlue;
			}
		}
//#endif
	}

	if(myobj.select_thread_started)
		pthread_cond_signal(&myobj.drawn_cond);
	fprintf(stderr,"Done drawing\n");
	pthread_mutex_unlock(&myobj.draw_mutex);
	double end_time = CurrentTime();
	fprintf(stderr,"Drawing time: %g\n",end_time-start_time);
}


int16_t
NPP_HandleEvent(NPP instance, void* aEvent) {
	NpPalmEventType *palmEvent = (NpPalmEventType *) aEvent;
	switch (palmEvent->eventType) {
	case npPalmPenDownEvent:
		my_fprintf(stderr, "term: NPP_HandleEvent: npPalmPenDownEvent(x:%d, y:%d, mod:%d)\n", palmEvent->data.penEvent.xCoord, palmEvent->data.penEvent.yCoord, palmEvent->data.penEvent.modifiers);
		break;
	case npPalmPenUpEvent:
		my_fprintf(stderr, "term: NPP_HandleEvent: npPalmPenUpEvent(x:%d, y:%d, mod:%d)\n", palmEvent->data.penEvent.xCoord, palmEvent->data.penEvent.yCoord, palmEvent->data.penEvent.modifiers);
		break;
	case npPalmPenMoveEvent:
		my_fprintf(stderr, "term: NPP_HandleEvent: npPalmPenMoveEvent(x:%d, y:%d, mod:%d)\n", palmEvent->data.penEvent.xCoord, palmEvent->data.penEvent.yCoord, palmEvent->data.penEvent.modifiers);
		HandlePenMoveEvent(&palmEvent->data.penEvent,instance);
		break;
	case npPalmKeyDownEvent:
		my_fprintf(stderr, "term: NPP_HandleEvent: npPalmKeyDownEvent(chr:%d, mod:%d, rawKey:%d, rawMod:%d)\n", palmEvent->data.keyEvent.chr, palmEvent->data.keyEvent.modifiers, palmEvent->data.keyEvent.rawkeyCode, palmEvent->data.keyEvent.rawModifier);
		HandleKeyEvent(&palmEvent->data.keyEvent,instance);
		break;
	case npPalmKeyUpEvent:
		my_fprintf(stderr, "term: NPP_HandleEvent: npPalmKeyUpEvent(chr:%d, mod:%d, rawKey:%d, rawMod:%d)\n", palmEvent->data.keyEvent.chr, palmEvent->data.keyEvent.modifiers, palmEvent->data.keyEvent.rawkeyCode, palmEvent->data.keyEvent.rawModifier);
		break;
	case npPalmKeyRepeatEvent:
		my_fprintf(stderr, "term: NPP_HandleEvent: npPalmKeyRepeatEvent(chr:%d, mod:%d, rawKey:%d, rawMod:%d)\n", palmEvent->data.keyEvent.chr, palmEvent->data.keyEvent.modifiers, palmEvent->data.keyEvent.rawkeyCode, palmEvent->data.keyEvent.rawModifier);
		break;
	case npPalmKeyPressEvent:
		my_fprintf(stderr, "term: NPP_HandleEvent: npPalmKeyPressEvent(chr:%d, mod:%d, rawKey:%d, rawMod:%d)\n", palmEvent->data.keyEvent.chr, palmEvent->data.keyEvent.modifiers, palmEvent->data.keyEvent.rawkeyCode, palmEvent->data.keyEvent.rawModifier);
		break;
	case npPalmDrawEvent:
		HandleDrawEvent(&palmEvent->data.drawEvent,instance);
		break;
	default:
		my_fprintf(stderr, "term: NPP_HandleEvent: unknown\n");
		break;
	}
	return 1;
}

void
NPP_URLNotify(NPP instance, const char* URL, NPReason reason, void* notifyData) {
	my_fprintf(stderr, "term: NPP_URLNotify\n");
}

const char * varname(NPPVariable variable)
{
	switch (variable) {
	case NPPVpluginNameString:
		return "NPPVpluginNameString";
		break;
	case NPPVpluginDescriptionString:
		return "NPPVpluginDescriptionString";
		break;
	case NPNVnetscapeWindow:
		return "NPNVnetscapeWindow";
		break;
	case NPPVpluginScriptableIID:
		return "NPPVpluginScriptableIID";
		break;
	case NPPVjavascriptPushCallerBool:
		return "NPPVjavascriptPushCallerBool";
		break;
	case NPPVpluginKeepLibraryInMemory:
		return "NPPVpluginKeepLibraryInMemory";
		break;
	case NPPVpluginScriptableNPObject:
		return "NPPVpluginScriptableNPObject";
		break;
	case NPPVpluginNeedsXEmbed:
		return "NPPVpluginNeedsXEmbed";
		break;
	case npPalmEventLoopValue:
		return "npPalmEventLoopValue";
		break;
	case npPalmCachePluginValue:
		return "npPalmCachePluginValue";
		break;
	case npPalmApplicationIdentifier:
		return "npPalmApplicationIdentifier";
		break;
	default:
		return "unknown";
		break;
	}
}

NPError
NPP_GetValue(NPP instance, NPPVariable variable, void *value) {
	logmsgv("NPP_GetValue",variable);
	switch (variable) {
	case NPPVpluginScriptableNPObject:
		*(NPObject **)value = GetScriptableObject(instance);
		break;
	//case npPalmCachePluginValue:
	//	*(int *)value = 1;
	//	break;
	default:
		return NPERR_GENERIC_ERROR;
	}
	return NPERR_NO_ERROR;
}

NPError
NPP_SetValue(NPP instance, NPNVariable variable, void *value) {
	my_fprintf(stderr, "term: NPP_SetValue");
	// logmsgv("NPP_SetValue",variable);
	return NPERR_GENERIC_ERROR;
}
