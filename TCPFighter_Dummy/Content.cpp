#include <windows.h>
#include <time.h>
#include <map>

using namespace std;

#include "PacketDefine.h"
#include "StreamQueue.h"
#include "ObjectType.h"
#include "NPacket.h"
#include "Session.h"
#include "Character.h"
#include "Network.h"
#include "Content.h"

#pragma comment (lib, "winmm.lib")

st_CHARACTER *CreateCharacter(st_SESSION *pSession, char Direction,
	short shX, short shY, char chHP)
{
	st_CHARACTER *pCharacter = new st_CHARACTER;

	pCharacter->pSession = pSession;
	pCharacter->dwSessionID = pSession->dwSessionID;

	pCharacter->shX = shX;
	pCharacter->shY = shY;

	pCharacter->dwActionTick = 0;
	pCharacter->dwAction = dfACTION_STAND;

	pCharacter->byDirection = Direction;
	pCharacter->byMoveDirection = dfACTION_STAND;

	pCharacter->byAITick = timeGetTime();
	pCharacter->byActionTick = 0;

	pCharacter->chHP = chHP;

	return pCharacter;
}

void Update()
{
	srand(time(NULL));
	int iCount = 0;

	Character::iterator cIter;
	for (cIter = g_Character.begin(); cIter != g_Character.end(); cIter++)
	{
		if (timeGetTime() - cIter->second->byAITick > 3000)
		{
			cIter->second->byActionTick = rand() % 12;
			iCount++;
		}

		if (iCount > 20)		break;
	}

	
}

void Action()
{
	st_NETWORK_PACKET_HEADER Header;
	CNPacket nPacket;

	st_CHARACTER *pCharacter = NULL;

	Character::iterator cIter;
	for (cIter = g_Character.begin(); cIter != g_Character.end(); cIter++)
	{
		pCharacter = cIter->second;

		switch (pCharacter->byActionTick)
		{
		//------------------------------------------------------------------------------
		// 이동
		//------------------------------------------------------------------------------
		case dfACTION_MOVE_LL :
		case dfACTION_MOVE_LU :
		case dfACTION_MOVE_UU :
		case dfACTION_MOVE_RU :
		case dfACTION_MOVE_RR :
		case dfACTION_MOVE_RD :
		case dfACTION_MOVE_DD :
		case dfACTION_MOVE_LD :
			SetActionMove(pCharacter);
			break;

		//------------------------------------------------------------------------------
		// 정지
		//------------------------------------------------------------------------------
		case dfACTION_STAND :
			sendProc_MoveStop(pCharacter, pCharacter->byDirection, 
				pCharacter->shX, pCharacter->shY);
			break;

		//------------------------------------------------------------------------------
		// 공격
		//------------------------------------------------------------------------------
		case dfACTION_ATTACK1 :
			sendProc_Attack1(pCharacter, pCharacter->byDirection, 
				pCharacter->shX, pCharacter->shY);
			break;

		case dfACTION_ATTACK2 :
			sendProc_Attack2(pCharacter, pCharacter->byDirection,
				pCharacter->shX, pCharacter->shY);
			break;

		case dfACTION_ATTACK3 :
			sendProc_Attack3(pCharacter, pCharacter->byDirection,
				pCharacter->shX, pCharacter->shY);
			break;
		}
	}
}

/*-------------------------------------------------------------------------------------*/
// Move Action input에 대한 Move Action 셋팅
/*-------------------------------------------------------------------------------------*/
void SetActionMove(st_CHARACTER *pCharacter)
{
	int shX = -1, shY = -1;

	if (pCharacter->byMoveDirection != pCharacter->byActionTick)
	{
		switch (pCharacter->byActionTick)
		{
		case dfPACKET_MOVE_DIR_RR:
		case dfPACKET_MOVE_DIR_RU:
		case dfPACKET_MOVE_DIR_RD:
			pCharacter->byDirection = dfACTION_MOVE_RR;
			break;

		case dfPACKET_MOVE_DIR_LL:
		case dfPACKET_MOVE_DIR_LU:
		case dfPACKET_MOVE_DIR_LD:
			pCharacter->byDirection = dfACTION_MOVE_LL;
			break;
		}

		DeadReckoningPos(pCharacter->byActionTick, pCharacter->byAITick,
			pCharacter->shX, pCharacter->shY, &shX, &shY);

		pCharacter->shX = shX;
		pCharacter->shY = shY;

		pCharacter->byMoveDirection = pCharacter->byActionTick;
		pCharacter->dwAction = pCharacter->byActionTick;

		sendProc_MoveStart(pCharacter, pCharacter->byActionTick,
			pCharacter->shX, pCharacter->shY);
	}
}

/*---------------------------------------------------------------------------------------------*/
// Move Action input에 대한 Stand Action 셋팅
/*---------------------------------------------------------------------------------------------*/
void SetActionStand(st_CHARACTER *pCharacter)
{
	
}

