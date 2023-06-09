/*
 * src/serv.c from hirc
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

#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <poll.h>
#ifdef TLS
#include <tls.h>
#endif /* TLS */
#include "hirc.h"

/* 1024 is enough to fit two max-length messages in the buffer at once.
 * To stress-test this, it is possible to set it to 2 as the lowest value.
 * (The last byte is reserved for '\0', so a value of 1 will act weird). */
#define INPUT_BUF_MIN 1024

void
serv_free(struct Server *server) {
	struct Support *sp, *sprev;
	struct Schedule *ep, *eprev;

	if (!server)
		return;

	pfree(&server->name);
	pfree(&server->username);
	pfree(&server->realname);
	pfree(&server->password);
	pfree(&server->host);
	pfree(&server->port);
	pfree(&server->rpollfd);
	nick_free(server->self);
	hist_free_list(server->history);
	chan_free_list(&server->channels);
	chan_free_list(&server->queries);
	sprev = server->supports;
	if (sprev)
		sp = sprev->next;
	while (sprev) {
		pfree(&sprev->key);
		pfree(&sprev->value);
		pfree(&sprev);
		sprev = sp;
		if (sp)
			sp = sp->next;
	}
	eprev = server->schedule;
	if (eprev)
		ep = eprev->next;
	while (eprev) {
		pfree(&eprev->msg);
		pfree(&eprev);
		eprev = ep;
		if (ep)
			ep = ep->next;
	}
#ifdef TLS
	if (server->tls)
		if (server->tls_ctx)
			tls_free(server->tls_ctx);
#endif /* TLS */
	pfree(&server);
}

struct Server *
serv_create(char *name, char *host, char *port, char *nick, char *username,
		char *realname, char *password, int tls, int tls_verify) {
	struct Server *server;
	int i;

	assert_warn(name && host && port && nick, NULL);

	server = emalloc(sizeof(struct Server));
	server->prev = server->next = NULL;
	server->wfd = server->rfd = -1;
	server->input.size = INPUT_BUF_MIN;
	server->input.pos = 0;
	server->input.buf = emalloc(server->input.size);
	server->rpollfd = emalloc(sizeof(struct pollfd));
	server->rpollfd->fd = -1;
	server->rpollfd->events = POLLIN;
	server->rpollfd->revents = 0;
	server->status = ConnStatus_notconnected;
	server->name = estrdup(name);
	server->username = username ? estrdup(username) : NULL;
	server->realname = realname ? estrdup(realname) : NULL;
	server->password = password ? estrdup(password) : NULL;
	server->host = estrdup(host);
	server->port = estrdup(port);
	server->supports = NULL;
	server->self = nick_create(nick, ' ', NULL);
	server->self->self = 1;
	server->history = emalloc(sizeof(struct HistInfo));
	server->history->activity = Activity_none;
	server->history->unread = server->history->ignored = 0;
	server->history->server = server;
	server->history->channel = NULL;
	server->history->history = NULL;
	server->channels = NULL;
	server->queries = NULL;
	server->schedule = NULL;
	server->reconnect = 0;
	for (i=0; i < Expect_last; i++)
		server->expect[i] = NULL;
	server->autocmds = NULL;
	server->connectfail = 0;
	server->lastconnected = server->lastrecv = server->pingsent = 0;

#ifdef TLS
	server->tls_verify = tls_verify;
	server->tls = tls;
	server->tls_ctx = NULL;
#else
	if (tls)
		hist_format(server->history, Activity_error, HIST_SHOW,
				"SELF_TLSNOTCOMPILED %s", server->name);
#endif /* TLS */

	return server;
}

void
serv_update(struct Server *sp, char *nick, char *username,
		char *realname, char *password, int tls, int tls_verify) {
	assert_warn(sp,);
	if (nick) {
		pfree(&sp->self->nick);
		sp->self->nick = estrdup(nick);
	}
	if (username) {
		pfree(&sp->username);
		sp->username = estrdup(nick);
	}
	if (realname) {
		pfree(&sp->realname);
		sp->username = estrdup(nick);
	}
	if (password) {
		pfree(&sp->password);
		sp->password = estrdup(password);
	}
#ifdef TLS
	if (tls >= 0 && !sp->tls) {
		sp->tls = tls;
		if (strcmp(sp->port, "6667") == 0) {
			pfree(&sp->port);
			sp->port = estrdup("6697");
		}
	}
	if (tls_verify >= 0)
		sp->tls_verify = tls_verify;
#endif /* TLS */
}

