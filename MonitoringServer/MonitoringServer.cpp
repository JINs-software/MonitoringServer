#include "MonitoringServer.h"
#include "MonitorProtocol.h"
#include <sstream>

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
	assert(dataType < m_MontDataVec.size());
	m_MontDataVec[dataType].dataValue = dataVal;
	m_MontDataVec[dataType].timeStamp = timeStamp;

	m_MontDataVec[dataType].dataValueAccumulate += dataVal;
	m_MontDataVec[dataType].accunmulateCnt += 1;
	m_MontDataVec[dataType].dataMin = min(m_MontDataVec[dataType].dataValue, m_MontDataVec[dataType].dataMin);
	m_MontDataVec[dataType].dataMax = max(m_MontDataVec[dataType].dataValue, m_MontDataVec[dataType].dataMax);
}

void MonitoringServer::Process_CS_MONITOR_TOOL_LOGIN(SessionID sessionID, char* loginSessionKey)
{
	for (BYTE i = 0; i < dfMAX_NUM_OF_MONT_CLIENT_TOOL; i++) {
		if (m_MontClientSessions[i] == sessionID) {
			DebugBreak();
		}
	}

	m_EmptyIdxQueueMtx.lock();
	if (m_EmptyIdxQueue.empty()) {
		return;
	}
	else {
		m_MontClientSessions[m_EmptyIdxQueue.front()] = sessionID;
		m_EmptyIdxQueue.pop();
	}
	m_EmptyIdxQueueMtx.unlock();

	JBuffer* resPacket = AllocSerialSendBuff(sizeof(WORD) + sizeof(BYTE));
	*resPacket << (WORD)en_PACKET_CS_MONITOR_TOOL_RES_LOGIN;
	*resPacket << (BYTE)dfMONITOR_TOOL_LOGIN_OK;
	SendPacket(sessionID, resPacket);
}


