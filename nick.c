#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
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
