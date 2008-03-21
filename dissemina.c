/*
 * dissemina.c 
 * 		very fast webserver
 *
 * listens for GET requests on port 6462 and carries them out
 */

/* Set DEBUG to 1 to kill all output */
#define DEBUG 0
#define _XOPEN_SOURCE 1 /* Needed for POLLRDNORM... */

#include "dstdio.h"
#include "dnetio.h"
#include "dstring.h"
#include <stdlib.h>
#include <ctype.h>
#include <poll.h>
#include <unistd.h>
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
const int LOCAL_PORT = 6462;

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

int listener; /* fd for listener socket */
struct pollfd fds[NUM_FDS];
struct Request *reqs[NUM_FDS]; /* maps fds[idx] to Requests in requests */
struct Request requests;

int PrintableMsgs = ErrMsg; /* SEE dstdio.h */

/* Display an error and quit */
void quit_err(const char *s) {
	perror(s);
	exit(1);
}

/* Creates and prepends a new Request to requests */
struct Request* create_and_prepend_request() {
	struct Request *nr = malloc(sizeof(struct Request)); /* create */
	memset(nr, 0, sizeof(struct Request));
	nr->next = requests.next;
	if (nr->next)
		nr->next->prev = nr; /* link with next*/
	nr->prev = &requests;
	requests.next = nr; /* link with head */
	return nr;
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
			logprintf(InfoMsg, "%d", cr->fd);
	logprintf(InfoMsg, "---");
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
int get_file_type(const char *fn) {
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
void setup_listener() {
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
	logprintf(MustPrintMsg, "listening on port %d", LOCAL_PORT);
}

#define FileHandleNum 5
const char *FileHandle[FileHandleNum][4] = {
	{"", "rb", "sending binary file", "application/octet-stream"},
	{".txt", "r", "sending text file", "text/plain"},
	{".c", "r", "sending text file", "text/plain"}, // should be text/x-c++src but FF refuses to open them
	{".html", "r", "sending html page", "text/html"},
	{".xml", "r", "sending xml document", "application/xml"}
};

/* Send file r->uri to s */
int sendPage(struct Request *r) {
	int i;
	int len;
	if (!r->fi) {
		int fh = 0; /* guess Content-Type from file extension */
		for (i = 1; i < FileHandleNum; ++i)
			if (endsWith(r->uri, FileHandle[i][0])) {
				fh = i;
				break;
			}
	
		r->fi = fopen(r->uri, FileHandle[fh][1]);
		logprintf(InfoMsg, "%s to %d", FileHandle[fh][2], r->fd);

		char header[256];
		sprintf(header , "HTTP/1.1 200 OK\r\n"
						 "Connection: close\r\n"
						 "Content-Type: %s\r\n"
						 "Server: Dissemina/0.0.1\r\n"
						 "\r\n", FileHandle[fh][3]);
		len = strlen(header);
		sendall(r->fd, header, len);
	}

	char buf[SENDBUFSIZE + 2];
	memset(buf, 0, sizeof(buf));
	if (!feof(r->fi) && !ferror(r->fi)) {
		len = fread(buf, 1, 1024, r->fi);
		/* logprintf(InfoMsg, "sending %d bytes to %d", len, r->fd); */
		sendall(r->fd, buf, len);
	}
	
	if (feof(r->fi) || ferror(r->fi) || (len < 1024)) {
		fclose(r->fi);
		return DONE;
	}

	return NOTDONE;
}

/* Send 404 Not Found to s */
int send404(struct Request *r) {
	logprintf(InfoMsg, "sending 404 Not Found");

	char text404[] = "HTTP/1.1 404 Not Found\r\n"
					 "Connection: close\r\n"
					 "Content-Type: text/plain\r\n"
					 "Server: Dissemina/0.0.1\r\n"
					 "\r\n"
					 "Not Found\n";
	int len = strlen(text404);
	sendall(r->fd, text404, len);

	return DONE;
}

/* check listener for new connections and write them into fds and requests  */
void get_new_connections() {
	if (fds[0].revents & POLLRDNORM) {
		struct sockaddr_in remoteaddr;
		socklen_t addrlen = sizeof(remoteaddr);
		int newfd;
		if ((newfd = accept(listener, (struct sockaddr*)&remoteaddr, &addrlen)) == -1)
			quit_err("accept");

		int i;
		for (i = 1; (i < NUM_FDS) && (fds[i].fd != -1); ++i)
			;
		if (i == NUM_FDS) {
			logprintf(WarnMsg, "too many open connections; dropping a new one");
			return;
		}

		fds[i].fd = newfd; /* now listening on newfd for data to read; this is used by poll() */
		fds[i].events = POLLRDNORM;
		/* Create and prepend a new Request */
		struct Request *nr = create_and_prepend_request();
		nr->fd = newfd;
		nr->state = ReadingRequest;
		reqs[i] = nr;
		logprintf(InfoMsg, "connection from %s on socket %d", inet_ntoa(remoteaddr.sin_addr), newfd);
	}
}

/* fills in a Request, mainly. by looking at it's text */
void fill_in_request(struct Request *r) {
	char *a = r->text + 3; /* jump over the GET */
	for (; *a == ' '; ++a);

	if (*a != '/') {
		r->lft = InexistantFile;
		r->state = ProcessingRequest;
		return; /* request is invalid */
	}

	r->uri[0] = '.'; /* request starts with a slash; prepend a dot so that it's a valid path */
	int j;
	for (j = 0; *a && !isspace(*a) && (j < MAXURISIZE - 2); ++j, ++a)
		r->uri[j + 1] = *a;
	r->uri[j + 1] = 0;
	r->state = ProcessingRequest; /* Mark the request for processing */

	r->lft = get_file_type(r->uri); /* is the file valid, a directory, etc... */
	if (r->lft == FileIsDirectory) {
		strcat(r->uri, "/index.xml"); /* if a directory, you will send out the index.xml file */
		r->lft = get_file_type(r->uri); /* check again; maybe the index does not exist */
	}
}

/* reads data from sockets marked by get_new_connections() */
void check_connections_for_data() {
	int i;
	for (i = 1; i < NUM_FDS; ++i)
		if (fds[i].revents & POLLRDNORM) {
			struct Request *cr = reqs[i]; /* The current request */
			int nbytes;
			if ((nbytes = recv(fds[i].fd, cr->text + cr->len, MAXREQSIZE - cr->len, 0)) <= 0) {
				if (nbytes == 0)
					logprintf(InfoMsg, "listen: socket %d closed", fds[i].fd);
				else
					perror("recv");
				close(fds[i].fd);
				fds[i].fd = -1;
				remove_and_free_request(cr);
			} else {
				cr->len += nbytes;
				//logprintf(InfoMsg, "data from %d: ``%s''", fds[i].fd, reqs[i].text);
				if (!endsWith(cr->text, "\r\n\r\n")) /* a HTTP request ends with \r\n\r\n */
					continue;
				if (startsWith(cr->text, "GET")) { // is it HTTP GET?
					fill_in_request(cr); /* extract data from request text and fill in the other fields*/
					fds[i].fd = -1; /* The fd won't be checked for reads anymore */
				} else { /* ignore all other requests */
					close(fds[i].fd);
					fds[i].fd = -1;	
					remove_and_free_request(cr);
				}
			}
		}
}

/* write data to sockets */
void process_requests() {
	struct Request *cr;
	for (cr = requests.next; cr; cr = cr->next) {
		if (cr->state != ProcessingRequest) /* Is the request ready for processing? */
			continue;
		if (cr->lft != NormalFile) {
			if (send404(cr) == DONE) {
				close(cr->fd);
				cr = remove_and_free_request(cr);
			}
		} else {
			if (sendPage(cr) == DONE) {
				close(cr->fd);
				cr = remove_and_free_request(cr);
			}
		}
	}
}

int main(int argc, char *argv[]) {
	setup_listener();

	int i;
	for (i = 0; i < NUM_FDS; ++i)
		fds[i].fd = -1;	/* fds that are -1 are ignored by poll() */

	fds[0].fd = listener;
	fds[0].events = POLLRDNORM;
	int timeout;
	for (;;) {
		timeout = -1; /* a negative timeout means poll forever */
		if (requests.next)
			timeout = 0; /* if there's a reqeust ready, timeout immediately */
		if (poll(fds, NUM_FDS, timeout) == -1)
			quit_err("poll");
		
		get_new_connections();
		check_connections_for_data();
		process_requests();
	}

	close(listener);
	return 0;
}

