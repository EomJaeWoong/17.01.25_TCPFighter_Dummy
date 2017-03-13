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
#include "Content.h"
#include "Network.h"
#include "Log.h"

#pragma comment(lib, "Ws2_32.lib")

Session g_Session;

bool netSetup()
{
	WSADATA wsa;

	if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
		return FALSE;

	return TRUE;
}

/*----------------------------------------------------------------------------------------------------*/
// Client �����ϱ�
/*----------------------------------------------------------------------------------------------------*/
bool ClientConnect()
{
	int retval;

	st_SESSION *pSession;
	SOCKET ClientSocket;

	SOCKADDR_IN sockaddr;
	sockaddr.sin_family = AF_INET;
	sockaddr.sin_port = htons(dfNETWORK_PORT);
	InetPton(AF_INET, L"127.0.0.1", &sockaddr.sin_addr);

	//--------------------------------------------------------------------------------------------------
	// Ŭ���̾�Ʈ ���� �� ����
	//--------------------------------------------------------------------------------------------------
	for (int iCnt = 0; iCnt < dfMAX_CLIENT; iCnt++)
	{
		ClientSocket = socket(AF_INET, SOCK_STREAM, 0);
		if (ClientSocket == INVALID_SOCKET)
			return FALSE;

		retval = connect(ClientSocket, (SOCKADDR *)&sockaddr, sizeof(SOCKADDR_IN));
		if (retval == SOCKET_ERROR)
		{
			wprintf(L"connect() ERROR\n");
			return FALSE;
		}

		//---------------------------------------------------------------------------------------------
		// ���� �ɼ�
		//---------------------------------------------------------------------------------------------
		BOOL optval;
		retval = setsockopt(ClientSocket, SOL_SOCKET, SO_REUSEADDR, (char *)&optval, sizeof(optval));
		if (retval == SOCKET_ERROR)
		{
			wprintf(L"setsockopt() ERROR\n");
			return FALSE;
		}

		//---------------------------------------------------------------------------------------------
		// ���� ���� �� ����
		//---------------------------------------------------------------------------------------------
		pSession = CreateSession(ClientSocket);
		g_Session.insert(pair<SOCKET, st_SESSION *>(ClientSocket, pSession));
	}

	return TRUE;
}

/*-------------------------------------------------------------------------------------*/
// ���� ����
/*-------------------------------------------------------------------------------------*/
st_SESSION *CreateSession(SOCKET socket)
{
	st_SESSION *pSession = new st_SESSION;

	pSession->socket = socket;
	pSession->dwSessionID = 0xffffffff;

	pSession->RecvQ.ClearBuffer();
	pSession->SendQ.ClearBuffer();

	return pSession;
}

/*-------------------------------------------------------------------------------------*/
// Network IO
/*-------------------------------------------------------------------------------------*/
void netIOProcess()
{
	//----------------------------------------------------------------------------------
	// Set �˻縦 ���� ���� ���̺�
	//----------------------------------------------------------------------------------
	SOCKET SocketTable[FD_SETSIZE];
	int iSocketCount = 0;

	FD_SET ReadSet;
	FD_SET WriteSet;

	for (int iCnt = 0; iCnt < FD_SETSIZE; iCnt++)
		SocketTable[iCnt] = INVALID_SOCKET;

	//----------------------------------------------------------------------------------
	// ReadSet, WriteSet �ʱ�ȭ
	//----------------------------------------------------------------------------------
	FD_ZERO(&ReadSet);
	FD_ZERO(&WriteSet);

	//----------------------------------------------------------------------------------
	// ��ü Session�� ���� �۾� ����
	// - ReadSet ���
	// - SendQ�� �����Ͱ� �����ϸ� WriteSet ���
	//----------------------------------------------------------------------------------
	Session::iterator SessionIter;
	for (SessionIter = g_Session.begin(); SessionIter != g_Session.end();)
	{
		//------------------------------------------------------------------------------
		// ReadSet ���
		//------------------------------------------------------------------------------
		FD_SET(SessionIter->second->socket, &ReadSet);

		//------------------------------------------------------------------------------
		// ReadSet, WriteSet �ʱ�ȭ
		//------------------------------------------------------------------------------
		if (SessionIter->second->SendQ.GetUseSize() > 0)
			FD_SET(SessionIter->second->socket, &WriteSet);

		SocketTable[iSocketCount] = SessionIter->second->socket;
		iSocketCount++;

		SessionIter++;
		//------------------------------------------------------------------------------
		// SetSize �ִ��� ��
		//------------------------------------------------------------------------------
		if (iSocketCount >= FD_SETSIZE)
		{
			SelectProc(SocketTable, &ReadSet, &WriteSet, iSocketCount);

			FD_ZERO(&ReadSet);
			FD_ZERO(&WriteSet);

			iSocketCount = 0;

			for (int iCnt = 0; iCnt < FD_SETSIZE; iCnt++)
				SocketTable[iCnt] = INVALID_SOCKET;
		}
	}

	//---------------------------------------------------------------------------------
	// ���� Set �鿡 ���� Socketó��
	//---------------------------------------------------------------------------------
	if (iSocketCount > 0)
		SelectProc(SocketTable, &ReadSet, &WriteSet, iSocketCount);
}

