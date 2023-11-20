#pragma comment(lib,"Ws2_32.lib")

#include "core_layer/base/base_inc.h"
#include "core_layer/os/os_win32_inc.h"

#include "core_layer/base/base_inc.c"
#include "core_layer/os/os_win32_inc.c"


#define TICK_RATE 30
#define TICK_LENGTH (1.0f / (F32)TICK_RATE)

#include "network/socket.h"
#include "network/net.h"
#include "network/net_event.h"

#include "app.h"

#include "network/socket.c"
#include "network/net.c"
#include "network/net_event.c"

#include "host.c"
#include "client.c"
#include "app.c"

internal S32
EntryPoint(String8List args)
{
	CoreInit();

	B32 is_host = false;

#define IF_CMD_ARG(s) if(Str8Match(node->string, Str8Lit(Glue("-", s))))
	for(String8Node *node = args.first;
			node != 0;
			node = node->next)
	{
		IF_CMD_ARG("host")
		{
			is_host = true;
		}

		DLL_Remove(args.first, args.last, node);
	}

	App *app = (App *)OS_AllocMem(sizeof(*app));

	F64 dt = 0;
	F64 start_counter = OS_SecondsSinceAppStart();

	B32 running = true;
	while(running)
	{
		TempMemoryArena scratch = GetScratch(0, 0);

		B32 requested_exit = UpdateApp(app, is_host, dt);

		if(requested_exit)
		{
			running = false;
		}

		F64 end_counter = OS_SecondsSinceAppStart();
		dt = end_counter - start_counter;
		start_counter = end_counter;

		ReleaseScratch(scratch);
	}

	return(0);
}