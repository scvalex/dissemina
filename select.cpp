#include <iostream>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

using namespace std;

int main(int argc, char *argv[]) {
	timeval tv;
	fd_set readfds;

	tv.tv_sec = 2;
	tv.tv_usec = 500000;

	FD_ZERO(&readfds);
	FD_SET(0, &readfds);

	select(0 + 1, &readfds, NULL, NULL, &tv);

	if (FD_ISSET(0, &readfds))
		out << "A key was pressed." << endl;
	else
		cout << "Timed out." << endl;

	return 0;
}

