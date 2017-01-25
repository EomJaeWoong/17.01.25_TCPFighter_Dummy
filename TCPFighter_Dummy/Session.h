#ifndef __SESSION__H__
#define __SESSION__H__

struct st_SESSTION
{
	SOCKET socket;
	DWORD dwSessionID;

	CAyaStreamSQ SendQ;
	CAyaStreamSQ RecvQ;
};

#endif