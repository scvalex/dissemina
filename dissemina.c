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
#include "drequest.h"
#include <stdlib.h>
#include <ctype.h>
#include <poll.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

/* Number of bytes sent in one go */
#define SENDBUFSIZE 1024

/* Maximum number of open network connections */
#define NUM_FDS 1024

/* On which port shall I listen? */
const int LOCAL_PORT = 6462;

#define DONE 1
#define NOTDONE 0

int listener; /* fd for listener socket */
struct pollfd fds[NUM_FDS];
Request *fdToRequest[NUM_FDS]; /* maps fds[idx] to Requests in readingRequests */

/* This is the global list of requests.
 * It's actually a linked list of requests with the first node as a sentinel
 * (i.e the first node is a Request object, but it's not used) */
RequestList processingRequests;

/* The list of requests that are currentrly beeing read into. */
RequestList readingRequests;

int PrintableMsgs = ErrMsg; /* SEE dstdio.h */

/* Display an error and quit */
void quit_err(const char *s) {
	perror(s);
	exit(1);
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
int send_page(Request *r) {
	int i;
	int len;
	if (!r->fi) {
		int fh = 0; /* guess Content-Type from file extension */
		for (i = 1; i < FileHandleNum; ++i)
			if (ends_with(r->uri, FileHandle[i][0])) {
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

/* send a list of the files in specified directory */
int send_directory_listing(Request *r) {
	DIR *dp;
	struct dirent *ep;

	char buf[4096];
	sprintf(buf , "HTTP/1.1 200 OK\r\n"
			   	  "Connection: close\r\n"
				  "Content-Type: text/html\r\n"
				  "Server: Dissemina/0.0.1\r\n"
				  "\r\n");
	sendall(r->fd, buf, strlen(buf));

	dp = opendir(r->uri);
	char buf2[1024];
	char buf3[1024];
	if (dp != NULL) {
		sprintf(buf, "<html>\n"
						"<head>\n"
						"	<title>%s</title>\n"
						"</head>\n"
						"<body>\n"
						"	<table>\n", r->uri);
		while ((ep = readdir(dp))) {
			sprintf(buf3, "%s%s", r->uri + 1, ep->d_name);
			sprintf(buf2, "		<tr><td><a href=\"%s\">%s</a></td></tr>\n", buf3, ep->d_name);
			strcat(buf, buf2);;
		}
		strcat(buf, "	</table>\n"
					"</body>\n"
					"</html>\n");
		sendall(r->fd, buf, strlen(buf));
		closedir(dp);
	} else {
		sprintf(buf, "<html><body>Can't list\n</body></html>");
		sendall(r->fd, buf, strlen(buf));
		return NOTDONE;
	}

	return DONE;
}

/* Send 404 Not Found to s */
int send404(Request *r) {
	logprintf(InfoMsg, "sending 404 Not Found");

	char text404[] = "HTTP/1.1 404 Not Found\r\n"
					 "Connection: close\r\n"
					 "Content-Type: text/plain\r\n"
					 "Server: Dissemina/0.0.1\r\n"
					 "\r\n"
					 "Not Found\n";
	sendall(r->fd, text404, strlen(text404));

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
		Request *nr = create_and_prepend_request(&readingRequests);
		nr->fd = newfd;
		nr->state = ReadingRequest;
		fdToRequest[i] = nr;
		logprintf(InfoMsg, "connection from %s on socket %d", inet_ntoa(remoteaddr.sin_addr), newfd);
	}
}

/* fills in a Request, mainly. by looking at it's text */
void fill_in_request(Request *r) {
	char *a = r->text + 3; /* jump over the GET */
	for (; *a == ' '; ++a);

	if ((*a != '/') || strstr(a, "..")) { /* client might be trying to jump out of server dir */
		r->state = ProcessingRequest;
		return; /* request is invalid */
	}

	r->uri[0] = '.'; /* request starts with a slash; prepend a dot so that it's a valid path */
	int j;
	for (j = 0; *a && !isspace(*a) && (j < MAXURISIZE - 2); ++j, ++a)
		r->uri[j + 1] = *a;
	r->uri[j + 1] = 0;
	r->state = ProcessingRequest; /* Mark the request for processing */

	r->exists = (stat(r->uri, &r->s) == 0); /* is the file valid, a directory, etc... */
	/* if a directory is requested, first dissemina attempts to find an
	 * index.xml and failing that send the directory listing */
	if (S_ISDIR(r->s.st_mode)) {
		Request aux = *r; /* create a new request that correspondes to the index.xml in this directory*/
		strcat(aux.uri, "/index.xml"); /* if a directory, you will send out the index.xml file */
		aux.exists = (stat(aux.uri, &aux.s) == 0);
		if (aux.exists) /* if the file exists */
			*r = aux; /* replace the current request with the new one */
		else if (r->uri[strlen(r->uri) - 1] != '/') /* there's no index.xml, so append a / to the dir */
			strcat(r->uri, "/");                    /* name so that it's clearly a directory */
	}
}

/* reads data from sockets marked by get_new_connections() */
void check_connections_for_data() {
	int i;
	for (i = 1; i < NUM_FDS; ++i)
		if (fds[i].revents & POLLRDNORM) {
			Request *cr = fdToRequest[i]; /* The current request */
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
				if (!ends_with(cr->text, "\r\n\r\n")) /* a HTTP request ends with \r\n\r\n */
					continue;
				if (starts_with(cr->text, "GET")) { // is it HTTP GET?
					cr = pop_request(cr);
					prepend_request(&processingRequests, cr);
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
	Request *cr;
	for (cr = processingRequests.next; cr; cr = cr->next) {
		if (cr->state != ProcessingRequest) /* Is the request ready for processing? */
			continue;
		if (S_ISDIR(cr->s.st_mode)) {
			if (send_directory_listing(cr) != DONE)
				logprintf(ErrMsg, "problem sending directory listing of %s\n", cr->uri);
			close(cr->fd);
			cr = remove_and_free_request(cr);
		} else if (!S_ISREG(cr->s.st_mode)) {
			if (send404(cr) == DONE) {
				close(cr->fd);
				cr = remove_and_free_request(cr);
			}
		} else {
			if (send_page(cr) == DONE) {
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
		if (processingRequests.next)
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

