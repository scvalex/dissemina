/*
 * dcheck.c
 * 		run tests against  a webserver
 */

#include "dstdio.h"
#include "dnetio.h"
#include "dstring.h"
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <netdb.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>

#define TARGET_PORT 6462
#define BUFSIZE 16384

int PrintableMsgs = ErrMsg;  /* SEE dstdio.h */

int sockfd; /* FD for the socket to server */

char condata[BUFSIZE + 1]; /* data received from the server (includes headers) */
int condata_size; /* size of data received (with headers) */

char filedata[BUFSIZE + 1]; /* file received from server (the part after the header) */
int filedata_size; /* the size of the file data received from the server */

int doChecks; /* if 1, check lines are interpreted */

char linebuf[1024]; /* current line read from file */
char *line;

/* Display an error and quit */
void quit_err(char *s) {
	perror(s);
	exit(1);
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

/* Returns 1 if c is either space or tab; 0 otherwise */
int iswhite(char c) {
	return (c == ' ') || (c == '\t');
}

/* Rturns a pointer to first not whitespace character in string */
char* skipwhite(char *c) {
	while (*c && iswhite(*c))
		++c;
	return c;
}

/* Retruns 1 if line starts with ``(whitespace)*#'' or if line is empty */
int iscomment(char *l) {
	if (*l == '\n')
		return 1;
	while (*l && iswhite(*l))
		++l;
	return *l && (*l == '#');
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
 * socket */
void doGET(char *fp) {
	setup_and_connect_socket();

	GET(fp);
	logprintf(InfoMsg, "Received %d bytes", condata_size);
	extract_data();
	logprintf(InfoMsg, "Received %d bytes of data", filedata_size);

	close(sockfd);
}

/* high-level for commands */
void doCommand() {
	if (starts_with(line, "GET")) {
		char *url = line + 4; /* jump GET */
		url[strlen(url) - 1] = 0; /* cut trailing newline */
		doGET(url);
		doChecks = 1;
		return;
	}
	logprintf(WarnMsg, "unknown command: %s", line);
	doChecks = 0;
}

/* check status against that specified in line */
void checkStatus() {
	int estatus; /* expected status */
	sscanf(line + 6, "%d", &estatus);
	int gstatus; /* got status */
	sscanf(strchr(condata, ' '), "%d", &gstatus);
	printf("# check status (%d against %d)\n", estatus, gstatus);
	if (gstatus == estatus)
		printf("OK\n");
	else
		printf("FAILED\n");
}

/* check data against that in file */
void checkData() {
	char *fn = line + 4;
	fn = skipwhite(fn);
	fn[strlen(fn) - 1] = 0;
	logprintf(InfoMsg, "checking data (received data against file %s)", fn);
	
	char edata[BUFSIZE]; /* expected data */
	FILE *f = fopen(fn, "rb");
	int rb;
	if (f) {
		rb = fread(edata, 1, BUFSIZE, f);
		fclose(f);
	}

	printf("# check data against file %s\n", fn);
	if (memcmp(edata, filedata, rb) == 0)
		printf("OK\n");
	else
		printf("FAILED\n");
}

/* high-level for check* functions */
void doCheck() {
	if (!doChecks) {
		logprintf(WarnMsg, "ignoring check: %s", line);
		return;
	}

	line = skipwhite(line);
	if (starts_with(line, "status"))
		checkStatus();
	else if (starts_with(line, "data"))
		checkData();

	logprintf(WarnMsg, "unknown check: %s", line);
}

int main(int argc, char *argv[]) {
	doChecks = 0;
	for (;;) {
		fgets(linebuf, sizeof(linebuf), stdin);
		if (feof(stdin))
			break;
		line = linebuf; /* linebuf is const char*, line is char* */

		if (iscomment(line)) {
			if (line[0] != '\n')
				printf(line);
		} else if (iswhite(line[0]))
			doCheck();
		else
			doCommand();
	}

	return 0;
}