void SelectProc(SOCKET *SocketTable, FD_SET *ReadSet, FD_SET *WriteSet, int iSize)
{
	int retval;

	Session::iterator SessionIter;

	TIMEVAL Time;
	Time.tv_sec = 0;
	Time.tv_usec = 0;

	//---------------------------------------------------------------------------------
	// Select
	//---------------------------------------------------------------------------------
	retval = select(0, ReadSet, WriteSet, NULL, &Time);

	//---------------------------------------------------------------------------------
	// Select ��� ó��
	//---------------------------------------------------------------------------------
	if (retval == 0)		return;

	else if (retval < 0)
	{
		DWORD dwError = GetLastError();
		wprintf(L"Select() Error : %d\n", dwError);
		exit(1);
	}

	else
	{
		for (int iSession = 0; iSession < FD_SETSIZE; iSession++)
		{
			for (int iResult = 0; iResult < retval; iResult++)
			{
				//-------------------------------------------------------------------------------
				// ����ִ� ���� �Ѿ
				//-------------------------------------------------------------------------------
				if (INVALID_SOCKET == SocketTable[iSession])
					continue;

				//-------------------------------------------------------------------------------
				// Send ó��
				//-------------------------------------------------------------------------------
				if (FD_ISSET(SocketTable[iSession], WriteSet))
				{
					netProc_Send(SocketTable[iSession]);
					break;
				}

				//---------------------------------------------------------------------------
				// Receive ó��
				//---------------------------------------------------------------------------
				if (FD_ISSET(SocketTable[iSession], ReadSet))
				{
					netProc_Recv(SocketTable[iSession]);

					break;
				}
			}
		}
	}
}

/*-------------------------------------------------------------------------------------*/
// Send Process
/*-------------------------------------------------------------------------------------*/
BOOL netProc_Send(SOCKET socket)
{
	int retval;
	st_SESSION *pSession = FindSession(socket);

	while (pSession->SendQ.GetUseSize() > 0)
	{
		retval = send(socket, pSession->SendQ.GetReadBufferPtr(),
			pSession->SendQ.GetNotBrokenGetSize(), 0);
		pSession->SendQ.RemoveData(retval);

		if (retval == 0)
			return FALSE;

		else if (retval < 0)
		{
			_LOG(dfLOG_LEVEL_ERROR, L"Send Error - SessionID : %d", pSession->dwSessionID);
			//DisconnectClient(pSession->dwSessionID);
			return FALSE;
		}
	}

	return TRUE;
}

/*-------------------------------------------------------------------------------------*/
// Receive Process
/*-------------------------------------------------------------------------------------*/
void netProc_Recv(SOCKET socket)
{
	int retval;
	st_SESSION *pSession = FindSession(socket);

	retval = recv(pSession->socket, pSession->RecvQ.GetWriteBufferPtr(),
		pSession->RecvQ.GetNotBrokenPutSize(), 0);
	pSession->RecvQ.MoveWritePos(retval);

	if (retval <= 0)
	{
		//Disconnect
		wprintf(L"Recv Error\n");
		return;
	}

	//-----------------------------------------------------------------------------------
	// RecvQ�� �����Ͱ� �����ִ� �� ��� ��Ŷ ó��
	//-----------------------------------------------------------------------------------
	while (pSession->RecvQ.GetUseSize() > 0)
	{
		if (retval < 0)
		{
			//Disconnect
			return;
		}

		else
			PacketProc(pSession);
	}
}

