#include "MonitoringServer.h"
#include <conio.h>

int main() {
	MonitoringServer montserver("127.0.0.1", 10001, 0, 2, 10);

	montserver.Start();

	char ctr;
	while (true) {
		if (_kbhit()) {
			ctr = _getch();
			if (ctr == 'q' || 'Q') {
				break;
			}
		}
	}

	montserver.Stop();

	return 0;
}