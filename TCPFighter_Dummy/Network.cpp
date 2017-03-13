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
// Client 연결하기
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
	// 클라이언트 생성 후 연결
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
		// 소켓 옵션
		//---------------------------------------------------------------------------------------------
		BOOL optval;
		retval = setsockopt(ClientSocket, SOL_SOCKET, SO_REUSEADDR, (char *)&optval, sizeof(optval));
		if (retval == SOCKET_ERROR)
		{
			wprintf(L"setsockopt() ERROR\n");
			return FALSE;
		}

		//---------------------------------------------------------------------------------------------
		// 세션 생성 후 삽입
		//---------------------------------------------------------------------------------------------
		pSession = CreateSession(ClientSocket);
		g_Session.insert(pair<SOCKET, st_SESSION *>(ClientSocket, pSession));
	}

	return TRUE;
}

/*-------------------------------------------------------------------------------------*/
// 세션 생성
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
	// Set 검사를 위한 소켓 테이블
	//----------------------------------------------------------------------------------
	SOCKET SocketTable[FD_SETSIZE];
	int iSocketCount = 0;

	FD_SET ReadSet;
	FD_SET WriteSet;

	for (int iCnt = 0; iCnt < FD_SETSIZE; iCnt++)
		SocketTable[iCnt] = INVALID_SOCKET;

	//----------------------------------------------------------------------------------
	// ReadSet, WriteSet 초기화
	//----------------------------------------------------------------------------------
	FD_ZERO(&ReadSet);
	FD_ZERO(&WriteSet);

	//----------------------------------------------------------------------------------
	// 전체 Session에 대해 작업 수행
	// - ReadSet 등록
	// - SendQ에 데이터가 존재하면 WriteSet 등록
	//----------------------------------------------------------------------------------
	Session::iterator SessionIter;
	for (SessionIter = g_Session.begin(); SessionIter != g_Session.end();)
	{
		//------------------------------------------------------------------------------
		// ReadSet 등록
		//------------------------------------------------------------------------------
		FD_SET(SessionIter->second->socket, &ReadSet);

		//------------------------------------------------------------------------------
		// ReadSet, WriteSet 초기화
		//------------------------------------------------------------------------------
		if (SessionIter->second->SendQ.GetUseSize() > 0)
			FD_SET(SessionIter->second->socket, &WriteSet);

		SocketTable[iSocketCount] = SessionIter->second->socket;
		iSocketCount++;

		SessionIter++;
		//------------------------------------------------------------------------------
		// SetSize 최대일 때
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
	// 남은 Set 들에 대한 Socket처리
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
	// Select 결과 처리
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
				// 비어있는 소켓 넘어감
				//-------------------------------------------------------------------------------
				if (INVALID_SOCKET == SocketTable[iSession])
					continue;

				//-------------------------------------------------------------------------------
				// Send 처리
				//-------------------------------------------------------------------------------
				if (FD_ISSET(SocketTable[iSession], WriteSet))
				{
					netProc_Send(SocketTable[iSession]);
					break;
				}

				//---------------------------------------------------------------------------
				// Receive 처리
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
	// RecvQ에 데이터가 남아있는 한 계속 패킷 처리
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
// 패킷에 대한 처리
/*-------------------------------------------------------------------------------------*/
BOOL PacketProc(st_SESSION *pSession)
{
	st_NETWORK_PACKET_HEADER header;
	CNPacket cPacket;
	BYTE byEndCode = 0;

	//--------------------------------------------------------------------------------------*/
	//RecvQ 용량이 header보다 작은지 검사
	/*--------------------------------------------------------------------------------------*/
	if (pSession->RecvQ.GetUseSize() < sizeof(st_NETWORK_PACKET_HEADER))
		return FALSE;

	pSession->RecvQ.Peek((char *)&header, sizeof(st_NETWORK_PACKET_HEADER));

	/*--------------------------------------------------------------------------------------*/
	//header + payload 용량이 RecvQ용량보다 큰지 검사
	/*--------------------------------------------------------------------------------------*/
	if (pSession->RecvQ.GetUseSize() < header.bySize + sizeof(st_NETWORK_PACKET_HEADER))
		return FALSE;

	pSession->RecvQ.RemoveData(sizeof(st_NETWORK_PACKET_HEADER));

	/*--------------------------------------------------------------------------------------*/
	//payload를 cPacket에 뽑고 같은지 검사
	/*--------------------------------------------------------------------------------------*/
	if (header.bySize !=
		pSession->RecvQ.Get((char *)cPacket.GetBufferPtr(), header.bySize))
		return FALSE;

	pSession->RecvQ.Get((char *)&byEndCode, 1);
	if (dfNETWORK_PACKET_END != byEndCode)
		return FALSE;

	/*--------------------------------------------------------------------------------------*/
	// Message 타입에 따른 Packet 처리
	/*--------------------------------------------------------------------------------------*/
	switch (header.byType)
	{
		//////////////////////////////////////////////////////////////////////////////////////////
		// 내 캐릭터 생성
		//////////////////////////////////////////////////////////////////////////////////////////
	case dfPACKET_SC_CREATE_MY_CHARACTER:
		recvProc_CreateMyCharacter(&cPacket);
		break;

		//////////////////////////////////////////////////////////////////////////////////////////
		// 다른 캐릭터 생성
		//////////////////////////////////////////////////////////////////////////////////////////
	case dfPACKET_SC_CREATE_OTHER_CHARACTER:
		break;

		/////////////////////////////////////////////////////////////////////////////////////////
		// 캐릭터 제거
		/////////////////////////////////////////////////////////////////////////////////////////
	case dfPACKET_SC_DELETE_CHARACTER:
		break;

		/////////////////////////////////////////////////////////////////////////////////////////
		// 움직임 시작
		/////////////////////////////////////////////////////////////////////////////////////////
	case dfPACKET_SC_MOVE_START:
		break;

		/////////////////////////////////////////////////////////////////////////////////////////
		// 움직임 멈춤
		/////////////////////////////////////////////////////////////////////////////////////////
	case dfPACKET_SC_MOVE_STOP:
		break;

		/////////////////////////////////////////////////////////////////////////////////////////
		// 공격 1
		/////////////////////////////////////////////////////////////////////////////////////////
	case dfPACKET_SC_ATTACK1:
		break;

		/////////////////////////////////////////////////////////////////////////////////////////
		// 공격 2
		/////////////////////////////////////////////////////////////////////////////////////////
	case dfPACKET_SC_ATTACK2:
		break;

		/////////////////////////////////////////////////////////////////////////////////////////
		// 공격 3
		/////////////////////////////////////////////////////////////////////////////////////////
	case dfPACKET_SC_ATTACK3:
		break;

		/////////////////////////////////////////////////////////////////////////////////////////
		// 데미지
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
// 클라이언트 자신의 캐릭터 할당
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
	// 캐릭터 생성
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
// 캐릭터 데미지
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
// 캐릭터 이동 시작
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
// 캐릭터 이동 중지
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
// 캐릭터 공격1
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
// 캐릭터 공격2
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
// 캐릭터 공격3
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
// 캐릭터 이동 시작 패킷
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
// 캐릭터 이동 중지 패킷
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
// 캐릭터 공격1 패킷
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
// 캐릭터 공격2 패킷
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
// 캐릭터 공격3 패킷
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
// 세션 찾기
/*-------------------------------------------------------------------------------------*/
st_SESSION *FindSession(SOCKET socket)
{
	Session::iterator sessionIter;
	sessionIter = g_Session.find(socket);

	return sessionIter->second;
}