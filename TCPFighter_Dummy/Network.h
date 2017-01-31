#ifndef __NETWORK__H__
#define __NETWORK__H__

bool netSetup();
bool ClientConnect();

extern map<DWORD, st_CHARACTER *> g_Client;

#endif