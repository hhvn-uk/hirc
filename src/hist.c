/*
 * src/hist.c from hirc
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
#include <fcntl.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdarg.h>
#include <stdlib.h>
#include <ncurses.h>
#include <sys/stat.h>
#include "hirc.h"

void
hist_free(struct History *history) {
	param_free(history->_params);
	if (history->from) {
		free(history->from->prefix);
		free(history->from);
	}
	free(history->raw);
	free(history->format);
	free(history);
}

void
hist_free_list(struct HistInfo *histinfo) {
	struct History *p;

	if (!histinfo->history)
		return;

	for (p = histinfo->history->next; p; p = p->next)
		hist_free(p->prev);
	histinfo->history = NULL;
}

struct History *
hist_create(struct HistInfo *histinfo, struct Nick *from, char *msg,
		enum Activity activity, time_t timestamp, enum HistOpt options) {
	struct History *new;
	struct Nick *np;
	char *nick;

	new = emalloc(sizeof(struct History));
	new->prev = new->next = NULL;
	new->timestamp = timestamp ? timestamp : time(NULL);
	new->activity = activity;
	new->raw = estrdup(msg);
	new->_params = new->params = param_create(msg);
	new->format = NULL;
	new->options = options;
	new->origin = histinfo;

	if (from) {
		new->from = nick_dup(from, histinfo->server);
	} else if (**new->_params == ':') {
		np = NULL;
		if (histinfo->channel) {
			prefix_tokenize(*new->_params, &nick, NULL, NULL);
			np = nick_get(&histinfo->channel->nicks, nick);
		}

		if (np)
			new->from = nick_dup(np, histinfo->server);
		else
			new->from = nick_create(*new->_params, ' ', histinfo->server);
	} else {
		new->from = NULL;
	}

	if (**new->_params == ':')
		new->params++;

	return new;
}

struct History *
hist_addp(struct HistInfo *histinfo, struct History *p, enum Activity activity, enum HistOpt options) {
	return hist_add(histinfo, p->raw, activity, p->timestamp, options);
}

struct History *
hist_add(struct HistInfo *histinfo,
		char *msg, enum Activity activity,
		time_t timestamp, enum HistOpt options) {
	struct Nick *from = NULL;
	struct History *new, *p;
	int i;

	if (options & HIST_MAIN) {
		if (options & HIST_TMP && histinfo == main_buf) {
			hist_add(main_buf, msg, activity, timestamp, HIST_SHOW);
			new = NULL;
			goto ui;
		} else if (histinfo != main_buf) {
			hist_add(main_buf, msg, activity, timestamp, HIST_SHOW);
		} else {
			ui_error("HIST_MAIN specified, but history is &main_buf", NULL);
		}
	}

	if (options & HIST_SELF && histinfo->server) {
		if (histinfo->channel && histinfo->channel->nicks)
			from = nick_get(&histinfo->channel->nicks, histinfo->server->self->nick);
		if (!from)
			from = histinfo->server->self;
	}

	new = hist_create(histinfo, from, msg, activity, timestamp, options);

	if (histinfo && options & HIST_SHOW && activity > histinfo->activity && histinfo != selected.history) {
		histinfo->activity = activity;
		windows[Win_buflist].refresh = 1;
	}
	if (histinfo && options & HIST_SHOW && histinfo != selected.history)
		histinfo->unread++;

	if (!histinfo->history) {
		histinfo->history = new;
		goto ui;
	}

	for (i=0, p = histinfo->history; p && p->next; p = p->next, i++);
	if (i == (HIST_MAX-1)) {
		free(p->next);
		p->next = NULL;
	}

	new->next = histinfo->history;
	new->next->prev = new;
	histinfo->history = new;

ui:
	if (options & HIST_LOG) {
		if (histinfo->server)
			hist_log(new);
		else
			ui_error("HIST_LOG specified, but server is NULL", NULL);
	}

	/* TODO: this triggers way too often, need to have some sort of delay */
	if (selected.history == histinfo) {
		if (options & HIST_SELF)
			windows[Win_main].scroll = -1;
		else if (windows[Win_main].scroll >= 0)
			windows[Win_main].scroll += 1;
		windows[Win_main].refresh = 1;
	}

	return new;
}

void
hist_purgeopt(struct HistInfo *histinfo, enum HistOpt options) {
	struct History *p, *next;

	if (!histinfo)
		return;

	p = histinfo->history;

	for (; p; p = next) {
		next = p->next;
		if (p->options & options) {
			if (p->prev)
				p->prev->next = p->next;
			else
				histinfo->history = p->next;

			if (p->next)
				p->next->prev = p->prev;
			else if (!p->prev)
				histinfo->history = NULL;

			free(p);
		}
	}
}

struct History *
hist_format(struct HistInfo *histinfo, enum Activity activity, enum HistOpt options, char *format, ...) {
	char msg[1024], **params;
	va_list ap;

	va_start(ap, format);
	vsnprintf(msg, sizeof(msg), format, ap);
	va_end(ap);

	params = param_create(msg);

	return hist_add(histinfo, msg, Activity_status, 0, options);
}

