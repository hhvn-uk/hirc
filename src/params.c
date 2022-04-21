/*
 * src/params.c from hirc
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

#include <string.h>
#include "hirc.h"

void
param_free(char **params) {
	char **p;

	for (p = params; p && *p; p++)
		pfree(&*p);
	pfree(&params);
}

int
param_len(char **params) {
	int i;

	for (i=0; params && *params; i++, params++);
	return i;
}

char **
param_create(char *msg) {
	char **ret, **rp;
	char *params[PARAM_MAX];
	char tmp[2048];
	char *p, *cur;
	int final = 0, i;

	memset(params, 0, sizeof(params));
	strlcpy(tmp, msg, sizeof(tmp));

	for (p=cur=tmp, i=0; p && *p && i < PARAM_MAX; p++) {
		if (!final && *p == ':' && *(p-1) == ' ') {
			final = 1;
			*(p-1) = '\0';
			params[i++] = cur;
			cur = p + 1;
		}
		if (!final && *p == ' ' && *(p+1) != ':') {
			*p = '\0';
			params[i++] = cur;
			cur = p + 1;
		}
	}
	*p = '\0';
	params[i] = cur;

	ret = emalloc(sizeof(params));
	for (rp=ret, i=0; params[i]; i++, rp++)
		*rp = estrdup(params[i]);
	*rp = NULL;

	return ret;
}
