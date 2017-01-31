#include <Windows.h>
#include <map>

using namespace std;

#include "StreamQueue.h"
#include "Session.h"
#include "Character.h"
#include "Network.h"

map<DWORD, st_CHARACTER *> g_Client;

void main()
{
	netSetup();

	ClientConnect();
}