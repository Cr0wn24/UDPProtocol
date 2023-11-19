function void
TickUpdateClient(App *app)
{
	TempMemoryArena scratch = GetScratch(0, 0);

	// Recieve all the network events from the host
	N_NetEventList *net_event_list = NE_RecieveNetEventsFromHost(&app->client, scratch.arena, TICK_LENGTH);

	// Iterate the events and update accordingly
	for(N_NetEventNode *node = net_event_list->first;
			node != 0;
			node = node->next)
	{
		N_NetEvent *event = node->event;
		B32 consumed_event = false;
		switch(event->type)
		{
			case N_NetEventType_Recieve:
			{
				printf("Recieved %d\n", event->data[0]);

				consumed_event = true;
			} break;

			case N_NetEventType_Disconnect:
			{
				// We were disconnected. Return back
				printf("Disconnect\n");

				return;
			} break;
		}

		// If the event was of interest, consume it and remove from the list
		if(consumed_event)
		{
			DLL_Remove(net_event_list->first, net_event_list->last, node);
		}
	}

	// Send data to the host
	U32 x = app->packet_count++;
	NE_SendDataToPeer(&app->client.host, &x, sizeof(x), true);

	ReleaseScratch(scratch);
}

function B32
UpdateClient(App *app, F64 dt)
{
	B32 exit = false;

	if(!app->initialized)
	{
		printf("Client\n");

		// Open socket on port 0
		N_Open(0);

		// Give the host some time to set up
		OS_Sleep(500);

		// Attempt to connect to 127:0:0:1:12345 (localhost)
		if(N_Client_ConnectToServer(&app->client, CreateAddress(127, 0, 0, 1, 12345)))
		{
			// Connection established
			printf("Connection established\n");
		}
		else
		{
			// Connection declined
			printf("Connection declined\n");
		}

		app->initialized = true;
	}

	switch(app->client.status)
	{
		case N_ClientStatus_Connected:
		{
			while(app->tick_time_elapsed >= TICK_LENGTH)
			{
				TickUpdateClient(app);

				app->tick_time_elapsed -= TICK_LENGTH;

				++app->tick;
			}

			app->time_elapsed += dt;
			app->tick_time_elapsed += dt;

		} break;
	}

	return(exit);
}