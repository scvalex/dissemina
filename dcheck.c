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
#define BUFSIZE 4096
#define DEBUG 0

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
void sendall(int s, char *buf, int len) {
	int total = 0,
		bytesleft = len,
		n;

	while (bytesleft > 0) {
		n = send(s, buf + total, bytesleft, MSG_NOSIGNAL);
		if (n < 0) {
			logprintf("sendall", "trouble sending data to %d\n", s);
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

	logprintf("recvall", "socket %d closed", s);
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
	printf("Sending GET...\n");
	sendall(sockfd, buf, strlen(buf));
	printf("Done\n");

	if ((condata_size = recvall(sockfd, condata, BUFSIZE)) == -1)
		quit_err("recv");
	condata[condata_size] = 0;
}

/* Extracts filedata from condata */
void extract_data() {
	char *aux;

	printf("Extracting data...\n");
	if ((aux = strstr(condata, "\r\n\r\n")) == 0) {
		logprintf("extact_data", "no data to extract");
		return;
	}

	filedata_size = condata_size - (aux - condata) - 4;
	memcpy(filedata, aux + 4, filedata_size); /* 4 is \r\n\r\n */
	printf("Done\n");
}

int main(int argc, char *argv[]) {
	setup_and_connect_socket();

	GET("/dcheck.c");
	printf("Received %d bytes\n", condata_size);
	extract_data();
	printf("Received %d bytes of data\n", filedata_size);
	printf("``%s''\n", filedata);

	close(sockfd);

	return 0;
}

