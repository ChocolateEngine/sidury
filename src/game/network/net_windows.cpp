#include "net_main.h"

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <IPTypes.h>
#include <IPHlpApi.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <stdlib.h>
#include <stdio.h>

LOG_REGISTER_CHANNEL2( Network, LogColor::DarkCyan );

static bool        gOfflineMode    = Args_Register( "Disable All Networking", "-offline" );
static const char* gTestServerIP   = Args_Register( "127.0.0.1", "Test Server IPv4", "-ip" );
static const char* gTestServerPort = Args_Register( "27016", "Test Server Port", "-port" );

static bool        gNetInit        = false;

constexpr int      CH_NET_BUF_LEN  = 1000;


// CONVAR( net_lan, 0 );
// CONVAR( net_loopback, 0 );


#define CH_SOCKET_ERROR WSAGetLastError()

static ChVector< Socket_t > gSockets;


inline SOCKET Net_ToSysSocket( Socket_t sSocket )
{
	return reinterpret_cast< SOCKET >( sSocket );
}

inline Socket_t Net_ToSocket( SOCKET sSocket )
{
	return reinterpret_cast< Socket_t >( sSocket );
}


const char* Net_ErrorString()
{
#ifdef _WIN32
	int code = CH_SOCKET_ERROR;

	switch ( code )
	{
		case WSAEINTR:              return "WSAEINTR";
		case WSAEBADF:              return "WSAEBADF";
		case WSAEACCES:             return "WSAEACCES";
		case WSAEDISCON:            return "WSAEDISCON";
		case WSAEFAULT:             return "WSAEFAULT";
		case WSAEINVAL:             return "WSAEINVAL";
		case WSAEMFILE:             return "WSAEMFILE";
		case WSAEWOULDBLOCK:        return "WSAEWOULDBLOCK";
		case WSAEINPROGRESS:        return "WSAEINPROGRESS";
		case WSAEALREADY:           return "WSAEALREADY";
		case WSAENOTSOCK:           return "WSAENOTSOCK";
		case WSAEDESTADDRREQ:       return "WSAEDESTADDRREQ";
		case WSAEMSGSIZE:           return "WSAEMSGSIZE";
		case WSAEPROTOTYPE:         return "WSAEPROTOTYPE";
		case WSAENOPROTOOPT:        return "WSAENOPROTOOPT";
		case WSAEPROTONOSUPPORT:    return "WSAEPROTONOSUPPORT";
		case WSAESOCKTNOSUPPORT:    return "WSAESOCKTNOSUPPORT";
		case WSAEOPNOTSUPP:         return "WSAEOPNOTSUPP";
		case WSAEPFNOSUPPORT:       return "WSAEPFNOSUPPORT";
		case WSAEAFNOSUPPORT:       return "WSAEAFNOSUPPORT";
		case WSAEADDRINUSE:         return "WSAEADDRINUSE";
		case WSAEADDRNOTAVAIL:      return "WSAEADDRNOTAVAIL";
		case WSAENETDOWN:           return "WSAENETDOWN";
		case WSAENETUNREACH:        return "WSAENETUNREACH";
		case WSAENETRESET:          return "WSAENETRESET";
		case WSAECONNABORTED:       return "WSAECONNABORTED";
		case WSAECONNRESET:         return "WSAECONNRESET";
		case WSAENOBUFS:            return "WSAENOBUFS";
		case WSAEISCONN:            return "WSAEISCONN";
		case WSAENOTCONN:           return "WSAENOTCONN";
		case WSAESHUTDOWN:          return "WSAESHUTDOWN";
		case WSAETOOMANYREFS:       return "WSAETOOMANYREFS";
		case WSAETIMEDOUT:          return "WSAETIMEDOUT";
		case WSAECONNREFUSED:       return "WSAECONNREFUSED";
		case WSAELOOP:              return "WSAELOOP";
		case WSAENAMETOOLONG:       return "WSAENAMETOOLONG";
		case WSAEHOSTDOWN:          return "WSAEHOSTDOWN";
		case WSASYSNOTREADY:        return "WSASYSNOTREADY";
		case WSAVERNOTSUPPORTED:    return "WSAVERNOTSUPPORTED";
		case WSANOTINITIALISED:     return "WSANOTINITIALISED";
		case WSAHOST_NOT_FOUND:     return "WSAHOST_NOT_FOUND";
		case WSATRY_AGAIN:          return "WSATRY_AGAIN";
		case WSANO_RECOVERY:        return "WSANO_RECOVERY";
		case WSANO_DATA:            return "WSANO_DATA";
		default:                    return "NO ERROR";
	}
#else
	return strerror( CH_SOCKET_ERROR );
#endif
}


