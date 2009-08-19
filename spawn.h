#include "spawndata.h"

#ifdef __cplusplus
extern "C" {
#endif

	extern void Spawn_Init();

	extern void Spawn_SetDefaultProcs(SpawnData *spawn_data,void *client_data);
	extern SpawnData *Spawn_Open(CommandHandlers *cmd_handlers,void *);
	extern void Spawn_Delete(SpawnData *spawn_data);
	extern void Spawn_Close(SpawnData *spawn_data);
	extern void Spawn_Exec(SpawnData *spawn_data,char **argv);
	extern void Spawn_WaitMaster(SpawnData *spawn_data);
	extern int Spawn_ReadMaster(SpawnData *spawn_data);
	/* returns 0 and deletes spawn_data if the master was closed */

	extern void Spawn_CheckChildExit(SpawnData *spawn_data,int wait_for_exit);
	extern int Spawn_Kill(SpawnData *spawn_data,const char *signame);
	extern void Spawn_Stty(SpawnData *spawn_data,int rows,int cols);
	extern void Spawn_SetOutputEnabled(SpawnData *spawn_data,int enabled);
	extern void Spawn_Send(SpawnData *spawn_data,const char *str,int len);

#ifdef __cplusplus
};
#endif