struct Server *
serv_add(struct Server **head, char *name, char *host, char *port, char *nick,
		char *username, char *realname, char *password, int tls, int tls_verify) {
	struct Server *new, *p;

	new = serv_create(name, host, port, nick, username, realname, password, tls, tls_verify);
	assert_warn(new, NULL);

	if (!*head) {
		*head = new;
		return new;
	}

	p = *head;
	for (; p && p->next; p = p->next);
	p->next = new;
	new->prev = p;

	return new;
}

struct Server *
serv_get(struct Server **head, char *name) {
	struct Server *p;

	assert_warn(head && name, NULL);
	if (!*head)
		return NULL;

	for (p = *head; p; p = p->next) {
		if (strcmp(p->name, name) == 0)
			return p;
	}

	return NULL;
}

int
serv_remove(struct Server **head, char *name) {
	struct Server *p;

	assert_warn(head && name, -1);

	if ((p = serv_get(head, name)) == NULL)
		return 0;

	if (*head == p)
		*head = p->next;
	if (p->next)
		p->next->prev = p->prev;
	if (p->prev)
		p->prev->next = p->next;
	serv_free(p);
	return 1;
}

void
serv_connect(struct Server *server) {
	struct tls_config *tls_conf;
	struct Support *s, *prev;
	struct addrinfo hints;
	struct addrinfo *ai = NULL;
	int fd, ret;

	assert_warn(server,);

	if (server->status != ConnStatus_notconnected) {
		ui_error("server '%s' is already connected", server->name);
		return;
	}

	for (s = server->supports, prev = NULL; s; s = s->next) {
		if (prev) {
			pfree(&prev->key);
			pfree(&prev->value);
			pfree(&prev);
		}
		prev = s;
	}
	server->supports = NULL;
	support_set(server, "CHANTYPES", config_gets("def.chantypes"));
	support_set(server, "PREFIX", config_gets("def.prefixes"));

	server->status = ConnStatus_connecting;
	hist_format(server->history, Activity_status, HIST_SHOW|HIST_MAIN,
			"SELF_CONNECTING %s %s", server->host, server->port);

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;

	if ((ret = getaddrinfo(server->host, server->port, &hints, &ai)) != 0 || ai == NULL) {
		hist_format(server->history, Activity_error, HIST_SHOW,
				"SELF_LOOKUPFAIL %s %s %s :%s",
				server->name, server->host, server->port, gai_strerror(ret));
		goto fail;
	}
	if ((fd = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol)) == -1 || connect(fd, ai->ai_addr, ai->ai_addrlen) == -1) {
		hist_format(server->history, Activity_error, HIST_SHOW,
				"SELF_CONNECTFAIL %s %s %s :%s",
				server->name, server->host, server->port, strerror(errno));
		goto fail;
	}

	server->rfd = server->wfd = fd;
	hist_format(server->history, Activity_status, HIST_SHOW|HIST_MAIN,
			"SELF_CONNECTED %s %s %s", server->name, server->host, server->port);

#ifdef TLS
	if (server->tls) {
		if (server->tls_ctx)
			tls_free(server->tls_ctx);
		server->tls_ctx = NULL;

		if ((tls_conf = tls_config_new()) == NULL) {
			ui_tls_config_error(tls_conf, "tls_config_new()");
			goto fail;
		}

		if (!server->tls_verify) {
			tls_config_insecure_noverifycert(tls_conf);
			tls_config_insecure_noverifyname(tls_conf);
		}

		if ((server->tls_ctx = tls_client()) == NULL) {
			ui_perror("tls_client()");
			goto fail;
		}

		if (tls_configure(server->tls_ctx, tls_conf) == -1) {
			ui_tls_error(server->tls_ctx, "tls_configure()");
			goto fail;
		}

		if (tls_connect_socket(server->tls_ctx, fd, server->host) == -1) {
			hist_format(server->history, Activity_error, HIST_SHOW,
					"SELF_CONNECTLOST %s %s %s :%s",
					server->name, server->host, server->port, tls_error(server->tls_ctx));
			goto fail;
		}

		if (tls_handshake(server->tls_ctx) == -1) {
			hist_format(server->history, Activity_error, HIST_SHOW,
					"SELF_CONNECTLOST %s %s %s :%s",
					server->name, server->host, server->port, tls_error(server->tls_ctx));
			goto fail;
		}

		tls_config_free(tls_conf);

		if (tls_peer_cert_provided(server->tls_ctx)) {
			hist_format(server->history, Activity_status, HIST_SHOW,
					"SELF_TLS_VERSION %s %s %d %s",
					server->name, tls_conn_version(server->tls_ctx),
					tls_conn_cipher_strength(server->tls_ctx),
					tls_conn_cipher(server->tls_ctx));
			hist_format(server->history, Activity_status, HIST_SHOW, "SELF_TLS_SNI %s :%s",
					server->name, tls_conn_servername(server->tls_ctx));
			hist_format(server->history, Activity_status, HIST_SHOW, "SELF_TLS_ISSUER %s :%s",
					server->name, tls_peer_cert_issuer(server->tls_ctx));
			hist_format(server->history, Activity_status, HIST_SHOW, "SELF_TLS_SUBJECT %s :%s",
					server->name, tls_peer_cert_subject(server->tls_ctx));
		}
	}