int
hist_log(struct History *hist) {
	char filename[2048];
	FILE *f;
	char *logdir;
	int ret, serrno;
	struct stat st;
	char *nick, *ident, *host, *raw;

	if (!config_getl("log.toggle"))
		return -2;

	if ((logdir = config_gets("log.dir")) == NULL)
		return -3;

	logdir = homepath(logdir);

	if (!hist || !hist->origin || !hist->origin->server)
		return -4;

	if (stat(logdir, &st) == -1) {
		if (mkdir(logdir, 0700) == -1) {
			ui_error("Could not create dir '%s': %s", logdir, strerror(errno));
			return -5;
		}
	}

	/* use ',' as it's illegal in dns hostnames (I think) and channel names */
	if (hist->origin->channel)
		snprintf(filename, sizeof(filename), "%s/%s,%s.log", logdir, hist->origin->server->name, hist->origin->channel->name);
	else
		snprintf(filename, sizeof(filename), "%s/%s.log", logdir, hist->origin->server->name);

	if (!(f = fopen(filename, "a"))) {
		ui_error("Could not open '%s': %s", filename, strerror(errno));
		return -6;
	}

	if (hist->from) {
		nick  = hist->from->nick  ? hist->from->nick  : " ";
		ident = hist->from->ident ? hist->from->ident : " ";
		host  = hist->from->host  ? hist->from->host  : " ";
	} else {
		nick = ident = host = " ";
	}

	if (*hist->raw == ':' && strchr(hist->raw, ' '))
		raw = strchr(hist->raw, ' ') + 1;
	else
		raw = hist->raw;

	ret = fprintf(f,
			"%lld\t%d\t%d\t%d\t%c\t%s\t%s\t%s\t%s\n",
			(long long)hist->timestamp,
			hist->activity,
			(hist->options & HIST_SHOW) ? 1 : 0,
			hist->from ? hist->from->self   : 0, /* If from does not exist, it's probably not from us */
			hist->from ? hist->from->priv   : ' ',
			nick, ident, host, raw);

	if (ret < 0) {
		ui_error("Could not write to '%s': %s", filename, strerror(errno));
		fclose(f);
		return -7;
	}

	fclose(f);
}

struct History *
hist_loadlog(struct HistInfo *hist, char *server, char *channel) {
	struct History *head = NULL, *p, *prev;
	char filename[2048];
	char *logdir;
	FILE *f;
	char *lines[HIST_MAX];
	char buf[2048];
	int i, j;
	char *tok[9];
	char *save;
	time_t timestamp;
	enum Activity activity;
	char *prefix;
	size_t len;
	struct Nick *from;
	char *format;

	if (!server || !hist)
		return NULL;

	if ((logdir = config_gets("log.dir")) == NULL)
		return NULL;

	logdir = homepath(logdir);

	if (channel)
		snprintf(filename, sizeof(filename), "%s/%s,%s.log", logdir, server, channel);
	else
		snprintf(filename, sizeof(filename), "%s/%s.log", logdir, server);

	if (!(f = fopen(filename, "rb")))
		return NULL;

	memset(lines, 0, sizeof(lines));

	while (fgets(buf, sizeof(buf), f)) {
		free(lines[HIST_MAX - 1]);
		memmove(lines + 1, lines, HIST_MAX - 1);
		buf[strlen(buf) - 1] = '\0'; /* strip newline */
		lines[0] = estrdup(buf);
	}

	for (i = 0, prev = NULL; i < HIST_MAX && lines[i]; i++) {
		tok[0] = strtok_r(lines[i], "\t", &save);
		for (j = 1; j < 9; j++)
			tok[j] = strtok_r(NULL, "\t", &save);

		if (!tok[0] || !tok[1] || !tok[2] ||
				!tok[3] || !tok[4] || !tok[5] ||
				!tok[6] || !tok[7] || !tok[8])
			continue;

		timestamp = (time_t)strtoll(tok[0], NULL, 10);
		activity = (int)strtol(tok[1], NULL, 10);

		len = 1;
		if (*tok[5] != ' ')
			len += strlen(tok[5]);
		if (*tok[6] != ' ')
			len += strlen(tok[6]) + 1;
		if (*tok[7] != ' ')
			len += strlen(tok[7]) + 1;
		prefix = emalloc(len);
		snprintf(prefix, len, "%s%s%s%s%s",
				tok[5] ? tok[5] : "",
				tok[6] ? "!" : "", tok[6] ? tok[6] : "",
				tok[7] ? "@" : "", tok[7] ? tok[7] : "");
		from = nick_create(prefix, *tok[4], hist->server);
		if (from)
			from->self = *tok[3] == '1';

		p = hist_create(hist, from, tok[8], activity, timestamp, HIST_RLOG|(*tok[2] == '1' ? HIST_SHOW : 0));

		if (!head)
			head = p;

		if (prev) {
			prev->next = p;
			p->prev = prev;
		}
		prev = p;

		nick_free(from);
		free(lines[i]);
	}

	fclose(f);

	if (head) {
		len = snprintf(format, 0, "SELF_LOG_RESTORE %lld :log restored up to", (long long)head->timestamp) + 1;
		format = emalloc(len);
		snprintf(format, len, "SELF_LOG_RESTORE %lld :log restored up to", (long long)head->timestamp);
		p = hist_create(hist, NULL, format, Activity_status, time(NULL), HIST_SHOW|HIST_RLOG);
		free(format);
		p->next = head;
		head->prev = p;
		head = p;
	}
	return head;
}

int
hist_len(struct History **history) {
	struct History *p;
	int i;

	for (i=0, p = *history; p; p = p->next)
		i++;
	return i;
}
