#include <iostream>
#include <cstdlib>
#include <cstring>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

using namespace std;

const int LocalPort = 6462;

int main(int argc, char *argv[]) {
	fd_set master,		// the ones that don't change
		   read_fds;	// the copy select() changes

	int listener;
	if ((listener = socket(PF_INET, SOCK_STREAM, 0)) == -1) {
		perror("socket");
		exit(1);
	}

	int yes(1);
	if (setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1) {
		perror("setsockopt");
		exit(1);
	}

	sockaddr_in myaddr;
	myaddr.sin_family = AF_INET;
	myaddr.sin_addr.s_addr = INADDR_ANY;
	myaddr.sin_port = htons(LocalPort);
	memset(myaddr.sin_zero, 0, sizeof(myaddr.sin_zero));
	if (bind(listener, (sockaddr*)&myaddr, sizeof(myaddr)) == -1) {
		perror("bind");
		exit(1);
	}

	if (listen(listener, 10) == -1) {
		perror("listen");
		exit(1);
	}

	FD_ZERO(&master);
	FD_SET(listener, &master);
	//cout << "selectserver: listening on socket " << listener << endl;

	int fdmax = listener;

	while (true) {
		read_fds = master;
		if (select(fdmax+1, &read_fds, NULL, NULL, NULL) == -1) {
			perror("select");
			exit(1);
		}

		for (int i(0); i <= fdmax; ++i) 
			if (FD_ISSET(i, &read_fds)) {
				//cout << "checking socket " << i << endl;
				if (i == listener) {
					sockaddr_in remoteaddr;
					socklen_t addrlen = sizeof(remoteaddr);
					int newfd;
					if ((newfd = accept(listener, (sockaddr*)&remoteaddr, &addrlen)) == -1) 
						perror("accept");
					else {
						FD_SET(newfd, &master);
						if (newfd > fdmax) 
							fdmax = newfd;
					}
					cout << "selectserver: new connection from " << inet_ntoa(remoteaddr.sin_addr) << " on socket " << newfd << endl;
				} else {
					int nbytes;
					char buf[128];
					//cout << "selectserver: got something from socket " << i << endl;
					if ((nbytes = recv(i, buf, sizeof(buf), 0)) <= 0) {
						if (nbytes == 0) 
							cout << "selectserver: socket " << i << " hung up" << endl;
						else 
							perror("recv");
						close(i);
						FD_CLR(i, &master);
					} else {
						for (int j = 0; j <= fdmax; ++j) {
							if (FD_ISSET(j, &master))
								if ((j != listener) && (j != i)) 
									if (send(j, buf, nbytes, 0) == -1)
										perror("send");
						}
					}
				}
			}
		}

	return 0;
}

