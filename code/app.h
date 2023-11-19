#ifndef APP_H
#define APP_H

typedef struct App
{
	MemoryArena permanent_arena;
	MemoryArena frame_arena;

	N_Host host;

	N_Client client;

	U32 packet_count;
	F64 tick_time_elapsed;
	F64 time_elapsed;
	U32 tick;

	B32 initialized;
} App;

#endif
