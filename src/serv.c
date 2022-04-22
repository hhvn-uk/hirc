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
#include <assert.h>
#include <errno.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <poll.h>
#ifdef TLS
#include <tls.h>
#endif /* TLS */
#include "hirc.h"

void
serv_free(struct Server *server) {
	struct Support *p, *prev;

	if (!server)
		return;

	pfree(&server->name);
	pfree(&server->username);
	pfree(&server->realname);
	pfree(&server->host);
	pfree(&server->port);
	pfree(&server->rpollfd);
	nick_free(server->self);
	hist_free_list(server->history);
	chan_free_list(&server->channels);
	chan_free_list(&server->privs);
	prev = server->supports;
	p = prev->next;
	while (prev) {
		pfree(&prev->key);
		pfree(&prev->value);
		pfree(&prev);
		prev = p;
		if (p)
			p = p->next;
	}
#ifdef TLS
	if (server->tls)
		if (server->tls_ctx)
			tls_free(server->tls_ctx);
#endif /* TLS */
	pfree(&p);
}

struct Server *
serv_create(char *name, char *host, char *port, char *nick,
		char *username, char *realname, int tls, int tls_verify) {
	struct Server *server;
	int i;

	if (!name || !host || !port || !nick)
		return NULL;

	server = emalloc(sizeof(struct Server));
	server->prev = server->next = NULL;
	server->wfd = server->rfd = -1;
	server->inputlen = 0;
	server->rpollfd = emalloc(sizeof(struct pollfd));
	server->rpollfd->fd = -1;
	server->rpollfd->events = POLLIN;
	server->rpollfd->revents = 0;
	server->status = ConnStatus_notconnected;
	server->name = estrdup(name);
	server->username = username ? estrdup(username) : NULL;
	server->realname = realname ? estrdup(realname) : NULL;
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
	server->privs = NULL;
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
		char *realname, int tls, int tls_verify) {
	if (!sp)
		return;
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
serv_add(struct Server **head, char *name, char *host, char *port,
		char *nick, char *username, char *realname, int tls, int tls_verify) {
	struct Server *new, *p;

	if ((new = serv_create(name, host, port, nick, username, realname, tls, tls_verify)) == NULL)
		return NULL;

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

	if (!head || !*head || !name)
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

	if (!head || !name)
		return -1;

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
	int fd, ret, serrno;

	if (!server)
		return;

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

	serv_write(server, "NICK %s\r\n", server->self->nick);
	serv_write(server, "USER %s * * :%s\r\n",
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

	if (!sp)
		return;

#ifdef TLS
	if (sp->tls) {
		switch (ret = tls_read(sp->tls_ctx, &sp->inputbuf[sp->inputlen], SERVER_INPUT_SIZE - sp->inputlen - 1)) {
		case -1:
			err = (char *)tls_error(sp->tls_ctx);
			len = CONSTLEN("tls_read(): ") + strlen(err) + 1;
			reason = emalloc(len);
			snprintf(reason, len, "tls_read(): %s", err);
			/* fallthrough */
		case 0:
			serv_disconnect(sp, 1, "EOF");
			hist_format(sp->history, Activity_error, HIST_SHOW,
					"SELF_CONNECTLOST %s %s %s :%s",
					sp->name, sp->host, sp->port, reason ? reason : "connection close");
			pfree(&reason);
			return;
		case TLS_WANT_POLLIN:
		case TLS_WANT_POLLOUT:
			return;
		default:
			sp->inputlen += ret;
			break;
		}
	} else {
#endif /* TLS */
		switch (ret = read(sp->rfd, &sp->inputbuf[sp->inputlen], SERVER_INPUT_SIZE - sp->inputlen - 1)) {
		case -1:
			err = estrdup(strerror(errno));
			len = CONSTLEN("read(): ") + strlen(err) + 1;
			reason = emalloc(len);
			snprintf(reason, len, "read(): %s", err);
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
			sp->inputlen += ret;
			break;
		}
#ifdef TLS
	}
#endif /* TLS */

	sp->inputbuf[SERVER_INPUT_SIZE - 1] = '\0';
	line = sp->inputbuf;
	while (end = strstr(line, "\r\n")) {
		*end = '\0';
		handle(sp, line);
		line = end + 2;
	}

	sp->inputlen -= line - sp->inputbuf;
	memmove(sp->inputbuf, line, sp->inputlen);
}

int
serv_write(struct Server *server, char *format, ...) {
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

	va_end(ap);
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
	struct Support *s, *prev = NULL;
	int ret;

	if (msg)
		serv_write(server, "QUIT :%s\r\n", msg);
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

	for (chan = server->channels; chan; chan = chan->next)
		chan_setold(chan, 1);

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

	if (!server)
		return;

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

	if (!str || !server)
		return 0;

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

	if (!server || !cmd)
		return;

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
schedule_push(struct Server *server, char *tmsg, char *msg) {
	struct Schedule *p;

	if (!server)
		return;

	if (!server->schedule) {
		server->schedule = emalloc(sizeof(struct Schedule));
		server->schedule->prev = server->schedule->next = NULL;
		server->schedule->tmsg = strdup(tmsg);
		server->schedule->msg  = strdup(msg);
		return;
	}

	for (p = server->schedule; p && p->next; p = p->next);

	p->next = emalloc(sizeof(struct Schedule));
	p->next->prev = p;
	p->next->next = NULL;
	p->next->tmsg = strdup(tmsg);
	p->next->msg  = strdup(msg);
}

char *
schedule_pull(struct Server *server, char *tmsg) {
	static char *ret = NULL;
	struct Schedule *p;

	if (!server || !tmsg)
		return NULL;

	for (p = server->schedule; p; p = p->next) {
		if (strcmp(p->tmsg, tmsg) == 0) {
			pfree(&p->tmsg);

			/* Don't free p->msg, instead save it to
			 * a static pointer that we free the next
			 * time schedule_pull is invoked. Since
			 * schedule_pull will probably be used in
			 * while loops until it equals NULL, this
			 * will likely be set free quite quickly */
			pfree(&ret);
			ret = p->msg;

			if (p->prev) p->prev->next = p->next;
			if (p->next) p->next->prev = p->prev;

			if (!p->prev)
				server->schedule = p->next;

			pfree(&p);
			return ret;
		}
	}

	pfree(&ret);
	ret = NULL;
	return NULL;
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