void MonitoringServer::Send_MONT_DATA_TO_CLIENT() {
	JBuffer* sendBuff = AllocSerialBuff();

#if defined (MONT_SERVER_MONITORING_MODE)
	for (BYTE dataType = dfMONITOR_DATA_TYPE_MONITOR_CPU_TOTAL; dataType <= dfMONITOR_DATA_TYPE_MONT_SERVER_CPU; dataType++) {
		if (sendBuff->GetFreeSize() < sizeof(stMSG_HDR) + sizeof(stMSG_MONT_DATA_UPDATE)) {
			DebugBreak();
		}
		stMSG_HDR* hdr;
		hdr = sendBuff->DirectReserve<stMSG_HDR>();
		hdr->code = dfPACKET_CODE;
		hdr->len = sizeof(stMSG_MONT_DATA_UPDATE);
		hdr->randKey = (BYTE)-1;
		stMSG_MONT_DATA_UPDATE* body = sendBuff->DirectReserve<stMSG_MONT_DATA_UPDATE>();
		body->Type = en_PACKET_CS_MONITOR_TOOL_DATA_UPDATE;
		body->DataType = dataType;
		body->DataValue = m_MontDataVec[dataType].dataValue;
		body->TimeStamp = m_MontDataVec[dataType].timeStamp;
	}
#else
	for (BYTE dataType = dfMONITOR_DATA_TYPE_MONITOR_CPU_TOTAL; dataType <= dfMONITOR_DATA_TYPE_MONITOR_AVAILABLE_MEMORY; dataType++) {
		if (sendBuff->GetFreeSize() < sizeof(stMSG_HDR) + sizeof(stMSG_MONT_DATA_UPDATE)) {
			DebugBreak();
		}
		stMSG_HDR* hdr;
		hdr = sendBuff->DirectReserve<stMSG_HDR>();
		hdr->code = dfPACKET_CODE;
		hdr->len = sizeof(stMSG_MONT_DATA_UPDATE);
		hdr->randKey = (BYTE)-1;
		stMSG_MONT_DATA_UPDATE* body = sendBuff->DirectReserve<stMSG_MONT_DATA_UPDATE>();
		body->Type = en_PACKET_CS_MONITOR_TOOL_DATA_UPDATE;
		body->DataType = dataType;
		body->DataValue = m_MontDataVec[dataType].dataValue;
		body->TimeStamp = m_MontDataVec[dataType].timeStamp;
	}
#endif
	if (m_MontDataVec[dfMONITOR_DATA_TYPE_LOGIN_SERVER_RUN].dataValue == 1) {
		for (BYTE dataType = dfMONITOR_DATA_TYPE_LOGIN_SERVER_RUN; dataType <= dfMONITOR_DATA_TYPE_LOGIN_PACKET_POOL; dataType++) {
			if (sendBuff->GetFreeSize() < sizeof(stMSG_HDR) + sizeof(stMSG_MONT_DATA_UPDATE)) {
				DebugBreak();
			}
			stMSG_HDR* hdr;
			hdr = sendBuff->DirectReserve<stMSG_HDR>();
			hdr->code = dfPACKET_CODE;
			hdr->len = sizeof(stMSG_MONT_DATA_UPDATE);
			hdr->randKey = (BYTE)-1;
			stMSG_MONT_DATA_UPDATE* body = sendBuff->DirectReserve<stMSG_MONT_DATA_UPDATE>();
			body->Type = en_PACKET_CS_MONITOR_TOOL_DATA_UPDATE;
			body->DataType = dataType;
			body->DataValue = m_MontDataVec[dataType].dataValue;
			body->TimeStamp = m_MontDataVec[dataType].timeStamp;
		}

		// 타임 아웃 체크
		if (time(NULL) > m_MontDataVec[dfMONITOR_DATA_TYPE_LOGIN_SERVER_RUN].timeStamp + 10) {
			memset(&m_MontDataVec[dfMONITOR_DATA_TYPE_LOGIN_SERVER_RUN], 0, sizeof(stMontData) * (dfMONITOR_DATA_TYPE_LOGIN_PACKET_POOL - dfMONITOR_DATA_TYPE_LOGIN_SERVER_RUN + 1));
		}
	}
	if (m_MontDataVec[dfMONITOR_DATA_TYPE_GAME_SERVER_RUN].dataValue == 1) {
		for (BYTE dataType = dfMONITOR_DATA_TYPE_GAME_SERVER_RUN; dataType <= dfMONITOR_DATA_TYPE_GAME_PACKET_POOL; dataType++) {
			if (sendBuff->GetFreeSize() < sizeof(stMSG_HDR) + sizeof(stMSG_MONT_DATA_UPDATE)) {
				DebugBreak();
			}
			stMSG_HDR* hdr;
			hdr = sendBuff->DirectReserve<stMSG_HDR>();
			hdr->code = dfPACKET_CODE;
			hdr->len = sizeof(stMSG_MONT_DATA_UPDATE);
			hdr->randKey = (BYTE)-1;
			stMSG_MONT_DATA_UPDATE* body = sendBuff->DirectReserve<stMSG_MONT_DATA_UPDATE>();
			body->Type = en_PACKET_CS_MONITOR_TOOL_DATA_UPDATE;
			body->DataType = dataType;
			body->DataValue = m_MontDataVec[dataType].dataValue;
			body->TimeStamp = m_MontDataVec[dataType].timeStamp;
		}

		if (time(NULL) > m_MontDataVec[dfMONITOR_DATA_TYPE_GAME_SERVER_RUN].timeStamp + 10) {
			memset(&m_MontDataVec[dfMONITOR_DATA_TYPE_GAME_SERVER_RUN], 0, sizeof(stMontData) * (dfMONITOR_DATA_TYPE_GAME_PACKET_POOL - dfMONITOR_DATA_TYPE_GAME_SERVER_RUN + 1));
		}
	}
	if (m_MontDataVec[dfMONITOR_DATA_TYPE_CHAT_SERVER_RUN].dataValue == 1) {
		for (BYTE dataType = dfMONITOR_DATA_TYPE_CHAT_SERVER_RUN; dataType <= dfMONITOR_DATA_TYPE_CHAT_UPDATE_WORKER_CPU; dataType++) {
			if (sendBuff->GetFreeSize() < sizeof(stMSG_HDR) + sizeof(stMSG_MONT_DATA_UPDATE)) {
				DebugBreak();
			}
			stMSG_HDR* hdr;
			hdr = sendBuff->DirectReserve<stMSG_HDR>();
			hdr->code = dfPACKET_CODE;
			hdr->len = sizeof(stMSG_MONT_DATA_UPDATE);
			hdr->randKey = (BYTE)-1;
			stMSG_MONT_DATA_UPDATE* body = sendBuff->DirectReserve<stMSG_MONT_DATA_UPDATE>();
			body->Type = en_PACKET_CS_MONITOR_TOOL_DATA_UPDATE;
			body->DataType = dataType;
			body->DataValue = m_MontDataVec[dataType].dataValue;
			body->TimeStamp = m_MontDataVec[dataType].timeStamp;
		}

		if (time(NULL) > m_MontDataVec[dfMONITOR_DATA_TYPE_CHAT_SERVER_RUN].timeStamp + 10) {
			memset(&m_MontDataVec[dfMONITOR_DATA_TYPE_CHAT_SERVER_RUN], 0, sizeof(stMontData) * (dfMONITOR_DATA_TYPE_CHAT_UPDATE_WORKER_CPU - dfMONITOR_DATA_TYPE_CHAT_SERVER_RUN + 1));
		}
	}

	//AcquireSRWLockShared(&m_MontClientSessionsSrwLock);
	//for (auto iter : m_MontClientSessions) {
	//	AddRefSerialBuff(sendBuff);
	//	SendPacket(iter, sendBuff);
	//}
	//ReleaseSRWLockShared(&m_MontClientSessionsSrwLock);

	for (BYTE i = 0; i < dfMAX_NUM_OF_MONT_CLIENT_TOOL; i++) {
		SessionID sessionID = m_MontClientSessions[i];
		if (sessionID == 0) {
			continue;
		}
		AddRefSerialBuff(sendBuff);
		SendPacket(sessionID, sendBuff);
	}

	FreeSerialBuff(sendBuff);
}



