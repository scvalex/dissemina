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
	RequestHandler handle;
	struct matcher_t *next;
} Matcher;
typedef Matcher MatcherList;

extern MatcherList matchers;

/* Returns non-zero if file exists or zero otherwise */
int fileexists(char *fp) 
{
	struct stat s;
	return (stat(fp, &s) == 0);
}

/* Return non-zero is file is normal or zero otherwise */
int isnormfile(char *fp) 
{
	struct stat s;
	if (stat(fp, &s) != 0)
		return 0;
	return S_ISREG(s.st_mode);
}

/* Return non-zero is file is a directory or zero otherwise */
int isdir(char *fp) 
{
	struct stat s;
	if (stat(fp, &s) != 0)
		return 0;
	return S_ISDIR(s.st_mode);
}

/* Prepend a matcher to the specified list */
void create_and_prepend_matcher(MatcherList *list, MatcherFunc f, RequestHandler rh) 
{
	Matcher *r = malloc(sizeof(Matcher));
	memset(r, 0, sizeof(Matcher));
	r->matches = f;
	r->handle = rh;
	r->next = list->next;
	list->next = r;
}

/* Sends out a generic error message. */
static void general_error_handler(Request *r, char *errcode) 
{
	logprintf(InfoMsg, "sending %s", errcode);

	char *msghead = malloc(sizeof(char) * 1024);
	sprintf(msghead, "HTTP/1.1 %s\r\n"
					 "Connection: close\r\n"
					 "Content-Type: text/xml\r\n"
					 "Server: Dissemina/%s\r\n"
					 "\r\n", errcode, dissemina_version_string);
	char *msgbody = malloc(sizeof(char) * 16284);
	sprintf(msgbody, errorpagetext, errcode, errcode);

	create_and_prepend_string_envelope(r->fd, msghead, msgbody);
}

/* Send 404 Not Found */
void error_handler(Request *r) 
{
	general_error_handler(r, "404 Not Found");
}

/* Send a 400 Bad Request */
void bad_request_handler(Request *r) 
{
	general_error_handler(r, "400 Bad Request");
}

/* Match requests r that have r->valid false. */
bool match_bad_requests(Request *r) 
{
	logprintf(DebugMsg, "trying bad request");
	return !r->valid;
}

/* send a list of the files in specified directory */
void directory_listing_handler(Request *r) 
{
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
}

int assign_handler(Request*); 
/* match is succesful if URI is a directory */
bool match_directory_listings(Request *r) 
{
	logprintf(DebugMsg, "trying directory listing");
	if (!fileexists(r->uri) || !isdir(r->uri)) 
		return false;

	if (r->uri[strlen(r->uri) - 1] != '/')
		strcat(r->uri, "/"); /* make sure there's a trailing slash after the dir name */

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
void simple_http_handler(Request *r) 
{
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

	create_and_prepend_file_envelope_uri(r->fd, msghead, r->uri);	
}

/* match is succesful if uri is a plain file */
bool match_simple_http(Request *r) 
{
	logprintf(DebugMsg, "trying simple http");
	if (!fileexists(r->uri))
		return false;

	if (isdir(r->uri)) {
		/* maybe there's an index.xml file to send?  */
		Request aux = *r;
		strcat(aux.uri, "/index.xml"); 
		logprintf(DebugMsg, "%s is dir; trying index.xml", r->uri);
		if (fileexists(aux.uri) && isnormfile(aux.uri)) {
			*r = aux;
			return true;
		}
	} else if (isnormfile(r->uri))
		return true;

	return false;
}

/* call the rest of the URI and send back it's output */
void call_handler(Request *r) 
{
	char *msghead = malloc(sizeof(char) * 1024);
	sprintf(msghead, "HTTP/1.1 200 OK\r\n"
					 "Connection: close\r\n"
					 "Content-Type: text/plain\r\n"
					 "Server: Dissemina/%s\r\n"
					 "\r\n", dissemina_version_string);

	char command[256];
	strcpy(command, r->uri + 7);
	logprintf(DebugMsg, "calling %s", command);
	create_and_prepend_file_envelope_file(r->fd, msghead, popen(command, "r"));	
}

/* matches if URI starts with /call/ */
bool match_call(Request *r)
{
	logprintf(DebugMsg, "trying call handler");
	return starts_with(r->uri, "./call/");
}

/* set up the basic matchers */
void init_matchers() 
{
	create_and_prepend_matcher(&matchers, match_directory_listings, directory_listing_handler);
	create_and_prepend_matcher(&matchers, match_simple_http, simple_http_handler);
	create_and_prepend_matcher(&matchers, match_call, call_handler);
	create_and_prepend_matcher(&matchers, match_bad_requests, bad_request_handler);
}

/* Given a request, takes action to complete it */
void handle(Request *r) 
{
	Matcher *m;
	for (m = matchers.next; m; m = m->next)
		if (m->matches(r)) {
			logprintf(DebugMsg, "found");
			break;
		}
	if (m)
		m->handle(r);
	else
		error_handler(r);
}