#endif /* TLS */

	freeaddrinfo(ai);
	server->connectfail = 0;

	if (server->password)
		serv_write(server, Sched_now, "PASS %s\r\n", server->password);
	serv_write(server, Sched_now, "NICK %s\r\n", server->self->nick);
	serv_write(server, Sched_now, "USER %s * * :%s\r\n",
			server->username ? server->username : server->self->nick,
			server->realname ? server->realname : server->self->nick);

	return;

fail:
	serv_disconnect(server, 1, NULL);
	if (server->connectfail * config_getl("reconnect.interval") < config_getl("reconnect.maxinterval"))
		server->connectfail += 1;
	if (ai)
		freeaddrinfo(ai);
}

void
serv_read(struct Server *sp) {
	char *line, *end;
	char *err;
	char *reason = NULL;
	size_t len;
	int ret;

	assert_warn(sp,);

#ifdef TLS
	if (sp->tls) {
		switch (ret = tls_read(sp->tls_ctx, &sp->input.buf[sp->input.pos], sp->input.size - sp->input.pos - 1)) {
		case -1:
			err = (char *)tls_error(sp->tls_ctx);
			len = CONSTLEN("tls_read(): ") + strlen(err) + 1;
			reason = smprintf(len, "tls_read(): %s", err);
			/* fallthrough */
		case 0:
			serv_disconnect(sp, 1, "EOF");
			hist_format(sp->history, Activity_error, HIST_SHOW,
					"SELF_CONNECTLOST %s %s %s :%s",
					sp->name, sp->host, sp->port, reason ? reason : "connection closed");
			pfree(&reason);
			return;
		case TLS_WANT_POLLIN:
		case TLS_WANT_POLLOUT:
			return;
		default:
			sp->input.pos += ret;
			break;
		}
	} else {
#endif /* TLS */
		switch (ret = read(sp->rfd, &sp->input.buf[sp->input.pos], sp->input.size - sp->input.pos - 1)) {
		case -1:
			err = estrdup(strerror(errno));
			len = CONSTLEN("read(): ") + strlen(err) + 1;
			reason = smprintf(len, "read(): %s", err);
			pfree(&err);
			/* fallthrough */
		case 0:
			serv_disconnect(sp, 1, "EOF");
			hist_format(sp->history, Activity_error, HIST_SHOW,
					"SELF_CONNECTLOST %s %s %s :%s",
					sp->name, sp->host, sp->port, reason ? reason : "connection closed");
			pfree(&reason);
			return;
		default:
			sp->input.pos += ret;
			break;
		}
#ifdef TLS
	}
#endif /* TLS */

	sp->input.buf[sp->input.size - 1] = '\0';
	line = sp->input.buf;
	while ((end = strstr(line, "\r\n"))) {
		*end = '\0';
		handle(sp, line);
		line = end + 2;
	}

	sp->input.pos -= line - sp->input.buf;
	memmove(sp->input.buf, line, sp->input.pos);

	/* Shrink and grow buffer as needed
	 * If we didn't read everything, serv_read will be called
	 * again in the main loop as poll gives another POLLIN. */
	if (sp->input.pos + ret > sp->input.size / 2) {
		sp->input.size *= 2;
		sp->input.buf = erealloc(sp->input.buf, sp->input.size);
	} else if (sp->input.pos + ret < sp->input.size / 2 && sp->input.size != INPUT_BUF_MIN) {
		sp->input.size /= 2;
		sp->input.buf = erealloc(sp->input.buf, sp->input.size);
	}
}

