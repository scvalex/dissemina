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
 * dstdio.h
 *		standard input and output used by dissemina programmes
 */

enum LogMessageCategories {
	InfoMsg = 1, /* Messages that relay non-critical messages */
	WarnMsg = 2, /* Warnings */
	ErrMsg  = 4, /* Errors; i.e. bad things */
	DebugMsg = 8, /* Guess */
	MustPrintMsg = 16 /* Messages that must be printed at all costs */
};

void logprintf(int cat, char *fmt, ...);

void quit_err(const char *s);

