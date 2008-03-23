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

/* The state in shich a requst is; what dissemina does with a request is
 * determinded by these states; SEE Request.state */
enum RequestState {
	ReadingRequest,
	ProcessingRequest,
	RequestDone
};

struct request_struct {
	/* holds the full text of the reuquest; that means both headers and data */
	char text[MAXREQSIZE];
	/* holds only the URI of the request; this looks like /vah/index.xml */
	char uri[MAXURISIZE];
	/* the length of the text; should be equal to strlen(text) */
	int len;
	/* network socket FD */
	int fd;
	/* the stream to the local file (if one is needed) */
	FILE *fi;
	/* local file type; i,e, is it a directory, normal file or inexistant;
	 * SEE the FileTypes enum */
	int lft;
	/* current state of the request; SEE the RequestState enum */
	int state;
	/* this is a node in a linked list and these are the pointers to the next
	 * and previous elements */
	struct request_struct *next, *prev;
};
typedef struct request_struct Request;
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

