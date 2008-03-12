/*
 * dissemina.c -- very fast HTTP server
 *
 * listens for GET requests on port 6462 and carries them out
 */

/* Set DEBUG to 1 to kill all output */
#define DEBUG 0
#define _XOPEN_SOURCE 1 /* Needed for POLLRDNORM... */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <ctype.h>
#include <poll.h>
#include <unistd.h>
#include <stdarg.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define MAXURISIZE 1024
#define MAXREQSIZE 4096

/* Maximum number of open network connections */
#define NUM_FDS 16

/* On which port shall I listen? */
#define LOCAL_PORT 6462

struct Request {
	char text[MAXREQSIZE];
	int len;
};

int listener;
struct pollfd fds[NUM_FDS];
struct Request reqs[NUM_FDS];

/* Display an error and quit */
void quit_err(const char *s) {
	perror(s);
	exit(1);
}

/* Return the current ctime as a string */
char* getCurrentTime() {
	time_t t = time(0);
	static char tmp[26];
	ctime_r(&t, tmp);
	tmp[strlen(tmp)-1] = 0;
	return tmp;
}

/* Output a warning */
void logprintf(char *fn, char *fmt, ...) {
	if (!DEBUG)
		return;
	va_list ap;
	va_start(ap, fmt);
	fprintf(stderr, "%16s: %s: ", fn, getCurrentTime());
	vfprintf(stderr, fmt, ap);
	fprintf(stderr, "\n");
}

/* Send all data in buf to s */
void sendall(int s, const char *buf, int *len) {
	int total = 0,
		bytesleft = *len,
		n;

	while (bytesleft > 0) {
		n = send(s, buf + total, bytesleft, 0);
		if (n < 0) {
			logprintf("sendall", "trouble sending data to %d", s);
			return;
		}
		total += n;
		bytesleft -= n;
	}
	*len = total;
}

/* Returns true if s ends with w */
int endsWith(const char *s, const char *w) {
	int i = strlen(s) - 1,
		j = strlen(w) - 1;

	while ((i >= 0) && (j >= 0) && (s[i] == w[j]))
		--i,
		--j;

	return (j < 0);
}

/* Returns true if s starts with w
 * NOTE: returns false if s == w */
int startsWith(const char *s, const char *w) {
	char *a = (char*)s,
		 *b = (char*)w;

	while (*a && *b && (*a == *b))
		++a,
		++b;

	return (!*a || !*b);
}

/* Returns the URI as a string */
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

#define FileHandleNum 5
const char *FileHandle[FileHandleNum][4] = {
	{"", "rb", "sending binary file", "application/octet-stream"},
	{".txt", "r", "sending text file", "text/plain"},
	{".c", "r", "sending text file", "text/plain"}, // should be text/x-c++src but FF refuses to open them
	{".html", "r", "sending html page", "text/html"},
	{".xml", "r", "sending xml document", "application/xml"}
};

/* Send file fn to s */
void sendPage(int s, const char *fn) {
	FILE *fi;
	char conttype[32];
	int fh = 0;
	int i;
	for (i = 1; i < FileHandleNum; ++i)
		if (endsWith(fn, FileHandle[i][0])) {
			fh = i;
			break;
		}
	
	fi = fopen(fn, FileHandle[fh][1]);
	logprintf("sendPage", "%s", FileHandle[fh][2]);
	strcpy(conttype, FileHandle[fh][3]);

	char header[256];
	sprintf(header , "HTTP/1.1 200 OK\r\n"
					 "Connection: close\r\n"
					 "Content-Type: %s\r\n"
					 "Server: Dissemina/0.0.0\r\n"
					 "\r\n", conttype);
	int len = strlen(header);
	sendall(s, header, &len);

	char buf[1025];
	while (!feof(fi) && !ferror(fi)) {
		len = fread(buf, 1, 1024, fi);
		sendall(s, buf, &len);
	}

	fclose(fi);
}

/* Returns true if file should be openable */
int canOpen(const char *fn) {
	static struct stat s;
	if ((stat(fn, &s) != 0) || !S_ISREG(s.st_mode))
		return 0;
	
	if (strstr(fn, ".."))
		return 0;

	return 1;
}

