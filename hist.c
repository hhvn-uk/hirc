/* See LICENSE for copyright details */

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
	param_free(history->params);
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
		char **params, enum Activity activity,
		time_t timestamp, enum HistOpt options) {
	struct History *new;

	new = emalloc(sizeof(struct History));
	new->prev = new->next = NULL;
	new->timestamp = timestamp ? timestamp : time(NULL);
	new->activity = activity;
	new->raw = estrdup(msg);
	new->params = params;
	new->options = options;
	new->origin = histinfo;

	if (from) {
		new->from = nick_dup(from, histinfo->server);
	} else if (**params == ':') {
		new->from = nick_create(*params, ' ', histinfo->server);
	} else {
		new->from = NULL;
	}

	return new;
}

struct History *
hist_add(struct HistInfo *histinfo, struct Nick *from,
		char *msg, char **params, enum Activity activity,
		time_t timestamp, enum HistOpt options) {
	struct History *new, *p;
	int i;

	if (options & HIST_MAIN) {
		if (histinfo != main_buf)
			hist_add(main_buf, from, msg, params, activity, timestamp, HIST_SHOW);
		else
			ui_error("HIST_MAIN specified, but history is &main_buf", NULL);
	}

	if (options & HIST_SELF && histinfo->server)
		from = histinfo->server->self;

	new = hist_create(histinfo, from, msg, params, activity, timestamp, options);

	if (histinfo && options & HIST_SHOW && activity > histinfo->activity)
		histinfo->activity = activity;
	if (histinfo && options & HIST_SHOW && !chan_selected(histinfo->channel) && !serv_selected(histinfo->server))
		histinfo->unread++;

	if (!histinfo->history) {
		histinfo->history = new;
		return new;
	}

	for (i=0, p = histinfo->history; p && p->next; p = p->next, i++);
	if (i == (MAX_HISTORY-1)) {
		free(p->next);
		p->next = NULL;
	}

	new->next = histinfo->history;
	new->next->prev = new;
	histinfo->history = new;

	/* TODO: this triggers way too often, need to have some sort of delay */
	if (selected.history == histinfo)
		windows[Win_main].refresh = 1;

	if (options & HIST_LOG) {
		if (histinfo->server)
			hist_log(new->raw, new->from, new->timestamp, histinfo->server);
		else
			ui_error("HIST_LOG specified, but server is NULL", NULL);
	}

	return new;
}

struct History *
hist_format(struct HistInfo *histinfo, enum Activity activity, enum HistOpt options, char *format, ...) {
	char msg[1024], **params;
	va_list ap;

	va_start(ap, format);
	vsnprintf(msg, sizeof(msg), format, ap);
	va_end(ap);

	params = param_create(msg);

	return hist_add(histinfo, NULL, msg, params, Activity_status, 0, options);
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

	if (dprintf(server->logfd, "!%lld :%s %s\n", (long long)timestamp, nick_strprefix(from), msg) <  0) {
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
	if (dprintf(server->logfd, "!%lld :%s %s\n", (long long)timestamp, nick_strprefix(from), msg) <  0) {
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
