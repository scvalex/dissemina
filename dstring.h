/*
 * dstring.h
 * 		convenience string manipulation functions
 */

#ifndef DSTRING_H
#define DSTRING_H

#include <string.h>

/* Returns 1 if s starts with w */
int starts_with(char *s, char *w) {
	char *a = s,
		 *b = w;

	while (*a && *b && (*a == *b))
		++a,
		++b;

	return (!*a || !*b);
}

/* Returns true if s ends with w */
int ends_with(const char *s, const char *w) {
	int i = strlen(s) - 1,
		j = strlen(w) - 1;

	while ((i >= 0) && (j >= 0) && (s[i] == w[j]))
		--i,
		--j;

	return (j < 0);
}

#endif