// temp
struct NetInterface_t
{
	unsigned long aIP;
	unsigned long aMask;
};

constexpr int                     CH_MAX_NET_ADAPTERS = 32;
static ChVector< NetInterface_t > gNetInterfaces;


void Net_InitAdapterInfo()
{
	PIP_ADAPTER_INFO adapterInfo = (IP_ADAPTER_INFO*)malloc( sizeof( IP_ADAPTER_INFO ) );
	if ( !adapterInfo )
	{
		Log_FatalF( gLC_Network, "Failed to allocate memory: %d bytes", sizeof( IP_ADAPTER_INFO ) );
	}

	ULONG outBufLen = sizeof( IP_ADAPTER_INFO );

	// Make an initial call to GetAdaptersInfo to get the necessary size
	if ( GetAdaptersInfo( adapterInfo, &outBufLen ) == ERROR_BUFFER_OVERFLOW )
	{
		free( adapterInfo );
		adapterInfo = (IP_ADAPTER_INFO*)malloc( outBufLen );
		if ( !adapterInfo )
		{
			Log_FatalF( gLC_Network, "Failed to allocate memory: %ld bytes", outBufLen );
		}
	}

	bool  foundloopback = false;
	DWORD ret = 0;
	if ( ( ret = GetAdaptersInfo( adapterInfo, &outBufLen ) ) != NO_ERROR )
	{
		// happens if you have no network connection
		Log_ErrorF( gLC_Network, "GetAdaptersInfo failed (%ld).\n", ret );
	}
	else
	{
		PIP_ADAPTER_INFO adapter = adapterInfo;
		while ( adapter )
		{
			PIP_ADDR_STRING ipAddrStr = &adapter->IpAddressList;
			while ( ipAddrStr )
			{
				if ( strcmp( "127.0.0.1", ipAddrStr->IpAddress.String ) == 0 )
				{
					foundloopback = true;
				}

				unsigned long addr = ntohl( inet_addr( ipAddrStr->IpAddress.String ) );
				unsigned long mask = ntohl( inet_addr( ipAddrStr->IpMask.String ) );

				// TODO: IPV6
				// unsigned long addr = ntohl( inet_pton( AF_INET, ipAddrStr->IpAddress.String, ) );
				// unsigned long mask = ntohl( inet_pton( AF_INET, ipAddrStr->IpMask.String ) );

				// Skip NULL netmasks
				if ( !mask )
				{
					Log_DevF( gLC_Network, 1, "Found Adapter: %s - %s NULL netmask - skipped\n",
					          adapter->Description, ipAddrStr->IpAddress.String );

					ipAddrStr = ipAddrStr->Next;
					continue;
				}

				Log_DevF( gLC_Network, 1, "Found Adapter: %s - %s/%s\n",
				          adapter->Description, ipAddrStr->IpAddress.String, ipAddrStr->IpMask.String );

				NetInterface_t& interface = gNetInterfaces.emplace_back();
				interface.aIP             = addr;
				interface.aMask           = mask;

				if ( gNetInterfaces.size() >= CH_MAX_NET_ADAPTERS )
				{
					Log_DevF( gLC_Network, 1, "MAX_INTERFACES(%d) hit.\n", CH_MAX_NET_ADAPTERS );
					free( adapterInfo );
					return;
				}

				ipAddrStr = ipAddrStr->Next;
			}

			adapter = adapter->Next;
		}
	}

	if ( !foundloopback && gNetInterfaces.size() < CH_MAX_NET_ADAPTERS )
	{
		Log_Dev( gLC_Network, 1, "Adding loopback Adapter\n" );

		NetInterface_t& interface = gNetInterfaces.emplace_back();
		interface.aIP             = ntohl( inet_addr( "127.0.0.1" ) );
		interface.aMask           = ntohl( inet_addr( "255.0.0.0" ) );
	}

	free( adapterInfo );
}


