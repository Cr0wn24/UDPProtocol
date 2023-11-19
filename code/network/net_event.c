function void
NE_SendDataToPeer(N_Peer *peer, void *data, U32 data_size, B32 reliable)
{
	N_NetEventType type = N_NetEventType_Recieve;

	N_Packet packet = {0};

	// NOTE(hampus): Copy event header
	CopyArray(packet.data, &type, sizeof(type));

	// NOTE(hampus): Copy event data
	CopyArray(packet.data + sizeof(type), data, data_size);

	packet.header.reliable = reliable;
	packet.header.data_size = data_size + sizeof(type);
	B32 force_send = false;
	if(!reliable)
	{
		force_send = true;
	}
	N_SendPacketToPeer(peer, &packet, force_send);
}

function N_NetEventList *
NE_RecieveNetEventsFromClients(N_Host *host, MemoryArena *arena, F64 dt)
{
	N_NetEventList *result = PushStruct(arena, N_NetEventList);

	N_PeerPacketList *packet_list = N_Host_RecieveFromClients(host, arena, dt);

	for(N_PeerPacketNode *node = packet_list->first;
			node != 0;
			node = node->next)
	{
		N_Packet *packet = node->packet;
		N_NetEvent *event = (N_NetEvent *)packet->data;
		if(event->type != N_NetEventType_None)
		{
			N_NetEventNode *event_node = PushStruct(arena, N_NetEventNode);
			event_node->event = event;
			event_node->peer = node->peer;
			DLL_PushBack(result->first, result->last, event_node);
		}
	}

	for(U32 i = 0; i < ArrayCount(host->clients); ++i)
	{
		N_Peer *peer = host->clients + i;
		if(peer->valid)
		{
			if(peer->local_sequence_number == 2)
			{
				// NOTE(hampus): A new connection
				N_NetEvent *event = (N_NetEvent *)PushArray(arena, sizeof(event->type), U8);
				event->type = N_NetEventType_Connect;

				N_NetEventNode *event_node = PushStruct(arena, N_NetEventNode);
				event_node->event = event;
				event_node->peer = peer;
				DLL_PushBack(result->first, result->last, event_node);
			}

			if(peer->time_since_last_recieve > 5)
			{
				// NOTE(hampus): Time out
				N_NetEvent *event = (N_NetEvent *)PushArray(arena, sizeof(event->type), U8);
				event->type = N_NetEventType_Disconnect;

				N_NetEventNode *event_node = PushStruct(arena, N_NetEventNode);
				event_node->event = event;
				event_node->peer = peer;
				DLL_PushBack(result->first, result->last, event_node);
			}
		}
	}

	return result;
}

function N_NetEventList *
NE_RecieveNetEventsFromHost(N_Client *client, MemoryArena *arena, F64 dt)
{
	N_NetEventList *result = PushStruct(arena, N_NetEventList);

	N_PacketList *packet_list = N_Client_RecieveFromHost(client, arena, dt);

	for(N_PacketNode *node = packet_list->first;
			node != 0;
			node = node->next)
	{
		N_Packet *packet = node->packet;
		N_NetEvent *event = (N_NetEvent *)packet->data;
		if(event->type != N_NetEventType_None)
		{
			N_NetEventNode *event_node = PushStruct(arena, N_NetEventNode);
			event_node->event = event;
			DLL_PushBack(result->first, result->last, event_node);
		}
	}

	if(client->host.time_since_last_recieve > 5)
	{
		// NOTE(hampus): Time out
		N_NetEvent *event = (N_NetEvent *)PushArray(arena, sizeof(event->type), U8);
		event->type = N_NetEventType_Disconnect;

		ZeroStruct(&client->host);
		client->status = N_ClientStatus_Disconnected;


		N_NetEventNode *event_node = PushStruct(arena, N_NetEventNode);
		event_node->event = event;
		DLL_PushBack(result->first, result->last, event_node);
	}

	return result;
}

