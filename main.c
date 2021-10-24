#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
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
	char tmp[NL_TEXTMAX];
	char *p, *cur;
	int final = 0, i;

	for (i=0; i < PARAM_MAX; i++)
		params[i] = NULL;
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
	for (rp=ret, i=0; params[i] && *params[i]; i++, rp++)
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

	if (server->status == ConnStatus_notconnected) {
		ui_error("Not connected to server '%s'", server->name);
		return -1;
	}

	va_start(ap, format);
	if (vsnprintf(msg, sizeof(msg), format, ap) < 0) {
		va_end(ap);
		return -1;
	}

	ret = write(server->wfd, msg, strlen(msg));

	if (ret == -1 && server->status == ConnStatus_connected) {
		serv_disconnect(server, 1);
		hist_format(server, server->history, Activity_error, HIST_SHOW,
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

void
sighandler(int signal) {
	return;
}

int
main(int argc, char **argv) {
	struct Channel *old_selected_channel;
	struct Server *old_selected_server;
	struct Server *sp;
	FILE *file;
	struct pollfd fds[] = {
		{ .fd = fileno(stdin), .events = POLLIN },
	};

	main_buf = emalloc(sizeof(struct HistInfo));
	main_buf->activity = Activity_ignore;
	main_buf->unread = 0;
	main_buf->server = NULL;
	main_buf->channel = NULL;
	main_buf->history = NULL;

	ui_init();
	selected_server = serv_add(&servers, "hlircnet", "irc.hhvn.uk", "6667", "hhvn", "Fanatic", "gopher://hhvn.uk", 1, 0);
	/* serv_add(&servers, "dataswamp", "127.0.0.1", "6697", "hhvn", "Fanatic", "gopher://hhvn.uk", 1, 0); */
	for (sp = servers; sp; sp = sp->next)
		serv_connect(sp);

	for (;;) {
		if (serv_poll(&servers, 5) < 0) {
			perror("serv_poll()");
			exit(EXIT_FAILURE);
		}

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
				serv_disconnect(sp, 1);
				hist_format(sp, sp->history, Activity_error, HIST_SHOW,
						"SELF_CONNECTLOST %s %s %s :No ping reply in %d seconds",
						sp->name, sp->host, sp->port, pinginact);
			} else if (sp->status == ConnStatus_notconnected && sp->reconnect && 
					(time(NULL) - sp->lastconnected) >= (sp->connectfail * reconnectinterval)) {
				/* time since last connected is sufficient to initiate reconnect */
				serv_connect(sp);
			}
		}

		if (old_selected_channel != selected_channel || old_selected_server != selected_server) {
			ui_draw_nicklist();
			wrefresh(nicklist.window);
		}

		wrefresh(winlist.window);
		wrefresh(mainwindow.window);

		ui_read();

		old_selected_channel = selected_channel;
		old_selected_server = selected_server;
	}

	return 0;
}
