#include "net_main.h"

#if 0

LOG_REGISTER_CHANNEL2( Network, LogColor::DarkRed );

static bool gOfflineMode = Args_Register( "Disable All Networking", "-offline" );
static bool gNetInit     = false;


// CONVAR( net_lan, 0 );
// CONVAR( net_loopback, 0 );


#ifdef WIN32
bool Net_InitWindows()
{
	WSADATA wsaData{};
	int     ret = WSAStartup( MAKEWORD( 2, 2 ), &wsaData );

	if ( ret != 0 )
	{
		// We could not find a usable Winsock DLL
		Log_ErrorF( gLC_Network, "WSAStartup failed with error: %d\n", ret );
		return false;
	}

	// Confirm that the WinSock DLL supports 2.2.
	// Note that if the DLL supports versions greater
	// than 2.2 in addition to 2.2, it will still return
	// 2.2 in wVersion since that is the version we requested.                                       
	if ( LOBYTE( wsaData.wVersion ) != 2 || HIBYTE( wsaData.wVersion ) != 2 )
	{
		Log_Error( gLC_Network, "Could not find a usable version of Winsock.dll\n" );
		WSACleanup();
		return false;
	}

	return true;
}


void Net_ShutdownWindows()
{
	WSACleanup();
}
#endif // WIN32


bool Net_Init()
{
	// just return true if we don't want networking
	if ( gOfflineMode )
		return true;

	bool ret = false;

#ifdef WIN32
	ret = Net_InitWindows();
#endif

	if ( !ret )
		Log_Error( gLC_Network, "Failed to Init Networking\n" );

	gNetInit = ret;
	return ret;
}


void Net_Shutdown()
{
	if ( gOfflineMode || !gNetInit )
		return;

#ifdef WIN32
	Net_ShutdownWindows();
#endif

	gNetInit = false;
}


void Net_OpenSocket()
{
}


bool Net_GetPacket( NetAddr_t& srFrom, void* spData, int& sSize, int sMaxSize )
{
	return false;
}


bool Net_GetPacketBlocking( NetAddr_t& srFrom, void* spData, int& sSize, int sMaxSize, int sTimeOut )
{
	return false;
}


void Net_SendPacket( const NetAddr_t& srTo, const void* spData, int sSize )
{
}

#endif