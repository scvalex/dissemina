/*
 * dnetio.h
 * 		wrapper functions around send and recv
 */

#ifndef DNETIO_H
#define DNETIO_H

#include <unistd.h>
#include <sys/socket.h>

/* Send all data in buf to s */
int sendall(int s, char *buf, int len) {
	int total = 0,
		bytesleft = len,
		n;

	while (bytesleft > 0) {
		n = send(s, buf + total, bytesleft, MSG_NOSIGNAL);
		if (n < 0) {
			logprintf(ErrMsg, "trouble sending data to %d", s);
			return -1;
		}
		total += n;
		bytesleft -= n;
	}

	return 0;
}

#endif

