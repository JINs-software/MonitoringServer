#include "MonitoringServer.h"
#include "MonitorProtocol.h"

void MonitoringServer::OnRecv(UINT64 sessionID, JBuffer& recvBuff)
{
	while (recvBuff.GetUseSize() > sizeof(WORD)) {			/// ????		// [TO DO] msgHdr.len까지만 디큐잉 하는 것으로 변경
		WORD type;
		recvBuff >> type;

		switch (type)
		{
		case en_PACKET_SS_MONITOR_LOGIN:
		{
			int serverNo;
			recvBuff >> serverNo;
			Process_SS_MONITOR_LOGIN(sessionID, serverNo);
		}
		break;
		case en_PACKET_SS_MONITOR_DATA_UPDATE:
		{
			BYTE dataType;
			int dataValue;
			int timeStamp;
			recvBuff >> dataType;
			recvBuff >> dataValue;
			recvBuff >> timeStamp;
			Process_SS_MONITOR_DATA_UPDATE(dataType, dataValue, timeStamp);
		}
		break;
		case en_PACKET_CS_MONITOR_TOOL_REQ_LOGIN:
		{
			char	loginSessionKey[32];
			recvBuff.Dequeue((BYTE*)loginSessionKey, sizeof(loginSessionKey));
			Process_CS_MONITOR_TOOL_LOGIN(sessionID, loginSessionKey);
		}
		break;
		default:
			DebugBreak();
			break;
		}
	}

	if (recvBuff.GetUseSize() > 0) {
		DebugBreak();
	}
}

void MonitoringServer::Process_SS_MONITOR_LOGIN(SessionID sessionID, int serverNo)
{
	// 세션 ID <-> 서버 종류 맵핑
	switch (serverNo)
	{
	case dfSERVER_LOGIN_SERVER:
	{
		if (m_LoginServerSession == -1) {
			m_LoginServerSession = sessionID;
		}
		else {
			// 중복 로그인
			DebugBreak();
		}
	}
		break;
	case dfSERVER_ECHO_GAME_SERVER:
	{
		if (m_EchoGameServerSession == -1) {
			m_EchoGameServerSession = sessionID;
		}
		else {
			// 중복 로그인
			DebugBreak();
		}
	}
		break;
	case dfSERVER_CHAT_SERVER:
	{
		if (m_ChatServerSession == -1) {
			m_ChatServerSession = sessionID;
		}
		else {
			// 중복 로그인
			DebugBreak();
		}
	}
		break;
	default:
		break;
	}
}

void MonitoringServer::Process_SS_MONITOR_DATA_UPDATE(BYTE dataType, int dataVal, int timeStamp)
{
	m_MontDataMap[dataType].dataValue = dataVal;
	m_MontDataMap[dataType].timeStamp = timeStamp;
}

void MonitoringServer::Process_CS_MONITOR_TOOL_LOGIN(SessionID sessionID, char* loginSessionKey)
{
	// 세션 ID <-> 모니터링 클라이언트 맵핑
	if (m_MontClientSessions.find(sessionID) != m_MontClientSessions.end()) {
		// 중복 로그인
		DebugBreak();
	}
	else {
		JBuffer* resPacket = AllocSerialSendBuff(sizeof(WORD) + sizeof(BYTE));
		*resPacket << (WORD)en_PACKET_CS_MONITOR_TOOL_RES_LOGIN;
		*resPacket << (BYTE)dfMONITOR_TOOL_LOGIN_OK;
		SendPacket(sessionID, resPacket);

		m_MontClientSessions.insert(sessionID);
	}
}

