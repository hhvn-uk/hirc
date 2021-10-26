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

unsigned short
nick_getcolour(char *nick) {
	unsigned short ret, sum;
	int i;

	if (othercolour[0] == othercolour[1])
		return othercolour[0];

	for (sum=i=0; nick && *nick; nick++, i++) {
		/* don't count certain trailing characters. The following:
		 * hhvn
		 * hhvn_
		 * hhvn2
		 * should all produce the same colour. */
		if ((*nick == '_' || isdigit(*nick)) && *(nick + 1) == '\0')
			break;

		sum += *nick * (i + 1);
		sum ^= *nick;
	}

	return (sum % (MAXA(othercolour) - MINA(othercolour)) + MINA(othercolour) - 1);
}

void
prefix_tokenize(char *prefix, char **nick, char **ident, char **host) {
	enum { ISNICK, ISIDENT, ISHOST } segment = ISNICK;

	if (*prefix == ':')
		prefix++;

	if (nick)	*nick = prefix;
	if (ident)	*ident = NULL;
	if (host)	*host = NULL;

	for (; prefix && *prefix && segment != ISHOST; prefix++) {
		if (segment == ISNICK && *prefix == '!') {
			*prefix = '\0';
			if (ident)
				*ident = prefix + 1;
			segment = ISIDENT;
		}
		if (segment == ISIDENT && *prefix == '@') {
			*prefix = '\0';
			if (host)
				*host = prefix + 1;
			segment = ISHOST;
		}
	}
}

void
nick_free(struct Nick *nick) {
	if (nick) {
		free(nick->prefix);
		free(nick);
	}
}

void
nick_free_list(struct Nick **head) {
	struct Nick *p;

	if (!head || !*head)
		return;

	for (p = (*head)->next; p; p = p->next)
		nick_free(p->prev);
	*head = NULL;
}

struct Nick *
nick_create(char *prefix, char priv, struct Server *server) {
	struct Nick *nick;

	if (!prefix || !priv)
		return NULL;

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

	if (strcmp(server->self->nick, nick->nick) == 0)
		return 1;
	else
		return 0;
}

struct Nick *
nick_add(struct Nick **head, char *prefix, char priv, struct Server *server) {
	struct Nick *nick, *p;

	if (!prefix || !priv)
		return NULL;

	if ((nick = nick_create(prefix, priv, server)) == NULL)
		return NULL;

	if (!*head) {
		*head = nick;
		return nick;
	}

	p = *head;
	for (; p && p->next; p = p->next);
	p->next = nick;
	nick->prev = p;

	return nick;
}

struct Nick *
nick_dup(struct Nick *nick, struct Server *server) {
	return nick_create(nick->prefix, nick->priv, server);
}

struct Nick *
nick_get(struct Nick **head, char *nick) {
	struct Nick *p;

	p = *head;
	for (; p; p = p->next) {
		if (strcmp(p->nick, nick) == 0)
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

	if (p->prev == NULL) {
		*head = p->next;
		nick_free(p);
		return 1;
	}

	p->prev->next = p->next;
	if (p->next != NULL)
		p->next->prev = p->prev;
	nick_free(p);

	return 1;
}

char *
nick_strprefix(struct Nick *nick) {
	static char ret[1024];

	if (!nick)
		return NULL;

	if (nick->nick && nick->ident && nick->host)
		snprintf(ret, sizeof(ret), "%s!%s@%s", nick->nick, nick->ident, nick->host);
	else if (nick->nick && nick->ident)
		snprintf(ret, sizeof(ret), "%s!%s", nick->nick, nick->ident);
	else if (nick->nick)
		snprintf(ret, sizeof(ret), "%s", nick->nick);
	else
		snprintf(ret, sizeof(ret), "");

	return ret;
}

static void
nick_swap(struct Nick **head, struct Nick *first, struct Nick *second) {
	struct Nick *next[2];
	struct Nick *prev[2];

	next[0] = first->next  != second ? first->next  : first;
	next[1] = second->next != first  ? second->next : second;
	prev[0] = first->prev  != second ? first->prev  : first;
	prev[1] = second->prev != first  ? second->prev : second;

	if (*head == first)
		*head = second;
	else if (*head == second)
		*head = first;

	first->next = next[1];
	first->prev = prev[1];
	if (first->next)
		first->next->prev = first;
	if (first->prev)
		first->prev->next = first;

	second->next = next[0];
	second->prev = prev[0];
	if (second->next)
		second->next->prev = second;
	if (second->prev)
		second->prev->next = second;
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
	char *supportedprivs, *s[2];
	struct Nick *p, *next;
	int i, swapped;
	int map[CHAR_MAX] = {
		['\0'] = S_null,
		[' '] = S_space, /* default p->priv */
		['-'] = S_dash,
		['{'] = S_lbrace, ['['] = S_lbrace,
		['}'] = S_rbrace, ['}'] = S_rbrace,
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

	/*
	supportedprivs = strchr(support_get(server, "PREFIX"), ')');
	if (supportedprivs == NULL || supportedprivs[0] == '\0')
		supportedprivs = "";
	else
		supportedprivs++;

	for (i = strlen(supportedprivs); *supportedprivs; supportedprivs++, i--)
		map[*supportedprivs] = S_ - i;
	*/

	/* TODO: something better than bubblesort */
	do {
		swapped = 0;
		for (p = (*head)->next; p; p = next) {
			next = p->next;
			/* TODO: sort using privs here, without causing infinite loop */
			for (s[0] = p->nick, s[1] = p->prev->nick; s[0] && s[1] && map[*s[0]] == map[*s[1]]; s[0]++, s[1]++);
			if (s[0] && s[1] && map[*s[0]] < map[*s[1]]) {
				nick_swap(head, p, p->prev);
				swapped = 1;
			}
		}
	} while (swapped);
}
