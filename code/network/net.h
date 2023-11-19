#ifndef NET_H
#define NET_H

typedef struct N_PacketHeader
{
	B32 connect_request;
	B32 reliable;

	U32 sequence_number;
	U32 ack;
	U32 ack_bitfield;
	U32 data_size;
} N_PacketHeader;

typedef struct N_Packet
{
	N_PacketHeader header;
	U32 data[256];
} N_Packet;

typedef struct N_PacketNode
{
	struct N_PacketNode *next;
	struct N_PacketNode *prev;

	union
	{
		Address sender;
		Address destination;
	};
	N_Packet *packet;
} N_PacketNode;

typedef struct N_PacketList
{
	N_PacketNode *first;
	N_PacketNode *last;
} N_PacketList;

typedef struct N_Peer
{
	B32 valid;
	Address address;

	U32 remote_sequence_number;
	U32 local_sequence_number;

	B32 sent[32];
	F64 time_since_send[32];
	N_Packet sent_packets[32];

	N_Packet recieved_packets[33];

	U32 reliable_packets_waiting_count;
	U32 reliable_packet_pos;
	N_Packet reliable_packet_buffer[256];
	B32 reliable_packet_in_flight;
	U32 reliable_sequence_number_in_flight;

	F64 time_since_last_recieve;
} N_Peer;

typedef struct N_PeerPacketNode
{
	struct N_PeerPacketNode *next;
	struct N_PeerPacketNode *prev;

	N_Peer *peer;
	N_Packet *packet;
} N_PeerPacketNode;

typedef struct N_FreePacket
{
	struct N_FreePacket *next;
} N_FreePacket;

typedef struct N_PacketStorage
{
	N_Packet *packet_array;
	U32 packet_array_size;
	N_FreePacket *next_free_packet;
} N_PacketStorage;

typedef struct N_NetState
{
	// NOTE(hampus): This only used to store
	// reliable packets since we need to remember them
	N_PacketStorage packet_storage;
	Socket socket;
} N_NetState;

typedef struct N_PeerPacketList
{
	N_PeerPacketNode *first;
	N_PeerPacketNode *last;
} N_PeerPacketList;

typedef enum N_ClientStatus
{
	N_ClientStatus_Disconnected,
	N_ClientStatus_Connecting,
	N_ClientStatus_Connected,
} N_ClientStatus;

typedef struct N_Client
{
	N_ClientStatus status;
	N_Peer host;
} N_Client;

#define MAX_NUM_CLIENTS 16
typedef struct N_Host
{
	N_Peer clients[MAX_NUM_CLIENTS];
} N_Host;

#endif
