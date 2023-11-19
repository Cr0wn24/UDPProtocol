#ifndef NET_EVENT_H
#define NET_EVENT_H

typedef enum N_NetEventType
{
	N_NetEventType_None,
	N_NetEventType_Recieve,
	N_NetEventType_Connect, // unused
	N_NetEventType_Disconnect,
} N_NetEventType;

typedef struct N_NetEvent
{
	N_NetEventType type;
	U8 data[256];
} N_NetEvent;

typedef struct N_NetEventNode
{
	struct N_NetEventNode *next;
	struct N_NetEventNode *prev;

	N_Peer *peer;
	N_NetEvent *event;
} N_NetEventNode;

typedef struct N_NetEventList
{
	struct N_NetEventNode *first;
	struct N_NetEventNode *last;
} N_NetEventList;

#endif