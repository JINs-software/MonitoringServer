#include "MonitoringServer.h"
#include <conio.h>

int main() {
	//MonitoringServer(const char* serverIP, uint16 serverPort,
	//	DWORD numOfIocpConcurrentThrd, uint16 numOfWorkerThreads, uint16 maxOfConnections,
	//	size_t tlsMemPoolDefaultUnitCnt = MONT_TLS_MEM_POOL_DEFAULT_UNIT_CNT, size_t tlsMemPoolDefaultCapacity = MONT_TLS_MEM_POOL_DEFAULT_UNIT_CAPACITY,
	//	UINT serialBufferSize = MONT_SERIAL_BUFFER_SIZE,
	//	uint32 sessionRecvBuffSize = MONT_SERV_SESSION_RECV_BUFF_SIZE
	//)
	MonitoringServer montserver(MONT_SERVER_IP, MONT_SERVER_PORT, 0, 1, 10);

	montserver.Start();

	char ctr;
	while (true) {
		if (_kbhit()) {
			ctr = _getch();
			if (ctr == 'q' || 'Q') {
				break;
			}
		}

		Sleep(10000);
	}

	montserver.Stop();

	return 0;
}