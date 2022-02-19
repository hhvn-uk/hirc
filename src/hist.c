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

	new = emalloc(sizeof(struct History));
	new->prev = new->next = NULL;
	new->timestamp = timestamp ? timestamp : time(NULL);
	new->activity = activity;
	new->raw = estrdup(msg);
	new->_params = new->params = param_create(msg);
	new->options = options;
	new->origin = histinfo;

	if (from)
		new->from = nick_dup(from, histinfo->server);
	else if (**new->_params == ':')
		new->from = nick_create(*new->_params, ' ', histinfo->server);
	else
		new->from = NULL;

	if (**new->_params == ':')
		new->params++;

	return new;
}

struct History *
hist_addp(struct HistInfo *histinfo, struct History *p, enum Activity activity, enum HistOpt options) {
	return hist_add(histinfo, p->from, p->raw, activity, p->timestamp, options);
}

struct History *
hist_add(struct HistInfo *histinfo, struct Nick *from,
		char *msg,  enum Activity activity,
		time_t timestamp, enum HistOpt options) {
	struct History *new, *p;
	int i;

	if (options & HIST_MAIN) {
		if (options & HIST_TMP && histinfo == main_buf) {
			hist_add(main_buf, from, msg, activity, timestamp, HIST_SHOW);
			new = NULL;
			goto ui;
		} else if (histinfo != main_buf) {
			hist_add(main_buf, from, msg, activity, timestamp, HIST_SHOW);
		} else {
			ui_error("HIST_MAIN specified, but history is &main_buf", NULL);
		}
	}

	if (options & HIST_SELF && histinfo->server)
		from = histinfo->server->self;

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

	if (options & HIST_LOG) {
		if (histinfo->server)
			hist_log(new->raw, new->from, new->timestamp, histinfo->server);
		else
			ui_error("HIST_LOG specified, but server is NULL", NULL);
	}

ui:
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
			else
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

	return hist_add(histinfo, NULL, msg, Activity_status, 0, options);
}

int
hist_log(char *msg, struct Nick *from, time_t timestamp, struct Server *server) {
	char filename[2048];
	char *logdir;
	int ret, serrno;

	if (!config_getl("log.toggle"))
		return -2;

	if ((logdir = config_gets("log.dir")) == NULL)
		return -3;

	if (*msg == ':' && strchr(msg, ' '))
		msg = strchr(msg, ' ') + 1;

	if (dprintf(server->logfd, "!%lld :%s %s\n", (long long)timestamp, from ? from->prefix : server->name, msg) <  0) {
		/* Can't write, try to open the file */
		snprintf(filename, sizeof(filename), "%s/%s.log", homepath(logdir), server->name);
		ret = open(filename, O_CREAT|O_APPEND|O_WRONLY);
		serrno = errno;
		if (ret == -1 && serrno == ENOENT) {
			/* No such directory: attempt to create logdir */
			if (mkdir(homepath(logdir), 0700) == -1) {
				ui_error("Could not create '%s' directory for logging: %s", logdir, strerror(errno));
				return -1;
			}
		} else if (ret == -1) {
			ui_error("Could not open '%s' for logging: %s", filename, strerror(serrno));
			return -1;
		} else {
			server->logfd = ret;
		}
	} else return 1;

	/* retry */
	if (dprintf(server->logfd, "!%lld :%s %s\n", (long long)timestamp, from ? from->prefix : server->name, msg) <  0) {
		ui_error("Failed to write to log of server '%s': %s", server->name, strerror(errno));
		return -1;
	}

	return 1;
}

int
hist_len(struct History **history) {
	struct History *p;
	int i;

	for (i=0, p = *history; p; p = p->next)
		i++;
	return i;
}