int
serv_write(struct Server *server, enum Sched when, char *format, ...) {
	char msg[512];
	va_list ap;
	int ret;

	assert_warn(server && server->status != ConnStatus_notconnected, -1);

	va_start(ap, format);
	ret = vsnprintf(msg, sizeof(msg), format, ap);
	va_end(ap);

	assert_warn(ret >= 0, -1);

	if (when != Sched_now) {
		switch (when) {
		case Sched_connected:
			if (server->status == ConnStatus_connected)
				goto write;
			break;
		}
		schedule(server, when, msg);
		return 0;
	}

write:

#ifdef TLS
	if (server->tls)
		do {
			ret = tls_write(server->tls_ctx, msg, strlen(msg));
		} while (ret == TLS_WANT_POLLIN || ret == TLS_WANT_POLLOUT);
	else
#endif /* TLS */
		ret = write(server->wfd, msg, strlen(msg));

	if (ret == -1 && server->status == ConnStatus_connected) {
		serv_disconnect(server, 1, NULL);
		hist_format(server->history, Activity_error, HIST_SHOW,
				"SELF_CONNECTLOST %s %s %s :%s",
				server->name, server->host, server->port, strerror(errno));
	} else if (ret == -1 && server->status != ConnStatus_connecting) {
		ui_error("Not connected to server '%s'", server->name);
	}

	return ret;
}


int
serv_len(struct Server **head) {
	struct Server *p;
	int i;

	for (p = *head, i = 0; p; p = p->next)
		i++;
	return i;
}

int
serv_poll(struct Server **head, int timeout) {
	struct pollfd fds[64];
	struct Server *sp;
	int i, ret;

	for (i=0, sp = *head; sp; sp = sp->next, i++) {
		sp->rpollfd->fd = sp->rfd;
		fds[i].fd = sp->rpollfd->fd;
		fds[i].events = POLLIN;
	}

	ret = poll(fds, serv_len(head), timeout);
	if (errno == EINTR) /* ncurses issue */
		ret = 0;

	for (i=0, sp = *head; sp; sp = sp->next, i++)
		if (sp->status == ConnStatus_connecting
				|| sp->status == ConnStatus_connected)
			sp->rpollfd->revents = fds[i].revents;

	return ret;
}

void
serv_disconnect(struct Server *server, int reconnect, char *msg) {
	struct Channel *chan;
	int ret;

	if (msg)
		serv_write(server, Sched_now, "QUIT :%s\r\n", msg);
#ifdef TLS
	if (server->tls) {
		if (server->tls_ctx) {
			do {
				ret = tls_close(server->tls_ctx);
			} while (ret == TLS_WANT_POLLIN || ret == TLS_WANT_POLLOUT);
			tls_free(server->tls_ctx);
		}
		server->tls_ctx = NULL;
	} else {
#endif /* TLS */
		shutdown(server->rfd, SHUT_RDWR);
		shutdown(server->wfd, SHUT_RDWR);
		close(server->rfd);
		close(server->wfd);
#ifdef TLS
	}
#endif /* TLS */

	server->rfd = server->wfd = server->rpollfd->fd = -1;
	server->status = ConnStatus_notconnected;
	server->lastrecv = server->pingsent = 0;
	server->lastconnected = time(NULL);
	server->reconnect = reconnect;

	/* Create a history item for disconnect:
	 *  - shows up in the log
	 *  - updates the file's mtime, so hist_laodlog knows when we disconnected */
	hist_format(server->history, Activity_none, HIST_LOG, "SELF_DISCONNECT");
	for (chan = server->channels; chan; chan = chan->next) {
		chan_setold(chan, 1);
		hist_format(chan->history, Activity_none, HIST_LOG, "SELF_DISCONNECT");
	}

	windows[Win_buflist].refresh = 1;
}

int
serv_selected(struct Server *server) {
	if (!selected.channel && selected.server == server)
		return 1;
	else
		return 0;
}

