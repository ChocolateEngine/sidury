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


// mimicing sockaddr struct
struct ch_sockaddr
{
	short         sa_family;
	unsigned char sa_data[ 14 ];
};


using Socket_t = void*;
#define CH_INVALID_SOCKET ( Socket_t )( ~0 )


const char* Net_ErrorString();

bool        Net_Init();
void        Net_Shutdown();

NetAddr_t   Net_GetNetAddrFromString( std::string_view sString );
char*       Net_AddrToString( ch_sockaddr& addr );
void        Net_GetSocketAddr( Socket_t sSocket, ch_sockaddr& srAddr );
int         Net_GetSocketPort( ch_sockaddr& srAddr );
void        Net_SetSocketPort( ch_sockaddr& srAddr, unsigned short sPort );

Socket_t    Net_OpenSocket( const char* spPort );
void        Net_CloseSocket( Socket_t sSocket );
int         Net_Connect( Socket_t sSocket, ch_sockaddr& srAddr );

bool        Net_GetPacket( NetAddr_t& srFrom, void* spData, int& sSize, int sMaxSize );
bool        Net_GetPacketBlocking( NetAddr_t& srFrom, void* spData, int& sSize, int sMaxSize, int sTimeOut );
void        Net_SendPacket( const NetAddr_t& srTo, const void* spData, int sSize );

Socket_t    Net_CheckNewConnections();

// Read Incoming Data from a Socket
int         Net_Read( Socket_t sSocket, char* spData, int sLen, ch_sockaddr* spFrom );

// Write Data to a Socket
int         Net_Write( Socket_t sSocket, const char* spData, int sLen, ch_sockaddr* spAddr );

int         Net_MakeSocketBroadcastCapable( Socket_t sSocket );


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

