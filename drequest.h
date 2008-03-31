/*
 * drequest.h
 * 		implementation of Request and RequestList
 */

#ifndef DREQUEST_H
#define DREQUEST_H

#include <stdlib.h>
#include "dstdio.h"
#include <stdbool.h>
#include <sys/stat.h>

/* Maximum size, in bytes, or URI */
#define MAXURISIZE 1024
/* Maximum size, in bytes, of request */
#define MAXREQSIZE 4096

/* The state in shich a requst is; what dissemina does with a request is
 * determinded by these states; SEE Request.state */
enum RequestState {
	ReadingRequest,
	ProcessingRequest,
	RequestDone
};

struct request_s;
/* Prototype pattern for RequestHandlers
 * These are functions that are given a request and act on it. (think
 * http_handler or error_handler) */
typedef bool (*RequestHandler)(struct request_s *);

typedef struct request_s {
	/* Holds the full text of the reuquest; that means both headers and data */
	char text[MAXREQSIZE];
	/* Holds only the URI of the request; this looks like /vah/index.xml */
	char uri[MAXURISIZE];
	/* The length of the text; should be equal to strlen(text) */
	int len;
	/* Network socket FD */
	int fd;
	/* The stream to the local file (if one is needed) */
	FILE *fi;
	/* Current state of the request; SEE the RequestState enum */
	int state;
	/* The stat strucuture of the uri */
	struct stat s;
	/* If non-zero, file exists and s is valid */
	int exists;
	/* Pointer to this Request's handler
	 * Initially, this is 0 but gets assigned a value by
	 * assign_handler. */		
	RequestHandler handler;
	/* this is a node in a linked list and these are the pointers to the next
	 * and previous elements */
	struct request_s *next, *prev;
} Request;
typedef Request RequestList;

/* Prepend a request to the specified list */
void prepend_request(RequestList *list, Request *r) {
	r->next = list->next;
	if (r->next)
		r->next->prev = r; /* link with next*/
	r->prev = list;
	list->next = r; /* link with head */
}

/* Creates and prepends a new Request to specified list */
Request* create_and_prepend_request(RequestList *list) {
	Request *nr = malloc(sizeof(Request)); /* create */
	memset(nr, 0, sizeof(Request));
	prepend_request(list, nr);
	return nr;
}

/* Removes a request from list and returns a
 * pointer to it */
Request* pop_request(Request *r) {
	Request *p = r->prev;
	p->next = r->next;
	if (r->next)
		r->next->prev = p;
	r->next = r->prev = 0;
	return r;
}

/* Remove a request from the list and free()s it 
 * Returns a pointer to the PREVIOUS location */
Request* remove_and_free_request(Request *r) {
	Request *p = r->prev;
	free(pop_request(r));
	return p;
}

/* Display all requests in specified list that
 * match specified state */
void display_requests(RequestList *list, int state) {
	Request *cr;
	for (cr = list->next; cr; cr = cr->next)
		if (cr->state == state) 
			logprintf(InfoMsg, "%d", cr->fd);
	logprintf(InfoMsg, "---");
}

#endif

