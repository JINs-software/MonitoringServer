#pragma once
#include "CLanServer.h"
#include "MontServerConfig.h"
#include "PerformanceCounter.h"

using SessionID = UINT64;

class MonitoringServer : public CLanServer
{
private:
	struct stMontData {
		int dataValue = 0;
		int timeStamp = 0;
	};
	//stMontData m_MontDataArr[dfMONITOR_DATA_TYPE_MAX_NUM];
	std::map<BYTE, stMontData> m_MontDataMap;

	// 모니터링 대상 서버
	SessionID				m_LoginServerSession;
	SessionID				m_EchoGameServerSession;
	SessionID				m_ChatServerSession;
	std::set<SessionID>		m_MontClientSessions;
	SRWLOCK					m_MontClientSessionsSrwLock;

	HANDLE					m_MontThread;
	bool					m_ExitThread;

	PerformanceCounter*		m_PerfCounter;

public:
	MonitoringServer(const char* serverIP, uint16 serverPort,
		DWORD numOfIocpConcurrentThrd, uint16 numOfWorkerThreads, uint16 maxOfConnections,
		size_t tlsMemPoolDefaultUnitCnt = MONT_TLS_MEM_POOL_DEFAULT_UNIT_CNT, size_t tlsMemPoolDefaultCapacity = MONT_TLS_MEM_POOL_DEFAULT_UNIT_CAPACITY,
		uint32 sessionSendBuffSize = MONT_SERV_SESSION_SEND_BUFF_SIZE, uint32 sessionRecvBuffSize = MONT_SERV_SESSION_RECV_BUFF_SIZE,
		bool beNagle = true
	)
		: CLanServer(serverIP, serverPort, numOfIocpConcurrentThrd, numOfWorkerThreads, maxOfConnections, true, false,
			tlsMemPoolDefaultUnitCnt, tlsMemPoolDefaultCapacity,
			sessionSendBuffSize, sessionRecvBuffSize
		), 
		m_LoginServerSession(0), m_EchoGameServerSession(0), m_ChatServerSession(0),
		m_ExitThread(false)
	{
		InitializeSRWLock(&m_MontClientSessionsSrwLock);

		m_MontDataMap.insert({1, { 0 }});
		m_MontDataMap.insert({2, { 0 }});
		m_MontDataMap.insert({3, { 0 }});
		m_MontDataMap.insert({4, { 0 }});
		m_MontDataMap.insert({5, { 0 }});
		m_MontDataMap.insert({6, { 0 }});
									
		m_MontDataMap.insert({10, { 0 }});
		m_MontDataMap.insert({11, { 0 }});
		m_MontDataMap.insert({12, { 0 }});
		m_MontDataMap.insert({13, { 0 }});
		m_MontDataMap.insert({14, { 0 }});
		m_MontDataMap.insert({15, { 0 }});
		m_MontDataMap.insert({16, { 0 }});
		m_MontDataMap.insert({17, { 0 }});
		m_MontDataMap.insert({18, { 0 }});
		m_MontDataMap.insert({19, { 0 }});
		m_MontDataMap.insert({20, { 0 }});
		m_MontDataMap.insert({21, { 0 }});
		m_MontDataMap.insert({22, { 0 }});
		m_MontDataMap.insert({23, { 0 }});
									
		m_MontDataMap.insert({30, { 0 }});
		m_MontDataMap.insert({31, { 0 }});
		m_MontDataMap.insert({32, { 0 }});
		m_MontDataMap.insert({33, { 0 }});
		m_MontDataMap.insert({34, { 0 }});
		m_MontDataMap.insert({35, { 0 }});
		m_MontDataMap.insert({36, { 0 }});
		m_MontDataMap.insert({37, { 0 }});
									
		m_MontDataMap.insert({40, { 0 }});
		m_MontDataMap.insert({41, { 0 }});
		m_MontDataMap.insert({42, { 0 }});
		m_MontDataMap.insert({43, { 0 }});
		m_MontDataMap.insert({44, { 0 }});
	}

	bool Start() {
		if (!CLanServer::Start()) {
			return false;
		}

		m_PerfCounter = new PerformanceCounter();
		//m_PerfCounter->SetCounter(MONITOR_DATA_TYPE_MONITOR_CPU_TOTAL, NULL);
		m_PerfCounter->SetCounter(MONITOR_DATA_TYPE_MONITOR_NONPAGED_MEMORY, dfQUERY_MEMORY_NON_PAGED);
		//m_PerfCounter->SetCounter(MONITOR_DATA_TYPE_MONITOR_NETWORK_RECV, NULL);
		//m_PerfCounter->SetCounter(MONITOR_DATA_TYPE_MONITOR_NETWORK_SEND, NULL);
		m_PerfCounter->SetCounter(MONITOR_DATA_TYPE_MONITOR_AVAILABLE_MEMORY, dfQUERY_MEMORY_AVAILABLE);

		m_PerfCounter->SetCpuUsageCounter();
		
		m_MontThread = (HANDLE)_beginthreadex(NULL, 0, PerformanceCountFunc, this, 0, NULL);
		if (m_MontThread == INVALID_HANDLE_VALUE) {
			return false;
		}

		return true;
	}

	virtual void OnRecv(UINT64 sessionID, JBuffer& recvBuff) override;
	virtual void OnClientJoin(UINT64 sessionID) {
		std::cout << "[OnClientJoin] sessionID: " << sessionID << std::endl;
	};
	virtual void OnClientLeave(UINT64 sessionID) {
		std::cout << "[OnClientLeave] sessionID: " << sessionID << std::endl;
		
		AcquireSRWLockExclusive(&m_MontClientSessionsSrwLock);
		if (m_MontClientSessions.find(sessionID) != m_MontClientSessions.end()) {
			m_MontClientSessions.erase(sessionID);
			std::cout << "[OnClientLeave] 모니터링 클라이언트 연결 종료" << std::endl;
		}
		ReleaseSRWLockExclusive(&m_MontClientSessionsSrwLock);

		if (m_LoginServerSession == sessionID) {
			m_LoginServerSession = 0;
			std::cout << "[OnClientLeave] 로그인 서버 연결 종료" << std::endl;
		}
		else if (m_EchoGameServerSession == sessionID) {
			m_EchoGameServerSession = 0;
			std::cout << "[OnClientLeave] 에코 게임 서버 연결 종료" << std::endl;
		}
		else if (m_ChatServerSession == sessionID) {
			m_ChatServerSession = 0;
			std::cout << "[OnClientLeave] 채팅 서버 연결 종료" << std::endl;
		}

	};

	void Process_SS_MONITOR_LOGIN(SessionID sessionID, int serverNo);
	void Process_SS_MONITOR_DATA_UPDATE(BYTE dataType, int dataVal, int timeStamp);
	void Process_CS_MONITOR_TOOL_LOGIN(SessionID sessionID, char* loginSessionKey);

	void Send_MONT_DATA_TO_CLIENT();

	static UINT __stdcall PerformanceCountFunc(void* arg);
};

