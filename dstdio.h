/*
 * dstdio.h
 *		standard input and output used by dissemina programmes
 *  
 * REQUIRES unistd.h sys/socket.h time.h stdio.h stdarg.h string.h
 */

/* LOG STDIO */

/* Bitmask for what's printed and what's not */
extern int PrintableMsgs;

enum LogMessageCategories {
	InfoMsg = 1, /* Messages that relay non-critical messages */
	WarnMsg = 2, /* Warnings */
	ErrMsg  = 4, /* Errors; i.e. bad things */
	DebugMsg = 8, /* Guess */
	MustPrintMsg = 16 /* Messages that must be printed at all costs */
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

// NETWORK IO

/* Send data in buf to s */
int wrappedsend(int s, char *buf, int len) {
	logprintf(DebugMsg, "Sending %d bytes to socket %d", len, s);

	int numsentbytes = send(s, buf, len, MSG_NOSIGNAL | MSG_DONTWAIT);
	if (numsentbytes < 0) {
		logprintf(ErrMsg, "trouble sending data to %d", s);
		return -1;
	}

	return numsentbytes;
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

#define SENDBUFSIZE 1024

enum DataTypes {
	DataIsString = 1,
	DataIsFile
};

enum States {
	SendingHeader = 1,
	SendingBody
};

/* Container for outgoing messages */
typedef struct envelope_t {
	/* Fd of the receiver */
	int receiver;

	/* Header of the message */
	char *header;

	/* Stores the current state of processing.
	 * Is either SendingHeader or SendingBody.
	 * This determines the behavious of process_envelopes */
	int state;

	/* What am I sending? A string? A file? */
	int datatype;

	/* Pointer to the string (is ignored if datatype is DataIsFile) */
	char *stringdata;
	/* Pointer to the next character that should be sent 
	 * NOTE this is also used when seanding out the header */
	char *nextchar;

	/* Pointer to the FILE stream (is ignored if datatype is
	 * DataIsString)*/
	FILE *filedata;

	/* Pointer to the previous and next envelopes in the list */
	struct envelope_t *prev, *next;
} Envelope;
typedef Envelope EnvelopeList;

extern EnvelopeList envelopes; /* see dissemina.c */

/* Creates a string envelope with the data given and prepends it to
 * the envelopes. */
void create_and_prepend_string_envelope(int rec, char *header, char *text) {
	Envelope *e = malloc(sizeof(Envelope));
	memset(e, 0, sizeof(Envelope));

	e->receiver = rec;
	e->header = header;
	e->datatype = DataIsString;
	e->stringdata = text;
	e->nextchar = header;
	e->state = SendingHeader;

	e->prev = &envelopes;
	e->next = envelopes.next;
	if (e->next)
		e->next->prev = e;
	envelopes.next = e;
}

/* Creates a file envelope with the filepath given and prepends it to
 * the envelopes. */
void create_and_prepend_file_envelope(int rec, char *header, char *fp) {
	Envelope *e = malloc(sizeof(Envelope));
	memset(e, 0, sizeof(Envelope));

	e->receiver = rec;
	e->header = header;
	e->datatype = DataIsFile;
	e->filedata = fopen(fp, "rb");
	e->nextchar = header;
	e->state = SendingHeader;

	e->prev = &envelopes;
	e->next = envelopes.next;
	if (e->next)
		e->next->prev = e;
	envelopes.next = e;
}

/* Removes an envelope from the list and free()s it */
Envelope* remove_envelope(Envelope *e) {
	Envelope *p = e->prev;
	p->next = e->next;
	if (e->next)
		e->next->prev = p;

	close(e->receiver);
	logprintf(InfoMsg, "closed connection on socket %d", e->receiver);
	free(e->header);
	if (e->datatype == DataIsString)
		free(e->stringdata);
	else if (e->datatype == DataIsFile)
		fclose(e->filedata);

	free(e);
	return p;
}

/* Processes an envelopes sending the data for each envelope and
 * removing used-up envelopes */
void process_envelopes() {
	Envelope *e;
	for (e = envelopes.next; e; e = e->next) {
		if (e->state == SendingHeader) {
			char buf[SENDBUFSIZE];
			strncpy(buf, e->nextchar, SENDBUFSIZE-1);
			buf[SENDBUFSIZE-1] = 0;
			int len = strlen(buf);
			int bs = wrappedsend(e->receiver, buf, len);
			if (bs >= 0)
				e->nextchar += bs;
			if (*(e->nextchar) == 0) {
				e->state = SendingBody;
				e->nextchar = e->stringdata; /* this could be 0 */
			}
		} else if (e->state == SendingBody) {
			if (e->datatype == DataIsString) {
				char buf[SENDBUFSIZE];
				strncpy(buf, e->nextchar, SENDBUFSIZE-1);
				buf[SENDBUFSIZE-1] = 0;
				int len = strlen(buf);
				int bs = wrappedsend(e->receiver, buf, len);
				if (bs >= 0)
					e->nextchar += bs;
				if (*(e->nextchar) == 0)
					e = remove_envelope(e);
			} else if (e->datatype == DataIsFile) {
				char buf[SENDBUFSIZE];
				int len = fread(buf, 1, 1024, e->filedata);
				int bs = wrappedsend(e->receiver, buf, len);
				if (bs < len) {
					logprintf(ErrMsg, "sent %d bytes instead of %d", bs, len);
					logprintf(ErrMsg, "the next instruction could result in an error");
					fseek(e->filedata, -(len - bs), SEEK_CUR);
				} else if (bs == -1)
					fseek(e->filedata, -len, SEEK_CUR);
				if (feof(e->filedata) || ferror(e->filedata))
					e = remove_envelope(e);
			}
		}
	}
}

