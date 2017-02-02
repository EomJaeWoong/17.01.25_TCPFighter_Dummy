#ifndef __SESSION__H__
#define __SESSION__H__

struct st_SESSION
{
	SOCKET socket;
	DWORD dwSessionID;

	CAyaStreamSQ SendQ;
	CAyaStreamSQ RecvQ;
};

typedef map<SOCKET, st_SESSION *> Session;

#endif