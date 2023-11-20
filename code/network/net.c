#define NET_DEBUG 0

#if NET_DEBUG
#define N_PRINT(s, ...) printf(s, __VA_ARGS__)
#else
#define N_PRINT(s, ...)
#endif

global N_NetState net_state;

internal void
N_PacketFree(N_PacketStorage *storage, N_Packet *packet)
{
	N_FreePacket *free_packet = (N_FreePacket *)packet;
	free_packet->next = storage->next_free_packet;
	storage->next_free_packet = free_packet;
}

internal N_Packet *
N_PacketAlloc(N_PacketStorage *storage)
{
	N_Packet *result = (N_Packet *)storage->next_free_packet;
	storage->next_free_packet = storage->next_free_packet->next;

	ZeroStruct(result);

	return(result);
}

internal void
N_InitPacketStorage(MemoryArena *arena, U32 packet_storage_size)
{
	N_PacketStorage *packet_storage = &net_state.packet_storage;
	packet_storage->packet_array = PushArray(arena, packet_storage_size, N_Packet);
	packet_storage->packet_array_size = packet_storage_size;
	for(U32 i = 0; i < packet_storage->packet_array_size; ++i)
	{
		N_FreePacket *free_packet = (N_FreePacket *)(packet_storage->packet_array + i);
		free_packet->next = packet_storage->next_free_packet;
		packet_storage->next_free_packet = free_packet;
	}
}

internal void
N_SendPacket(Address address, N_Packet *packet)
{
	Assert(packet);

	SocketSend(net_state.socket, address, packet, sizeof(N_PacketHeader) + packet->header.data_size);
}

internal N_Packet *
N_GetNextEmptyReliableQueueEntry(N_Peer *peer)
{
	N_Packet *result = 0;

	U32 next_entry_index = (peer->reliable_packet_pos +
													peer->reliable_packets_waiting_count) % ArrayCount(peer->reliable_packet_buffer);
	result = peer->reliable_packet_buffer + next_entry_index;

	return(result);
}

#if NET_DEBUG
#define N_SendPacketToPeer(peer, packet, force) N_SendPacketToPeer_(peer, packet, force, __FILE__, __LINE__) 
internal void
N_SendPacketToPeer_(N_Peer *peer, N_Packet *packet, B32 force_send, char *file, S32 line)
#else
internal void
N_SendPacketToPeer(N_Peer *peer, N_Packet *packet, B32 force_send)
#endif
{
	Assert(peer);
	Assert(peer->address.ip);
	Assert(peer->address.port);
	// Assert(peer->valid);
	Assert(packet);

	B32 should_send = force_send;

	if(!force_send)
	{
		if(packet->header.reliable)
		{
			// This packet is marked as reliable. We will put it in the queue 
			// and then check if we can send it
			//
			// If there isn't any reliable packet waiting for 
			// ack and there are no reliable packets in queue,
			// just send it now, no need to wait

			Assert(peer->reliable_packets_waiting_count < ArrayCount(peer->reliable_packet_buffer));

			should_send = !peer->reliable_packet_in_flight && !peer->reliable_packets_waiting_count;

			N_Packet *reliable_packet_queue_entry = N_GetNextEmptyReliableQueueEntry(peer);
			CopyStruct(reliable_packet_queue_entry, packet);
			peer->reliable_packets_waiting_count++;

			// N_PRINT("Reliable packet queue size: %d\n", peer->reliable_packets_waiting_count);
		}
		else
		{
			should_send = true;
		}
	}

	if(should_send)
	{
		U32 index = peer->local_sequence_number % ArrayCount(peer->sent_packets);

		// Check so that if the packet in this slot was reliable,
		// it has been acked
		{
			N_Packet *sent_packet = peer->sent_packets + index;
			if(sent_packet->header.reliable)
			{
				Assert(!peer->sent[index]);
			}
		}

		packet->header.ack = peer->remote_sequence_number;
		packet->header.sequence_number = peer->local_sequence_number;
		packet->header.ack_bitfield = 0;

		if(packet->header.reliable)
		{
			peer->reliable_packet_in_flight = true;
			peer->reliable_sequence_number_in_flight = packet->header.sequence_number;

			N_PRINT("Sent relible packet %d with data %d\n", packet->header.sequence_number, *((U32 *)packet->data));
		}

		// Generate the ack bitfield
		for(S32 i = 0; i < 32; ++i)
		{
			U32 sequence_number = packet->header.ack - i - 1;
			U32 j = sequence_number % ArrayCount(peer->recieved_packets);
			N_Packet *recieved_packet = peer->recieved_packets + j;
			if(recieved_packet->header.sequence_number == sequence_number)
			{
				if(recieved_packet->header.reliable)
				{
					// N_PRINT("Sent reliable ack %d with packet %d\n", sequence_number, packet->sequence_number);
				}
				packet->header.ack_bitfield |= (1 << i);
			}
		}

#if NET_DEBUG
		N_PRINT("Ack sent (%s:%d): [", file, line);

		// Generate the ack bitfield
		for(S32 i = 0; i < 32; ++i)
		{
			B32 ack = (packet->header.ack_bitfield & (1 << i)) != 0;

			if(ack)
			{
				N_PRINT("%d, ", packet->header.ack - i - 1);
			}
			else
			{
				N_PRINT("0, ");
			}
		}

		if(packet->header.reliable)
		{
			N_PRINT("r");
		}
		N_PRINT("]\n");
#endif
		peer->sent_packets[index] = *packet;
		peer->time_since_send[index] = 0;
		peer->sent[index] = true;

		N_SendPacket(peer->address, packet);

		// N_PRINT("Sent packet %d\n", packet->sequence_number);

		peer->local_sequence_number++;
	}
}

