/*
 * dissemina.cpp -- very simple HTTP server
 *
 * listens for GET requests on port 6462 and carries them out
 */

#define DEBUG 0
const int MAXURISIZE = 1024;
const int NUM_FDS = 1024;

using namespace std;

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <cctype>
#include <cstring>
#include <poll.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

const short LocalPort = 6462;

char buf[1024];
int listener;

void quit_err(const char *s) {
	perror(s);
	exit(1);
}

char* getCurrentTime() {
	time_t t = time(0);
	static char tmp[26];
	ctime_r(&t, tmp);
	tmp[strlen(tmp)-1] = 0;
	return tmp;
}

const char* tostr(const char *s) {
	return s;
}

char* tostr(int n) {
	static char buf[16];
	sprintf(buf, "%d", n);
	return buf;
}

#define warn(a) if (DEBUG) fprintf(stderr,"dissemina: %s - %s\n", getCurrentTime(), a)
#define warn3(a, b, c) if (DEBUG) fprintf(stderr, "dissemina: %s - %s%s%s\n", getCurrentTime(), tostr(a), tostr(b), tostr(c))
#define warn4(a, b, c, d) if (DEBUG) fprintf(stderr, "dissemina: %s - %s%s%s%s\n", getCurrentTime(), tostr(a), tostr(b), tostr(c), tostr(d))

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

bool startsWith(const char *s, const char *w) {
	char *a = (char*)s,
		 *b = (char*)w;

	while (*a && *b && (*a == *b))
		++a,
		++b;

	return (!*a || !*b);
}

int getRequestUri(char *uri, char *buf) {
	char *a = buf;
	for (; *a == ' '; ++a);

	if (*a != '/')
		return -1;

	uri[0] = '.';
	int j;
	for (j = 0; *a && !isspace(*a) && (j < MAXURISIZE - 2); ++j, ++a)
		uri[j + 1] = *a;
	uri[j + 1] = 0;

	if ((j == 1) && (uri[1] == '/'))
		strcpy(uri, "./index.xml");

	return 0;
}

const int FileHandleNum = 5;
const char *FileHandle[FileHandleNum][4] = {
	{"", "rb", "sending binary file", "application/octet-stream"},
	{".txt", "r", "sending text file", "text/plain"},
	{".cpp", "r", "sending text file", "text/plain"}, // should be text/x-c++src but FF refuses to open them
	{".html", "r", "sending html page", "text/html"},
	{".xml", "r", "sending xml document", "application/xml"}
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

	fclose(fi);
}

int canOpen(const char *fn) {
	static struct stat s;
	if ((stat(fn, &s) != 0) || !S_ISREG(s.st_mode))
		return 0;
	
	if (strstr(fn, ".."))
		return 0;

	return 1;
}

void send400(int s) {
	warn("sending 400 Bad Request");

	char text400[] = "HTTP/1.1 400 Bad Request\r\n"
					 "Connection: close\r\n"
					 "Content-Type: text/plain\r\n"
					 "Server: Dissemina/0.0.0\r\n"
					 "\r\n"
					 "Bad Request\n";
	int len = strlen(text400);
	trySendall(s, text400, len);
}

void send404(int s) {
	warn("problem reading requested file");
	warn("sending 404 Not Found");

	char text404[] = "HTTP/1.1 404 Not Found\r\n"
					 "Connection: close\r\n"
					 "Content-Type: text/plain\r\n"
					 "Server: Dissemina/0.0.0\r\n"
					 "\r\n"
					 "Not Found\n";
	int len = strlen(text404);
	trySendall(s, text404, len);
}

void setuplistener() {
	if ((listener = socket(PF_INET, SOCK_STREAM, 0)) == -1)
		quit_err("socket");

	int yes(1);
	if (setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, (char*)&yes, sizeof(yes)) < 0)
		quit_err("setsockopt");

	sockaddr_in myaddr;
	memset(&myaddr, 0, sizeof(myaddr));
	myaddr.sin_family = AF_INET;
	myaddr.sin_port = htons(LocalPort);
	myaddr.sin_addr.s_addr = INADDR_ANY;
	if (bind(listener, (sockaddr*)&myaddr, sizeof(myaddr)) == -1)
		quit_err("bind");

	if (listen(listener, 10) == -1)
		quit_err("listen");
	warn3("*** listening on port ", LocalPort, " ***");
}

#define close_and_unset_fd(i)\
	{\
		close(fds[i].fd);\
		fds[i].fd = -1;\
	}

int main(int argc, char *argv[]) {
	setuplistener();

	struct pollfd fds[NUM_FDS];
	for (int i(0); i < NUM_FDS; ++i)
		fds[i].fd = -1;

	fds[0].fd = listener;
	fds[0].events = POLLRDNORM;
	for (;;) {
		if (poll(fds, NUM_FDS, -1) == -1)
			quit_err("poll");

		if (fds[0].revents & POLLRDNORM) {
			sockaddr_in remoteaddr;
			socklen_t addrlen = sizeof(remoteaddr);
			int newfd;
			if ((newfd = accept(listener, (sockaddr*)&remoteaddr, &addrlen)) == -1) {
				perror("accept");
				continue;
			}

			int i;
			for (i = 1; i < NUM_FDS; ++i)
				if (fds[i].fd == -1)
					break;
			if (i == NUM_FDS) {
				warn("too many open connections; dropping a new one");
				continue;
			}

			fds[i].fd = newfd;
			fds[i].events = POLLRDNORM;
			warn4("connection from ", inet_ntoa(remoteaddr.sin_addr), " on socket ", newfd);
		}

		for (int i(1); i < NUM_FDS; ++i)
			if (fds[i].revents & POLLRDNORM) {
				int nbytes;
				if ((nbytes = recv(fds[i].fd, buf, sizeof(buf), 0)) <= 0) {
					if (nbytes == 0)
						warn3("listen: socket ", fds[i].fd, " closed");
					else
						perror("recv");
					close_and_unset_fd(i);
				} else {
					//cerr << "dissemina: data from " << fds[i].fd << ": ``" << buf << "''" << endl;
					if (startsWith(buf, "GET")) { // HTTP GET
						char uri[MAXURISIZE];
						if (getRequestUri(uri, buf + 3) == -1) {
							warn("malformed request");
							send400(fds[i].fd);
							close_and_unset_fd(i);
						} else if (!canOpen(uri)) {
							warn3("can't find ``", uri, "''");
							send404(fds[i].fd);
							close_and_unset_fd(i);
						} else {
							sendPage(fds[i].fd, uri);
							close_and_unset_fd(i);
						}
					}
					memset(buf, 0, sizeof(buf));
				}
			}
	}

	close(listener);
	return 0;
}