/*-------------------------------------------------------------------------------------*/
// ��Ŷ�� ���� ó��
/*-------------------------------------------------------------------------------------*/
BOOL PacketProc(st_SESSION *pSession)
{
	st_NETWORK_PACKET_HEADER header;
	CNPacket cPacket;
	BYTE byEndCode = 0;

	//--------------------------------------------------------------------------------------*/
	//RecvQ �뷮�� header���� ������ �˻�
	/*--------------------------------------------------------------------------------------*/
	if (pSession->RecvQ.GetUseSize() < sizeof(st_NETWORK_PACKET_HEADER))
		return FALSE;

	pSession->RecvQ.Peek((char *)&header, sizeof(st_NETWORK_PACKET_HEADER));

	/*--------------------------------------------------------------------------------------*/
	//header + payload �뷮�� RecvQ�뷮���� ū�� �˻�
	/*--------------------------------------------------------------------------------------*/
	if (pSession->RecvQ.GetUseSize() < header.bySize + sizeof(st_NETWORK_PACKET_HEADER))
		return FALSE;

	pSession->RecvQ.RemoveData(sizeof(st_NETWORK_PACKET_HEADER));

	/*--------------------------------------------------------------------------------------*/
	//payload�� cPacket�� �̰� ������ �˻�
	/*--------------------------------------------------------------------------------------*/
	if (header.bySize !=
		pSession->RecvQ.Get((char *)cPacket.GetBufferPtr(), header.bySize))
		return FALSE;

	pSession->RecvQ.Get((char *)&byEndCode, 1);
	if (dfNETWORK_PACKET_END != byEndCode)
		return FALSE;

	/*--------------------------------------------------------------------------------------*/
	// Message Ÿ�Կ� ���� Packet ó��
	/*--------------------------------------------------------------------------------------*/
	switch (header.byType)
	{
		//////////////////////////////////////////////////////////////////////////////////////////
		// �� ĳ���� ����
		//////////////////////////////////////////////////////////////////////////////////////////
	case dfPACKET_SC_CREATE_MY_CHARACTER:
		recvProc_CreateMyCharacter(&cPacket);
		break;

		//////////////////////////////////////////////////////////////////////////////////////////
		// �ٸ� ĳ���� ����
		//////////////////////////////////////////////////////////////////////////////////////////
	case dfPACKET_SC_CREATE_OTHER_CHARACTER:
		break;

		/////////////////////////////////////////////////////////////////////////////////////////
		// ĳ���� ����
		/////////////////////////////////////////////////////////////////////////////////////////
	case dfPACKET_SC_DELETE_CHARACTER:
		break;

		/////////////////////////////////////////////////////////////////////////////////////////
		// ������ ����
		/////////////////////////////////////////////////////////////////////////////////////////
	case dfPACKET_SC_MOVE_START:
		break;

		/////////////////////////////////////////////////////////////////////////////////////////
		// ������ ����
		/////////////////////////////////////////////////////////////////////////////////////////
	case dfPACKET_SC_MOVE_STOP:
		break;

		/////////////////////////////////////////////////////////////////////////////////////////
		// ���� 1
		/////////////////////////////////////////////////////////////////////////////////////////
	case dfPACKET_SC_ATTACK1:
		break;

		/////////////////////////////////////////////////////////////////////////////////////////
		// ���� 2
		/////////////////////////////////////////////////////////////////////////////////////////
	case dfPACKET_SC_ATTACK2:
		break;

		/////////////////////////////////////////////////////////////////////////////////////////
		// ���� 3
		/////////////////////////////////////////////////////////////////////////////////////////
	case dfPACKET_SC_ATTACK3:
		break;

		/////////////////////////////////////////////////////////////////////////////////////////
		// ������
		/////////////////////////////////////////////////////////////////////////////////////////
	case dfPACKET_SC_DAMAGE:
		//recvProc_Damage(&cPacket);
		break;

	case dfPACKET_SC_SYNC:
		break;
	}

	return TRUE;
}

/*----------------------------------------------------------------------------*/
// Receive Packet

//------------------------------------------------------------------------------
// Ŭ���̾�Ʈ �ڽ��� ĳ���� �Ҵ�
//------------------------------------------------------------------------------
BOOL recvProc_CreateMyCharacter(CNPacket *pPacket)
{
	unsigned int	ID;
	BYTE			Direction;
	short			X;
	short			Y;
	BYTE			HP;

	*pPacket >> ID;
	*pPacket >> Direction;
	*pPacket >> X;
	*pPacket >> Y;
	*pPacket >> HP;

	//-------------------------------------------------------------------------
	// ĳ���� ����
	//-------------------------------------------------------------------------
	Session::iterator SessionIter;
	for (SessionIter = g_Session.begin(); SessionIter != g_Session.end(); SessionIter++)
	{
		if (0xffffffff == SessionIter->second->dwSessionID)
		{
			SessionIter->second->dwSessionID = ID;
			st_CHARACTER *pClient = 
				CreateCharacter(SessionIter->second, Direction, X, Y, HP);
			g_Character.insert(pair<DWORD, st_CHARACTER *>(ID, pClient));

			break;
		}
	}
	
	return TRUE;
}