internal void
N_SendPacketToHost(N_Client *client, N_Packet *packet)
{
	Assert(client);
	Assert(packet);
	// Assert(client->host.valid);

	N_SendPacketToPeer(&client->host, packet, false);
}

internal void
N_SendPacketToClient(N_Peer *client, N_Packet *packet)
{
	Assert(client);
	Assert(packet);
	Assert(client->valid);

	if(packet->header.reliable)
	{
		N_SendPacketToPeer(client, packet, false);
	}
	else
	{
		N_SendPacketToPeer(client, packet, true);
	}
}


internal N_PacketList *
N_RecieveAllPackets(MemoryArena *arena)
{
	Assert(arena);

	N_PacketList *result = PushStruct(arena, N_PacketList);

	S32 max_data_recieve_size = 256;

	// TODO(hampus): We are wasting 256 bytes at the end
	// the arena since we push 256 bytes even though
	// there may be no more data left 
	Address sender;
	U8 *recieve_data = PushArray(arena, max_data_recieve_size, U8);
	S32 bytes_recieved = SocketRecieve(net_state.socket,
																		 &sender,
																		 recieve_data,
																		 max_data_recieve_size);
	while(bytes_recieved)
	{
		if(bytes_recieved != -1)
		{
			N_PacketNode *node = PushStruct(arena, N_PacketNode);
			node->packet = (N_Packet *)recieve_data;
			node->sender = sender;

			// TODO(hampus): Do some validation of the packet

			DLL_PushBack(result->first, result->last, node);

			recieve_data = PushArray(arena, max_data_recieve_size, U8);
		}


		bytes_recieved = SocketRecieve(net_state.socket,
																	 &sender,
																	 recieve_data,
																	 max_data_recieve_size);
	}

	return result;
}

internal N_Peer *
N_Host_GetNextEmptyPeer(N_Host *host)
{
	N_Peer *result = 0;
	for(U32 i = 0; i < ArrayCount(host->clients); ++i)
	{
		N_Peer *client = host->clients + i;
		if(!client->valid)
		{
			// Found empty slot
			result = client;
			break;
		}
	}

	return result;
}

