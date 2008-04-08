/* Copyright (C) Alexandru Scvortov <scvalex@gmail.com>
 * This software is distributed under the terms of the MIT License.
 * See the LICENSE file for details. */

/*
 * dstdio.h
 *		standard input and output used by dissemina programmes
 */

/* IO FUNCTIONS */

enum LogMessageCategories {
	InfoMsg = 1, /* Messages that relay non-critical messages */
	WarnMsg = 2, /* Warnings */
	ErrMsg  = 4, /* Errors; i.e. bad things */
	DebugMsg = 8, /* Guess */
	MustPrintMsg = 16 /* Messages that must be printed at all costs */
};

void logprintf(int cat, char *fmt, ...);

void quit_err(const char *s);

/* NET IO FUNCTIONS */

int setup_listener(int port);
	
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

