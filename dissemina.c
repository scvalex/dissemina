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
 * dissemina.c 
 * 		very fast webserver
 *
 * listens for GETs on port 6462
 */

/* Set DEBUG to 1 to kill all output */
#define DEBUG 0
#define _XOPEN_SOURCE 1 /* Needed for POLLRDNORM... */

#include <ctype.h>
#include <dirent.h>
#include <poll.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>

#include "dstdio.h"
#include "dnet.h"
#include "dstring.h"
#include "drequest.h"
#include "dhandlers.h"

/* Maximum number of open network connections */
#define NUM_FDS 1024

/* On which port shall I listen? */
const int LOCAL_PORT = 6462;

int PrintableMsgs = ErrMsg; /* SEE dstdio.h */

const char dissemina_version_string[] = VERSION;

int listener; /* fd for listener socket */
struct pollfd fds[NUM_FDS];
Request *fdToRequest[NUM_FDS]; /* maps fds[idx] to Requests in readingRequests */

/* This is the global list of requests.
 * It's actually a linked list of requests with the first node as a sentinel
 * (i.e the first node is a Request object, but it's not used) */
RequestList processingRequests;		/* see drequest.h */

/* The list of requests that are currentrly being read into. */
RequestList readingRequests;		/* see drequest.h */

/* The list of mathers used to assing handlers to requests */
MatcherList matchers;		/* see dhandlers.h */

/* The list of envelopes that are currently beeing sent */
EnvelopeList envelopes;		/* see dstdio.h */

/* check listener for new connections and write them into fds and requests  */
void get_new_connections() 
{
	if (fds[0].revents & POLLRDNORM) {
		int newfd = accept_connection(listener);

		int i;
		for (i = 1; (i < NUM_FDS) && (fds[i].fd != -1); ++i)
			;
		if (i == NUM_FDS) {
			logprintf(WarnMsg, "too many open connections; dropping a new one");
			return;
		}

		fds[i].fd = newfd; /* now listening on newfd for data to read; this is used by poll() */
		fds[i].events = POLLRDNORM;
		/* Create and prepend a new Request */
		Request *nr = create_and_prepend_request(&readingRequests);
		nr->fd = newfd;
		fdToRequest[i] = nr;
		logprintf(InfoMsg, "connection from somewhere on socket %d", newfd);
	}
}

/* reads data from sockets marked by get_new_connections() */
void check_connections_for_data() 
{
	int i;
	for (i = 1; i < NUM_FDS; ++i)
		if (fds[i].revents & POLLRDNORM) {
			Request *cr = fdToRequest[i]; /* The current request */
			int nbytes;
			if ((nbytes = drecv(fds[i].fd, cr->text + cr->len, MAXREQSIZE - cr->len, 0)) <= 0) {
				if (nbytes == 0)
					logprintf(InfoMsg, "socket %d closed", fds[i].fd);
				else
					perror("drecv");
				close(fds[i].fd);
				fds[i].fd = -1;
				remove_and_free_request(cr);
			} else {
				cr->len += nbytes;
				if (!ends_with(cr->text, "\r\n\r\n")) /* a HTTP request ends with \r\n\r\n */
					continue;
				cr = pop_request(cr);
				prepend_request(&processingRequests, cr);
				if (starts_with(cr->text, "GET")) // is it HTTP GET?
					parsereq(cr);
				else 
					; /* send an error for all other requests */

				assign_handler(cr);
				fds[i].fd = -1; /* The fd won't be checked for reads anymore */
			}
		}
}

/* write data to sockets */
void process_requests() 
{
	Request *cr;
	for (cr = processingRequests.next; cr; cr = cr->next) {
		if (cr->handle) {
			if (cr->handle(cr) == Done) {
				cr = remove_and_free_request(cr);
			}
		} else {
			logprintf(ErrMsg, "null handler encountered");
			cr = remove_and_free_request(cr);
		}
	}
}

int main(int argc, char *argv[]) 
{
	listener = setup_listener(LOCAL_PORT);		/* see dnetio.h */
	init_matchers();							/* see dhandlers.h */

	int i;
	for (i = 0; i < NUM_FDS; ++i)
		fds[i].fd = -1;	/* fds that are -1 are ignored by poll() */

	fds[0].fd = listener;
	fds[0].events = POLLRDNORM;
	int timeout;
	for (;;) {
		timeout = -1; 
		if (processingRequests.next || envelopes.next)
			timeout = 0; /* if there's a reqeust ready, timeout immediately */
		if (poll(fds, NUM_FDS, timeout) == -1)
			quit_err("poll");
		
		get_new_connections();
		check_connections_for_data();
		process_requests();
		process_envelopes();
	}

	close(listener);
	return 0;
}


