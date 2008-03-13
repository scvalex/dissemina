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
#define NUM_FDS 64

/* On which port shall I listen? */
#define LOCAL_PORT 6462

#define DONE 1
#define NOTDONE 0

struct Request {
	char text[MAXREQSIZE];
	char uri[MAXURISIZE];
	int len;
	char ready;
	FILE *fi;
};

int listener;
struct pollfd fds[NUM_FDS];
struct Request reqs[NUM_FDS];
int requestsReady; /* the number of requests that are ready for handling */

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

	return 1;
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
int sendPage(int idx) {
	char conttype[32];
	int fh = 0;
	int i;
	for (i = 1; i < FileHandleNum; ++i)
		if (endsWith(reqs[idx].uri, FileHandle[i][0])) {
			fh = i;
			break;
		}
	
	int len;
	if (!reqs[idx].fi) {
		reqs[idx].fi = fopen(reqs[idx].uri, FileHandle[fh][1]);
		logprintf("sendPage", "%s", FileHandle[fh][2]);
		strcpy(conttype, FileHandle[fh][3]);

		char header[256];
		sprintf(header , "HTTP/1.1 200 OK\r\n"
						 "Connection: close\r\n"
						 "Content-Type: %s\r\n"
						 "Server: Dissemina/0.0.0\r\n"
						 "\r\n", conttype);
		len = strlen(header);
		sendall(fds[idx].fd, header, &len);
	}

	char buf[1025];
	if (!feof(reqs[idx].fi) && !ferror(reqs[idx].fi)) {
		len = fread(buf, 1, 1024, reqs[idx].fi);
		sendall(fds[idx].fd, buf, &len);
	} else {
		fclose(reqs[idx].fi);
		return DONE;
	}

	logprintf("sendPage", "not done sending page");
	return NOTDONE;
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
int send400(int idx) {
	logprintf("send400", "sending 400 Bad Request");

	char text400[] = "HTTP/1.1 400 Bad Request\r\n"
					 "Connection: close\r\n"
					 "Content-Type: text/plain\r\n"
					 "Server: Dissemina/0.0.0\r\n"
					 "\r\n"
					 "Bad Request\n";
	int len = strlen(text400);
	sendall(fds[idx].fd, text400, &len);

	return DONE;
}

/* Send 404 Not Found to s */
int send404(int idx) {
	logprintf("send404", "problem reading requested file");
	logprintf("send404", "sending 404 Not Found");

	char text404[] = "HTTP/1.1 404 Not Found\r\n"
					 "Connection: close\r\n"
					 "Content-Type: text/plain\r\n"
					 "Server: Dissemina/0.0.0\r\n"
					 "\r\n"
					 "Not Found\n";
	int len = strlen(text404);
	sendall(fds[idx].fd, text404, &len);

	return DONE;
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

void close_and_unset_fd(int i) {
	close(fds[i].fd);
	logprintf("listen", "socket %d closed", fds[i].fd);
	fds[i].fd = -1;
	reqs[i].ready = 0;
	--requestsReady;
}

/* check listener for new connections and write them into fds and reqs */
void getNewConnections() {
	if (fds[0].revents & POLLRDNORM) {
		struct sockaddr_in remoteaddr;
		socklen_t addrlen = sizeof(remoteaddr);
		int newfd;
		if ((newfd = accept(listener, (struct sockaddr*)&remoteaddr, &addrlen)) == -1) {
			perror("accept");
			return;
		}

		int i;
		for (i = 1; i < NUM_FDS; ++i)
			if (fds[i].fd == -1)
				break;
		if (i == NUM_FDS) {
			logprintf("dissemina", "too many open connections; dropping a new one");
			return;
		}

		fds[i].fd = newfd;
		fds[i].events = POLLRDNORM;
		memset(&reqs[i], 0, sizeof(reqs[0]));
		logprintf("dissemina", "connection from %s on socket %d", inet_ntoa(remoteaddr.sin_addr), newfd);
	}
}

/* read and write data from/to sockets */
void processConnections() {
	int i;
	/* check if there's anything to read */
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
					reqs[i].ready = getRequestUri(reqs[i].uri, reqs[i].text + 3);
					++requestsReady;
				} else
					fds[i].fd = -1; /* ignore all other requests */
			}
		}
	
	for (i = 1; i < NUM_FDS; ++i)
		if (reqs[i].ready) {
			if (reqs[i].ready == -1) {
				logprintf("dissemina", "malformed request");
				if (send400(i) == DONE)
					close_and_unset_fd(i);
			} else if (!canOpen(reqs[i].uri)) {
				logprintf("dissemina", "can't find ``%s''", reqs[i].uri);
				if (send404(i) == DONE)
					close_and_unset_fd(i);
			} else {
				if (sendPage(i) == DONE)
					close_and_unset_fd(i);
			}
		}
}

int main(int argc, char *argv[]) {
	setupListener();

	int i;
	for (i = 0; i < NUM_FDS; ++i)
		fds[i].fd = -1;

	fds[0].fd = listener;
	fds[0].events = POLLRDNORM;
	int timeout;
	requestsReady = 0;
	for (;;) {
		timeout = -1;
		if (requestsReady > 0)
			timeout = 1;
		logprintf("dissemina", "polling");
		if (poll(fds, NUM_FDS, timeout) == -1)
			quit_err("poll");
		
		getNewConnections();
		
		processConnections();
	}

	close(listener);
	return 0;
}

