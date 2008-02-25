#include <iostream>
#include <stdlib.h>
#include <errno.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

using namespace std;

int main(int argc, char *argv[]) {
	hostent *h;

	if (argc != 2) {
		cerr << "usage: getip <address>" << endl;
		exit(1);
	}

	if ((h = gethostbyname(argv[1])) == NULL) {
		herror("gethostbyname");
		exit(1);
	}

	cout << "Host name: " << h->h_name << endl;
	cout << "IP       : " << inet_ntoa(*((in_addr*)h->h_addr)) << endl;

	return 0;
}