internal N_Peer *
N_Host_GetConnectedPeerByAddress(N_Host *host, Address address)
{
	N_Peer *result = 0;
	for(U32 i = 0; i < ArrayCount(host->clients); ++i)
	{
		N_Peer *client = host->clients + i;

		if(client->valid)
		{
			if(AddressMatch(client->address, address))
			{
				result = client;
				break;
			}
		}
	}
	return(result);
}

internal N_Packet *
N_GetNextReliablePacketInQueue(N_Peer *peer)
{
	Assert(peer->reliable_packets_waiting_count);
	Assert(peer);

	N_Packet *result = peer->reliable_packet_buffer + peer->reliable_packet_pos % ArrayCount(peer->reliable_packet_buffer);

	return result;
}

internal void
N_ProccessRecievedPacket(N_Peer *peer, N_Packet *packet)
{
	peer->recieved_packets[packet->header.sequence_number % ArrayCount(peer->recieved_packets)] = *packet;
	U32 ack = packet->header.ack;

	B32 waiting_for_reliable_packet_ack = peer->reliable_packet_in_flight;
	if(waiting_for_reliable_packet_ack)
	{
		if(ack == peer->reliable_sequence_number_in_flight)
		{
			N_PRINT("Recieved ack for reliable packet number: %d fst\n", ack);
			peer->reliable_packet_in_flight = false;
			peer->reliable_packet_pos++;
			peer->reliable_packets_waiting_count--;

			peer->sent[ack % 32] = false;
			peer->time_since_send[ack % 32] = 0;
		}
	}

	for(U32 i = 0; i < 32; ++i)
	{
		B32 acked = (packet->header.ack_bitfield & (1 << i)) != 0;
		U32 sequence_number = ack - i - 1;
		S32 index = sequence_number % 32;
		if(acked &&
			 peer->sent_packets[index].header.sequence_number == sequence_number && peer->sent[index])
		{
			if(sequence_number == peer->reliable_sequence_number_in_flight && peer->reliable_packet_in_flight)
			{
				N_PRINT("Recieved ack for reliable packet number: %d snd\n", sequence_number);
				peer->reliable_packet_in_flight = false;
				peer->reliable_packet_pos++;
				peer->reliable_packets_waiting_count--;
			}
			peer->sent[index] = false;
			peer->time_since_send[index] = 0;
		}
	}
}

internal void
N_ProcessSentPackets(N_Peer *peer, F64 dt)
{
	Assert(peer);
	for(U32 i = 0; i < 32; ++i)
	{
		N_Packet *sent_packet = peer->sent_packets + i;
		if(peer->sent[i])
		{
			peer->time_since_send[i] += dt;
		}
	}

	for(U32 i = 0; i < 32; ++i)
	{
		N_Packet *sent_packet = peer->sent_packets + i;
		if(peer->time_since_send[i] > 1)
		{
			if(sent_packet->header.reliable)
			{
				N_PRINT("Reliable packet %d with data %d lost. Resending . . .\n", sent_packet->header.sequence_number, *((U32 *)sent_packet->data));
				// This was the reliable packet that we last sent.
				// Send a new one.
				N_SendPacketToPeer(peer, sent_packet, true);
			}

			peer->time_since_send[i] = 0;
			peer->sent[i] = false;
		}
	}

	B32 can_send_new_reliable_packet = !peer->reliable_packet_in_flight &&
		peer->reliable_packets_waiting_count;
	if(can_send_new_reliable_packet)
	{
		N_Packet *next_reliable_packet = N_GetNextReliablePacketInQueue(peer);
		Assert(next_reliable_packet);

		N_SendPacketToPeer(peer, next_reliable_packet, true);
	}
	N_Packet packet = {0};
	N_SendPacketToPeer(peer, &packet, true);
}

