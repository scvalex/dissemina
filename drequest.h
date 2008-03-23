/*
 * drequest.h
 * 		implementation of Request and RequestList
 */

#ifndef DREQUEST_H
#define DREQUEST_H

#include <stdlib.h>
#include "dstdio.h"

/* Maximum size, in bytes, or URI */
#define MAXURISIZE 1024
/* Maximum size, in bytes, of request */
#define MAXREQSIZE 4096

extern struct Request *reqs[]; /* maps fds[idx] to Requests in requests */
extern struct Request requests;

enum RequestState {
	ReadingRequest,
	ProcessingRequest,
	RequestDone
};

struct Request {
	char text[MAXREQSIZE];
	char uri[MAXURISIZE];
	int len;
	int fd;	/* network socket FD */
	FILE *fi;
	int lft; /* local file type; i,e, is it a directory, normal file or inexistant*/
	int state;
	struct Request *next, *prev;
};

/* Creates and prepends a new Request to requests */
struct Request* create_and_prepend_request() {
	struct Request *nr = malloc(sizeof(struct Request)); /* create */
	memset(nr, 0, sizeof(struct Request));
	nr->next = requests.next;
	if (nr->next)
		nr->next->prev = nr; /* link with next*/
	nr->prev = &requests;
	requests.next = nr; /* link with head */
	return nr;
}

/* Remove a request from the requests and free() it 
 * Returns a pointer to the PREVIOUS location */
struct Request* remove_and_free_request(struct Request *r) {
	struct Request *p = r->prev;
	p->next = r->next;
	if (r->next)
		r->next->prev = p;
	free(r);
	return p;
}

/* Display queued requests */
void displayRequests() {
	struct Request *cr;
	for (cr = requests.next; cr; cr = cr->next)
		if (cr->state == ProcessingRequest) 
			logprintf(InfoMsg, "%d", cr->fd);
	logprintf(InfoMsg, "---");
}


#endif

