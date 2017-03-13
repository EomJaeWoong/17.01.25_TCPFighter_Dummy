#ifndef __CHARACTER__H__
#define __CHARACTER__H__

struct st_CHARACTER
{
	st_SESSION *pSession;
	DWORD dwSessionID;

	DWORD dwAction;
	DWORD dwActionTick;

	BYTE byDirection;
	BYTE byMoveDirection;

	BYTE byAITick;
	BYTE byActionTick;

	short shX;
	short shY;

	char chHP;
};

typedef map<DWORD, st_CHARACTER *> Character;

#endif