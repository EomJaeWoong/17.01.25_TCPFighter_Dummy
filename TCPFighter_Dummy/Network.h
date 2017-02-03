#ifndef __NETWORK__H__
#define __NETWORK__H__

bool netSetup();
bool ClientConnect();

void netIOProcess();
void SelectProc(SOCKET *SocketTable, FD_SET *ReadSet, FD_SET *WriteSet, int iSize);

st_SESSION *CreateSession(SOCKET socket);				// 技记 积己
st_SESSION *FindSession(SOCKET socket);					// 技记 茫扁

BOOL netProc_Send(SOCKET socket);
void netProc_Recv(SOCKET socket);

BOOL PacketProc(st_SESSION *pSession);

/*----------------------------------------------------------------------------*/
// Recv Packet
/*----------------------------------------------------------------------------*/
BOOL recvProc_CreateMyCharacter(CNPacket *pPacket);

/*----------------------------------------------------------------------------*/
// Send Packet
/*----------------------------------------------------------------------------*/
BOOL sendProc_MoveStart(st_CHARACTER *pCharacter, int iDir, int iX, int iY);
BOOL sendProc_MoveStop(st_CHARACTER *pCharacter, int iDir, int iX, int iY);
BOOL sendProc_Attack1(st_CHARACTER *pCharacter, int iDir, int iX, int iY);
BOOL sendProc_Attack2(st_CHARACTER *pCharacter, int iDir, int iX, int iY);
BOOL sendProc_Attack3(st_CHARACTER *pCharacter, int iDir, int iX, int iY);

/*----------------------------------------------------------------------------*/
// Make Packet
/*----------------------------------------------------------------------------*/
int makePacket_MoveStart(st_NETWORK_PACKET_HEADER *pHeader, CNPacket *pPacket,
	int iDir, int iX, int iY);
int makePacket_MoveStop(st_NETWORK_PACKET_HEADER *pHeader, CNPacket *pPacket,
	int iDir, int iX, int iY);
int makePacket_Attack1(st_NETWORK_PACKET_HEADER *pHeader, CNPacket *pPacket,
	int iDir, int iX, int iY);
int makePacket_Attack2(st_NETWORK_PACKET_HEADER *pHeader, CNPacket *pPacket,
	int iDir, int iX, int iY);
int makePacket_Attack3(st_NETWORK_PACKET_HEADER *pHeader, CNPacket *pPacket,
	int iDir, int iX, int iY);


extern map<DWORD, st_CHARACTER *> g_Character;

#endif