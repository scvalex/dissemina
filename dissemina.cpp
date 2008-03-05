/*
 * dissemina.cpp -- very simple HTTP server
 *
 * listens for GET requests on port 6462 and carries them out
 */

#define DEBUG 1

using namespace std;

#include <iostream>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <cstring>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

const short LocalPort = 6462;

char buf[1024];
int listener;

char* getCurrentTime() {
	time_t t = time(0);
	static char tmp[26];
	ctime_r(&t, tmp);
	tmp[strlen(tmp)-1] = 0;
	return tmp;
}

#define warn(a) if (DEBUG) cerr << "dissemina: " << getCurrentTime() << " - " << a << endl
#define warn3(a, b, c) if (DEBUG) cerr << "dissemina: " << getCurrentTime() << " - " << a << b << c << endl
#define warn4(a, b, c, d) if (DEBUG) cerr << "dissemina: " << getCurrentTime() << " - " << a << b << c << d << endl

int sendall(int s, const char *buf, int &len) {
	int total(0),
		bytesleft = len,
		n;

	while (bytesleft > 0) {
		n = send(s, buf + total, bytesleft, 0);
		if (n < 0)
			break;
		total += n;
		bytesleft -= n;
	}
	len = total;

	return ((n == -1) ? (-1) : (0));
}

void trySendall(int s, const char *buf, int &len) {
	if (sendall(s, buf, len) == -1) {
		char aux[256];
		sprintf(aux, "sendall (%d)", s);
		perror(aux);
		exit(1);
	}
}

bool endsWith(const char *s, const char *w) {
	int i = strlen(s) - 1;
	int j = strlen(w) - 1;

	while ((i >= 0) && (j >= 0) && (s[i] == w[j]))
		--i,
		--j;

	return (j < 0);
}

const int FileHandleNum = 5;
const char *FileHandle[FileHandleNum][4] = {
	{"", "rb", "sending binary file", "application/octet-stream"},
	{".txt", "r", "sending text file", "text/plain"},
	{".cpp", "r", "sending text file", "text/plain"},
	{".html", "r", "sending html page", "text/html"},
	{".xml", "r", "sending html page", "text/html"}
};

void sendPage(int s, const char *fn) {
	FILE *fi;
	char conttype[32];
	int fh(0);
	for (int i(1); i < FileHandleNum; ++i)
		if (endsWith(fn, FileHandle[i][0])) {
			fh = i;
			break;
		}
	
	fi = fopen(fn, FileHandle[fh][1]);
	warn(FileHandle[fh][2]);
	strcpy(conttype, FileHandle[fh][3]);

	char header[256];
	sprintf(header , "HTTP/1.1 200 OK\r\n"
					 "Connection: close\r\n"
					 "Content-Type: %s\r\n"
					 "Server: Dissemina/0.0.0\r\n"
					 "\r\n", conttype);
	int len = strlen(header);
	trySendall(s, header, len);

	char buf[1025];
	while (!feof(fi) && !ferror(fi)) {
		len = fread(buf, 1, 1024, fi);
		trySendall(s, buf, len);
	}
}

bool canOpen(char *fn) {
	FILE *fi = fopen(fn, "r");
	return (fi != 0);
}

void send404(int s) {
	warn("problem reading requested file");
	warn("sending 404 Not Found");

	char text404[] = "HTTP/1.1 404 Not Found\r\n"
					 "Connection: close\r\n"
					 "Content-Type: text/plain\r\n"
					 "Server: Dissemina/0.0.0\r\n"
					 "\r\n"
					 "No\n";
	int len = strlen(text404);
	trySendall(s, text404, len);
}

void setuplistener() {
	if ((listener = socket(PF_INET, SOCK_STREAM, 0)) == -1) {
		perror("socket");
		exit(1);
	}

	int yes(1);
	if (setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, (char*)&yes, sizeof(yes)) < 0) {
		perror("setsockopt");
		exit(1);
	}

	sockaddr_in myaddr;
	memset(&myaddr, 0, sizeof(myaddr));
	myaddr.sin_family = AF_INET;
	myaddr.sin_port = htons(LocalPort);
	myaddr.sin_addr.s_addr = INADDR_ANY;
	if (bind(listener, (sockaddr*)&myaddr, sizeof(myaddr)) == -1) {
		perror("bind");
		exit(1);
	}

	if (listen(listener, 10) == -1) {
		perror("listen");
		exit(1);
	}
	warn3("*** listening on port ", LocalPort, " ***");
}

int main(int argc, char *argv[]) {
	setuplistener();

	fd_set master,
		   rfds;

	FD_ZERO(&master);
	FD_SET(listener, &master);
	int maxfd = listener;
	while (1) {
		rfds = master;
		if (select(maxfd + 1, &rfds, NULL, NULL, NULL) == -1) {
			perror("select");
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
						warn4("connection from ", inet_ntoa(remoteaddr.sin_addr), " on socket ", newfd);
					}
				} else {
					int nbytes;
					if ((nbytes = recv(i, buf, sizeof(buf), 0)) <= 0) {
						if (nbytes == 0)
							warn3("listen: socket ", i, " closed");
						else
							perror("recv");
						close(i);
						FD_CLR(i, &master);
					} else {
						//cerr << "dissemina: data from " << i << ": ``" << buf << "''" << endl;
						if ((buf[0] == 'G') && (buf[1] == 'E') && (buf[2] == 'T')) { // HTTP GET
							char filename[1024];
							filename[0] = '.';
							int j;
							for (j = 0; buf[4 + j] && (buf[4 + j] != ' ') && (j < 1022); ++j)
								filename[j + 1] = buf[4 + j];
							filename[j + 1] = 0;

							if ((j == 1) && (filename[1] == '/'))
								strcpy(filename, "./index.xml");

							if (!canOpen(filename)) {
								warn3("can't find ``", filename, "''");
								send404(i);
								close(i);
								FD_CLR(i, &master);
							} else {
								sendPage(i, filename);
								close(i);
								FD_CLR(i, &master);
							}
						}
						memset(buf, 0, sizeof(buf));
					}
				}
			}
	}

	close(listener);
	return 0;
}

