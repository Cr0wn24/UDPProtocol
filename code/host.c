internal void
TickUpdateHost(App *app)
{
	TempMemoryArena scratch = GetScratch(0, 0);

	// Recieve all the network events from all connected clients
	N_NetEventList *net_event_list = NE_RecieveNetEventsFromClients(&app->host, scratch.arena, TICK_LENGTH);

	// Iterate the events and update accordingly
	for(N_NetEventNode *node = net_event_list->first;
			node != 0;
			node = node->next)
	{
		N_NetEvent *event = node->event;
		B32 consumed_event = false;
		switch(event->type)
		{
			case N_NetEventType_Disconnect:
			{
				// Client was disconnected. Reset their data
				printf("Client disconnected\n");
				ZeroStruct(node->peer);
				consumed_event = true;
			} break;

			case N_NetEventType_Connect:
			{
				// A client connected
				printf("Client connected\n");
				consumed_event = true;
			} break;

			case N_NetEventType_Recieve:
			{
				printf("Recieved %d\n", event->data[0]);
				consumed_event = true;
			} break;
		}

		// If the event was of interest, consume it and remove from the list
		if(consumed_event)
		{
			DLL_Remove(net_event_list->first, net_event_list->last, node);
		}
	}

	// Send data to client 0 if they are connected
	if(N_PeerIsValid(&app->host.clients[0]))
	{
		U32 x = app->packet_count++;
		NE_SendDataToPeer(&app->host.clients[0], &x, sizeof(x), true);
	}

	ReleaseScratch(scratch);
}

internal B32
UpdateHost(App *app, F64 dt)
{
	B32 exit = false;
	if(!app->initialized)
	{
		printf("Host\n");

		// Open socket on port 12345
		N_Open(12345);

		app->initialized = true;
	}

	while(app->tick_time_elapsed >= TICK_LENGTH)
	{
		TickUpdateHost(app);
		app->tick_time_elapsed -= TICK_LENGTH;

		++app->tick;
	}

	app->time_elapsed += dt;
	app->tick_time_elapsed += dt;

	return(exit);
}