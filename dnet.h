/* Dissemina
 *
 * Copyright (c) 2008 Alexandru Scvortov
 *
 * This programme is distributed under
 *
 *     The MIT License
 * 
 * See the LICENSE file for details. */

/*
 * dnet.h
 *		functions relating to networking
 */

int setup_listener(int port);
	
int drecv(int s, char *buf, int sz, int flgs);

int accept_connection(int s);

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

void create_and_prepend_string_envelope(int rec, char *header, char *text);
void create_and_prepend_file_envelope(int rec, char *header, char *fp);

void process_envelopes();

