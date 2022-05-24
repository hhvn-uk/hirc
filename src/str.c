/*
 * src/str.c from hirc
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
#include <stdlib.h>
#include <limits.h>
#include "hirc.h"

/* Helper functions for widechar <-> utf-8 conversion */
wchar_t *
stowc(char *str) {
	wchar_t *ret;
	size_t len;

	if (!str) return NULL;

	len = mbstowcs(NULL, str, 0) + 1;
	if (!len) return NULL;
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
	if (!len) return NULL;
	ret = emalloc(len);
	wcstombs(ret, str, len);
	return ret;
}

char *
homepath(char *path) {
	static char ret[1024];

	if (*path == '~') {
		snprintf(ret, sizeof(ret), "%s/%s", getenv("HOME"), path + 1);
		return ret;
	}

	return path;
}

int
strcmp_n(const char *s1, const char *s2) {
	if (!s1 && !s2)
		return 0;
	else if (!s1)
		return INT_MIN;
	else if (!s2)
		return INT_MAX;
	else
		return strcmp(s1, s2);
}

char *
struntil(char *str, char until) {
	static char ret[1024];
	int i;

	for (i=0; str && *str && i < 1024 && *str != until; i++, str++)
		ret[i] = *str;

	ret[i] = '\0';
	return ret;
}

int
strisnum(char *str, int allowneg) {
	if (!str)
		return 0;

	if ((allowneg && *str == '-') || *str == '+')
		str += 1;

	for (; *str; str++)
		if (*str > '9' || *str < '0')
			return 0;
	return 1;
}

char *
strntok(char *str, char *sep, int n) {
	static char buf[8192];
	char *ret, *save;

	strlcpy(buf, str, sizeof(buf));

	ret = strtok_r(buf, sep, &save);
	if (--n == 0)
		return ret;
	while ((ret = strtok_r(NULL, sep, &save)) != NULL)
		if (--n == 0)
			return ret;
	return NULL;
}

#define S_YEAR	31557600 /* 60*60*24*365.25 */
#define S_MONTH	2629800  /* 60*60*24*(365.25 / 12) */
#define S_WEEK	604800   /* 60*60*24*7 */
#define S_DAY	86400    /* 60*60*24 */
#define S_HOUR	3600     /* 60*60 */
#define S_MIN	60

char *
strrdate(time_t secs) {
	static char ret[1024];
	size_t rc = 0;
	long shrt = config_getl("rdate.short");
	long avg  = config_getl("rdate.averages");
	long verb = config_getl("rdate.verbose");
	int years = 0, months = 0, weeks = 0,
	    days  = 0, hours  = 0, mins  = 0;

	if (avg) {
		years  = secs   / S_YEAR;
		secs  -= years  * S_YEAR;

		months = secs   / S_MONTH;
		secs  -= months * S_MONTH;
	}

	weeks = secs  / S_WEEK;
	secs -= weeks * S_WEEK;

	days  = secs  / S_DAY;
	secs -= days  * S_DAY;

	hours = secs  / S_HOUR;
	secs -= hours * S_HOUR;

	mins  = secs  / S_MIN;
	secs -= mins  * S_MIN;

	if (years || (verb && avg))
		rc += snprintf(&ret[rc], sizeof(ret) - rc, "%d%s%s, ", years,
				shrt ? "y" : " year", !shrt && years != 1 ? "s" : "");
	if (months || (verb && avg))
		rc += snprintf(&ret[rc], sizeof(ret) - rc, "%d%s%s, ", months,
				shrt ? "mo" : " month", !shrt && months != 1 ? "s" : "");
	if (weeks || verb)
		rc += snprintf(&ret[rc], sizeof(ret) - rc, "%d%s%s, ", weeks,
				shrt ? "w" : " week", !shrt && weeks != 1 ? "s" : "");
	if (days || verb)
		rc += snprintf(&ret[rc], sizeof(ret) - rc, "%d%s%s, ", days,
				shrt ? "d" : " day", !shrt && days != 1 ? "s" : "");
	if (hours || verb)
		rc += snprintf(&ret[rc], sizeof(ret) - rc, "%d%s%s, ", hours,
				shrt ? "h" : " hour", !shrt && hours != 1 ? "s" : "");
	if (mins || verb)
		rc += snprintf(&ret[rc], sizeof(ret) - rc, "%d%s%s, ", mins,
				shrt ? "m" : " min", !shrt && mins != 1 ? "s" : "");
	if (secs || verb)
		rc += snprintf(&ret[rc], sizeof(ret) - rc, "%d%s%s, ", (int)secs,
				shrt ? "s" : " sec", !shrt && secs != 1 ? "s" : "");
	if (rc >= 2)
		ret[rc - 2] = '\0';
	else
		ret[rc] = '\0';

	return ret;
}
