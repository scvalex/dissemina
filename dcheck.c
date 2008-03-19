/*
 * dcheck.c -- run tests against  a webserver
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <time.h>
#include <netdb.h>
#include <stdarg.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>

#define TARGET_PORT 6462
#define BUFSIZE 16384

enum MessageCategories {
	InfoMsg = 1,
	WarnMsg = 2,
	ErrMsg  = 4
};

int PrintableMsgs = InfoMsg + WarnMsg + ErrMsg;  /* Categories of messages to print */

int sockfd; /* FD for the socket to server */

char condata[BUFSIZE + 1]; /* data received from the server (includes headers) */
int condata_size; /* size of data received (with headers) */

char filedata[BUFSIZE + 1]; /* file received from server (the part after the header) */
int filedata_size; /* the size of the file data received from the server */

/* Display an error and quit */
void quit_err(char *s) {
	perror(s);
	exit(1);
}

/* Return the current ctime as a string */
char* getCurrentTime() {
	time_t t = time(NULL);
	struct tm *ltm = localtime(&t);
	static char tmp[64];
	strftime(tmp, sizeof(tmp), "%T", ltm);
	return tmp;
}

/* Output a warning */
void logprintf(int cat, char *fmt, ...) {
	if ((PrintableMsgs & cat) == 0)
		return;
	va_list ap;
	va_start(ap, fmt);
	fprintf(stderr, "%s: ", getCurrentTime());
	vfprintf(stderr, fmt, ap);
	fprintf(stderr, "\n");
}
/* Send all data in buf to s */
void sendall(int s, char *buf, int len) {
	int total = 0,
		bytesleft = len,
		n;

	while (bytesleft > 0) {
		n = send(s, buf + total, bytesleft, MSG_NOSIGNAL);
		if (n < 0) {
			logprintf(ErrMsg, "trouble sending data to %d", s);
			return;
		}
		total += n;
		bytesleft -= n;
	}
}

/* Receive all data from s and put it in buf */
int recvall(int s, char *buf, int size) {
	int nbytes;
	int got = 0;
	while ((got < size) && ((nbytes = recv(s, buf + got, size - got, 0)) > 0))
		got += nbytes;

	if (nbytes < 0)
		return -1;

	logprintf(InfoMsg, "socket %d closed", s);
	close(s);

	return got;
}

/* initialises the connection to the socket and stores the FD in sockfd */
void setup_and_connect_socket() {
	struct hostent *he;
	if ((he = gethostbyname("localhost")) == NULL) { /* get the server's address */
		herror("gethostbyname");
		exit(1);
	}

	if ((sockfd = socket(PF_INET, SOCK_STREAM, 0)) == -1)
		quit_err("socket");

	struct sockaddr_in ta;
	memset(&ta, 0, sizeof(ta));
	ta.sin_family = AF_INET;
	ta.sin_port = htons(TARGET_PORT);
	ta.sin_addr = *((struct in_addr *)he->h_addr);

	if (connect(sockfd, (struct sockaddr *)&ta, sizeof(ta)) == -1)
		quit_err("connect");
}

/* GETs the file fp from server and stores the result in condata and
 * condata_size */
void GET(char *fp) {
	char buf[1024];
	sprintf(buf, "GET %s HTTP/1.1\r\n\r\n", fp);
	logprintf(InfoMsg, "Sending GET...");
	sendall(sockfd, buf, strlen(buf));
	logprintf(InfoMsg, "Done");

	if ((condata_size = recvall(sockfd, condata, BUFSIZE)) == -1)
		quit_err("recv");
	condata[condata_size] = 0;
}

/* Extracts filedata from condata */
void extract_data() {
	char *aux;

	logprintf(InfoMsg, "Extracting data...");
	if ((aux = strstr(condata, "\r\n\r\n")) == 0) {
		logprintf(ErrMsg, "no data to extract");
		return;
	}

	filedata_size = condata_size - (aux - condata) - 4;
	memcpy(filedata, aux + 4, filedata_size); /* 4 is \r\n\r\n */
	logprintf(InfoMsg, "Done");
}

/* sets up a socket, GET fp, fills in the variables and closes the
 * socket 
 * USE this and not low-level GET */
void doGET(char *fp) {
	setup_and_connect_socket();

	GET(fp);
	logprintf(InfoMsg, "Received %d bytes", condata_size);
	extract_data();
	logprintf(InfoMsg, "Received %d bytes of data", filedata_size);

	close(sockfd);
}

int main(int argc, char *argv[]) {
	doGET("/file100.txt");
	doGET("/file1024.txt");

	return 0;
}

