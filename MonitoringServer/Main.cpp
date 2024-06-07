#include "MonitoringServer.h"
#include <conio.h>

int main() {
	MonitoringServer montserver(NULL, 12121, 0, 1, 10);

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