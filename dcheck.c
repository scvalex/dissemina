/*
 * dcheck.c -- run tests against  a webserver
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>

#define TARGET_PORT 6462

int main(int argc, char *argv[]) {
	if (argc  != 2) {
		fprintf(stderr, "usage: dcheck hostname\n");
		exit(1);
	}

	struct hostent *he;
	if ((he = gethostbyname(argv[1])) == NULL) {
		herror("gethostbyname");
		exit(1);
	}

	int sockfd;
	if ((sockfd = socket(PF_INET, SOCK_STREAM, 0)) == -1) {
		perror("socket");
		exit(1);
	}

	struct sockaddr_in ta;
	memset(&ta, 0, sizeof(ta));
	ta.sin_family = AF_INET;
	ta.sin_port = htons(TARGET_PORT);
	ta.sin_addr = *((struct in_addr *)he->h_addr);

	if (connect(sockfd, (struct sockaddr *)&ta, sizeof(ta)) == -1) {
		perror("connect");
		exit(1);
	}

	char buf[101];
	int numbytes;
	if ((numbytes = recv(sockfd, buf, 100, 0)) == -1) {
		perror("recv");
		exit(1);
	}

	buf[numbytes] = 0;

	printf("Received: %s\n", buf);

	close(sockfd);

	return 0;
}

