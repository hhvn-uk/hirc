#include <stdlib.h>
#include <string.h>
#include "hirc.h"

void
chan_free(struct Channel *channel) {
	if (channel) {
		free(channel->name);
		free(channel->mode);
		nick_free_list(&channel->nicks);
		free(channel->nicks);
		hist_free_list(channel->history);
		free(channel);
	}
}

void
chan_free_list(struct Channel **head) {
	struct Channel *p;

	if (!head || !*head)
		return;

	for (p = (*head)->next; p; p = p->next)
		chan_free(p->prev);
	*head = NULL;
}

struct Channel *
chan_create(struct Server *server, char *name) {
	struct Channel *channel;

	channel = emalloc(sizeof(struct Channel));
	channel->name = name ? estrdup(name) : NULL;
	channel->next = channel->prev = NULL;
	channel->nicks = NULL;
	channel->old = 0;
	channel->server = server;
	channel->history = emalloc(sizeof(struct HistInfo));
	channel->history->activity = Activity_ignore;
	channel->history->unread = 0;
	channel->history->server = server;
	channel->history->channel = channel;
	channel->history->history = NULL;

	return channel;
}

int
chan_selected(struct Channel *channel) {
	if (selected_channel == channel)
		return 1;
	else
		return 0;
}

struct Channel *
chan_add(struct Server *server, struct Channel **head, char *name) {
	struct Channel *channel, *p;

	if (!name)
		return NULL;

	if ((channel = chan_create(server, name)) == NULL)
		return NULL;

	if (!*head) {
		*head = channel;
		return channel;
	}

	p = *head;
	for (; p && p->next; p = p->next);
	p->next = channel;
	channel->prev = p;

	return channel;
}

struct Channel *
chan_get(struct Channel **head, char *name, int old) {
	struct Channel *p;

	/* if old is negative, match regardless of p->old
	 * else return only when p->old and old match */

	if (!head || !*head || !name)
		return NULL;

	for (p = *head; p; p = p->next) {
		if (strcmp(p->name, name) == 0 && (old < 0 || p->old == old))
			return p;
	}

	return NULL;
}

int
chan_isold(struct Channel *channel) {
	if (channel)
		return channel->old;
	else
		return 0;
}

void
chan_setold(struct Channel *channel, int old) {
	channel->old = old;
}

int
chan_remove(struct Channel **head, char *name) {
	struct Channel *p;

	if (!head || !name)
		return -1;

	if ((p = chan_get(head, name, -1)) == NULL)
		return 0;

	if (p->prev == NULL) {
		*head = p->next;
		chan_free(p);
		return 1;
	}
	
	p->prev->next = p->next;
	if (p->next != NULL)
		p->next->prev = p->prev;
	chan_free(p);
	return 1;
}
