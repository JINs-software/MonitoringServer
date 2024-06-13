#pragma once
#include "CLanServer.h"
#include "PerformanceCounter.h"
#include "MontServerConfig.h"
#include "MonitorProtocol.h"

using SessionID = UINT64;

class MonitoringServer : public CLanServer
{
private:
	struct stMontData {
		int dataValue = 0;
		int timeStamp = 0;
	};
	//std::map<BYTE, stMontData> m_MontDataMap;
	std::vector<stMontData>		m_MontDataVec;

	// 모니터링 대상 서버
	SessionID				m_LoginServerSession;
	SessionID				m_EchoGameServerSession;
	SessionID				m_ChatServerSession;
	//std::set<SessionID>		m_MontClientSessions;
	//SRWLOCK					m_MontClientSessionsSrwLock;

	SessionID				m_MontClientSessions[dfMAX_NUM_OF_MONT_CLIENT_TOOL];
	std::queue<BYTE>		m_EmptyIdxQueue;
	std::mutex				m_EmptyIdxQueueMtx;

	HANDLE					m_MontThread;
	bool					m_ExitThread;

	PerformanceCounter*		m_PerfCounter;

public:
	//CLanServer(const char* serverIP, uint16 serverPort,
	//	DWORD numOfIocpConcurrentThrd, uint16 numOfWorkerThreads, uint16 maxOfConnections,
	//	size_t tlsMemPoolDefaultUnitCnt = 0, size_t tlsMemPoolDefaultUnitCapacity = 0,
	//	bool tlsMemPoolReferenceFlag = false, bool tlsMemPoolPlacementNewFlag = false,
	//	UINT serialBufferSize = DEFAULT_SERIAL_BUFFER_SIZE,
	//	uint32 sessionRecvBuffSize = SESSION_RECV_BUFFER_DEFAULT_SIZE,
	//	bool beNagle = true, bool zeroCopySend = false
	//);
	MonitoringServer(const char* serverIP, uint16 serverPort,
		DWORD numOfIocpConcurrentThrd, uint16 numOfWorkerThreads, uint16 maxOfConnections,
		size_t tlsMemPoolDefaultUnitCnt = MONT_TLS_MEM_POOL_DEFAULT_UNIT_CNT, size_t tlsMemPoolDefaultCapacity = MONT_TLS_MEM_POOL_DEFAULT_UNIT_CAPACITY,
		UINT serialBufferSize = MONT_SERIAL_BUFFER_SIZE,
		uint32 sessionRecvBuffSize = MONT_SERV_SESSION_RECV_BUFF_SIZE
	)
		: CLanServer(serverIP, serverPort, numOfIocpConcurrentThrd, numOfWorkerThreads, maxOfConnections,
			tlsMemPoolDefaultUnitCnt, tlsMemPoolDefaultCapacity, true, false,
			serialBufferSize,
			sessionRecvBuffSize
		), 
		m_LoginServerSession(-1), m_EchoGameServerSession(-1), m_ChatServerSession(-1),
		m_ExitThread(false)
	{
		//InitializeSRWLock(&m_MontClientSessionsSrwLock);
		memset(m_MontClientSessions, 0, sizeof(m_MontClientSessions));
		for (BYTE i = 0; i < dfMAX_NUM_OF_MONT_CLIENT_TOOL; i++) {
			m_EmptyIdxQueue.push(i);
		}

		m_MontDataVec.resize(dfMONITOR_DATA_TYPE_MAX_NUM, {0});
	}

	bool Start() {
		if (!CLanServer::Start()) {
			return false;
		}

		m_PerfCounter = new PerformanceCounter();
		m_PerfCounter->SetCounter(dfMONITOR_DATA_TYPE_MONITOR_NONPAGED_MEMORY, dfQUERY_MEMORY_NON_PAGED);
		m_PerfCounter->SetCounter(dfMONITOR_DATA_TYPE_MONITOR_AVAILABLE_MEMORY, dfQUERY_MEMORY_AVAILABLE);
		m_PerfCounter->SetCpuUsageCounter();

		m_PerfCounter->SetEthernetCounter();
		//m_PerfCounter->SetCounter(dfMONITOR_DATA_TYPE_MONITOR_NETWORK_RECV, dfQUERY_ETHERNET_BYTES_RECEIVED_SEC);
		//m_PerfCounter->SetCounter(dfMONITOR_DATA_TYPE_MONITOR_NETWORK_SEND, dfQUERY_ETHERNET_BYTES_SENT_SEC);

#if defined(MONT_SERVER_MONITORING_MODE)
		m_PerfCounter->SetProcessCounter(dfMONITOR_DATA_TYPE_MONT_SERVER_MEM, dfQUERY_PROCESS_USER_VMEMORY_USAGE, L"MonitoringServer");
#endif
		
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

		for (BYTE i = 0; i < dfMAX_NUM_OF_MONT_CLIENT_TOOL; i++) {
			if (m_MontClientSessions[i] == sessionID) {
				m_MontClientSessions[i] = 0;

				m_EmptyIdxQueueMtx.lock();
				m_EmptyIdxQueue.push(i);
				m_EmptyIdxQueueMtx.unlock();
				std::cout << "[OnClientLeave] 모니터링 클라이언트 연결 종료" << std::endl;

				break;
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
	void Process_CS_MONITOR_TOOL_LOGIN(SessionID sessionID, char* loginSessionKey);

	void Send_MONT_DATA_TO_CLIENT();

	static UINT __stdcall PerformanceCountFunc(void* arg);
};