wstring MonitoringServer::Create_LogDbTable(SQL_TIMESTAMP_STRUCT  currentTime)
{
	m_DbConnection->Unbind();

	SQLLEN logtimeLen = sizeof(currentTime);
	SQLLEN len = 0;

	// 테이블 이름 생성
	std::wstringstream tableName;
	tableName << L"monitorlog_" << currentTime.year << (currentTime.month < 10 ? L"0" : L"") << currentTime.month;

	// 테이블 존재 여부 확인 및 생성 쿼리
	std::wstringstream createTableQuery;
	createTableQuery << L"IF NOT EXISTS (SELECT * FROM sysobjects WHERE name='" << tableName.str() << L"' AND xtype='U') "
		<< L"CREATE TABLE [logdb].[" << tableName.str() << L"] ("
		<< L"[no] BIGINT NOT NULL AUTO_INCREMENT, "
		<< L"[logtime] DATETIME, "
		<< L"[serverno] INT NOT NULL, "
		<< L"[type] INT NOT NULL, "
		<< L"[avr] INT NOT NULL, "
		<< L"[min] INT NOT NULL, "
		<< L"[max] INT NOT NULL, "
		<< L"PRIMARY KEY([no]))";

	// 테이블 생성
	assert(m_DbConnection->Execute(createTableQuery.str()));

	return createTableQuery.str();
}

void MonitoringServer::Insert_LogDB(const wstring& tableName, SQL_TIMESTAMP_STRUCT  currentTime, int serverNo, int type, int dataAvr, int dataMin, int dataMax)
{
	m_DbConnection->Unbind();

	SQLLEN logtimeLen = sizeof(currentTime);
	SQLLEN len = 0;

	// 인자 바인딩
	assert(m_DbConnection->BindParam(1, SQL_C_TYPE_TIMESTAMP, SQL_TYPE_TIMESTAMP, logtimeLen, &currentTime, &logtimeLen));
	assert(m_DbConnection->BindParam(2, SQL_C_LONG, SQL_INTEGER, sizeof(serverno), &serverno, &len));
	assert(m_DbConnection->BindParam(3, SQL_C_LONG, SQL_INTEGER, sizeof(type), &type, &len));
	assert(m_DbConnection->BindParam(4, SQL_C_LONG, SQL_INTEGER, sizeof(dataAvr), &dataAvr, &len));
	assert(m_DbConnection->BindParam(5, SQL_C_LONG, SQL_INTEGER, sizeof(dataMin), &dataMin, &len));
	assert(m_DbConnection->BindParam(6, SQL_C_LONG, SQL_INTEGER, sizeof(dataMax), &dataMax, &len));

	// SQL 실행
	assert(m_DbConnection->Execute(L"INSERT INTO [logdb].[" + tableName + L"] ([logtime], [serverno], [type], [avr], [min], [max]) VALUES(? , ? , ? , ? , ? , ? )"));
}

