#include <iostream>
#include <cstdlib>
#include <unistd.h>
#include <errno.h>
#include <cstring>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <signal.h>

using namespace std;

const int LocalPort = 6462;
const int Backlog = 10;

void sigchld_handler(int s) {
	while (waitpid(-1, NULL, WNOHANG) > 0);
}

int main(int argc, char *argv[]) {
	int sockfd;

	if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
		perror("socket");
		exit(1);
	}

	int yes(1);
	if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1) {
		perror("setsockopt");
		exit(1);
	}

	sockaddr_in myAddr;
	myAddr.sin_family = AF_INET;
	myAddr.sin_port = htons(LocalPort);
	myAddr.sin_addr.s_addr = INADDR_ANY;
	memset(myAddr.sin_zero, 0, sizeof(myAddr.sin_zero));

	if (bind(sockfd, (sockaddr *)&myAddr, sizeof(myAddr)) == -1) {
		perror("bind");
		exit(1);
	}

	if (listen(sockfd, Backlog) == -1) {
		perror("listen");
		exit(1);
	}

	struct sigaction sa;
	sa.sa_handler = sigchld_handler;
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = SA_RESTART;

	if (sigaction(SIGCHLD, &sa, NULL) == -1) {
		perror("sigaction");
		exit(1);
	}

	socklen_t sinSize;
	sockaddr_in theirAddr;
	int newfd;
	while (true) {
		sinSize = sizeof(theirAddr);
		if ((newfd = accept(sockfd, (sockaddr*)&theirAddr, &sinSize)) == -1) {
			perror("accept");
			continue;
		}

		cout << "server: got connection from " << inet_ntoa(theirAddr.sin_addr) << endl;

		if (!fork()) {
			close(sockfd);
			if (send(newfd, "Hello, world!\n", 14, 0) == -1)
				perror("send");
			close(newfd);
			exit(0);
		}

		close(newfd);
	}

	return 0;
}

