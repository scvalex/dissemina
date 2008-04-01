/*
 * dhandler.h
 *   base handlers and their associated match functions
 * 
 * also contains assign_handler
 *
 * REQUIRES stdbool.h dirent.h drequest.h
 */

/* Number of bytes sent in one go */
#define SENDBUFSIZE 1024

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

/* Prepend a matcher to the specified list */
void create_and_prepend_matcher(MatcherList *list, MatcherFunc f) {
	Matcher *r = malloc(sizeof(Matcher));
	memset(r, 0, sizeof(Matcher));
	r->matches = f;
	r->next = list->next;
	list->next = r;
}

/* Send 404 Not Found to s */
int error_handler(Request *r) {
	logprintf(InfoMsg, "sending 404 Not Found");

	char text404[] = "HTTP/1.1 404 Not Found\r\n"
					 "Connection: close\r\n"
					 "Content-Type: text/plain\r\n"
					 "Server: Dissemina/0.0.1\r\n"
					 "\r\n"
					 "Not Found\n";
	sendall(r->fd, text404, strlen(text404));

	return Done;
}

/* send a list of the files in specified directory */
int directory_listing_handler(Request *r) {
	DIR *dp;
	struct dirent *ep;

	char buf[4096];
	sprintf(buf , "HTTP/1.1 200 OK\r\n"
			   	  "Connection: close\r\n"
				  "Content-Type: text/html\r\n"
				  "Server: Dissemina/0.0.1\r\n"
				  "\r\n");
	if (sendall(r->fd, buf, strlen(buf)) == -1)
		return Done;

	dp = opendir(r->uri);
	char buf2[1024];
	char buf3[1024];
	if (dp != NULL) {
		sprintf(buf, "<html>\n"
						"<head>\n"
						"	<title>%s</title>\n"
						"</head>\n"
						"<body>\n"
						"	<table>\n", r->uri);
		while ((ep = readdir(dp))) {
			sprintf(buf3, "%s%s", r->uri + 1, ep->d_name);
			sprintf(buf2, "		<tr><td><a href=\"%s\">%s</a></td></tr>\n", buf3, ep->d_name);
			strcat(buf, buf2);;
		}
		strcat(buf, "	</table>\n"
					"</body>\n"
					"</html>\n");
		sendall(r->fd, buf, strlen(buf));
		closedir(dp);
	} else {
		sprintf(buf, "<html><body>Can't list\n</body></html>");
		sendall(r->fd, buf, strlen(buf));
	}

	return Done;
}

int assign_handler(Request*); 
/* match is succesful if URI is a directory */
bool match_directory_listing_handler(Request *r) {
	if (!r->exists || !S_ISDIR(r->s.st_mode))
		return false;

	if (r->uri[strlen(r->uri) - 1] != '/')
		strcat(r->uri, "/"); /* make sure there's a trailing slash after the dir name */
	r->handle = directory_listing_handler;

	/* maybe there's an index.xml file to send?  */
	Request aux = *r;
	strcat(aux.uri, "/index.xml"); 
	aux.exists = (stat(aux.uri, &aux.s) == 0);
	logprintf(InfoMsg, "trying handler of index.xml");
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
	int i;
	int len = 0;
	if (!r->fi) {
		int fh = 0; /* guess Content-Type from file extension */
		for (i = 1; i < FileHandleNum; ++i)
			if (ends_with(r->uri, FileHandle[i][0])) {
				fh = i;
				break;
			}
	
		r->fi = fopen(r->uri, FileHandle[fh][1]);
		logprintf(InfoMsg, "%s to %d", FileHandle[fh][2], r->fd);

		char header[256];
		sprintf(header , "HTTP/1.1 200 OK\r\n"
						 "Connection: close\r\n"
						 "Content-Type: %s\r\n"
						 "Server: Dissemina/0.0.1\r\n"
						 "\r\n", FileHandle[fh][3]);
		len = strlen(header);
		if (sendall(r->fd, header, len) == -1)
			return Done; /* error, so close connection */
	}

	char buf[SENDBUFSIZE + 2];
	memset(buf, 0, sizeof(buf));
	if (!feof(r->fi) && !ferror(r->fi)) {
		len = fread(buf, 1, 1024, r->fi);
		if (sendall(r->fd, buf, len) == -1)
			return Done; /* error, so close connection */
	}
	
	if (feof(r->fi) || ferror(r->fi) || (len < 1024)) {
		fclose(r->fi);
		return Done;
	}

	return NotDone;
}

/* match is succesful if uri is a plain file */
bool match_simple_http_handler(Request *r) {
	if (!r->exists || !S_ISREG(r->s.st_mode))
		return false; /* not interested */

	r->handle = simple_http_handler;
	return true;
}

/* set up the basic matchers */
void init_matchers() {
	create_and_prepend_matcher(&matchers, match_directory_listing_handler);
	create_and_prepend_matcher(&matchers, match_simple_http_handler);
}

/* NOTE this function might make other modifications to the Request
 * (such as changing the URI)
 */
int /* 0 on success; -1 on error */
assign_handler(Request *r) {
	Matcher *m;
	for (m = matchers.next; m; m = m->next)
		if (m->matches(r))
			return 0;
	
	r->handle = error_handler; /* don't know what to do with this request so send an error */
	return -1;
}

