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
 * dhandler.h
 *   base handlers and their associated match functions
 * 
 * REQUIRES stdbool.h dirent.h drequest.h dstdio.h
 */

/* Number of bytes sent in one go */

extern const char dissemina_version_string[];

extern const char errorpagetext[];
extern const char simplepagetext[];

enum DoneNotDone {
	NotDone,
	Done
};

/* a linked list to hold the matchers */
typedef bool (*MatcherFunc)(Request*);
typedef struct matcher_t {
	MatcherFunc matches;
	struct matcher_t *next;
} Matcher;
typedef Matcher MatcherList;

extern MatcherList matchers;

/* Returns non-zero if file exists or zero otherwise */
int fileexists(char *fp) {
	struct stat s;
	return (stat(fp, &s) == 0);
}

/* Return non-zero is file is normal or zero otherwise */
int isnormfile(char *fp) {
	struct stat s;
	if (stat(fp, &s) != 0)
		return 0;
	return S_ISREG(s.st_mode);
}

/* Return non-zero is file is a directory or zero otherwise */
int isdir(char *fp) {
	struct stat s;
	if (stat(fp, &s) != 0)
		return 0;
	return S_ISDIR(s.st_mode);
}

/* Prepend a matcher to the specified list */
void create_and_prepend_matcher(MatcherList *list, MatcherFunc f) {
	Matcher *r = malloc(sizeof(Matcher));
	memset(r, 0, sizeof(Matcher));
	r->matches = f;
	r->next = list->next;
	list->next = r;
}

/* Send 404 Not Found */
int error_handler(Request *r) {
	logprintf(InfoMsg, "sending 404 Not Found");

	char *msghead = malloc(sizeof(char) * 1024);
	sprintf(msghead, "HTTP/1.1 404 Not Found\r\n"
					 "Connection: close\r\n"
					 "Content-Type: text/xml\r\n"
					 "Server: Dissemina/%s\r\n"
					 "\r\n", dissemina_version_string);
	char *msgbody = malloc(sizeof(char) * 16284);
	sprintf(msgbody, errorpagetext, "404 Not Found", "404 Not Found");

	create_and_prepend_string_envelope(r->fd, msghead, msgbody);

	return Done;
}

/* Send a 400 Bad Request */
int bad_request_handler(Request *r) {
	logprintf(WarnMsg, "sending 400 Bad Request");

	char *msghead = malloc(sizeof(char) * 1024);
	sprintf(msghead, "HTTP/1.1 400 Bad Request\r\n"
					 "Connection: close\r\n"
					 "Content-Type: text/xml\r\n"
					 "Server: Dissemina/%s\r\n"
					 "\r\n", dissemina_version_string);
	char *msgbody = malloc(sizeof(char) * 16284);
	sprintf(msgbody, errorpagetext, "400 Bad Request", "400 Bad Request");

	create_and_prepend_string_envelope(r->fd, msghead, msgbody);

	return Done;
}

/* Match requests r that have r->valid false. */
bool match_bad_requests(Request *r) {
	if (!r->valid) 
		r->handle = bad_request_handler;
	return !r->valid;
}

/* send a list of the files in specified directory */
int directory_listing_handler(Request *r) {
	DIR *dp;
	struct dirent *ep;

	char *msghead = malloc(sizeof(char) * 1024);
	sprintf(msghead , "HTTP/1.1 200 OK\r\n"
					  "Connection: close\r\n"
					  "Content-Type: text/xml\r\n"
					  "Server: Dissemina/%s\r\n"
					  "\r\n", dissemina_version_string);

	dp = opendir(r->uri);
	char contbuf[8192];
	char *msgbody = malloc(sizeof(char) * 16182);
	char buf2[1024];
	if (dp != NULL) {
		sprintf(contbuf, "<table>\n");
		while ((ep = readdir(dp))) {
			sprintf(buf2, "<tr><td><a href=\"%s%s\">%s</a></td></tr>\n",
					r->uri + 1, ep->d_name, ep->d_name);
			strcat(contbuf, buf2);;
		}
		strcat(contbuf, "</table>\n");

		sprintf(msgbody, simplepagetext, r->uri, contbuf);

		create_and_prepend_string_envelope(r->fd, msghead, msgbody);
		closedir(dp);
	} else {
		sprintf(msgbody, simplepagetext, r->uri, "Can't list dir\n");
		create_and_prepend_string_envelope(r->fd, msghead, msgbody);
	}

	return Done;
}

int assign_handler(Request*); 
/* match is succesful if URI is a directory */
bool match_directory_listings(Request *r) {
	if (!fileexists(r->uri) || !isdir(r->uri))
		return false;

	if (r->uri[strlen(r->uri) - 1] != '/')
		strcat(r->uri, "/"); /* make sure there's a trailing slash after the dir name */
	r->handle = directory_listing_handler;

	/* maybe there's an index.xml file to send?  */
	Request aux = *r;
	strcat(aux.uri, "/index.xml"); 
	logprintf(DebugMsg, "match_directory_listing_handler: trying handler of index.xml");
	if (assign_handler(&aux) == 0) 
		*r = aux; /* replace the current request with the new one */

	return true;
}

#define FileHandleNum 5
const char *FileHandle[FileHandleNum][4] = {
	{"", "rb", "sending binary file", "application/octet-stream"},
	{".txt", "r", "sending text file", "text/plain"},
	{".c", "r", "sending text file", "text/plain"}, // should be text/x-c++src but FF refuses to open them
	{".html", "r", "sending html page", "text/html"},
	{".xml", "r", "sending xml document", "application/xml"}
};

/* Send file r->uri to s */
int simple_http_handler(Request *r) {
	int fh = 0; /* guess Content-Type from file extension */
	int i;
	for (i = 1; i < FileHandleNum; ++i)
		if (ends_with(r->uri, FileHandle[i][0])) {
			fh = i;
			break;
		}

	char *msghead = malloc(sizeof(char) * 1024);
	sprintf(msghead, "HTTP/1.1 200 OK\r\n"
					 "Connection: close\r\n"
					 "Content-Type: %s\r\n"
					 "Server: Dissemina/%s\r\n"
					 "\r\n", FileHandle[fh][3], dissemina_version_string);

	create_and_prepend_file_envelope(r->fd, msghead, r->uri);	

	return Done;
}

/* match is succesful if uri is a plain file */
bool match_simple_http(Request *r) {
	if (!fileexists(r->uri) || !isnormfile(r->uri))
		return false; /* not interested */

	r->handle = simple_http_handler;
	return true;
}

/* set up the basic matchers */
void init_matchers() {
	create_and_prepend_matcher(&matchers, match_directory_listings);
	create_and_prepend_matcher(&matchers, match_simple_http);
	create_and_prepend_matcher(&matchers, match_bad_requests);
}

/* NOTE this function might make other modifications to the Request
 * (such as changing the URI) 
 * RETURNS -1 on error; 0 on success */
int assign_handler(Request *r) {
	Matcher *m;
	for (m = matchers.next; m; m = m->next)
		if (m->matches(r))
			return 0;
	
	r->handle = error_handler; /* don't know what to do with this request so send an error */
	return -1;
}

