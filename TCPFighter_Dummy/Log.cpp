#include <winsock2.h>
#include <wchar.h>
#include <time.h>

#include "Log.h"

int				g_iLogLevel;					//���,���� ����� �α� ����
WCHAR			g_szLogBuff[1024];				//�α� ����� �ʿ��� �ӽ� ����

void Log(WCHAR *szString, int iLogLevel)
{
	time_t timer;
	tm today;

	time(&timer);

	localtime_s(&today, &timer); // �� ������ �ð��� �и��Ͽ� ����ü�� �ֱ�

	wprintf(L"[%4d/%02d/%02d][%02d:%02d:%02d] ",
		today.tm_year + 1900, today.tm_mon + 1, today.tm_mday,
		today.tm_hour, today.tm_min, today.tm_sec);

	wprintf(L"%s \n", szString);
}