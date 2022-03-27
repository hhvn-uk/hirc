/*
 * src/mem.c from hirc
 *
 * Copyright (c) 2021-2022 hhvn <dev@hhvn.uk>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 */

#include <stdio.h>
#include <wchar.h>
#include <string.h>
#include <stdlib.h>
#include <ncurses.h>
#include "hirc.h"

/* Free memory and make the original pointer point to NULL.
 * This makes it hard to double free or access free'd memory.
 * Anything that attempts to dereference free'd memory will
 * instead dereference NULL resulting in a segfault, rather
 * than producing potential heisenbugs. */
void
pfree_(void **ptr) {
	size_t i;
	if (!ptr || !*ptr)
		return;

	free(*ptr);
	*ptr = NULL;
}

/* Error-checking memory allocation.
 * IRC isn't that critical in most cases, better 
 * to kill ourself than let oomkiller do it */
void *
emalloc(size_t size) {
	void *mem;

	if ((mem = malloc(size)) == NULL) {
		endwin();
		perror("malloc()");
		exit(EXIT_FAILURE);
	}

	return mem;
}

void *
erealloc(void *ptr, size_t size) {
	void *mem;

	if ((mem = realloc(ptr, size)) == NULL) {
		endwin();
		perror("realloc()");
		exit(EXIT_FAILURE);
	}

	return mem;
}

char *
estrdup(const char *str) {
	char *ret;

	if ((ret = strdup(str)) == NULL) {
		endwin();
		perror("strdup()");
		exit(EXIT_FAILURE);
	}

	return ret;
}

wchar_t *
ewcsdup(const wchar_t *str) {
	wchar_t *ret;
	if ((ret = wcsdup(str)) == NULL) {
		endwin();
		perror("wcsdup()");
		exit(EXIT_FAILURE);
	}
	return ret;
}

/* Helper functions for widechar <-> utf-8 conversion */
wchar_t *
stowc(char *str) {
	wchar_t *ret;
	size_t len;

	if (!str) return NULL;

	len = mbstowcs(NULL, str, 0) + 1;
	ret = emalloc(len * sizeof(wchar_t));
	mbstowcs(ret, str, len);
	return ret;
}

char *
wctos(wchar_t *str) {
	char *ret;
	size_t len;

	if (!str) return NULL;

	len = wcstombs(NULL, str, 0) + 1;
	ret = emalloc(len);
	wcstombs(ret, str, len);
	return ret;
}
