#include "stdio.h"
#include "termplugin.h"
#include "common.h"
#include "api.h"


#define debug(level, fmt, ...)					my_fprintf(stderr, fmt "\n", ##__VA_ARGS__)
static NPObject *normalizeObj(NPP instance, NPObject *obj)
	{
	if(obj == PLUGIN_OBJ)
		{
		NPError err = NPNFuncs.getvalue(instance, NPNVPluginElementNPObject, &obj);
		if(err || !obj)
			{
			debug(DBG_MAIN, "	normalizeObj: FAILED to get OBJ, err:%d, obj:%p", err, obj);
			return 0;
			}
		}
	return obj;
	}

static NPError getObj_and_ID(NPP instance, NPObject **pObj, const char *str, NPIdentifier *pID)
	{
	if(!pObj || !pID || !str)
		return NPERR_GENERIC_ERROR;
	NPObject *obj = normalizeObj(instance, *pObj);
	if(!obj)
		{
		debug(DBG_MAIN, "	getObj_and_ID(%s): FAILED to get OBJ", str);
		return NPERR_GENERIC_ERROR;
		}
	*pObj = obj;
	NPIdentifier id	= NPNFuncs.getstringidentifier(str);
	if(!id)
		{
		debug(DBG_MAIN, "	getObj_and_ID(%s): FAILED to get ID, obj:%p", str, obj);
		return NPERR_GENERIC_ERROR;
		}
	*pID = id;
	if(!NPNFuncs.hasproperty(instance, obj, id)	&& !NPNFuncs.hasmethod(instance, obj, id))
		{
		debug(DBG_MAIN, "	getObj_and_ID(%s): FAILED: no such property/method, obj:%p, id:%p", str, obj, id);
		return NPERR_GENERIC_ERROR;
		}
	return NPERR_NO_ERROR;
	}

NPError getObjValue(NPP instance, NPObject *obj, const char *str, NPVariant *result)
	{
	NPIdentifier id;
	NPError err = getObj_and_ID(instance, &obj, str, &id);
	if(err == NPERR_NO_ERROR)
		{
		bool ok = NPNFuncs.getproperty(instance, obj, id, result);
		if(!ok)
			err = NPERR_GENERIC_ERROR;
		//debugLogVariant(__LINE__, result, ok, str);
		}
	return err;
	}
NPError setObjValue(NPP instance, NPObject *obj, const char *str, NPVariant *val)
	{
	NPIdentifier id;
	NPError err = getObj_and_ID(instance, &obj, str, &id);
	if(err == NPERR_NO_ERROR)
		{
		bool ok = NPNFuncs.setproperty(instance, obj, id, val);
		if(!ok)
			err = NPERR_GENERIC_ERROR;
		//debugLogVariant(__LINE__, val, ok, str);
		}
	return err;
	}
NPError setObjInt(NPP instance, NPObject *obj, const char *str, int val)
	{
	NPVariant var;
	INT32_TO_NPVARIANT(val, var);
	return setObjValue(instance, obj, str, &var);
	}


static NPObject *get_scroller_mojo(NPP instance, NPObject *scroller)
	{
	if(!scroller)
		{
		debug(DBG_MAIN, "get_scroller_mojo: no scroller");
		return 0;
		}
	//debug(DBG_MAIN, "get_scroller_mojo");
	NPIdentifier mojoID = NPNFuncs.getstringidentifier("mojo");
	//debug(DBG_MAIN, "get_scroller_mojo npp:%p, scroller:%p, id:%p", instance, scroller, mojoID);
	if(!NPNFuncs.hasproperty(instance, scroller, mojoID) && !NPNFuncs.hasmethod(instance, scroller, mojoID))
		{
		debug(DBG_MAIN, "get_scroller_mojo: no property/method mojo");
		return 0;
		}
	NPVariant var;
	int ok = NPNFuncs.getproperty(instance, scroller, mojoID, &var);
	if(!ok || !NPVARIANT_IS_OBJECT(var))
		{
		debug(DBG_MAIN, "get_scroller_mojo: ok:%d, mojo - var.type:%d (not object)", ok, var.type);
		return 0;
		}
	//debug(DBG_MAIN, "get_scroller_mojo");
	NPObject *mojoObj = NPVARIANT_TO_OBJECT(var);
	return mojoObj;
	}

