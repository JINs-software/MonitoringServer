#include "MonitoringServer.h"
#include <conio.h>

int main() {
	MonitoringServer montserver(MONT_DB_CONN_COUNT, MOND_ODBC_CONNECTION_STRING, MONT_SERVER_IP, MONT_SERVER_PORT, 0, 1, 10);

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