UINT __stdcall MonitoringServer::PerformanceCountFunc(void* arg)
{
	MonitoringServer* montserver = (MonitoringServer*)arg;
	montserver->m_SerialBuffPoolMgr.AllocTlsMemPool();

	while (!montserver->m_ExitThread) {
		time_t now = time(NULL);
		//////////////////////////////////////////////////
		// 카운트 리셋
		//////////////////////////////////////////////////
		montserver->m_PerfCounter->ResetPerfCounterItems();

		// 서버 프로세서 정보 갱신
		montserver->m_MontDataVec[dfMONITOR_DATA_TYPE_MONITOR_CPU_TOTAL].dataValue = montserver->m_PerfCounter->ProcessorTotal();
		montserver->m_MontDataVec[dfMONITOR_DATA_TYPE_MONITOR_CPU_TOTAL].timeStamp = now;
		montserver->m_MontDataVec[dfMONITOR_DATA_TYPE_MONITOR_CPU_TOTAL].dataValueAccumulate += montserver->m_MontDataVec[dfMONITOR_DATA_TYPE_MONITOR_CPU_TOTAL].dataValue;
		montserver->m_MontDataVec[dfMONITOR_DATA_TYPE_MONITOR_CPU_TOTAL].accunmulateCnt += 1;
		montserver->m_MontDataVec[dfMONITOR_DATA_TYPE_MONITOR_CPU_TOTAL].dataMin = min(montserver->m_MontDataVec[dfMONITOR_DATA_TYPE_MONITOR_CPU_TOTAL].dataValue, montserver->m_MontDataVec[dfMONITOR_DATA_TYPE_MONITOR_CPU_TOTAL].dataMin);
		montserver->m_MontDataVec[dfMONITOR_DATA_TYPE_MONITOR_CPU_TOTAL].dataMax = max(montserver->m_MontDataVec[dfMONITOR_DATA_TYPE_MONITOR_CPU_TOTAL].dataValue, montserver->m_MontDataVec[dfMONITOR_DATA_TYPE_MONITOR_CPU_TOTAL].dataMax);

		// 서버 논-페이지 풀 정보 갱신
		montserver->m_MontDataVec[dfMONITOR_DATA_TYPE_MONITOR_NONPAGED_MEMORY].dataValue = montserver->m_PerfCounter->GetPerfCounterItem(dfMONITOR_DATA_TYPE_MONITOR_NONPAGED_MEMORY);
		montserver->m_MontDataVec[dfMONITOR_DATA_TYPE_MONITOR_NONPAGED_MEMORY].dataValue /= (1024 * 1024);	// 논-페이지 풀 MBytes
		montserver->m_MontDataVec[dfMONITOR_DATA_TYPE_MONITOR_NONPAGED_MEMORY].timeStamp = now;
		montserver->m_MontDataVec[dfMONITOR_DATA_TYPE_MONITOR_NONPAGED_MEMORY].dataValueAccumulate += montserver->m_MontDataVec[dfMONITOR_DATA_TYPE_MONITOR_NONPAGED_MEMORY].dataValue;
		montserver->m_MontDataVec[dfMONITOR_DATA_TYPE_MONITOR_NONPAGED_MEMORY].accunmulateCnt += 1;
		montserver->m_MontDataVec[dfMONITOR_DATA_TYPE_MONITOR_NONPAGED_MEMORY].dataMin = min(montserver->m_MontDataVec[dfMONITOR_DATA_TYPE_MONITOR_NONPAGED_MEMORY].dataValue, montserver->m_MontDataVec[dfMONITOR_DATA_TYPE_MONITOR_NONPAGED_MEMORY].dataMin);
		montserver->m_MontDataVec[dfMONITOR_DATA_TYPE_MONITOR_NONPAGED_MEMORY].dataMax = max(montserver->m_MontDataVec[dfMONITOR_DATA_TYPE_MONITOR_NONPAGED_MEMORY].dataValue, montserver->m_MontDataVec[dfMONITOR_DATA_TYPE_MONITOR_NONPAGED_MEMORY].dataMax);

		// 서버 수신 바이트 정보 갱신
		montserver->m_MontDataVec[dfMONITOR_DATA_TYPE_MONITOR_NETWORK_RECV].dataValue = montserver->m_PerfCounter->GetPerfEthernetRecvBytes();
		//montserver->m_MontDataVec[dfMONITOR_DATA_TYPE_MONITOR_NETWORK_RECV].dataValue = montserver->m_PerfCounter->GetPerfCounterItem(dfMONITOR_DATA_TYPE_MONITOR_NETWORK_RECV);
		montserver->m_MontDataVec[dfMONITOR_DATA_TYPE_MONITOR_NETWORK_RECV].dataValue /= 1024;
		montserver->m_MontDataVec[dfMONITOR_DATA_TYPE_MONITOR_NETWORK_RECV].timeStamp = now;
		montserver->m_MontDataVec[dfMONITOR_DATA_TYPE_MONITOR_NETWORK_RECV].dataValueAccumulate += montserver->m_MontDataVec[dfMONITOR_DATA_TYPE_MONITOR_NETWORK_RECV].dataValue;
		montserver->m_MontDataVec[dfMONITOR_DATA_TYPE_MONITOR_NETWORK_RECV].accunmulateCnt += 1;
		montserver->m_MontDataVec[dfMONITOR_DATA_TYPE_MONITOR_NETWORK_RECV].dataMin = min(montserver->m_MontDataVec[dfMONITOR_DATA_TYPE_MONITOR_NETWORK_RECV].dataValue, montserver->m_MontDataVec[dfMONITOR_DATA_TYPE_MONITOR_NETWORK_RECV].dataMin);
		montserver->m_MontDataVec[dfMONITOR_DATA_TYPE_MONITOR_NETWORK_RECV].dataMax = max(montserver->m_MontDataVec[dfMONITOR_DATA_TYPE_MONITOR_NETWORK_RECV].dataValue, montserver->m_MontDataVec[dfMONITOR_DATA_TYPE_MONITOR_NETWORK_RECV].dataMax);

		// 서버 송신 바이트 정보 갱신
		montserver->m_MontDataVec[dfMONITOR_DATA_TYPE_MONITOR_NETWORK_SEND].dataValue = montserver->m_PerfCounter->GetPerfEthernetSendBytes();
		//montserver->m_MontDataVec[dfMONITOR_DATA_TYPE_MONITOR_NETWORK_SEND].dataValue = montserver->m_PerfCounter->GetPerfCounterItem(dfMONITOR_DATA_TYPE_MONITOR_NETWORK_SEND);
		montserver->m_MontDataVec[dfMONITOR_DATA_TYPE_MONITOR_NETWORK_SEND].dataValue /= 1024;
		montserver->m_MontDataVec[dfMONITOR_DATA_TYPE_MONITOR_NETWORK_SEND].timeStamp = now;
		montserver->m_MontDataVec[dfMONITOR_DATA_TYPE_MONITOR_NETWORK_SEND].dataValueAccumulate += montserver->m_MontDataVec[dfMONITOR_DATA_TYPE_MONITOR_NETWORK_SEND].dataValue;
		montserver->m_MontDataVec[dfMONITOR_DATA_TYPE_MONITOR_NETWORK_SEND].accunmulateCnt += 1;
		montserver->m_MontDataVec[dfMONITOR_DATA_TYPE_MONITOR_NETWORK_SEND].dataMin = min(montserver->m_MontDataVec[dfMONITOR_DATA_TYPE_MONITOR_NETWORK_SEND].dataValue, montserver->m_MontDataVec[dfMONITOR_DATA_TYPE_MONITOR_NETWORK_SEND].dataMin);
		montserver->m_MontDataVec[dfMONITOR_DATA_TYPE_MONITOR_NETWORK_SEND].dataMax = max(montserver->m_MontDataVec[dfMONITOR_DATA_TYPE_MONITOR_NETWORK_SEND].dataValue, montserver->m_MontDataVec[dfMONITOR_DATA_TYPE_MONITOR_NETWORK_SEND].dataMax);

		// 서버 사용 가능 메모리 정보 갱신
		montserver->m_MontDataVec[dfMONITOR_DATA_TYPE_MONITOR_AVAILABLE_MEMORY].dataValue = montserver->m_PerfCounter->GetPerfCounterItem(dfMONITOR_DATA_TYPE_MONITOR_AVAILABLE_MEMORY);
		montserver->m_MontDataVec[dfMONITOR_DATA_TYPE_MONITOR_AVAILABLE_MEMORY].timeStamp = now;
		montserver->m_MontDataVec[dfMONITOR_DATA_TYPE_MONITOR_AVAILABLE_MEMORY].dataValueAccumulate += montserver->m_MontDataVec[dfMONITOR_DATA_TYPE_MONITOR_NETWORK_SEND].dataValue;
		montserver->m_MontDataVec[dfMONITOR_DATA_TYPE_MONITOR_AVAILABLE_MEMORY].accunmulateCnt += 1;
		montserver->m_MontDataVec[dfMONITOR_DATA_TYPE_MONITOR_AVAILABLE_MEMORY].dataMin = min(montserver->m_MontDataVec[dfMONITOR_DATA_TYPE_MONITOR_AVAILABLE_MEMORY].dataValue, montserver->m_MontDataVec[dfMONITOR_DATA_TYPE_MONITOR_AVAILABLE_MEMORY].dataMin);
		montserver->m_MontDataVec[dfMONITOR_DATA_TYPE_MONITOR_AVAILABLE_MEMORY].dataMax = max(montserver->m_MontDataVec[dfMONITOR_DATA_TYPE_MONITOR_AVAILABLE_MEMORY].dataValue, montserver->m_MontDataVec[dfMONITOR_DATA_TYPE_MONITOR_AVAILABLE_MEMORY].dataMax);
		
#if defined(MONT_SERVER_MONITORING_MODE)
		montserver->m_MontDataVec[dfMONITOR_DATA_TYPE_MONT_SERVER_RUN].dataValue = 1;
		montserver->m_MontDataVec[dfMONITOR_DATA_TYPE_MONT_SERVER_RUN].timeStamp = now;
		montserver->m_MontDataVec[dfMONITOR_DATA_TYPE_MONT_SERVER_CPU].dataValue = montserver->m_PerfCounter->ProcessTotal();
		montserver->m_MontDataVec[dfMONITOR_DATA_TYPE_MONT_SERVER_CPU].timeStamp = now;
		montserver->m_MontDataVec[dfMONITOR_DATA_TYPE_MONT_SERVER_MEM].dataValue = montserver->m_PerfCounter->GetPerfCounterItem(dfMONITOR_DATA_TYPE_MONT_SERVER_MEM);
		montserver->m_MontDataVec[dfMONITOR_DATA_TYPE_MONT_SERVER_MEM].timeStamp = now;
#endif

		montserver->Send_MONT_DATA_TO_CLIENT();

		Sleep(1000);
	}


	return 0;
}

