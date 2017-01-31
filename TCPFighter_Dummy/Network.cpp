#include <WinSock2.h>
#include <WS2tcpip.h>
#include <map>

using namespace std;

#include "TypeDefine.h"
#include "StreamQueue.h"
#include "Session.h"
#include "Character.h"
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

bool ClientConnect()
{
	int retval;
	st_CHARACTER *pClient;

	SOCKADDR_IN sockaddr;
	sockaddr.sin_family = AF_INET;
	sockaddr.sin_port = htons(dfNETWORK_PORT);
	InetPton(AF_INET, L"127.0.0.1", &sockaddr.sin_addr);

	for (int iCnt = 0; iCnt < dfMAX_CLIENT; iCnt++)
	{
		pClient = new st_CHARACTER;
		pClient->pSession = new st_SESSTION;
		pClient->pSession->socket = socket(AF_INET, SOCK_STREAM, 0);

		retval = connect(pClient->pSession->socket, (SOCKADDR *)&sockaddr, sizeof(SOCKADDR_IN));
		if (retval == SOCKET_ERROR)
		{
			int iErrorCode = GetLastError();
			wprintf(L"connect() ERROR\n");
			return FALSE;
		}

		g_Client.insert(pair<DWORD, st_CHARACTER *>((DWORD)iCnt, pClient));
	}

	return TRUE;
}