/* Send 400 Bad Request to s */
void send400(int s) {
	logprintf("send400", "sending 400 Bad Request");

	char text400[] = "HTTP/1.1 400 Bad Request\r\n"
					 "Connection: close\r\n"
					 "Content-Type: text/plain\r\n"
					 "Server: Dissemina/0.0.0\r\n"
					 "\r\n"
					 "Bad Request\n";
	int len = strlen(text400);
	sendall(s, text400, &len);
}

/* Send 404 Not Found to s */
void send404(int s) {
	logprintf("send404", "problem reading requested file");
	logprintf("send404", "sending 404 Not Found");

	char text404[] = "HTTP/1.1 404 Not Found\r\n"
					 "Connection: close\r\n"
					 "Content-Type: text/plain\r\n"
					 "Server: Dissemina/0.0.0\r\n"
					 "\r\n"
					 "Not Found\n";
	int len = strlen(text404);
	sendall(s, text404, &len);
}

/* Create the listener and return it */
void setupListener() {
	if ((listener = socket(PF_INET, SOCK_STREAM, 0)) == -1)
		quit_err("socket");

	int yes = 1;
	if (setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, (char*)&yes, sizeof(yes)) < 0)
		quit_err("setsockopt");

	struct sockaddr_in myaddr;
	memset(&myaddr, 0, sizeof(myaddr));
	myaddr.sin_family = AF_INET;
	myaddr.sin_port = htons(LOCAL_PORT);
	myaddr.sin_addr.s_addr = INADDR_ANY;
	if (bind(listener, (struct sockaddr*)&myaddr, sizeof(myaddr)) == -1)
		quit_err("bind");

	if (listen(listener, 10) == -1)
		quit_err("listen");
	logprintf("setupListener", "*** listening on port %d ***", LOCAL_PORT);
}

#define close_and_unset_fd(i)\
		close(fds[i].fd);\
		logprintf("listen", "socket %d closed", fds[i].fd);\
		fds[i].fd = -1;

int main(int argc, char *argv[]) {
	setupListener();

	int i;
	for (i = 0; i < NUM_FDS; ++i)
		fds[i].fd = -1;

	fds[0].fd = listener;
	fds[0].events = POLLRDNORM;
	for (;;) {
		if (poll(fds, NUM_FDS, -1) == -1)
			quit_err("poll");

		if (fds[0].revents & POLLRDNORM) {
			struct sockaddr_in remoteaddr;
			socklen_t addrlen = sizeof(remoteaddr);
			int newfd;
			if ((newfd = accept(listener, (struct sockaddr*)&remoteaddr, &addrlen)) == -1) {
				perror("accept");
				continue;
			}

			int i;
			for (i = 1; i < NUM_FDS; ++i)
				if (fds[i].fd == -1)
					break;
			if (i == NUM_FDS) {
				logprintf("dissemina", "too many open connections; dropping a new one");
				continue;
			}

			fds[i].fd = newfd;
			fds[i].events = POLLRDNORM;
			memset(&reqs[i], 0, sizeof(reqs[0]));
			logprintf("dissemina", "connection from %s on socket %d", inet_ntoa(remoteaddr.sin_addr), newfd);
		}

		for (i = 1; i < NUM_FDS; ++i)
			if (fds[i].revents & POLLRDNORM) {
				int nbytes;
				if ((nbytes = recv(fds[i].fd, reqs[i].text + reqs[i].len, sizeof(reqs[0].text) - reqs[i].len, 0)) <= 0) {
					if (nbytes == 0)
						logprintf("dissemina", "listen: socket %d closed", fds[i].fd);
					else
						perror("recv");
					close_and_unset_fd(i);
				} else {
					reqs[i].len += nbytes;
					//logprintf("dissemina", "data from %d: ``%s''", fds[i].fd, reqs[i].text);
					if (!endsWith(reqs[i].text, "\r\n\r\n"))
						continue;
					if (startsWith(reqs[i].text, "GET")) { // HTTP GET
						char uri[MAXURISIZE];
						if (getRequestUri(uri, reqs[i].text + 3) == -1) {
							logprintf("dissemina", "malformed request");
							send400(fds[i].fd);
							close_and_unset_fd(i);
						} else if (!canOpen(uri)) {
							logprintf("dissemina", "can't find ``%s''", uri);
							send404(fds[i].fd);
							close_and_unset_fd(i);
						} else {
							sendPage(fds[i].fd, uri);
							close_and_unset_fd(i);
						}
					}
				}
			}
	}

	close(listener);
	return 0;
}

