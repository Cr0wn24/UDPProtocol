#include "app.h"

internal B32
UpdateApp(App *app, B32 is_host, F64 dt)
{
	B32 exit = false;

	if(!app->initialized)
	{
		ArenaInit(&app->permanent_arena, OS_AllocMem(GIGABYTES(1)), GIGABYTES(1));
		ArenaInit(&app->frame_arena, OS_AllocMem(GIGABYTES(1)), GIGABYTES(1));

		if(!InitializeSockets())
		{
			printf("Failed to initialize sockets!\n");
			Assert(false);
			exit = true;
		}
	}

	if(is_host)
	{
		UpdateHost(app, dt);
	}
	else
	{
		UpdateClient(app, dt);
	}

	ArenaZero(&app->frame_arena);

	return(exit);
}