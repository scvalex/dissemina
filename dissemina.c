/*
 * dissemina.c 
 * 		very fast webserver
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

/* Maximum size, in bytes, or URI */
#define MAXURISIZE 1024
/* Maximum size, in bytes, of request */
#define MAXREQSIZE 4096

/* Number of bytes sent in one go */
#define SENDBUFSIZE 1024

/* Maximum number of open network connections */
#define NUM_FDS 128

/* On which port shall I listen? */
#define LOCAL_PORT 6462

#define DONE 1
#define NOTDONE 0

enum RequestState {
	ReadingRequest,
	ProcessingRequest,
	RequestDone
};

struct Request {
	char text[MAXREQSIZE];
	char uri[MAXURISIZE];
	int len;
	int fd;	/* network socket FD */
	FILE *fi;
	int lft; /* local file type; i,e, is it a directory, normal file or inexistant*/
	int state;
	struct Request *next, *prev;
};

int listener;
struct pollfd fds[NUM_FDS];
struct Request *reqs2[NUM_FDS];
struct Request requests;

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

/* Create a new Request and return a pointer to it */
struct Request* create_request() {
	struct Request *r = malloc(sizeof(struct Request));
	memset(r, 0, sizeof(struct Request));
	return r;
}

/* Remove a request from the requests and free() it 
 * Returns a pointer to the PREVIOUS location */
struct Request* remove_and_free_request(struct Request *r) {
	struct Request *p = r->prev;
	p->next = r->next;
	if (r->next)
		r->next->prev = p;
	free(r);
	return p;
}

/* Display queued requests */
void displayRequests() {
	struct Request *cr;
	for (cr = requests.next; cr; cr = cr->next)
		if (cr->state == ProcessingRequest) 
			logprintf("processRequests", "%d", cr->fd);
	logprintf("processRequests", "---");
}

enum FileTypes {
	InexistantFile = -555,
	FileIsDirectory = 444,
	OtherFileType = 2,
	InvalidPath = 0,
	NormalFile = 1
};

/* Returns a numeric constant that represents the file's type (directory,
 * normal file, inexistant) */
