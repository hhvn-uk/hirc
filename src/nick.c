/*
 * src/nick.c from hirc
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
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include "hirc.h"

#define MAX(var1, var2)	(var1 > var2 ? var1 : var2)
#define MIN(var1, var2) (var1 < var2 ? var1 : var2)
#define MAXA(array) MAX(array[0], array[1])
#define MINA(array) MIN(array[0], array[1])

short
nick_getcolour(struct Nick *nick) {
	unsigned short sum;
	int i;
	long range[2];
	char *s = nick->nick;

	if (nick->self)
		return config_getl("nickcolour.self");

	config_getr("nickcolour.range", &range[0], &range[1]);

	if (range[0] < 0 || range[0] > 99 ||
			range[1] < 0 || range[1] > 99)
		return -1;

	if (range[0] == range[1])
		return range[0];

	for (sum=i=0; s && *s; s++, i++) {
		/* don't count certain trailing characters. The following:
		 * hhvn
		 * hhvn_
		 * hhvn2
		 * should all produce the same colour. */
		if ((*s == '_' || isdigit(*s)) && *(s + 1) == '\0')
			break;

		sum += *s * (i + 1);
		sum ^= *s;
	}

	return (sum % (MAXA(range) - MINA(range)) + MINA(range) - 1);
}

void
prefix_tokenize(char *prefix, char **nick, char **ident, char **host) {
	enum { ISNICK, ISIDENT, ISHOST } segment = ISNICK;
	char *p, *dup;

	p = dup = estrdup(prefix);

	if (*p == ':')
		p++;

	if (nick)	*nick = p;
	if (ident)	*ident = NULL;
	if (host)	*host = NULL;

	for (; p && *p && segment != ISHOST; p++) {
		if (segment == ISNICK && *p == '!') {
			*p = '\0';
			if (ident)
				*ident = p + 1;
			segment = ISIDENT;
		}
		if (segment == ISIDENT && *p == '@') {
			*p = '\0';
			if (host)
				*host = p + 1;
			segment = ISHOST;
		}
	}

	if (nick && *nick)	*nick = estrdup(*nick);
	if (ident && *ident)	*ident = estrdup(*ident);
	if (host && *host)	*host = estrdup(*host);
	pfree(&dup);
}

void
nick_free(struct Nick *nick) {
	if (nick) {
		pfree(&nick->prefix);
		pfree(&nick->nick);
		pfree(&nick->ident);
		pfree(&nick->host);
		pfree(&nick);
	}
}

void
nick_free_list(struct Nick **head) {
	struct Nick *p, *prev;

	if (!head || !*head)
		return;

	prev = *head;
	p = prev->next;
	while (prev) {
		nick_free(prev);
		prev = p;
		if (p)
			p = p->next;
	}
	*head = NULL;
}

struct Nick *
nick_create(char *prefix, char priv, struct Server *server) {
	struct Nick *nick;

	assert_warn(prefix && priv, NULL);

	nick = emalloc(sizeof(struct Nick));
	nick->prefix = estrdup(prefix);
	nick->next = nick->prev = NULL;
	nick->priv = priv;
	prefix_tokenize(nick->prefix, &nick->nick, &nick->ident, &nick->host);
	nick->self = nick_isself_server(nick, server);

	return nick;
}

int
nick_isself(struct Nick *nick) {
	if (!nick)
		return 0;

	return nick->self;
}

int
nick_isself_server(struct Nick *nick, struct Server *server) {
	if (!nick || !server || !nick->nick)
		return 0;

	if (strcmp_n(server->self->nick, nick->nick) == 0)
		return 1;
	else
		return 0;
}

struct Nick *
nick_add(struct Nick **head, char *prefix, char priv, struct Server *server) {
	struct Nick *nick;

	assert_warn(prefix && priv, NULL);

	nick = nick_create(prefix, priv, server);
	assert_warn(nick, NULL);

	nick->next = *head;
	nick->prev = NULL;
	if (*head)
		(*head)->prev = nick;
	*head = nick;

	return nick;
}