internal N_PeerPacketList *
N_Host_RecieveFromClients(N_Host *host, MemoryArena *arena, F64 dt)
{
	Assert(host);
	Assert(arena);

	N_PeerPacketList *result = PushStruct(arena, N_PeerPacketList);

	N_PacketList *packet_list = N_RecieveAllPackets(arena);

	for(N_PacketNode *node = packet_list->first;
			node != 0;
			node = node->next)
	{
		N_Packet *packet = node->packet;

		N_Peer *connected_peer = N_Host_GetConnectedPeerByAddress(host, node->sender);

		if(!connected_peer)
		{
			Assert(packet->header.connect_request);
			// Peer was not connected. Check if there are any
			// slots left for conncetion
			B32 found_empty_slot = false;
			N_Peer *empty_peer_slot = N_Host_GetNextEmptyPeer(host);
			if(empty_peer_slot)
			{
				B32 accept = true;
				found_empty_slot = true;

				if(accept)
				{
					empty_peer_slot->address = node->sender;
					empty_peer_slot->valid = true;
					connected_peer = empty_peer_slot;

					N_Packet packet = {0};
					packet.header.reliable = true;
					packet.header.connect_request = true;
					N_SendPacketToPeer(connected_peer, &packet, false);
				}
				else
				{
					N_Packet packet = {0};
					packet.header.reliable = true;
					packet.header.connect_request = false;
					N_SendPacket(node->sender, &packet);
				}
			}

			if(!found_empty_slot)
			{
				// TODO(hampus): Do something? Send a SERVER_FULL 
				// message or something.
			}
		}

		if(connected_peer)
		{
			N_ProccessRecievedPacket(connected_peer, packet);
			connected_peer->time_since_last_recieve = 0;

			if(packet->header.reliable)
				N_PRINT("Recieved reliable packet %d\n", packet->header.sequence_number);
			B32 first_packet = connected_peer->remote_sequence_number == 0;
			if(packet->header.sequence_number > connected_peer->remote_sequence_number || first_packet)
			{
				// NOTE(hampus): The packet came in order
				connected_peer->remote_sequence_number = packet->header.sequence_number;
				N_PeerPacketNode *peer_node = PushStruct(arena, N_PeerPacketNode);
				peer_node->packet = node->packet;
				peer_node->peer = connected_peer;
				DLL_PushBack(result->first, result->last, peer_node);
			}
			else if(packet->header.sequence_number == connected_peer->remote_sequence_number)
			{
				// NOTE(hampus): The packet came as a duplicate
			}
			else
			{
				// NOTE(hampus): The packet probably came out-of-order
				if(packet->header.reliable)
				{
					N_PeerPacketNode *peer_node = PushStruct(arena, N_PeerPacketNode);
					peer_node->packet = node->packet;
					peer_node->peer = connected_peer;
					DLL_PushBack(result->first, result->last, peer_node);
				}
			}
		}
	}
	for(U32 i = 0; i < ArrayCount(host->clients); ++i)
	{
		N_Peer *peer = &host->clients[i];
		if(peer->valid)
		{
			N_ProcessSentPackets(peer, dt);
			peer->time_since_last_recieve += dt;

#if NET_DEBUG
			N_PRINT("Recieved packets: [");
			for(U32 i = 0; i < 33; ++i)
			{
				N_Packet *packet = peer->recieved_packets + i;
				char string[256] = {0};
				if(packet->header.reliable)
				{
					sprintf(string, "r%d", packet->header.sequence_number);
				}
				else
				{
					sprintf(string, "%d", packet->header.sequence_number);
				}

				N_PRINT("%4s, ", string);
			}
			N_PRINT("]\n");

			N_PRINT("Sent packets:     [");
			for(U32 i = 0; i < 32; ++i)
			{
				N_Packet *packet = peer->sent_packets + i;
				char string[256] = {0};
				if(packet->header.reliable)
				{
					sprintf(string, "r%d", packet->header.sequence_number);
				}
				else
				{
					sprintf(string, "%d", packet->header.sequence_number);
				}

				N_PRINT("%4s, ", string);
			}
			N_PRINT("]\n");
#endif
		}
	}
	return result;
}

