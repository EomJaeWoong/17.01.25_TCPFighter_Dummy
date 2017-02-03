#include <Windows.h>
#include <map>

using namespace std;

#include "PacketDefine.h"
#include "StreamQueue.h"
#include "NPacket.h"
#include "Session.h"
#include "Character.h"
#include "Network.h"
#include "Content.h"

map<DWORD, st_CHARACTER *> g_Character;

void main()
{
	netSetup();

	ClientConnect();

	while (1)
	{
		netIOProcess();
		Update();
		Action();
	}
}