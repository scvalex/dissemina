/*
 * listen.cpp -- listens on a selected port and echos all incoming
 * transmissions
 */

using namespace std;

#include <iostream>
#include <cstdlib>
#include <cstring>
#include <unistd.h>
#include <poll.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

const short LocalPort = 6462;
const int nfds = 128;

static char buf[1024];
static int listener;

#define MAX(a, b) ((a > b) ? (a) : (b))

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

	pollfd fds[nfds];
	for (int i(0); i < nfds; ++i)
		fds[i].fd = -1;

	fds[0].fd = listener;
	fds[0].events = POLLRDNORM;
	while (1) {
		//cout << "listen: waiting for poll" << endl;
		if (poll(fds, nfds, -1) == -1) {
			perror("poll");
			close(listener);
			exit(1);
		}

		if (fds[0].revents & POLLRDNORM) {
			sockaddr_in remoteaddr;
			socklen_t addrlen = sizeof(remoteaddr);

			int newfd;
			if ((newfd = accept(listener, (sockaddr*)&remoteaddr, &addrlen)) == -1) {
				perror("accept");
				continue;
			} 

			int i;
			for (i = 1; i < nfds; ++i)
				if (fds[i].fd == -1) 
					break;
			if (i == nfds) {
				cout << "listen: too many clients" << endl;
				continue;
			}

			cout << "listen: new connection on " << newfd << endl;
			fds[i].fd = newfd;
			fds[i].events = POLLRDNORM;
		}

		for (int i(1); i < nfds; ++i)
			if (fds[i].revents & POLLRDNORM) {
				cout << "listen: socket " << fds[i].fd << " seems to have data to read" << endl;
				int nbytes;
				if ((nbytes = recv(fds[i].fd, buf, sizeof(buf), 0)) <= 0) {
					if (nbytes == 0)
						cout << "listen: socket " << fds[i].fd << " closed" << endl;
					else
						perror("recv");
					close(fds[i].fd);
					fds[i].fd = -1;
				} else {
					cout << "listen: data from " << fds[i].fd << ": ``" << buf << "''" << endl;
					memset(buf, 0, sizeof(buf));
				}
			}
	}

	close(listener);
	return 0;
}