internal N_PacketList *
N_Client_RecieveFromHost(N_Client *client, MemoryArena *arena, F64 dt)
{
	Assert(client);
	Assert(arena);

	client->host.time_since_last_recieve += dt;

	N_PacketList *result = N_RecieveAllPackets(arena);

	for(N_PacketNode *node = result->first;
			node != 0;
			node = node->next)
	{
		N_Packet *packet = node->packet;

		if(AddressMatch(node->sender, client->host.address))
		{
			N_ProccessRecievedPacket(&client->host, packet);
			client->host.time_since_last_recieve = 0;

			B32 first_packet = packet->header.sequence_number == 0;
			if(packet->header.sequence_number > client->host.remote_sequence_number || first_packet)
			{
				// NOTE(hampus): The packet came in order
				client->host.remote_sequence_number = packet->header.sequence_number;
			}
			else if(packet->header.sequence_number == client->host.remote_sequence_number)
			{
				// NOTE(hampus): The packet came as a duplicate
				DLL_Remove(result->first, result->last, node);
			}
			else
			{
				// NOTE(hampus): The packet came out of order probably

				// If it is a non-reliable packet, we don't care 
				// if it came out-of-order
				if(!packet->header.reliable)
				{
					DLL_Remove(result->first, result->last, node);
				}
			}
		}
		else
		{
			DLL_Remove(result->first, result->last, node);
		}
	}

	N_ProcessSentPackets(&client->host, dt);

#if NET_DEBUG
	N_PRINT("Recieved packets: [");
	for(U32 i = 0; i < 33; ++i)
	{
		N_Packet *packet = client->host.recieved_packets + i;
		char string[256] = {0};
		if(packet->header.reliable)
		{
			sprintf(string, "r%d", packet->header.sequence_number);
		}
		else
		{
			sprintf(string, "%d", packet->header.sequence_number);
		}

		N_PRINT("%4s, ", string);
	}
	N_PRINT("]\n");

	N_PRINT("Sent packets:     [");
	for(U32 i = 0; i < 32; ++i)
	{
		N_Packet *packet = client->host.sent_packets + i;
		char string[256] = {0};
		if(packet->header.reliable)
		{
			sprintf(string, "r%d", packet->header.sequence_number);
		}
		else
		{
			sprintf(string, "%d", packet->header.sequence_number);
		}

		N_PRINT("%4s, ", string);
	}
	N_PRINT("]\n");
#endif
	return result;
}

internal void
N_Open(U16 port)
{
	SocketOpen(&net_state.socket, port);
	SocketSetNonBlocking(net_state.socket);
}

internal B32
N_Client_ConnectToServer(N_Client *client, Address address)
{
	// NOTE(hampus): Send connection request and wait for answeer
	B32 accepted = false;

	client->status = N_ClientStatus_Connecting;

	{
		N_Packet packet = {0};
		packet.header.connect_request = true;
		N_SendPacket(address, &packet);
	}

	Address sender;
	U8 recieve_data[256] = {0};
	S32 bytes_recieved = SocketRecieve(net_state.socket,
																		 &sender,
																		 recieve_data,
																		 ArrayCount(recieve_data));

	F64 start_timer = OS_SecondsSinceAppStart();
	F32 time_elapsed = 0;

	while(!bytes_recieved)
	{
		bytes_recieved = SocketRecieve(net_state.socket,
																	 &sender,
																	 recieve_data,
																	 ArrayCount(recieve_data));

		if(time_elapsed > 1)
		{
			printf("Resent connection request\n");
			N_Packet packet = {0};
			packet.header.connect_request = true;
			N_SendPacket(address, &packet);

			time_elapsed -= 1;
		}

		F64 end_timer = OS_SecondsSinceAppStart();

		time_elapsed += (F32)(end_timer - start_timer);

		start_timer = end_timer;
	}

	if(bytes_recieved)
	{
		N_Packet *packet = (N_Packet *)recieve_data;
		accepted = packet->header.connect_request;
	}

	if(accepted)
	{
		client->status = N_ClientStatus_Connected;
		client->host.address = address;
		client->host.valid = true;
		SocketSetNonBlocking(net_state.socket);
	}
	else
	{
		client->status = N_ClientStatus_Disconnected;
	}

	return(accepted);
}

internal B32
N_PeerIsValid(N_Peer *peer)
{
	return(peer->valid);
}