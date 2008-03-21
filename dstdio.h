/*
 * dstdio.h
 *		standard input and output used by dissemina programmes
 */

#ifndef DSTDIO_H
#define DSTDIO_H

#include <time.h>
#include <stdio.h>
#include <stdarg.h>

/* This variable controls what's printed and what's not. 
 * It MUST be declared in every binary to avoid linker errors. */
extern int PrintableMsgs;

enum MessageCategories {
	InfoMsg = 1, /* Messages that relay non-critical messages */
	WarnMsg = 2, /* Warnings */
	ErrMsg  = 4, /* Errors; i.e. bad things */
	MustPrintMsg = 8 /* Messages that must be printed at all costs */
};

/* Return the current ctime as a string */
char* getCurrentTime() {
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

#endif