void MonitoringServer::Send_MONT_DATA_TO_CLIENT() {
	JBuffer* sendBuff = AllocSerialBuff();

	for (auto iter : m_MontDataMap) {
		if (sendBuff->GetFreeSize() < sizeof(stMSG_HDR) + sizeof(stMSG_MONT_DATA_UPDATE)) {
			DebugBreak();
		}
		BYTE dataType = iter.first;
		stMontData& montData = iter.second;

		stMSG_HDR* hdr;
		hdr = sendBuff->DirectReserve<stMSG_HDR>();
		hdr->code = dfPACKET_CODE;
		hdr->len = sizeof(stMSG_MONT_DATA_UPDATE);
		hdr->randKey = (BYTE)-1;
		stMSG_MONT_DATA_UPDATE* body = sendBuff->DirectReserve<stMSG_MONT_DATA_UPDATE>();
		body->Type = en_PACKET_CS_MONITOR_TOOL_DATA_UPDATE;
		body->DataType = dataType;
		body->DataValue = montData.dataValue;
		body->TimeStamp = montData.timeStamp;
		if (dataType >= dfMONITOR_DATA_TYPE_LOGIN_SERVER_RUN && dataType <= dfMONITOR_DATA_TYPE_LOGIN_PACKET_POOL) {
			body->ServerNo = dfSERVER_LOGIN_SERVER;
		}
		else if (dataType >= dfMONITOR_DATA_TYPE_GAME_SERVER_RUN && dataType <= dfMONITOR_DATA_TYPE_GAME_PACKET_POOL) {
			body->ServerNo = dfSERVER_ECHO_GAME_SERVER;
		}
		else if (dataType >= dfMONITOR_DATA_TYPE_CHAT_SERVER_RUN && dataType <= dfMONITOR_DATA_TYPE_CHAT_UPDATEMSG_POOL) {
			body->ServerNo = dfSERVER_CHAT_SERVER;
		}
		else if (dataType >= dfMONITOR_DATA_TYPE_MONITOR_CPU_TOTAL && dataType <= dfMONITOR_DATA_TYPE_MONITOR_AVAILABLE_MEMORY) {
			body->ServerNo = dfSERVER_SYSTEM;
		}
		else {
			DebugBreak();
		}
	}

	AcquireSRWLockShared(&m_MontClientSessionsSrwLock);
	for (auto iter : m_MontClientSessions) {
		AddRefSerialBuff(sendBuff);
		SendPacket(iter, sendBuff);
	}
	ReleaseSRWLockShared(&m_MontClientSessionsSrwLock);

	FreeSerialBuff(sendBuff);
}


UINT __stdcall MonitoringServer::PerformanceCountFunc(void* arg)
{
	MonitoringServer* montserver = (MonitoringServer*)arg;
	montserver->m_SerialBuffPoolMgr.AllocTlsMemPool();

	while (!montserver->m_ExitThread) {
		time_t now = time(NULL);
		montserver->m_PerfCounter->ResetPerfCounter();
		montserver->m_MontDataMap[dfMONITOR_DATA_TYPE_MONITOR_CPU_TOTAL].dataValue = montserver->m_PerfCounter->ProcessorTotal();
		montserver->m_MontDataMap[dfMONITOR_DATA_TYPE_MONITOR_CPU_TOTAL].timeStamp = now;
		montserver->m_MontDataMap[dfMONITOR_DATA_TYPE_MONITOR_NONPAGED_MEMORY].dataValue = montserver->m_PerfCounter->GetPerfCounter(MONITOR_DATA_TYPE_MONITOR_NONPAGED_MEMORY);
		montserver->m_MontDataMap[dfMONITOR_DATA_TYPE_MONITOR_NONPAGED_MEMORY].dataValue /= 1'000'000.0;	// 논-페이지 풀 MBytes
		montserver->m_MontDataMap[dfMONITOR_DATA_TYPE_MONITOR_NONPAGED_MEMORY].timeStamp = now;
		montserver->m_MontDataMap[dfMONITOR_DATA_TYPE_MONITOR_NETWORK_RECV].dataValue = 0;
		montserver->m_MontDataMap[dfMONITOR_DATA_TYPE_MONITOR_NETWORK_RECV].timeStamp = now;
		montserver->m_MontDataMap[dfMONITOR_DATA_TYPE_MONITOR_NETWORK_SEND].dataValue = 0;
		montserver->m_MontDataMap[dfMONITOR_DATA_TYPE_MONITOR_NETWORK_SEND].timeStamp = now;
		montserver->m_MontDataMap[dfMONITOR_DATA_TYPE_MONITOR_AVAILABLE_MEMORY].dataValue = montserver->m_PerfCounter->GetPerfCounter(MONITOR_DATA_TYPE_MONITOR_AVAILABLE_MEMORY);
		montserver->m_MontDataMap[dfMONITOR_DATA_TYPE_MONITOR_AVAILABLE_MEMORY].timeStamp = now;
		
		montserver->Send_MONT_DATA_TO_CLIENT();


		Sleep(1000);
	}


	return 0;
}
