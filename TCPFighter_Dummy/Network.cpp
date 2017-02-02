#include <WinSock2.h>
#include <WS2tcpip.h>
#include <map>

using namespace std;

#include "PacketDefine.h"
#include "TypeDefine.h"
#include "StreamQueue.h"
#include "NPacket.h"
#include "Session.h"
#include "Character.h"
#include "Network.h"

#pragma comment(lib, "Ws2_32.lib")

Session g_Session;

bool netSetup()
{
	int retval;
	WSADATA wsa;

	if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
		return FALSE;

	return TRUE;
}

/*----------------------------------------------------------------------------------------------------*/
// Client 연결하기
/*----------------------------------------------------------------------------------------------------*/
bool ClientConnect(int iMaxClient)
{
	int retval;
	st_SESSION *pSession;
	st_CHARACTER *pClient;
	SOCKET ClientSocket;

	SOCKADDR_IN sockaddr;
	sockaddr.sin_family = AF_INET;
	sockaddr.sin_port = htons(dfNETWORK_PORT);
	InetPton(AF_INET, L"127.0.0.1", &sockaddr.sin_addr);

	//--------------------------------------------------------------------------------------------------
	// 클라이언트 생성 후 연결
	//--------------------------------------------------------------------------------------------------
	for (int iCnt = 0; iCnt < iMaxClient; iCnt++)
	{
		pSession = CreateSession(ClientSocket);

		pClient = new st_CHARACTER;
		pClient->pSession = new st_SESSION;
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

/*-------------------------------------------------------------------------------------*/
// 세션 생성
/*-------------------------------------------------------------------------------------*/
st_SESSION *CreateSession(SOCKET socket)
{
	st_SESSION *pSession = new st_SESSION;

	pSession->socket = INVALID_SOCKET;

	pSession->RecvQ.ClearBuffer();
	pSession->SendQ.ClearBuffer();

	return pSession;
}