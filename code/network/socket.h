#ifndef SOCKET_H
#define SOCKET_H

#include <winsock2.h>

typedef int socklen_t;

typedef struct Socket
{
	SOCKET handle;
} Socket;

typedef struct Address
{
	U32 ip;
	U16 port;
} Address;

#endif