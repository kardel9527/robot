#include <winsock2.h>
#include "robot.h"

#pragma comment(lib,"Ws2_32.lib ")

void init_win_net() {
	WSAData wsa;
	::WSAStartup(MAKEWORD(2, 2), &wsa);
}

void uninit_win_net() {
	WSACleanup();
}

int main(int argc, char *argv[]) {
	init_win_net();

	Robot *robot = Robot::instance();
	robot->init();
	robot->run();
	robot->stop();
	robot->uninit();

	uninit_win_net();
	return 0;
}