bool Net_Init()
{
	gNetInit = false;

	// just return true if we don't want networking
	if ( gOfflineMode )
		return true;

	WSADATA wsaData{};
	int     ret = WSAStartup( MAKEWORD( 2, 2 ), &wsaData );

	if ( ret != 0 )
	{
		// We could not find a usable Winsock DLL
		Log_ErrorF( gLC_Network, "Failed to Init Networking: WSAStartup failed with error: %d\n", ret );
		return false;
	}

	// Confirm that the WinSock DLL supports 2.2.
	// Note that if the DLL supports versions greater
	// than 2.2 in addition to 2.2, it will still return
	// 2.2 in wVersion since that is the version we requested.
	if ( LOBYTE( wsaData.wVersion ) != 2 || HIBYTE( wsaData.wVersion ) != 2 )
	{
		Log_Error( gLC_Network, "Failed to Init Networking: Could not find a usable version of Winsock.dll\n" );
		WSACleanup();
		return false;
	}

	Net_InitAdapterInfo();

	gNetInit = true;

	// temp
	if ( !Net_OpenServer() )
	{
		Log_Error( gLC_Network, "Failed to Open Test Server\n" );
		return false;
	}

	if ( !Net_ConnectToServer() )
	{
		Log_Error( gLC_Network, "Failed to Connect To Test Server\n" );
		return false;
	}

	return true;
}


void Net_Shutdown()
{
	if ( gOfflineMode || !gNetInit )
		return;

	WSACleanup();
	gNetInit = false;
}


void Net_NetadrToSockadr( const NetAddr_t* spNetAddr, struct sockaddr* spSockAddr )
{
	memset( spSockAddr, 0, sizeof( *spSockAddr ) );

	if ( spNetAddr->aType == ENetType_Broadcast )
	{
		( (struct sockaddr_in*)spSockAddr )->sin_family      = AF_INET;
		( (struct sockaddr_in*)spSockAddr )->sin_addr.s_addr = INADDR_BROADCAST;
	}
	else if ( spNetAddr->aType == ENetType_IP || spNetAddr->aType == ENetType_Loopback )
	{
		// uhhh
		( (struct sockaddr_in*)spSockAddr )->sin_family      = AF_INET;
		( (struct sockaddr_in*)spSockAddr )->sin_addr.s_addr = *(int*)&spNetAddr->aIPV4;
	}

	( (struct sockaddr_in*)spSockAddr )->sin_port = htons( (short)spNetAddr->aPort );
}


Socket_t Net_OpenSocket( const char* spPort )
{
	if ( !spPort )
		return CH_INVALID_SOCKET;

	struct addrinfo* result = NULL;
	struct addrinfo  hints;

	int              iSendResult;
	char             recvbuf[ CH_NET_BUF_LEN ];
	int              recvbuflen = CH_NET_BUF_LEN;

	memset( &hints, 0, sizeof( hints ) );
	hints.ai_family   = AF_INET;
	// hints.ai_socktype = SOCK_STREAM;
	hints.ai_socktype = SOCK_DGRAM;
	// hints.ai_protocol = IPPROTO_TCP;
	hints.ai_protocol = IPPROTO_UDP;
	hints.ai_flags    = AI_PASSIVE;

	// Resolve the server address and port
	int iResult        = getaddrinfo( NULL, spPort, &hints, &result );
	if ( iResult != 0 )
	{
		Log_ErrorF( "getaddrinfo failed with error: %d\n", iResult );
		return CH_INVALID_SOCKET;
	}

	// Create a SOCKET for the server to listen for client connections.
	SOCKET newSocket = socket( result->ai_family, result->ai_socktype, result->ai_protocol );
	if ( newSocket == INVALID_SOCKET )
	{
		Log_ErrorF( gLC_Network, "socket failed with error: %s\n", Net_ErrorString() );
		freeaddrinfo( result );
		return CH_INVALID_SOCKET;
	}

	// Make this socket non-blocking
	unsigned long nonBlock = 1;
	if ( ioctlsocket( newSocket, FIONBIO, &nonBlock ) == SOCKET_ERROR )
	{
		Log_ErrorF( gLC_Network, "Failed to make socket non-blocking ioctl FIONBIO: %s\n", Net_ErrorString() );
		return CH_INVALID_SOCKET;
	}

	// Make this socket broadcast capable
	int i = 1;
	if ( setsockopt( newSocket, SOL_SOCKET, SO_BROADCAST, (char*)&i, sizeof( i ) ) == SOCKET_ERROR )
	{
		Log_ErrorF( gLC_Network, "Failed to set socket option SO_BROADCAST: %s\n", Net_ErrorString() );
		return CH_INVALID_SOCKET;
	}

	iResult = bind( newSocket, result->ai_addr, (int)result->ai_addrlen );
	if ( iResult == SOCKET_ERROR )
	{
		Log_ErrorF( gLC_Network, "Failed to bind socket: %s\n", Net_ErrorString() );
		freeaddrinfo( result );
		closesocket( newSocket );
		return CH_INVALID_SOCKET;
	}

	Log_DevF( gLC_Network, 1, "Opened Socket on Port \"%s\"\n", gTestServerPort );

	freeaddrinfo( result );

	return Net_ToSocket( newSocket );
}