/*
//------------------------------------------------------------------------------
// ĳ���� ������
//------------------------------------------------------------------------------
BOOL recvProc_Damage(CNPacket *pPacket)
{
	Object::iterator damageIter;
	unsigned int	AttackID;
	unsigned int	DamageID;
	BYTE			DamageHP;

	*pPacket >> AttackID;
	*pPacket >> DamageID;
	*pPacket >> DamageHP;

	for (damageIter = g_Object.begin(); damageIter != g_Object.end(); ++damageIter)
	{
		if (eTYPE_PLAYER == (*damageIter)->GetObjectType())
		{
			CPlayerObject *pDamagePlayer = (CPlayerObject *)(*damageIter);

			if (DamageID == (*damageIter)->GetObjectID())
			{
				CBaseObject *pEffectObject = new CEffectObject(0, eTYPE_EFFECT,
					pDamagePlayer->GetCurX(), pDamagePlayer->GetCurY() - 70,
					4, eEFFECT_SPARK_01, eEFFECT_SPARK_04);
				g_Object.push_back(pEffectObject);

				pDamagePlayer->SetHP(DamageHP);
				return TRUE;
			}
		}
	}

	return FALSE;
}
*/
/*----------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------*/
// Send Packet

//------------------------------------------------------------------------------
// ĳ���� �̵� ����
//------------------------------------------------------------------------------
BOOL sendProc_MoveStart(st_CHARACTER *pCharacter, int iDir, int iX, int iY)
{
	st_NETWORK_PACKET_HEADER header;
	CNPacket cPacket;
	
	int len = makePacket_MoveStart(&header, &cPacket, iDir, iX, iY);

	pCharacter->pSession->SendQ.Put((char *)&header, sizeof(header));
	pCharacter->pSession->SendQ.Put((char *)cPacket.GetBufferPtr(), len);

	return TRUE;
}

//------------------------------------------------------------------------------
// ĳ���� �̵� ����
//------------------------------------------------------------------------------
BOOL sendProc_MoveStop(st_CHARACTER *pCharacter, int iDir, int iX, int iY)
{
	st_NETWORK_PACKET_HEADER header;
	CNPacket cPacket;

	int len = makePacket_MoveStop(&header, &cPacket, iDir, iX, iY);

	pCharacter->pSession->SendQ.Put((char *)&header, sizeof(header));
	pCharacter->pSession->SendQ.Put((char *)cPacket.GetBufferPtr(), len);

	return TRUE;
}

//------------------------------------------------------------------------------
// ĳ���� ����1
//------------------------------------------------------------------------------
BOOL sendProc_Attack1(st_CHARACTER *pCharacter, int iDir, int iX, int iY)
{
	st_NETWORK_PACKET_HEADER header;
	CNPacket cPacket;

	int len = makePacket_Attack1(&header, &cPacket, iDir, iX, iY);

	pCharacter->pSession->SendQ.Put((char *)&header, sizeof(header));
	pCharacter->pSession->SendQ.Put((char *)cPacket.GetBufferPtr(), len);

	return TRUE;
}

//------------------------------------------------------------------------------
// ĳ���� ����2
//------------------------------------------------------------------------------
BOOL sendProc_Attack2(st_CHARACTER *pCharacter, int iDir, int iX, int iY)
{
	st_NETWORK_PACKET_HEADER header;
	CNPacket cPacket;

	int len = makePacket_Attack2(&header, &cPacket, iDir, iX, iY);

	pCharacter->pSession->SendQ.Put((char *)&header, sizeof(header));
	pCharacter->pSession->SendQ.Put((char *)cPacket.GetBufferPtr(), len);

	return TRUE;
}