/*-------------------------------------------------------------------------------------*/
// DeadReckoning
/*-------------------------------------------------------------------------------------*/
int DeadReckoningPos(DWORD dwAction, DWORD dwActionTick, int iOldPosX, int iOldPosY, int *pPosX, int *pPosY)
{
	// 프레임 계산
	DWORD dwIntervalTick = timeGetTime() - dwActionTick;

	int iActionFrame = dwIntervalTick / 20;
	int iRemoveFrame = 0;

	int iValue;

	int iRPosX = iOldPosX;
	int iRPosY = iOldPosY;

	//-----------------------------------------------------------------------------------
	// 계산 된 프레임으로 x, y축 좌표 이동값 구하기
	//-----------------------------------------------------------------------------------
	int iDX = iActionFrame * dfRECKONING_SPEED_PLAYER_X;
	int iDY = iActionFrame * dfRECKONING_SPEED_PLAYER_Y;

	switch (dwAction)
	{
	case dfACTION_MOVE_LL:
		iRPosX = iOldPosX - iDX;
		iRPosY = iOldPosY;
		break;

	case dfACTION_MOVE_LU:
		iRPosX = iOldPosX - iDX;
		iRPosY = iOldPosY - iDY;
		break;

	case dfACTION_MOVE_UU:
		iRPosX = iOldPosX;
		iRPosY = iOldPosY - iDY;
		break;

	case dfACTION_MOVE_RU:
		iRPosX = iOldPosX + iDX;
		iRPosY = iOldPosY - iDY;
		break;

	case dfACTION_MOVE_RR:
		iRPosX = iOldPosX + iDX;
		iRPosY = iOldPosY;
		break;

	case dfACTION_MOVE_RD:
		iRPosX = iOldPosX + iDX;
		iRPosY = iOldPosY + iDY;
		break;

	case dfACTION_MOVE_DD:
		iRPosX = iOldPosX;
		iRPosY = iOldPosY + iDY;
		break;

	case dfACTION_MOVE_LD:
		iRPosX = iOldPosX - iDX;
		iRPosY = iOldPosY + iDY;
		break;
	}

	// 화면 영역을 벗어났을 때 처리
	if (iRPosX <= dfRANGE_MOVE_LEFT)
	{
		iValue = abs(dfRANGE_MOVE_LEFT - abs(iRPosX)) / dfRECKONING_SPEED_PLAYER_X;
		iRemoveFrame = max(iValue, iRemoveFrame);
	}

	if (iRPosX >= dfRANGE_MOVE_RIGHT)
	{
		iValue = abs(dfRANGE_MOVE_RIGHT - iRPosX) / dfRECKONING_SPEED_PLAYER_X;
		iRemoveFrame = max(iValue, iRemoveFrame);
	}

	if (iRPosY <= dfRANGE_MOVE_TOP)
	{
		iValue = abs(dfRANGE_MOVE_TOP - abs(iRPosY)) / dfRECKONING_SPEED_PLAYER_Y;
		iRemoveFrame = max(iValue, iRemoveFrame);
	}

	if (iRPosY >= dfRANGE_MOVE_BOTTOM)
	{
		iValue = abs(dfRANGE_MOVE_BOTTOM - abs(iRPosY)) / dfRECKONING_SPEED_PLAYER_Y;
		iRemoveFrame = max(iValue, iRemoveFrame);
	}

	// 삭제할 프레임이 있으면 좌표를 재계산
	if (iRemoveFrame > 0)
	{
		iActionFrame -= iRemoveFrame;

		iDX = iActionFrame * dfRECKONING_SPEED_PLAYER_X;
		iDY = iActionFrame * dfRECKONING_SPEED_PLAYER_Y;

		switch (dwAction)
		{
		case dfACTION_MOVE_LL:
			iRPosX = iOldPosX - iDX;
			iRPosY = iOldPosY;
			break;

		case dfACTION_MOVE_LU:
			iRPosX = iOldPosX - iDX;
			iRPosY = iOldPosY - iDY;
			break;

		case dfACTION_MOVE_UU:
			iRPosX = iOldPosX;
			iRPosY = iOldPosY - iDY;
			break;

		case dfACTION_MOVE_RU:
			iRPosX = iOldPosX + iDX;
			iRPosY = iOldPosY - iDY;
			break;

		case dfACTION_MOVE_RR:
			iRPosX = iOldPosX + iDX;
			iRPosY = iOldPosY;
			break;

		case dfACTION_MOVE_RD:
			iRPosX = iOldPosX + iDX;
			iRPosY = iOldPosY + iDY;
			break;

		case dfACTION_MOVE_DD:
			iRPosX = iOldPosX;
			iRPosY = iOldPosY + iDY;
			break;

		case dfACTION_MOVE_LD:
			iRPosX = iOldPosX - iDX;
			iRPosY = iOldPosY + iDY;
			break;
		}
	}

	iRPosX = min(iRPosX, dfRANGE_MOVE_RIGHT);
	iRPosX = max(iRPosX, dfRANGE_MOVE_LEFT);
	iRPosY = min(iRPosY, dfRANGE_MOVE_BOTTOM);
	iRPosY = max(iRPosY, dfRANGE_MOVE_TOP);

	*pPosX = iRPosX;
	*pPosY = iRPosY;

	return iActionFrame;
}