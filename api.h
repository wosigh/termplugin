#ifndef __API_H__
#define __API_H__


#define PLUGIN_OBJ		(NPObject *)-1




#ifdef __cplusplus
extern "C" {
#endif


void scrollTop(NPP instance, NPObject *scroller);
void scrollBottom(NPP instance, NPObject *scroller);
void scrollTo(NPP instance, NPObject *scroller, int y, bool animate);
int getScrollPosition(NPP instance, NPObject *scroller);

NPError setObjInt(NPP instance, NPObject *obj, const char *str, int val);
NPError setObjValue(NPP instance, NPObject *obj, const char *str, NPVariant *val);
NPError getObjValue(NPP instance, NPObject *obj, const char *str, NPVariant *result);
NPError invokeObj(NPP instance, NPObject *obj, const char *function, const NPVariant *args, uint32_t argCount, NPVariant *result);



#ifdef __cplusplus
}  /* end extern "C" */
#endif


#endif	//__API_H__
