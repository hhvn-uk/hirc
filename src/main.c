/*
 * src/main.c from hirc
 *
 * Copyright (c) 2021 hhvn <dev@hhvn.uk>
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
#include <errno.h>
#include <stdlib.h>
#include <libgen.h>
#include <limits.h>
#include <string.h>
#include <unistd.h>
#include <stdarg.h>
#include <signal.h>
#include <poll.h>
#include "hirc.h"

struct Server *servers = NULL;
struct HistInfo *main_buf;

void *
emalloc(size_t size) {
	void *mem;

	if ((mem = malloc(size)) == NULL) {
		perror("malloc()");
		exit(EXIT_FAILURE);
	}

	return mem;
}

char *
estrdup(const char *str) {
	char *ret;

	if ((ret = strdup(str)) == NULL) {
		perror("strdup()");
		exit(EXIT_FAILURE);
	}

	return ret;
}

void
cleanup(char *quitmsg) {
	struct Server *sp, *prev;

	for (sp = servers, prev = NULL; sp; sp = sp->next) {
		if (prev)
			serv_free(prev);
		serv_disconnect(sp, 0, quitmsg);
		prev = sp;
	}

	serv_free(prev);
	ui_deinit();
}

void
param_free(char **params) {
	char **p;

	for (p = params; p && *p; p++)
		free(*p);
	free(params);
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

int
read_line(int fd, char *buf, size_t buf_len) {
	size_t  i = 0;
	char    c = 0;

	do {
		if (read(fd, &c, sizeof(char)) != sizeof(char))
			return 0;
		if (c != '\r')
			buf[i++] = c;
	} while (c != '\n' && i < buf_len);
	buf[i - 1] = '\0';
	return 1;
}

int
ircprintf(struct Server *server, char *format, ...) {
	char msg[512];
	va_list ap;
	int ret, serrno;

	if (!server || server->status == ConnStatus_notconnected) {
		ui_error("Not connected to server '%s'", server ? server->name : "");
		return -1;
	}

	va_start(ap, format);
	if (vsnprintf(msg, sizeof(msg), format, ap) < 0) {
		va_end(ap);
		return -1;
	}

	ret = write(server->wfd, msg, strlen(msg));

	if (ret == -1 && server->status == ConnStatus_connected) {
		serv_disconnect(server, 1, NULL);
		hist_format(server->history, Activity_error, HIST_SHOW,
				"SELF_CONNECTLOST %s %s %s :%s",
				server->name, server->host, server->port, strerror(errno));
	} else if (ret == -1 && server->status != ConnStatus_connecting) {
		ui_error("Not connected to server '%s'", server->name);
	}

	va_end(ap);
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

char
chrcmp(char c, char *s) {
	for (; s && *s; s++)
		if (c == *s)
			return c;

	return 0;
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
strisnum(char *str) {
	if (!str)
		return 0;

	if (*str == '-' || *str == '+')
		str += 1;

	for (; *str; str++)
		if (*str > '9' || *str < '0')
			return 0;
	return 1;
}

void
sighandler(int signal) {
	return;
}

int
main(int argc, char *argv[]) {
	struct Selected oldselected;
	struct Server *sp;
	FILE *file;
	int i, j, refreshed, inputrefreshed;
	long pinginact, reconnectinterval, maxreconnectinterval;
	char *tmp;

	if (argc > 2) {
		fprintf(stderr, "usage: %s [configfile]", dirname(argv[0]));
		fprintf(stderr, "       %s -d", dirname(argv[0]));
		return EXIT_FAILURE;
	}

	if (argc == 2 && strcmp(argv[1], "-d") == 0) {
		printf(".Bl -tag\n");
		for (i=0; config[i].name; i++) {
			printf(".It %s\n", config[i].name);
			printf(".Bd -literal -compact\n");
			if (config[i].valtype == Val_string)
				printf("Default value: %s\n", config[i].str);
			else if (config[i].valtype == Val_pair || config[i].valtype == Val_colourpair)
				printf("Default value: {%02ld, %02ld}\n", config[i].pair[0], config[i].pair[1]);
			else
				printf("Default value: %02ld\n", config[i].num);
			for (j=0; config[i].description[j]; j++)
				printf("%s\n", config[i].description[j]);
			printf(".Ed\n");
		}
		printf(".El\n");
		printf(".Sh COMMANDS\n");
		printf(".Bl -tag\n");
		for (i=0; commands[i].name && commands[i].func; i++) {
			printf(".It /%s\n", commands[i].name);
			printf(".Bd -literal -compact\n");
			for (j=0; commands[i].description[j]; j++)
				printf("%s\n", commands[i].description[j]);
			printf(".Ed\n");
		}
		printf(".El\n");
		return 0;
	}

	main_buf = emalloc(sizeof(struct HistInfo));
	main_buf->activity = Activity_ignore;
	main_buf->unread = 0;
	main_buf->server = NULL;
	main_buf->channel = NULL;
	main_buf->history = NULL;

	ui_init();

	if (argc == 2)
		config_read(argv[1]);

	for (;;) {
		/* 25 seems fast enough not to cause any visual lag */
		if (serv_poll(&servers, 25) < 0) {
			perror("serv_poll()");
			exit(EXIT_FAILURE);
		}

		pinginact = config_getl("misc.pingtime");
		reconnectinterval = config_getl("reconnect.interval");
		maxreconnectinterval = config_getl("reconnect.maxinterval");
		for (sp = servers; sp; sp = sp->next) {
			if (sp->rpollfd->revents) {
				/* received an event */
				sp->pingsent = 0;
				sp->lastrecv = time(NULL);
				sp->rpollfd->revents = 0;
				handle(sp->rfd, sp);
			} else if (!sp->pingsent && sp->lastrecv && (time(NULL) - sp->lastrecv) >= pinginact) {
				/* haven't heard from server in pinginact seconds, sending a ping */
				ircprintf(sp, "PING :ground control to Major Tom\r\n");
				sp->pingsent = time(NULL);
			} else if (sp->pingsent && (time(NULL) - sp->pingsent) >= pinginact) {
				/* haven't gotten a response in pinginact seconds since
				 * sending ping, this connexion is probably dead now */
				serv_disconnect(sp, 1, NULL);
				hist_format(sp->history, Activity_error, HIST_SHOW,
						"SELF_CONNECTLOST %s %s %s :No ping reply in %d seconds",
						sp->name, sp->host, sp->port, pinginact);
			} else if (sp->status == ConnStatus_notconnected && sp->reconnect &&
					(time(NULL) - sp->lastconnected) >= (sp->connectfail * reconnectinterval)) {
				/* time since last connected is sufficient to initiate reconnect */
				serv_connect(sp);
			}
		}

		if (oldselected.channel != selected.channel || oldselected.server != selected.server) {
			if (windows[Win_nicklist].location)
				windows[Win_nicklist].refresh = 1;
			if (windows[Win_buflist].location)
				windows[Win_buflist].refresh = 1;
		}

		if (oldselected.history != selected.history)
			windows[Win_main].refresh = 1;

		oldselected.channel = selected.channel;
		oldselected.server = selected.server;
		oldselected.history = selected.history;
		oldselected.name = selected.name;

		if (uineedredraw) {
			uineedredraw = 0;
			ui_redraw();
			for (i=0; i < Win_last; i++)
				windows[i].refresh = 0;
			continue;
		}

		refreshed = inputrefreshed = 0;
		for (i=0; i < Win_last; i++) {
			if (windows[i].refresh && windows[i].location) {
				if (windows[i].handler)
					windows[i].handler();
				wnoutrefresh(windows[i].window);
				windows[i].refresh = 0;
				refreshed = 1;
				if (i == Win_input)
					inputrefreshed = 1;
			}
		}
		doupdate();

		/* refresh Win_input after any other window to
		 * force ncurses to place the cursor here. */
		if (refreshed && !inputrefreshed)
			wrefresh(windows[Win_input].window);

		ui_read();
	}

	return 0;
}