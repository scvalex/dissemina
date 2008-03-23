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

/*
 * If you were expecting to find a recvall() here, you're in for a
 * dissapointment.
 *
 * I've thought about it, and there's no way to write an all purpose
 * receiving funtion. Why? Because there's no generalised way to know when
 * a request if done. Just look at dissemina.c. A request is considered
 * done when ``\r\n\r\n'' is encountered and, then, the socket is NOT
 * closed. Dcheck.c, on the other hand, waits for the other end (i.e. the
 * server) to close the connection.
 */

#endif