struct Nick *
nick_dup(struct Nick *nick) {
	struct Nick *ret;
	if (!nick)
		return NULL;
	ret = emalloc(sizeof(struct Nick));
	ret->prev = ret->next = NULL;
	ret->priv   = nick->priv;
	ret->prefix = nick->prefix ? strdup(nick->prefix) : NULL;
	ret->nick   = nick->nick ? strdup(nick->nick) : NULL;
	ret->ident  = nick->ident ? strdup(nick->ident) : NULL;
	ret->host   = nick->host ? strdup(nick->host) : NULL;
	ret->self   = nick->self;
	return ret;
}

struct Nick *
nick_get(struct Nick **head, char *nick) {
	struct Nick *p;

	p = *head;
	for (; p; p = p->next) {
		if (strcmp_n(p->nick, nick) == 0)
			return p;
	}

	return NULL;
}

int
nick_remove(struct Nick **head, char *nick) {
	struct Nick *p;

	if (!head || !nick)
		return -1;

	if ((p = nick_get(head, nick)) == NULL)
		return 0;

	if (*head == p)
		*head = p->next;
	if (p->next)
		p->next->prev = p->prev;
	if (p->prev)
		p->prev->next = p->next;
	nick_free(p);
	return 1;
}

static inline void
nick_dcpy(struct Nick *dest, struct Nick *origin) {
	dest->priv   = origin->priv;
	dest->prefix = origin->prefix;
	dest->nick   = origin->nick;
	dest->ident  = origin->ident;
	dest->host   = origin->host;
	dest->self   = origin->self;
}

static inline void
nick_swap(struct Nick *first, struct Nick *second) {
	struct Nick temp;

	nick_dcpy(&temp, first);
	nick_dcpy(first, second);
	nick_dcpy(second, &temp);
}

enum {
	S_0, S_1, S_2, S_3, S_4, S_5, S_6, S_7, S_8, S_9,
	S_a, S_b, S_c, S_d, S_e, S_f, S_g, S_h, S_i,
	S_j, S_k, S_l, S_m, S_n, S_o, S_p, S_q, S_r,
	S_s, S_t, S_u, S_v, S_w, S_x, S_y, S_z,
	S_dash, S_lbrace, S_rbrace, S_pipe, S_grave, S_caret,
	S_null, S_space,
};

void
nick_sort(struct Nick **head, struct Server *server) {
	char *s[2];
	struct Nick *p, *next;
	int swapped;
	int map[CHAR_MAX] = {
		['\0'] = S_null,
		[' '] = S_space, /* default p->priv */
		['-'] = S_dash,
		['{'] = S_lbrace, ['['] = S_lbrace,
		['}'] = S_rbrace, [']'] = S_rbrace,
		['|'] = S_pipe,  ['\\'] = S_pipe,
		['`'] = S_grave,
		['^'] = S_caret,
		['a'] = S_a, S_b, S_c, S_d, S_e, S_f, S_g, S_h, S_i, S_j,
			S_k, S_l, S_m, S_n, S_o, S_p, S_q, S_r, S_s, S_t,
			S_u, S_v, S_w, S_x, S_y, S_z,
		['A'] = S_a, S_b, S_c, S_d, S_e, S_f, S_g, S_h, S_i, S_j,
			S_k, S_l, S_m, S_n, S_o, S_p, S_q, S_r, S_s, S_t,
			S_u, S_v, S_w, S_x, S_y, S_z,
		['0'] = S_0, S_1, S_2, S_3, S_4, S_5, S_6, S_7, S_8, S_9,
	};

	if (!head || !*head)
		return;

	/* TODO: something better than bubblesort */
	do {
		swapped = 0;
		for (p = (*head)->next; p; p = next) {
			next = p->next;
			for (s[0] = p->nick, s[1] = p->prev->nick; s[0] && s[1] && map[*s[0]] == map[*s[1]]; s[0]++, s[1]++);
			if (s[0] && s[1] && map[*s[0]] < map[*s[1]]) {
				nick_swap(p, p->prev);
				swapped = 1;
			}
		}
	} while (swapped);
}