int get_file_type(char *fn) {
	static struct stat s;
	if (stat(fn, &s) != 0)
		return InexistantFile;

	if (S_ISDIR(s.st_mode))
		return FileIsDirectory;

	if (!S_ISREG(s.st_mode))
		return OtherFileType;
	
	if (strstr(fn, ".."))
		return InvalidPath;

	return NormalFile;
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

/* Send all data in buf to s */
void sendall(int s, const char *buf, int *len) {
	int total = 0,
		bytesleft = *len,
		n;

	while (bytesleft > 0) {
		n = send(s, buf + total, bytesleft, MSG_NOSIGNAL);
		if (n < 0) {
			printf("trouble sending data to %d\n", s);
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

#define FileHandleNum 5
const char *FileHandle[FileHandleNum][4] = {
	{"", "rb", "sending binary file", "application/octet-stream"},
	{".txt", "r", "sending text file", "text/plain"},
	{".c", "r", "sending text file", "text/plain"}, // should be text/x-c++src but FF refuses to open them
	{".html", "r", "sending html page", "text/html"},
	{".xml", "r", "sending xml document", "application/xml"}
};

/* Send file fn to s */
int sendPage(struct Request *r) {
	int fh = 0;
	int i;
	for (i = 1; i < FileHandleNum; ++i)
		if (endsWith(r->uri, FileHandle[i][0])) {
			fh = i;
			break;
		}
	
	int len;
	if (!r->fi) {
		r->fi = fopen(r->uri, FileHandle[fh][1]);
		logprintf("sendPage", "%s to %d", FileHandle[fh][2], r->fd);

		char header[256];
		sprintf(header , "HTTP/1.1 200 OK\r\n"
						 "Connection: close\r\n"
						 "Content-Type: %s\r\n"
						 "Server: Dissemina/0.0.0\r\n"
						 "\r\n", FileHandle[fh][3]);
		len = strlen(header);
		sendall(r->fd, header, &len);
	}

	char buf[SENDBUFSIZE + 2];
	memset(buf, 0, sizeof(buf));
	if (!feof(r->fi) && !ferror(r->fi)) {
		len = fread(buf, 1, 1024, r->fi);
		/* logprintf("sendPage", "sending %d bytes to %d", len, r->fd); */
		sendall(r->fd, buf, &len);
	}
	
	if (feof(r->fi) || ferror(r->fi) || (len < 1024)) {
		fclose(r->fi);
		return DONE;
	}

	return NOTDONE;
}

/* Send 404 Not Found to s */
int send404(struct Request *r) {
	logprintf("send404", "problem reading requested file");
	logprintf("send404", "sending 404 Not Found");

	char text404[] = "HTTP/1.1 404 Not Found\r\n"
					 "Connection: close\r\n"
					 "Content-Type: text/plain\r\n"
					 "Server: Dissemina/0.0.0\r\n"
					 "\r\n"
					 "Not Found\n";
	int len = strlen(text404);
	sendall(r->fd, text404, &len);

	return DONE;
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
		/* Create and prepend a new Request */
		//logprintf("getNewConnections", "adding a new Request to list");
		struct Request *nr = create_request();
		nr->next = requests.next;
		if (nr->next)
			nr->next->prev = nr;
		nr->prev = &requests;
		nr->fd = newfd;
		nr->state = ReadingRequest;
		reqs2[i] = nr;
		requests.next = nr;
		logprintf("getNewConnections", "connection from %s on socket %d", inet_ntoa(remoteaddr.sin_addr), newfd);
	}
}

/* fills in a Request, mainly. by looking at it's text */
void fill_in_request(struct Request *r) {
	char *a = r->text + 3; /* jump over the GET */
	for (; *a == ' '; ++a);

	if (*a != '/')
		return; /* request is invalid */

	r->uri[0] = '.';
	int j;
	for (j = 0; *a && !isspace(*a) && (j < MAXURISIZE - 2); ++j, ++a)
		r->uri[j + 1] = *a;
	r->uri[j + 1] = 0;
	r->state = ProcessingRequest; /* Mark the request for processing */

	r->lft = get_file_type(r->uri);
	if (r->lft == FileIsDirectory) {
		strcat(r->uri, "/index.xml");
		r->lft = get_file_type(r->uri);
	}
}

/* reads data from sockets */
void checkReadingConnections() {
	int i;
	for (i = 1; i < NUM_FDS; ++i)
		if (fds[i].revents & POLLRDNORM) {
			struct Request *cr = reqs2[i]; /* The current request */
			int nbytes;
			if ((nbytes = recv(fds[i].fd, cr->text + cr->len, MAXREQSIZE - cr->len, 0)) <= 0) {
				if (nbytes == 0)
					logprintf("dissemina", "listen: socket %d closed", fds[i].fd);
				else
					perror("recv");
				close(fds[i].fd);
				fds[i].fd = -1;
				remove_and_free_request(cr);
			} else {
				cr->len += nbytes;
				//logprintf("dissemina", "data from %d: ``%s''", fds[i].fd, reqs[i].text);
				if (!endsWith(cr->text, "\r\n\r\n"))
					continue;
				if (startsWith(cr->text, "GET")) { // HTTP GET
					fill_in_request(cr);
					fds[i].fd = -1; /* The fd won't be checked for reads anymore */
				} else {
					close(fds[i].fd);
					fds[i].fd = -1; /* ignore all other requests */
					remove_and_free_request(cr);
				}
			}
		}
}

/* write data to sockets */
void processRequests() {
	struct Request *cr;
	/*displayRequests();*/
	for (cr = requests.next; cr; cr = cr->next) {
		if (cr->state != ProcessingRequest)
			continue;
		if (cr->lft != NormalFile) {
			logprintf("processRequest", "can't find ``%s''", cr->uri);
			if (send404(cr) == DONE) {
				close(cr->fd);
				cr = remove_and_free_request(cr);
			}
		} else {
			if (sendPage(cr) == DONE) {
				close(cr->fd);
				/*logprintf("processRequest", "connection to %d closed", cr->fd);*/
				cr = remove_and_free_request(cr);
				/*displayRequests();*/
			}
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
	for (;;) {
		timeout = -1;
		if (requests.next)
			timeout = 1;
		if (poll(fds, NUM_FDS, timeout) == -1)
			quit_err("poll");
		
		getNewConnections();
		checkReadingConnections();
		processRequests();
	}

	close(listener);
	return 0;
}

