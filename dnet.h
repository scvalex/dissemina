/*
 * dissemina -- a fast webserver
 * Copyright (C) 2008  Alexandru Scvortov
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
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

