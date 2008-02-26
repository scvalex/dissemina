/*
 * listen.cpp -- listens on a selected port and echos all incoming
 * transmissions
 */

using namespace std;

#include <iostream>
#include <cstdlib>
#include <cstring>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

const short LocalPort = 6462;

static char buf[1024];
static int listener;

int main(int argc, char *argv[]) {
	if ((listener = socket(PF_INET, SOCK_STREAM, 0)) == -1) {
		perror("socket");
		exit(1);
	}

	int yes(1);
	if (setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, (char*)&yes, sizeof(yes)) < 0) {
		perror("setsockopt");
		close(listener);
		exit(1);
	}

	sockaddr_in myaddr;
	memset(&myaddr, 0, sizeof(myaddr));
	myaddr.sin_family = AF_INET;
	myaddr.sin_port = htons(LocalPort);
	myaddr.sin_addr.s_addr = INADDR_ANY;
	if (bind(listener, (sockaddr*)&myaddr, sizeof(myaddr)) == -1) {
		perror("bind");
		close(listener);
		exit(1);
	}

	if (listen(listener, 10) == -1) {
		perror("listen");
		close(listener);
		exit(1);
	}
	cout << "listen: *** listening on port " << LocalPort << " ***" << endl;

	fd_set master,
		   rfds;

	FD_ZERO(&master);
	FD_SET(listener, &master);
	int maxfd = listener;
	while (1) {
		rfds = master;
		if (select(maxfd + 1, &rfds, NULL, NULL, NULL) == -1) {
			perror("select");
			close(listener);
			exit(1);
		}

		for (int i(0); i <= maxfd; ++i)
			if (FD_ISSET(i, &rfds)) {
				if (i == listener) {
					sockaddr_in remoteaddr;
					socklen_t addrlen = sizeof(remoteaddr);
					int newfd;
					if ((newfd = accept(listener, (sockaddr*)&remoteaddr, &addrlen)) == -1)
						perror("accept");
					else {
						FD_SET(newfd, &master);
						if (newfd > maxfd)
							maxfd = newfd;
						cout << "listen: connection from " << inet_ntoa(remoteaddr.sin_addr) << " on socket " << newfd << endl;
					}
				} else {
					int nbytes;
					if ((nbytes = recv(i, buf, sizeof(buf), 0)) <= 0) {
						if (nbytes == 0)
							cout << "listen: socket " << i << " closed" << endl;
						else
							perror("recv");
						close(i);
						FD_CLR(i, &master);
					} else {
						cout << "listen: data from " << i << ": ``" << buf << "''" << endl;
						memset(buf, 0, sizeof(buf));
					}
				}
			}
	}

	close(listener);
	return 0;
}

