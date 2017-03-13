#ifndef __CONTENT__H__
#define __CONTENT__H__

st_CHARACTER *CreateCharacter(st_SESSION *pSession, char Direction,
	short shX, short shY, char chHP);	//캐릭터 생성

void Update();
void Action();

void SetActionMove(DWORD actionMove);
int DeadReckoningPos(DWORD dwAction, DWORD dwActionTick,
	int iOldPosX, int iOldPosY, int *pPosX, int *pPosY);

extern map<DWORD, st_CHARACTER *> g_Character;

#endif