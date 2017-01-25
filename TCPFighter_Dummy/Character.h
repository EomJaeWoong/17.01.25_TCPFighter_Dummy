#ifndef __CHARACTER__H__
#define __CHARACTER__H__

struct st_CHARACTER
{
	st_SESSTION *pSession;
	DWORD dwSessionID;

	DWORD dwAction;
	DWORD dwActionTick;

	BYTE byDirection;
	BYTE byMoveDirection;

	short shX;
	short shY;

	char chHP;
};

#endif