char *
support_get(struct Server *server, char *key) {
	struct Support *p;
	for (p = server->supports; p; p = p->next)
		if (strcmp(p->key, key) == 0)
			return p->value;

	return NULL;
}

void
support_set(struct Server *server, char *key, char *value) {
	struct Support *p;

	assert_warn(server,);

	if (!server->supports) {
		server->supports = emalloc(sizeof(struct Support));
		server->supports->prev = server->supports->next = NULL;
		server->supports->key = key ? strdup(key) : NULL;
		server->supports->value = value ? strdup(value) : NULL;
		return;
	}

	for (p = server->supports; p && p->next; p = p->next) {
		if (strcmp(p->key, key) == 0) {
			pfree(&p->value);
			p->value = strdup(value);
			return;
		}
	}

	p->next = emalloc(sizeof(struct Support));
	p->next->prev = p;
	p->next->next = NULL;
	p->next->key = key ? strdup(key) : NULL;
	p->next->value = value ? strdup(value) : NULL;
}

int
serv_ischannel(struct Server *server, char *str) {
	char *chantypes;

	assert_warn(str && server,0);

	chantypes = support_get(server, "CHANTYPES");
	if (!chantypes)
		chantypes = config_gets("def.chantypes");
	assert(chantypes != NULL);

	return strchr(chantypes, *str) != NULL;
}

void
serv_auto_add(struct Server *server, char *cmd) {
	char **p;
	size_t len;

	assert_warn(server && cmd,);

	if (!server->autocmds) {
		len = 1;
		server->autocmds = emalloc(sizeof(char *) * (len + 1));
	} else {
		for (p = server->autocmds, len = 1; *p; p++)
			len++;
		server->autocmds = erealloc(server->autocmds, sizeof(char *) * (len + 1));
	}

	*(server->autocmds + len - 1) = estrdup(cmd);
	*(server->autocmds + len) = NULL;
}

void
serv_auto_free(struct Server *server) {
	char **p;

	if (!server || !server->autocmds)
		return;

	for (p = server->autocmds; *p; p++)
		pfree(&*p);
	pfree(&server->autocmds);
	server->autocmds = NULL;
}

void
serv_auto_send(struct Server *server) {
	char **p;
	int save;

	if (!server || !server->autocmds)
		return;

	save = nouich;
	nouich = 1;
	for (p = server->autocmds; *p; p++)
		command_eval(server, *p);
	nouich = save;
}

/* check if autocmds has '/join <chan>' */
int
serv_auto_haschannel(struct Server *server, char *chan) {
	char **p;

	if (!server || !server->autocmds)
		return 0;

	for (p = server->autocmds; *p; p++)
		if (strncmp(*p, "/join ", CONSTLEN("/join ")) == 0 &&
				strcmp((*p) + CONSTLEN("/join "), chan) == 0)
			return 1;
	return 0;
}

void
schedule(struct Server *server, enum Sched when, char *msg) {
	struct Schedule *p;

	assert_warn(server && msg,);

	if (!server->schedule) {
		server->schedule = emalloc(sizeof(struct Schedule));
		server->schedule->prev = server->schedule->next = NULL;
		server->schedule->when = when;
		server->schedule->msg  = estrdup(msg);
		return;
	}

	for (p = server->schedule; p && p->next; p = p->next);

	p->next = emalloc(sizeof(struct Schedule));
	p->next->prev = p;
	p->next->next = NULL;
	p->next->when = when;
	p->next->msg  = estrdup(msg);
}

void
schedule_send(struct Server *server, enum Sched when) {
	struct Schedule *p;

	assert_warn(server,);

	for (p = server->schedule; p; p = p->next) {
		if (p->when == when) {
			serv_write(server, Sched_now, "%s", p->msg);

			if (p->prev) p->prev->next = p->next;
			if (p->next) p->next->prev = p->prev;

			if (!p->prev)
				server->schedule = p->next;

			pfree(&p->msg);
			pfree(&p);
		}
	}
}

void
expect_set(struct Server *server, enum Expect cmd, char *about) {
	if (cmd >= Expect_last || cmd < 0 || nouich)
		return;

	pfree(&server->expect[cmd]);
	server->expect[cmd] = about ? estrdup(about) : NULL;
}

char *
expect_get(struct Server *server, enum Expect cmd) {
	if (cmd >= Expect_last || cmd < 0)
		return NULL;
	else
		return server->expect[cmd];
}
