#include <WinSock2.h>
#include <WS2tcpip.h>
#include "Network.h"

#pragma comment(lib, "Ws2_32.lib")

bool netSetup()
{
	int retval;
	WSADATA wsa;

	if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
		return FALSE;

	return TRUE;
}