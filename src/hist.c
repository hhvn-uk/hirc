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
#include <regex.h>
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
	nick_free(history->from);
	pfree(&history->raw);
	pfree(&history->format);
	pfree(&history->rformat);
	pfree(&history);
}

void
hist_free_list(struct HistInfo *histinfo) {
	struct History *p, *prev;

	if (!histinfo->history)
		return;

	prev = histinfo->history;
	p = prev->next;
	while (prev) {
		hist_free(prev);
		prev = p;
		if (p)
			p = p->next;
	}
	histinfo->history = NULL;
}

struct History *
hist_create(struct HistInfo *histinfo, struct Nick *from, char *msg,
		enum Activity activity, time_t timestamp, enum HistOpt options) {
	struct History *new;
	struct Nick *np;
	char *nick;

	assert_warn(msg, NULL);

	new = emalloc(sizeof(struct History));
	new->prev = new->next = NULL;
	new->timestamp = timestamp ? timestamp : time(NULL);
	new->activity = activity;
	new->raw = estrdup(msg);
	new->_params = new->params = param_create(msg);
	new->rformat = new->format = NULL;
	new->options = options;
	new->origin = histinfo;

	if (from) {
		new->from = nick_dup(from);
	} else if (**new->_params == ':') {
		np = NULL;
		if (histinfo->channel && histinfo->channel->nicks) {
			prefix_tokenize(*new->_params, &nick, NULL, NULL);
			np = nick_get(&histinfo->channel->nicks, nick);
			free(nick);
		}

		if (np)
			new->from = nick_dup(np);
		else
			new->from = nick_create(*new->_params, ' ', histinfo->server);
	} else {
		new->from = NULL;
	}

	/* Update histinfo->server->self */
	if (new->from && new->from->self && histinfo->server) {
		if (new->from->ident && strcmp_n(new->from->ident, histinfo->server->self->ident) != 0) {
			free(histinfo->server->self->ident);
			histinfo->server->self->ident = strdup(new->from->ident);
		}
		if (new->from->host && strcmp_n(new->from->host, histinfo->server->self->host) != 0) {
			free(histinfo->server->self->host);
			histinfo->server->self->host = strdup(new->from->host);
		}
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
	struct Ignore *ign;
	struct tm ptm, ctm;
	int i;

	assert_warn(histinfo && msg, NULL);

	if (options & HIST_MAIN) {
		if (options & HIST_TMP && histinfo == main_buf) {
			hist_add(main_buf, msg, activity, timestamp, options & ~(HIST_MAIN|HIST_TMP|HIST_LOG));
			new = NULL;
			goto ui;
		} else if (histinfo != main_buf) {
			hist_add(main_buf, msg, activity, timestamp, options & ~(HIST_MAIN|HIST_TMP|HIST_LOG));
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

	if (!(options & HIST_NIGN)) {
		for (ign = ignores; ign; ign = ign->next) {
			if ((!ign->server ||
					(histinfo->server && strcmp_n(ign->server, histinfo->server->name) == 0)) &&
					(!ign->format || strcmp_n(format_get(new), ign->format) == 0) &&
					regexec(&ign->regex, msg, 0, NULL, 0) == 0) {
				if (!ign->noact) {
					options |= HIST_IGN;
					new->options = options;
				}
				activity = Activity_none;
			}
		}
	}

	if (strcmp(msg, "SELF_NEW_DAY") != 0 &&
			histinfo && histinfo->history &&
			histinfo->history->timestamp < timestamp &&
			!(histinfo->history->options & HIST_RLOG) &&
			!(histinfo->history->options & HIST_GREP) &&
			!(options & HIST_GREP)) {
		localtime_r(&histinfo->history->timestamp, &ptm);
		localtime_r(&timestamp, &ctm);
		if (ptm.tm_mday != ctm.tm_mday || ptm.tm_mon != ctm.tm_mon || ptm.tm_year != ctm.tm_year) {
			ctm.tm_sec = ctm.tm_min = ctm.tm_hour = 0;
			hist_format(histinfo, Activity_none, histinfo->server ? HIST_DFL : HIST_SHOW,
					"SELF_NEW_DAY %lld :day changed to", (long long)mktime(&ctm));
			histinfo->history->timestamp = mktime(&ctm); /* set timestamp of SELF_NEW_DAY:
									really hist_format should take a timestamp */
		}
	}

	if (!histinfo->history) {
		histinfo->history = new;
		goto ui;
	}

	for (i=0, p = histinfo->history; p && p->next; p = p->next, i++);
	if (i == (HIST_MAX-1)) {
		pfree(&p->next);
		p->next = NULL;
	}

	new->next = histinfo->history;
	new->next->prev = new;
	histinfo->history = new;

ui:
	if (options & HIST_SHOW &&
			activity >= Activity_hilight &&
			config_getl("misc.bell"))
		beep();

	if (histinfo && options & HIST_SHOW &&
			activity > histinfo->activity &&
			histinfo != selected.history) {
		histinfo->activity = activity;
		windows[Win_buflist].refresh = 1;
	}

	if (histinfo && options & HIST_SHOW && histinfo != selected.history) {
		if (options & HIST_IGN)
			histinfo->ignored++;
		else
			histinfo->unread++;
	}

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

	assert_warn(histinfo,);

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

			pfree(&p);
		}
	}
}

struct History *
hist_format(struct HistInfo *histinfo, enum Activity activity, enum HistOpt options, char *format, ...) {
	char msg[1024];
	va_list ap;

	va_start(ap, format);
	vsnprintf(msg, sizeof(msg), format, ap);
	va_end(ap);

	if (histinfo)
		return hist_add(histinfo, msg, Activity_status, 0, options);
	else
		return hist_create(histinfo, NULL, msg, Activity_status, 0, options);
}

int
hist_log(struct History *hist) {
	char filename[2048];
	FILE *f;
	char *logdir;
	int ret;
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
			"v2\t%lld\t%d\t%d\t%d\t%c\t%s\t%s\t%s\t%s\n",
			(long long)hist->timestamp,
			hist->activity,
			hist->options, /* write all options - only options ANDing with HIST_LOGACCEPT are read later */
			hist->from ? hist->from->self : 0, /* If from does not exist, it's probably not from us */
			hist->from ? hist->from->priv : ' ',
			nick, ident, host, raw);

	if (ret < 0) {
		ui_error("Could not write to '%s': %s", filename, strerror(errno));
		fclose(f);
		return -7;
	}

	fclose(f);
	return 0;
}

