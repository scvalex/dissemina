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
 * dstdio.c
 *		implementation of dstdio.h
 */

#include <time.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

#include "dstdio.h"

extern int PrintableMsgs;

/* Return the current ctime as a string */
static char* getCurrentTime() {
	time_t t = time(NULL);
	struct tm *ltm = localtime(&t);
	static char tmp[64];
	strftime(tmp, sizeof(tmp), "%T", ltm);
	return tmp;
}

/* Output a warning */
void logprintf(int cat, char *fmt, ...) {
	if (((PrintableMsgs & cat) == 0) && ((cat & MustPrintMsg) == 0))
		return;
	va_list ap;
	va_start(ap, fmt);
	fprintf(stderr, "%s: ", getCurrentTime());
	vfprintf(stderr, fmt, ap);
	fprintf(stderr, "\n");
}

/* Display an error and quit */
void quit_err(const char *s) {
	perror(s);
	exit(1);
}

