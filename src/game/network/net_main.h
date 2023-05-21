#pragma once


enum ENetType : char
{
	ENetType_Invalid,
	ENetType_Loopback,
	ENetType_Broadcast,
	ENetType_IP,
};


struct NetAddr_t
{
	ENetType aType   = ENetType_Invalid;
	bool     aIsIPV6 = false;
	short    aPort;

	union
	{
		unsigned char aIPV4[ 4 ];
		char          aIPV6[ 8 ][ 4 ];
	};
};


using Socket_t = void*;
#define CH_INVALID_SOCKET ( Socket_t )( ~0 );


const char* Net_ErrorString();

bool        Net_Init();
void        Net_Shutdown();

NetAddr_t   Net_GetNetAddrFromString( const char* spString );

Socket_t    Net_OpenSocket( const char* spPort );
void        Net_Connect();

bool        Net_GetPacket( NetAddr_t& srFrom, void* spData, int& sSize, int sMaxSize );
bool        Net_GetPacketBlocking( NetAddr_t& srFrom, void* spData, int& sSize, int sMaxSize, int sTimeOut );
void        Net_SendPacket( const NetAddr_t& srTo, const void* spData, int sSize );

// ---------------------------------------------------------------------------
// Temp Functions, unsure how long these will actually last

bool        Net_OpenServer();
void        Net_CloseServer();
void        Net_UpdateServer();

bool        Net_ConnectToServer();
void        Net_UpdateClient();
void        Net_Disconnect();

// Used to check if the program is running a server and/or is a client
bool        Net_IsServer();
bool        Net_IsClient();

