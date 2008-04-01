/*
 * dstdio.h
 *		standard input and output used by dissemina programmes
 *  
 * REQUIRES unistd.h sys/socket.h time.h stdio.h stdarg.h
 */

/* Bitmask for what's printed and what's not */
extern int PrintableMsgs;

enum MessageCategories {
	InfoMsg = 1, /* Messages that relay non-critical messages */
	WarnMsg = 2, /* Warnings */
	ErrMsg  = 4, /* Errors; i.e. bad things */
	MustPrintMsg = 8 /* Messages that must be printed at all costs */
};

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
	if (((PrintableMsgs & cat) == 0) && ((cat & MustPrintMsg) == 0))
		return;
	va_list ap;
	va_start(ap, fmt);
	fprintf(stderr, "%s: ", getCurrentTime());
	vfprintf(stderr, fmt, ap);
	fprintf(stderr, "\n");
}

// NETWORK IO FUNCTIONS START HERE

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