bool Net_GetPacket( Socket_t sSocket, NetAddr_t& srFrom, void* spData, int& sSize, int sMaxSize )
{
	SOCKET socket = Net_ToSysSocket( sSocket );

	return false;
}


bool Net_GetPacketBlocking( NetAddr_t& srFrom, void* spData, int& sSize, int sMaxSize, int sTimeOut )
{
	return false;
}


void Net_SendPacket( const NetAddr_t& srTo, const void* spData, int sSize )
{
}


// ---------------------------------------------------------------------------


static Socket_t gSrv_ListenSocket = CH_INVALID_SOCKET;
static Socket_t gSrv_ClientSocket = CH_INVALID_SOCKET;


// open a loopback server
bool Net_OpenServer()
{
	gSrv_ListenSocket = Net_OpenSocket( gTestServerPort );
	return gSrv_ListenSocket != CH_INVALID_SOCKET;

#if 0
	iResult = listen( gSrv_ListenSocket, SOMAXCONN );
	if ( iResult == SOCKET_ERROR )
	{
		printf( "listen failed with error: %d\n", WSAGetLastError() );
		closesocket( gSrv_ListenSocket );
		return false;
	}

	// Accept a client socket (BLOCKING OPERATION)
	gSrv_ClientSocket = accept( gSrv_ListenSocket, NULL, NULL );
	if ( gSrv_ClientSocket == INVALID_SOCKET )
	{
		printf( "accept failed with error: %d\n", WSAGetLastError() );
		closesocket( gSrv_ListenSocket );
		return false;
	}

	// No longer need server socket
	closesocket( gSrv_ListenSocket );

	// Receive until the peer shuts down the connection
	do
	{
		iResult = recv( gSrv_ClientSocket, recvbuf, recvbuflen, 0 );
		if ( iResult > 0 )
		{
			printf( "Bytes received: %d\n", iResult );

			// Echo the buffer back to the sender
			iSendResult = send( gSrv_ClientSocket, recvbuf, iResult, 0 );
			if ( iSendResult == SOCKET_ERROR )
			{
				printf( "send failed with error: %d\n", WSAGetLastError() );
				closesocket( gSrv_ClientSocket );
				return false;
			}
			printf( "Bytes sent: %d\n", iSendResult );
		}
		else if ( iResult == 0 )
			printf( "Connection closing...\n" );
		else
		{
			printf( "recv failed with error: %d\n", WSAGetLastError() );
			closesocket( gSrv_ClientSocket );
			return false;
		}

	} while ( iResult > 0 );
	
	return true;
#endif
}


void Net_CloseServer()
{
	// shutdown the connection since we're done
	// int iResult = shutdown( gSrv_ClientSocket, SD_SEND );
	// 
	// if ( iResult == SOCKET_ERROR )
	// {
	// 	printf( "shutdown failed with error: %d\n", WSAGetLastError() );
	// 	closesocket( gSrv_ClientSocket );
	// }
	// 
	// // cleanup
	// closesocket( gSrv_ClientSocket );
}


void Net_UpdateServer()
{
}


void Net_UpdateClient()
{
}


bool Net_ConnectToServer()
{
	SOCKET           ConnectSocket = INVALID_SOCKET;
	struct addrinfo *result        = NULL,
					*ptr           = NULL,
					hints;

	const char* sendbuf = "Sidury Test Connection";
	char        recvbuf[ CH_NET_BUF_LEN ];
	int         iResult;
	int         recvbuflen = CH_NET_BUF_LEN;

	return true;
}


void Net_Disconnect()
{
}


// Used to check if the program is running a server and/or is a client
bool Net_IsServer()
{
	return true;
}


bool Net_IsClient()
{
	return true;
}