NPError invokeObj(NPP instance, NPObject *obj, const char *function, const NPVariant *args, uint32_t argCount, NPVariant *result)
	{
	NPIdentifier id;
	NPError err = getObj_and_ID(instance, &obj, function, &id);
	if(err == NPERR_NO_ERROR)
		{
		result->type = NPVariantType_Void;
		bool ok = NPNFuncs.invoke(instance, obj, id, args, argCount, result);
		if(!ok)
			err = NPERR_GENERIC_ERROR;
		//debugLogVariant(__LINE__, val, ok, str);
		}
	return err;
	}

static void scrollCommon(NPP instance, NPObject *scroller, const char *function, const NPVariant *args, uint32_t argCount, NPVariant *result)
	{
	NPObject *mojo = get_scroller_mojo(instance, scroller);
	if(!mojo)
		return;

	NPIdentifier id = NPNFuncs.getstringidentifier(function);
	if(!NPNFuncs.hasproperty(instance, mojo, id) && !NPNFuncs.hasmethod(instance, mojo, id))
		{
		debug(DBG_MAIN, "scrollCommon: no property/method: %s", function);
		return;
		}
	result->type = NPVariantType_Void;
	//debug(DBG_MAIN, "scrollCommon() %s(argCount:%d, type:%d, val:%p)", function, argCount, args ? args->type : -1, args ? args->value.objectValue : 0);
#ifdef DEBUG_OUTPUT
	bool ok =
#endif
		NPNFuncs.invoke(instance, mojo, id, args, argCount, result);
	debug(DBG_MAIN, "scrollCommon: scroller.mojo.%s(): %s, var.type:%d", function, ok ? "OK" : "ERROR", result->type);
	}

void scrollTo(NPP instance, NPObject *scroller, int y, bool animate)
	{
	NPVariant var, args[3];
	INT32_TO_NPVARIANT(/*x*/0, args[0]);
	INT32_TO_NPVARIANT(y, args[1]);
	BOOLEAN_TO_NPVARIANT(animate, args[2]);	//	
	//BOOLEAN_TO_NPVARIANT(true, args[3]);	//	supress notifications
	scrollCommon(instance, scroller, "scrollTo", args, 3, &var);
	}
void scrollTop(NPP instance, NPObject *scroller)
	{
#if 1
	scrollTo(instance, scroller, 0, false);
#else
	//scroller.mojo.revealTop(0)
	//	TODO:	test again with NULL_TO_NPVARIANT(args) and/or VOID_TO_NPVARIANT(args)
	//		as when OBJECT_TO_NPVARIANT(0, args) was used it crashed
	NPVariant args;
	NULL_TO_NPVARIANT(args);
	//debug(DBG_MAIN, "scrollTop()");
	NPVariant var;
	scrollCommon(instance, scroller, "revealTop", args, 1, &var);
#endif
	}

void scrollBottom(NPP instance, NPObject *scroller)
	{
	//scroller.mojo.revealBottom()
	NPVariant var;
	//debug(DBG_MAIN, "revealBottom()");
	scrollCommon(instance, scroller, "revealBottom", 0, 0, &var);
	}


int getScrollPosition(NPP instance, NPObject *scroller)
	{
	NPVariant var;
	//var.type = NPVariantType_Void;
	scrollCommon(instance, scroller, "getScrollPosition", 0, 0, &var);
	NPObject *obj = NPVARIANT_TO_OBJECT(var);
	if(!NPVARIANT_IS_OBJECT(var) || !obj)
		{
		debug(DBG_MAIN, "getScrollPosition() expected obj, got %d (obj:%p)", var.type, obj);
		return -1;
		}
	//	Get the current position of the scroller. Returns {left: nnn px, top: nnn px}
	NPIdentifier id = NPNFuncs.getstringidentifier("top");
	//debug(DBG_MAIN, "getScrollPosition()");

	if(!NPNFuncs.hasproperty(instance, obj, id) && !NPNFuncs.hasmethod(instance, obj, id))
		{
		debug(DBG_MAIN, "getScrollPosition: no property/method: top, obj:%p", obj);
		return -2;
		}

#ifdef DEBUG_OUTPUT
	bool ok =
#endif
		NPNFuncs.getproperty(instance, obj, id, &var);
	int val = IntValue(&var);
	debug(DBG_MAIN, "getScrollPosition() %s val:%d, type:%d", ok ? "OK" : "ERROR", val, var.type);
	//if(NPVARIANT_IS_DOUBLE(var))
	//	debug(DBG_MAIN, "+++++ getScrollPosition() = double:%g", NPVARIANT_TO_DOUBLE(var));
	//else if(NPVARIANT_IS_INT32(var))
	//	debug(DBG_MAIN, "----- getScrollPosition() = int:%d", NPVARIANT_TO_INT32(var));
	return val;
	}

