function B32
InitializeSockets()
{
	WSADATA wsa_data;

	B32 result = WSAStartup(MAKEWORD(2, 2), &wsa_data) == NO_ERROR;

	return(result);
}

function B32
SocketOpen(Socket *sock, U16 port)
{
	B32 result = true;

	// NOTE(hampus): Create a socket
	SOCKET handle = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);

	if(handle <= 0)
	{
		printf("Failed to create a socket!\n");
		result = false;
	}

	SOCKADDR_IN address;
	address.sin_family = AF_INET;
	address.sin_addr.s_addr = INADDR_ANY;
	address.sin_port = htons((unsigned short)port);

	if(bind(handle, (const SOCKADDR*)&address, sizeof(SOCKADDR_IN)) < 0)
	{
		printf("failed to bind socket\n");
		result = false;
	}

	sock->handle = handle;

	return(result);
}

function B32
SocketSetNonBlocking(Socket sock)
{
	B32 result = true;
	// NOTE(hampus): Set the socket as non-blocking
	DWORD non_blocking = true;
	if(ioctlsocket(sock.handle, FIONBIO, &non_blocking) != 0)
	{
		printf("Failed to set socket non-blocking!\n");
		result = false;
	}

	return(result);
}

function B32
SocketSend(Socket sock, Address address, void *data, S32 data_size)
{
	B32 result = false;

	SOCKADDR_IN addr;
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = htonl(address.ip);
	addr.sin_port = htons(address.port);

	S32 sent_bytes = sendto(sock.handle, (const char *)data, data_size, 0, (SOCKADDR *)&addr, sizeof(SOCKADDR_IN));
	if(sent_bytes != data_size)
	{
		printf("Failed to send packet!\n");
		result = false;
	}

	return(result);
}

function S32
SocketRecieve(Socket socket, Address *sender, void *data, S32 data_size)
{
	SOCKADDR_IN from;
	socklen_t from_length = sizeof(from);
	S32 bytes_recieved = recvfrom(socket.handle, (char *)data, data_size, 0, (SOCKADDR *)&from, &from_length);

	if(bytes_recieved == -1)
	{
		S32 error = WSAGetLastError();
		bytes_recieved = 0;
	}

	sender->ip = ntohl(from.sin_addr.s_addr);
	sender->port = ntohs(from.sin_port);

	return(bytes_recieved);
}

function Address
CreateAddress(U8 a, U8 b, U8 c, U8 d, U16 port)
{
	Address result;

	result.ip = (a << 24) | (b << 16) | (c << 8) | d;
	result.port = port;

	return(result);
}

function B32
AddressMatch(Address a, Address b)
{
	return a.ip == b.ip && a.port == b.port;
}