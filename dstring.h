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

/* Returns 1 if s starts with w */
int starts_with(char *s, char *w) 
{
	char *a = s,
		 *b = w;

	while (*a && *b && (*a == *b))
		++a,
		++b;

	return (!*a || !*b);
}

/* Returns true if s ends with w */
int ends_with(const char *s, const char *w) 
{
	int i = strlen(s) - 1,
		j = strlen(w) - 1;

	while ((i >= 0) && (j >= 0) && (s[i] == w[j]))
		--i,
		--j;

	return (j < 0);
}