UINT __stdcall MonitoringServer::LoggingToDbFunc(void* arg)
{
	MonitoringServer* montserver = (MonitoringServer*)arg;

	while (!montserver->m_ExitThread) {

		auto now = std::chrono::system_clock::now();
		std::time_t now_time = std::chrono::system_clock::to_time_t(now);
		std::tm* now_tm = std::localtime(&now_time);

		SQL_TIMESTAMP_STRUCT timestamp;
		timestamp.year = now_tm->tm_year + 1900;
		timestamp.month = now_tm->tm_mon + 1;
		timestamp.day = now_tm->tm_mday;
		timestamp.hour = now_tm->tm_hour;
		timestamp.minute = now_tm->tm_min;
		timestamp.second = now_tm->tm_sec;
		timestamp.fraction = 0;

		wstring tableName = montserver->Create_LogDbTable(timestamp);

		// 서버 공통 
		for (BYTE dataType = dfMONITOR_DATA_TYPE_MONITOR_CPU_TOTAL; dataType <= dfMONITOR_DATA_TYPE_MONT_SERVER_CPU; dataType++) {
			int dataValueAcc = montserver->m_MontDataVec[dataType].dataValueAccumulate;
			int accCnt = montserver->m_MontDataVec[dataType].accunmulateCnt;
			montserver->m_MontDataVec[dataType].dataValueAccumulate = 0;
			montserver->m_MontDataVec[dataType].accunmulateCnt = 0;

			if (accCnt != 0) {
				montserver->m_MontDataVec[dataType].dataAvr = dataValueAcc / accCnt;
			}

			// 로그 정보 DB 삽입
			montserver->Insert_LogDB(tableName, timestamp, dfSERVER_SYSTEM, dataType, montserver->m_MontDataVec[dataType].dataAvr, montserver->m_MontDataVec[dataType].dataMin, montserver->m_MontDataVec[dataType].dataMax);
		}

		// 로그인 서버
		if (montserver->m_MontDataVec[dfMONITOR_DATA_TYPE_LOGIN_SERVER_RUN].dataValue == 1) {
			for (BYTE dataType = dfMONITOR_DATA_TYPE_LOGIN_SERVER_RUN; dataType <= dfMONITOR_DATA_TYPE_LOGIN_PACKET_POOL; dataType++) {
				int dataValueAcc = montserver->m_MontDataVec[dataType].dataValueAccumulate;
				int accCnt = montserver->m_MontDataVec[dataType].accunmulateCnt;
				montserver->m_MontDataVec[dataType].dataValueAccumulate = 0;
				montserver->m_MontDataVec[dataType].accunmulateCnt = 0;

				if (accCnt != 0) {
					montserver->m_MontDataVec[dataType].dataAvr = dataValueAcc / accCnt;
				}
				montserver->Insert_LogDB(tableName, timestamp, dfSERVER_SYSTEM, dataType, montserver->m_MontDataVec[dataType].dataAvr, montserver->m_MontDataVec[dataType].dataMin, montserver->m_MontDataVec[dataType].dataMax);
			}
		}

		// 에코-게임 서버
		if (montserver->m_MontDataVec[dfMONITOR_DATA_TYPE_GAME_SERVER_RUN].dataValue == 1) {
			for (BYTE dataType = dfMONITOR_DATA_TYPE_GAME_SERVER_RUN; dataType <= dfMONITOR_DATA_TYPE_GAME_PACKET_POOL; dataType++) {
				int dataValueAcc = montserver->m_MontDataVec[dataType].dataValueAccumulate;
				int accCnt = montserver->m_MontDataVec[dataType].accunmulateCnt;
				montserver->m_MontDataVec[dataType].dataValueAccumulate = 0;
				montserver->m_MontDataVec[dataType].accunmulateCnt = 0;

				if (accCnt != 0) {
					montserver->m_MontDataVec[dataType].dataAvr = dataValueAcc / accCnt;
				}
				montserver->Insert_LogDB(tableName, timestamp, dfSERVER_SYSTEM, dataType, montserver->m_MontDataVec[dataType].dataAvr, montserver->m_MontDataVec[dataType].dataMin, montserver->m_MontDataVec[dataType].dataMax);
			}

		}

		// 채팅 서버
		if (montserver->m_MontDataVec[dfMONITOR_DATA_TYPE_CHAT_SERVER_RUN].dataValue == 1) {
			for (BYTE dataType = dfMONITOR_DATA_TYPE_CHAT_SERVER_RUN; dataType <= dfMONITOR_DATA_TYPE_CHAT_UPDATE_WORKER_CPU; dataType++) {
				int dataValueAcc = montserver->m_MontDataVec[dataType].dataValueAccumulate;
				int accCnt = montserver->m_MontDataVec[dataType].accunmulateCnt;
				montserver->m_MontDataVec[dataType].dataValueAccumulate = 0;
				montserver->m_MontDataVec[dataType].accunmulateCnt = 0;

				if (accCnt != 0) {
					montserver->m_MontDataVec[dataType].dataAvr = dataValueAcc / accCnt;
				}
				montserver->Insert_LogDB(tableName, timestamp, dfSERVER_SYSTEM, dataType, montserver->m_MontDataVec[dataType].dataAvr, montserver->m_MontDataVec[dataType].dataMin, montserver->m_MontDataVec[dataType].dataMax);
			}
		}

		Sleep(60000);
	}

	return 0;
}
