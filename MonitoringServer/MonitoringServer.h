#pragma once
#include "CLanOdbcServer.h"
#include "PerformanceCounter.h"
#include "MontServerConfig.h"
#include "MonitorProtocol.h"

using SessionID = UINT64;

class MonitoringServer : public CLanOdbcServer
{
private:
	struct stMontData {
		int dataValue = 0;
		int timeStamp = 0;

		int dataValueAccumulate = 0;
		int accunmulateCnt = 0;

		int dataAvr = 0;
		int dataMin = 0;
		int dataMax = 0;
	};
	
	std::vector<stMontData>		m_MontDataVec;

	PerformanceCounter*			m_PerfCounter;

	// 모니터링 대상 서버
	SessionID				m_LoginServerSession;
	SessionID				m_EchoGameServerSession;
	SessionID				m_ChatServerSession;

	SessionID				m_MontClientSessions[dfMAX_NUM_OF_MONT_CLIENT_TOOL];
	std::queue<BYTE>		m_EmptyIdxQueue;
	std::mutex				m_EmptyIdxQueueMtx;

	
	HANDLE					m_MontThread;
	
	DBConnection*			m_DbConnection;
	HANDLE					m_DbConnThread;

	bool					m_ExitThread;

public:
	MonitoringServer(
		int32 dbConnectionCnt, const WCHAR* odbcConnStr,
		const char* serverIP, uint16 serverPort,
		DWORD numOfIocpConcurrentThrd, uint16 numOfWorkerThreads, uint16 maxOfConnections,
		size_t tlsMemPoolDefaultUnitCnt = MONT_TLS_MEM_POOL_DEFAULT_UNIT_CNT, size_t tlsMemPoolDefaultCapacity = MONT_TLS_MEM_POOL_DEFAULT_UNIT_CAPACITY,
		UINT serialBufferSize = MONT_SERIAL_BUFFER_SIZE,
		uint32 sessionRecvBuffSize = MONT_SERV_SESSION_RECV_BUFF_SIZE,
		BYTE protocolCode = MONTSERVER_PROTOCOL_CODE, BYTE packetKey = MONTSERVER_PACKET_KEY
	)
		: CLanOdbcServer(
			dbConnectionCnt, odbcConnStr,
			serverIP, serverPort, numOfIocpConcurrentThrd, numOfWorkerThreads, maxOfConnections,
			tlsMemPoolDefaultUnitCnt, tlsMemPoolDefaultCapacity, true, false,
			serialBufferSize,
			sessionRecvBuffSize,
			protocolCode, packetKey
		),
		m_LoginServerSession(-1), m_EchoGameServerSession(-1), m_ChatServerSession(-1),
		m_ExitThread(false),
		m_PerfCounter(NULL), m_DbConnection(NULL)
	{
		memset(m_MontClientSessions, 0, sizeof(m_MontClientSessions));
		for (BYTE i = 0; i < dfMAX_NUM_OF_MONT_CLIENT_TOOL; i++) {
			m_EmptyIdxQueue.push(i);
		}

		m_MontDataVec.resize(dfMONITOR_DATA_TYPE_MAX_NUM, {0});
	}

	bool Start() {
		if (!CLanOdbcServer::Start()) {
			return false;
		}

		m_PerfCounter = new PerformanceCounter();
		m_PerfCounter->SetCounter(dfMONITOR_DATA_TYPE_MONITOR_NONPAGED_MEMORY, dfQUERY_MEMORY_NON_PAGED);
		m_PerfCounter->SetCounter(dfMONITOR_DATA_TYPE_MONITOR_AVAILABLE_MEMORY, dfQUERY_MEMORY_AVAILABLE);
		m_PerfCounter->SetCpuUsageCounter();

		m_PerfCounter->SetEthernetCounter();

#if defined(MONT_SERVER_MONITORING_MODE)
		m_PerfCounter->SetProcessCounter(dfMONITOR_DATA_TYPE_MONT_SERVER_MEM, dfQUERY_PROCESS_USER_VMEMORY_USAGE, L"MonitoringServer");
#endif

		// DB 연결
		m_DbConnection = HoldDBConnection();

		// DB 스레드
		m_DbConnThread = (HANDLE)_beginthreadex(NULL, 0, LoggingToDbFunc, this, 0, NULL);
		if (m_DbConnThread == INVALID_HANDLE_VALUE) {
			return false;
		}
		
		// 모니터링 서버 스레드
		m_MontThread = (HANDLE)_beginthreadex(NULL, 0, PerformanceCountFunc, this, 0, NULL);
		if (m_MontThread == INVALID_HANDLE_VALUE) {
			return false;
		}

		return true;
	}
	void Stop() {
		m_ExitThread = true;
		CLanOdbcServer::Stop();
	}

	virtual void OnRecv(UINT64 sessionID, JBuffer& recvBuff) override;
	virtual void OnClientJoin(UINT64 sessionID) {
		std::cout << "[OnClientJoin] sessionID: " << sessionID << std::endl;
	};
	virtual void OnClientLeave(UINT64 sessionID) {
		std::cout << "[OnClientLeave] sessionID: " << sessionID << std::endl;

		for (BYTE i = 0; i < dfMAX_NUM_OF_MONT_CLIENT_TOOL; i++) {
			if (m_MontClientSessions[i] == sessionID) {
				m_MontClientSessions[i] = 0;

				m_EmptyIdxQueueMtx.lock();
				m_EmptyIdxQueue.push(i);
				m_EmptyIdxQueueMtx.unlock();
				std::cout << "[OnClientLeave] 모니터링 클라이언트 연결 종료" << std::endl;

				return;
			}
		}

		if (m_LoginServerSession == sessionID) {
			m_LoginServerSession = -1;
			std::cout << "[OnClientLeave] 로그인 서버 연결 종료" << std::endl;
		}
		else if (m_EchoGameServerSession == sessionID) {
			m_EchoGameServerSession = -1;
			std::cout << "[OnClientLeave] 에코 게임 서버 연결 종료" << std::endl;
		}
		else if (m_ChatServerSession == sessionID) {
			m_ChatServerSession = -1;
			std::cout << "[OnClientLeave] 채팅 서버 연결 종료" << std::endl;
		}

	};

	void Process_SS_MONITOR_LOGIN(SessionID sessionID, int serverNo);
	void Process_SS_MONITOR_DATA_UPDATE(BYTE dataType, int dataVal, int timeStamp);
	bool Process_CS_MONITOR_TOOL_LOGIN(SessionID sessionID, char* loginSessionKey);

	void Send_MONT_DATA_TO_CLIENT();

	wstring Create_LogDbTable(SQL_TIMESTAMP_STRUCT  currentTime);
	void Insert_LogDB(const wstring& tableName, SQL_TIMESTAMP_STRUCT  currentTime, int serverNo, int type, int dataAvr, int dataMin, int dataMax);

	static UINT __stdcall PerformanceCountFunc(void* arg);
	static UINT __stdcall LoggingToDbFunc(void* arg);
};

