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

void
serv_free(struct Server *server) {
	struct Support *p;

	if (!server)
		return;

	free(server->name);
	free(server->username);
	free(server->realname);
	free(server->host);
	free(server->port);
	free(server->rpollfd);
	nick_free(server->self);
	hist_free_list(server->history);
	chan_free_list(&server->channels);
	chan_free_list(&server->privs);
	for (p = server->supports; p; p = p->next) {
		free(p->prev);
		free(p->key);
		free(p->value);
	}
#ifdef TLS
	if (server->tls)
		tls_free(server->tls_ctx);
#endif /* TLS */
	free(p);
}

struct Server *
serv_create(char *name, char *host, char *port, char *nick, 
		char *username, char *realname, int tls, int tls_verify) {
	struct Server *server;
	struct tls_config *conf;

	if (!name || !host || !port || !nick)
		return NULL;

	server = emalloc(sizeof(struct Server));
	server->prev = server->next = NULL;
	server->wfd = server->rfd = server->logfd = -1;
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
	support_set(server, "CHANTYPES", default_chantypes);
	support_set(server, "PREFIX", default_prefixes);
	server->self = nick_create(nick, ' ', NULL);
	server->self->self = 1;
	server->history = emalloc(sizeof(struct HistInfo));
	server->history->activity = Activity_ignore;
	server->history->unread = 0;
	server->history->server = server;
	server->history->channel = NULL;
	server->history->history = NULL;
	server->channels = NULL;
	server->privs = NULL;
	server->reconnect = 0;
	server->connectfail = 0;
	server->lastconnected = server->lastrecv = server->pingsent = 0;

#ifdef TLS
	server->tls = tls;
	server->tls_ctx = NULL;
	if (server->tls && (conf = tls_config_new()) == NULL) {
		ui_tls_config_error(conf, "tls_config_new()");
		server->tls = 0;
	}

	if (server->tls && !tls_verify) {
		tls_config_insecure_noverifycert(conf);
		tls_config_insecure_noverifyname(conf);
	}

	if (server->tls && (server->tls_ctx = tls_client()) == NULL) {
		ui_perror("tls_client()");
		server->tls = 0;
	}

	if (server->tls && tls_configure(server->tls_ctx, conf) == -1) {
		ui_tls_error(server->tls_ctx, "tls_configure()");
		server->tls = 0;
	}

	tls_config_free(conf);
#else
	if (tls)
		hist_format(server->history, Activity_error, HIST_SHOW,
				"SELF_TLSNOTCOMPILED %s", server->name);
#endif /* TLS */

	return server;
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

struct Server *
serv_get_byrfd(struct Server **head, int rfd) {
	struct Server *p;

	if (!head || !*head)
		return NULL;

	for (p = *head; p; p = p->next) {
		if (p->rfd == rfd)
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

	if (p->prev = NULL) {
		*head = p->next;
		serv_free(p);
		return 1;
	}

	p->prev->next = p->next;
	if (p->next != NULL)
		p->next->prev = p->prev;
	serv_free(p);
	return 1;
}

void
serv_connect(struct Server *server) {
	struct addrinfo hints;
	struct addrinfo *ai;
	int fd, ret, serrno;

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

	server->connectfail = 0;
	server->status = ConnStatus_connected;
	server->rfd = server->wfd = fd;
	hist_format(server->history, Activity_status, HIST_SHOW|HIST_MAIN,
			"SELF_CONNECTED %s %s %s", server->name, server->host, server->port);
	freeaddrinfo(ai);

	ircprintf(server, "NICK %s\r\n", server->self->nick);
	ircprintf(server, "USER %s * * :%s\r\n", 
			server->username ? server->username : server->self->nick,
			server->realname ? server->realname : server->self->nick);

	return;

fail:
	serv_disconnect(server, 1);
	if (server->connectfail * reconnectinterval < maxreconnectinterval)
		server->connectfail += 1;
	freeaddrinfo(ai);
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
	ircprintf(server, "QUIT %s\r\n", msg);
	shutdown(server->rfd, SHUT_RDWR);
	shutdown(server->wfd, SHUT_RDWR);
	close(server->rfd);
	close(server->wfd);

	server->rfd = server->wfd = server->rpollfd->fd = -1;
	server->status = ConnStatus_notconnected;
	server->lastrecv = server->pingsent = 0;
	server->lastconnected = time(NULL);
	server->reconnect = reconnect;
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

	if (!server->supports) {
		server->supports = malloc(sizeof(struct Support));
		server->supports->prev = server->supports->next = NULL;
		server->supports->key = key ? strdup(key) : NULL;
		server->supports->value = value ? strdup(value) : NULL;
		return;
	}

	for (p = server->supports; p && p->next; p = p->next) {
		if (strcmp(p->key, key) == 0) {
			free(p->value);
			p->value = strdup(value);
			return;
		}
	}

	p->next = malloc(sizeof(struct Support));
	p->next->prev = p;
	p->next->next = NULL;
	p->next->key = key ? strdup(key) : NULL;
	p->next->value = value ? strdup(value) : NULL;
}