//------------------------------------------------------------------------------
// ĳ���� ����3
//------------------------------------------------------------------------------
BOOL sendProc_Attack3(st_CHARACTER *pCharacter, int iDir, int iX, int iY)
{
	st_NETWORK_PACKET_HEADER header;
	CNPacket cPacket;

	int len = makePacket_Attack3(&header, &cPacket, iDir, iX, iY);

	pCharacter->pSession->SendQ.Put((char *)&header, sizeof(header));
	pCharacter->pSession->SendQ.Put((char *)cPacket.GetBufferPtr(), len);

	return TRUE;
}
/*----------------------------------------------------------------------------*/


/*----------------------------------------------------------------------------*/
// Make Packet

//------------------------------------------------------------------------------
// ĳ���� �̵� ���� ��Ŷ
//------------------------------------------------------------------------------
int makePacket_MoveStart(st_NETWORK_PACKET_HEADER *pHeader, CNPacket *pPacket,
	int iDir, int iX, int iY)
{
	*pPacket << (BYTE)iDir;
	*pPacket << (short)iX;
	*pPacket << (short)iY;
	*pPacket << (BYTE)dfNETWORK_PACKET_END;

	pHeader->byCode = dfNETWORK_PACKET_CODE;
	pHeader->bySize = pPacket->GetDataSize() - 1;
	pHeader->byType = dfPACKET_CS_MOVE_START;

	return pPacket->GetDataSize();
}

//------------------------------------------------------------------------------
// ĳ���� �̵� ���� ��Ŷ
//------------------------------------------------------------------------------
int makePacket_MoveStop(st_NETWORK_PACKET_HEADER *pHeader, CNPacket *pPacket,
	int iDir, int iX, int iY)
{
	*pPacket << (BYTE)iDir;
	*pPacket << (short)iX;
	*pPacket << (short)iY;
	*pPacket << (BYTE)dfNETWORK_PACKET_END;

	pHeader->byCode = dfNETWORK_PACKET_CODE;
	pHeader->bySize = pPacket->GetDataSize() - 1;
	pHeader->byType = dfPACKET_CS_MOVE_STOP;

	return pPacket->GetDataSize();
}

//------------------------------------------------------------------------------
// ĳ���� ����1 ��Ŷ
//------------------------------------------------------------------------------
int makePacket_Attack1(st_NETWORK_PACKET_HEADER *pHeader, CNPacket *pPacket,
	int iDir, int iX, int iY)
{
	*pPacket << (BYTE)iDir;
	*pPacket << (short)iX;
	*pPacket << (short)iY;
	*pPacket << (BYTE)dfNETWORK_PACKET_END;

	pHeader->byCode = dfNETWORK_PACKET_CODE;
	pHeader->bySize = pPacket->GetDataSize() - 1;
	pHeader->byType = dfPACKET_CS_ATTACK1;

	return pPacket->GetDataSize();
}

//------------------------------------------------------------------------------
// ĳ���� ����2 ��Ŷ
//------------------------------------------------------------------------------
int makePacket_Attack2(st_NETWORK_PACKET_HEADER *pHeader, CNPacket *pPacket,
	int iDir, int iX, int iY)
{
	*pPacket << (BYTE)iDir;
	*pPacket << (short)iX;
	*pPacket << (short)iY;
	*pPacket << (BYTE)dfNETWORK_PACKET_END;

	pHeader->byCode = dfNETWORK_PACKET_CODE;
	pHeader->bySize = pPacket->GetDataSize() - 1;
	pHeader->byType = dfPACKET_CS_ATTACK2;

	return pPacket->GetDataSize();
}

//------------------------------------------------------------------------------
// ĳ���� ����3 ��Ŷ
//------------------------------------------------------------------------------
int makePacket_Attack3(st_NETWORK_PACKET_HEADER *pHeader, CNPacket *pPacket,
	int iDir, int iX, int iY)
{
	*pPacket << (BYTE)iDir;
	*pPacket << (short)iX;
	*pPacket << (short)iY;
	*pPacket << (BYTE)dfNETWORK_PACKET_END;

	pHeader->byCode = dfNETWORK_PACKET_CODE;
	pHeader->bySize = pPacket->GetDataSize() - 1;
	pHeader->byType = dfPACKET_CS_ATTACK3;

	return pPacket->GetDataSize();
}
/*----------------------------------------------------------------------------*/

/*-------------------------------------------------------------------------------------*/
// ���� ã��
/*-------------------------------------------------------------------------------------*/
st_SESSION *FindSession(SOCKET socket)
{
	Session::iterator sessionIter;
	sessionIter = g_Session.find(socket);

	return sessionIter->second;
}