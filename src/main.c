/*
 * src/main.c from hirc
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
#include <wchar.h>
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

void
die(int code, char *format, ...) {
	static int dying = 0;
	va_list ap;

	/* prevent loop if a function in cleanup() calls die() */
	if (!dying) {
		dying = 1;
		cleanup("Client error");
		dying = 0;

		fprintf(stderr, "Fatal: ");
		va_start(ap, format);
		vfprintf(stderr, format, ap);
		va_end(ap);
#ifdef DIE_CORE
		raise(SIGABRT);
#else
		exit(code);
#endif /* DIE_CORE */
	}
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

int
main(int argc, char *argv[]) {
	struct Selected oldselected;
	struct Server *sp;
	int i, j, refreshed, inputrefreshed;
	long pinginact, reconnectinterval, maxreconnectinterval;

	if (argc > 2) {
		fprintf(stderr, "usage: %s [configfile]\n", basename(argv[0]));
		fprintf(stderr, "       %s -d\n", basename(argv[0]));
		return EXIT_FAILURE;
	}

	if (argc == 2 && strcmp(argv[1], "-d") == 0) {
		printf(".Bl -tag\n");
		for (i=0; config[i].name; i++) {
			printf(".It %s\n", config[i].name);
			printf(".Bd -literal -compact\n");
			printf("Default value: %s\n", config_get_pretty(&config[i], 1));
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
	main_buf->activity = Activity_none;
	main_buf->unread = main_buf->ignored = 0;
	main_buf->server = NULL;
	main_buf->channel = NULL;
	main_buf->history = NULL;

	ui_init();

	if (argc == 2)
		if (config_read(argv[1]) == -1)
			die(1, "cannot read config file '%s': %s\n", argv[1], strerror(errno));

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
				serv_read(sp);
			} else if (!sp->pingsent && sp->lastrecv && (time(NULL) - sp->lastrecv) >= pinginact) {
				/* haven't heard from server in pinginact seconds, sending a ping */
				serv_write(sp, Sched_now, "PING :ground control to Major Tom\r\n");
				sp->pingsent = time(NULL);
			} else if (sp->pingsent && (time(NULL) - sp->pingsent) >= pinginact) {
				/* haven't gotten a response in pinginact seconds since
				 * sending ping, this connexion is probably dead now */
				serv_disconnect(sp, 1, NULL);
				hist_format(sp->history, Activity_error, HIST_SHOW,
						"SELF_CONNECTLOST %s %s %s :No ping reply in %d seconds",
						sp->name, sp->host, sp->port, pinginact);
			} else if (sp->status == ConnStatus_notconnected && sp->reconnect &&
					((time(NULL) - sp->lastconnected) >= maxreconnectinterval ||
					(time(NULL) - sp->lastconnected) >= (sp->connectfail * reconnectinterval))) {
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