struct History *
hist_loadlog(struct HistInfo *hist, char *server, char *channel) {
	struct History *head = NULL, *p, *prev;
	struct stat st;
	char filename[2048];
	char *logdir;
	FILE *f;
	char *lines[HIST_MAX];
	char buf[2048];
	int i, j;
	char *version;
	char *tok[8];
	char *msg;
	time_t timestamp;
	enum Activity activity;
	enum HistOpt options;
	char *prefix;
	size_t len;
	struct Nick *from;

	assert_warn(server && hist, NULL);

	if ((logdir = config_gets("log.dir")) == NULL)
		return NULL;

	logdir = homepath(logdir);

	if (channel)
		snprintf(filename, sizeof(filename), "%s/%s,%s.log", logdir, server, channel);
	else
		snprintf(filename, sizeof(filename), "%s/%s.log", logdir, server);

	if (stat(filename, &st) == -1)
		return NULL;

	if (!(f = fopen(filename, "rb")))
		return NULL;

	memset(lines, 0, sizeof(lines));

	while (fgets(buf, sizeof(buf), f)) {
		pfree(&lines[HIST_MAX - 1]);
		memmove(lines + 1, lines, HIST_MAX - 1);
		buf[strlen(buf) - 1] = '\0'; /* strip newline */
		lines[0] = estrdup(buf);
	}

	for (i = 0, prev = NULL; i < HIST_MAX && lines[i]; i++) {
		if (*lines[i] == 'v')
			version = strtok_r(lines[i], "\t", &msg) + 1; /* in future versioning could allow for back-compat */
		else
			version = NULL;
		tok[0] = strtok_r(*lines[i] == 'v' ? NULL : lines[i], "\t", &msg);
		for (j = 1; j < (sizeof(tok) / sizeof(tok[0])); j++)
			tok[j] = strtok_r(NULL, "\t", &msg); /* strtok_r will store remaining text after the tokens in msg.
							      * This is used instead of a tok[8] as messages can contain tabs. */

		if (!tok[0] || !tok[1] || !tok[2] ||
				!tok[3] || !tok[4] || !tok[5] ||
				!tok[6] || !tok[7] || !msg) {
			pfree(&lines[i]);
			continue;
		}

		timestamp = (time_t)strtoll(tok[0], NULL, 10);
		activity = (int)strtol(tok[1], NULL, 10);
		options = HIST_RLOG|(strtol(tok[2], NULL, 10) & HIST_LOGACCEPT);

		len = 1;
		if (*tok[5] != ' ')
			len += strlen(tok[5]);
		if (*tok[6] != ' ')
			len += strlen(tok[6]) + 1;
		if (*tok[7] != ' ')
			len += strlen(tok[7]) + 1;
		prefix = smprintf(len, "%s%s%s%s%s",
				tok[5] ? tok[5] : "",
				tok[6] ? "!" : "", tok[6] ? tok[6] : "",
				tok[7] ? "@" : "", tok[7] ? tok[7] : "");
		from = nick_create(prefix, *tok[4], hist->server);
		if (from)
			from->self = *tok[3] == '1';

		p = hist_create(hist, from, msg, activity, timestamp, options);

		if (!head)
			head = p;

		if (prev) {
			prev->next = p;
			p->prev = prev;
		}
		prev = p;

		nick_free(from);
		pfree(&prefix);
		pfree(&lines[i]);
	}

	fclose(f);

	if (head) {
		p = hist_format(NULL, Activity_none, HIST_SHOW|HIST_RLOG, "SELF_LOG_RESTORE %lld :log restored up to", (long long)st.st_mtime);
		p->origin = hist;
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
