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
 * drequest.h
 * 		implementation of Request and RequestList
 * 		
 * REQUIRES stdlib.h stdbool.h sys/stat.h dstdio.h
 */

/* Maximum size, in bytes, or URI */
#define MAXURISIZE 1024
/* Maximum size, in bytes, of request */
#define MAXREQSIZE 4096
/* Maximum number of headers */
#define MAXHEADERSNUM 32

struct request_s;
/* Prototype pattern for RequestHandlers
 * These are functions that are given a request and act on it. (think
 * http_handler or error_handler) */
typedef int (*RequestHandler)(struct request_s *);

/* A simple entry in a dictionary. */
typedef struct {
	char *key, *value;
} KeyValue;

typedef struct request_s {
	/* Is true if the request is valid, false otherwise. */
	bool valid;
	/* Holds the HTTP method (HEAD, GET, POST, PUT, DELETE, TRACE,
	 * OPTIONS or CONNECT)*/
	char method[10];
	/* Holds the full text of the reuquest; that means both headers and data */
	char text[MAXREQSIZE];
	/* Holds only the URI of the request; this looks like /vah/index.xml */
	char uri[MAXURISIZE];
	/* The length of the text; should be equal to strlen(text) */
	int len;
	/* Dictionary of headers */
	KeyValue headers[MAXHEADERSNUM];
	/* Network socket FD */
	int fd;
	/* Pointer to this Request's handler
	 * Initially, this is 0 but gets assigned a value by
	 * assign_handler. */		
	RequestHandler handle;
	/* Pointers to the next and previous elements in the linked list */
	struct request_s *next, *prev;
} Request;
typedef Request RequestList;

void skipwhite(char *buf, int *i) {
	while (*i && isspace(buf[*i]))
		++(*i);
}

/* Parse a received request extracting its URI into r->uri and its
 * headers into r->headers. */
void parsereq(Request *r) {
	int i = 0;
	/* Extract the HTTP method */
	while ((i < r->len) && (i < 9) && !isspace(r->text[i])) {
		r->method[i] = r->text[i];
		++i;
	}
	if ((i == 10) || (i == r->len)) 
		return;
	skipwhite(r->text, &i);

	/* Extract the URI */
	int laststop = i;
	r->uri[0] = '.'; /* This is ugly. */
	while ((i < r->len) && (i - laststop < MAXURISIZE - 2) && !isspace(r->text[i])) {
		r->uri[i - laststop + 1] = r->text[i];
		++i;
	}
	logprintf(DebugMsg, "%s %s", r->method, r->uri);
	if ((i == r->len) || (i - laststop == MAXURISIZE - 1))
		return;
	if ((r->uri[1] != '/') || (strstr(r->uri, "..")))
		return;

	/* Skip the protocol string */
	while ((i < r->len) && !isspace(r->text[i]))
		++i;
	if (i == r->len)
		return;

	r->valid = true;
}

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

