/* See LICENSE for copyright details */

#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include "hirc.h"

struct Handler handlers[] = {
	{ "PING", 	handle_PING		},
	{ "PONG",	handle_PONG		},
	{ "JOIN",	handle_JOIN		},
	{ "PART",	handle_PART		},
	{ "QUIT",	handle_QUIT		},
	{ "NICK",	handle_NICK		},
	{ "PRIVMSG",	handle_PRIVMSG  	},
	{ "NOTICE",	handle_PRIVMSG	  	},
	{ "001",	handle_WELCOME		},
	{ "005",	handle_ISUPPORT		},
	{ "353",	handle_NAMREPLY		},
	{ "366",	NULL /* end of names */	},
	{ "376",	handle_ENDOFMOTD	},
	{ "433",	handle_NICKNAMEINUSE	},
	{ NULL,		NULL 			},
};

void
handle_PING(char *msg, char **params, struct Server *server, time_t timestamp) {
	if (**params == ':')
		params++;

	if (param_len(params) < 2)
		return;

	ircprintf(server, "PONG :%s\r\n", *(params+1));
}

void
handle_PONG(char *msg, char **params, struct Server *server, time_t timestamp) {
	int len;

	if (**params == ':')
		params++;

	if ((len = param_len(params)) < 2)
		return;

	/* RFC1459 says that PONG should have a list of daemons,
	 * but that's not how PONG seems to work in modern IRC. 
	 * Therefore, consider the last parameter as the "message" */
	if (strcmp_n(*(params + len - 1), handle_expect_get(server, Expect_pong)) == 0) {
		hist_add(server->history, NULL, msg, params, Activity_status, timestamp, HIST_DFL);
		handle_expect(server, Expect_pong, NULL);
	}
}

void
handle_JOIN(char *msg, char **params, struct Server *server, time_t timestamp) {
	struct Channel *chan;
	struct Nick *nick;
	char *target;

	if (**params != ':' || param_len(params) < 3)
		return;

	target = *(params+2);
	if ((chan = chan_get(&server->channels, target, -1)) == NULL)
		chan = chan_add(server, &server->channels, target);
	chan_setold(chan, 0);

	nick = nick_create(*params, ' ', server);
	if (nick_get(&chan->nicks, nick->nick) == NULL)
		nick_add(&chan->nicks, *params, ' ', server);

	hist_add(server->history, nick, msg, params, Activity_status, timestamp, HIST_LOG);
	hist_add(chan->history, nick, msg, params, Activity_status, timestamp, HIST_SHOW);

	if (nick_isself(nick)) {
		if (strcmp_n(target, handle_expect_get(server, Expect_join)) == 0)
			ui_select(server, chan);
		else
			windows[Win_buflist].refresh = 1;
		handle_expect(server, Expect_join, NULL);
	} else if (selected.channel == chan) {
		windows[Win_nicklist].refresh = 1;
	}

	nick_free(nick);
}

void
handle_PART(char *msg, char **params, struct Server *server, time_t timestamp) {
	struct Channel *chan;
	struct Nick *nick;
	char *target;

	if (**params != ':' || param_len(params) < 3)
		return;

	target = *(params+2);
	if ((chan = chan_get(&server->channels, target, -1)) == NULL)
		chan = chan_add(server, &server->channels, target);

	nick = nick_create(*params, ' ', server);
	if (nick_isself(nick)) {
		chan_setold(chan, 1);
		nick_free_list(&chan->nicks);
		if (chan == selected.channel && strcmp_n(target, handle_expect_get(server, Expect_part))) {
			ui_select(selected.server, NULL);
			handle_expect(server, Expect_part, NULL);
		}
		windows[Win_buflist].refresh = 1;
	} else {
		nick_remove(&chan->nicks, nick->nick);
		if (chan == selected.channel)
			windows[Win_nicklist].refresh = 1;
	}

	hist_add(server->history, nick, msg, params, Activity_status, timestamp, HIST_LOG);
	hist_add(chan->history, nick, msg, params, Activity_status, timestamp, HIST_SHOW);
	nick_free(nick);
}

void
handle_QUIT(char *msg, char **params, struct Server *server, time_t timestamp) {
	struct Channel *chan;
	struct Nick *nick;

	if (**params != ':' || param_len(params) < 2)
		return;

	nick = nick_create(*params, ' ', server);
	if (nick_isself(nick)) {
		/* TODO: umm, sound like a big deal anyone? */
		(void)0;
	}

	hist_add(server->history, nick, msg, params, Activity_status, timestamp, HIST_LOG);
	for (chan = server->channels; chan; chan = chan->next) {
		if (nick_get(&chan->nicks, nick->nick) != NULL) {
			nick_remove(&chan->nicks, nick->nick);
			hist_add(chan->history, nick, msg, params, Activity_status, timestamp, HIST_SHOW);
			if (chan == selected.channel)
				windows[Win_nicklist].refresh = 1;
		}
	}

	nick_free(nick);
}

void
handle_PRIVMSG(char *msg, char **params, struct Server *server, time_t timestamp) {
	int act_direct = Activity_hilight, act_regular = Activity_message;
	struct Channel *chan;
	struct Channel *priv;
	struct Nick *nick;
	char *target;

	if (**params != ':' || param_len(params) < 4)
		return;

	if (strcmp(*params, "NOTICE") == 0)
		act_direct = act_regular = Activity_notice;

	target = *(params + 2);
	nick = nick_create(*params, ' ', server);
	if (strchr(nick->nick, '.')) {
		/* it's a server */
		hist_add(server->history, NULL, msg, params, Activity_status, timestamp, HIST_DFL);
	} else if (strcmp(target, server->self->nick) == 0) {
		/* it's messaging me */
		if ((priv = chan_get(&server->privs, nick->nick, -1)) == NULL)
			priv = chan_add(server, &server->privs, nick->nick);
		chan_setold(priv, 0);

		hist_add(priv->history, nick, msg, params, act_direct, timestamp, HIST_DFL);
	} else if (nick_isself(nick) && !chrcmp(*target, "#&!+")) {
		/* i'm messaging someone */
		if ((priv = chan_get(&server->privs, target, -1)) == NULL)
			priv = chan_add(server, &server->privs, target);
		chan_setold(priv, 0);

		hist_add(priv->history, nick, msg, params, act_regular, timestamp, HIST_DFL);
	} else {
		/* message to a channel */
		if ((chan = chan_get(&server->channels, target, -1)) == NULL)
			chan = chan_add(server, &server->channels, target);

		hist_add(chan->history, nick, msg, params, act_regular, timestamp, HIST_DFL);
	}

	nick_free(nick);
}

void
handle_ISUPPORT(char *msg, char **params, struct Server *server, time_t timestamp) {
	char *key, *value;

	hist_add(server->history, NULL, msg, params, Activity_status, timestamp, HIST_DFL);
	if (**params == ':')
		params++;

	if (param_len(params) < 4)
		return;

	params += 2;

	/* skip the last param ".... :are supported by this server" */
	for (; *params && *(params+1); params++) {
		key = *params;
		if ((value = strchr(key, '=')) != NULL) {
			*value = '\0';
			if (*(value+1))
				value++;
			else
				value = NULL;
		}

		support_set(server, key, value);
	}
}

void
handle_NAMREPLY(char *msg, char **params, struct Server *server, time_t timestamp) {
	struct Channel *chan;
	struct Nick *oldnick;
	char *nick, priv, *target;
	char **nicks, **nicksref;
	char *supportedprivs;

	if (**params == ':')
		params++;

	if (param_len(params) < 5)
		return;

	params += 3;

	target = *params;
	if ((chan = chan_get(&server->channels, target, -1)) == NULL)
		chan = chan_add(server, &server->channels, target);
	params++;

	supportedprivs = strchr(support_get(server, "PREFIX"), ')');
	if (supportedprivs == NULL || supportedprivs[0] == '\0')
		supportedprivs = "";
	else
		supportedprivs++;

	nicksref = nicks = param_create(*params);
	for (; *nicks && **nicks; nicks++) {
		priv = ' ';
		nick = *nicks;
		if (chrcmp(**nicks, supportedprivs)) {
			priv = **nicks;
			while (chrcmp(*nick, supportedprivs))
				nick++;
		}
		if ((oldnick = nick_get(&chan->nicks, nick)) == NULL)
			nick_add(&chan->nicks, nick, priv, server);
		else
			oldnick->priv = priv;
	}

	if (selected.channel == chan)
		windows[Win_nicklist].refresh = 1;
	param_free(nicksref);
}

void
handle_NICKNAMEINUSE(char *msg, char **params, struct Server *server, time_t timestamp) {
	char nick[64]; /* should be limited to 9 chars, but newer servers *shrug*/

	hist_add(server->history, NULL, msg, params, Activity_status, timestamp, HIST_DFL);
	snprintf(nick, sizeof(nick), "%s_", server->self->nick);
	nick_free(server->self);
	server->self = nick_create(nick, ' ', server);
	server->self->self = 1;
	ircprintf(server, "NICK %s\r\n", nick);
}

void
handle_NICK(char *msg, char **params, struct Server *server, time_t timestamp) {
	struct Nick *nick, *chnick;
	struct Channel *chan;
	char prefix[128];
	char *newnick;

	if (**params != ':' || !*(params+1) || !*(params+2))
		return;

	nick = nick_create(*params, ' ', server);
	hist_add(server->history, nick, msg, params, Activity_status, timestamp, HIST_DFL);
	newnick = *(params+2);

	if (strcmp(nick->nick, newnick) == 0)
		return;

	if (nick_isself(nick)) {
		nick_free(server->self);
		server->self = nick_create(newnick, ' ', server);
		server->self->self = 1;
	}

	for (chan = server->channels; chan; chan = chan->next) {
		if ((chnick = nick_get(&chan->nicks, nick->nick)) != NULL) {
			snprintf(prefix, sizeof(prefix), ":%s!%s@%s",
					newnick, chnick->ident, chnick->host);
			nick_add(&chan->nicks, prefix, chnick->priv, server);
			nick_remove(&chan->nicks, nick->nick);
			if (selected.channel == chan)
				windows[Win_nicklist].refresh = 1;
		}
	}
}

void
handle_WELCOME(char *msg, char **params, struct Server *server, time_t timestamp) {
	server->status = ConnStatus_connected;
	hist_add(server->history, NULL, msg, params, Activity_status, timestamp, HIST_DFL);
}

void
handle_ENDOFMOTD(char *msg, char **params, struct Server *server, time_t timestamp) {
	/* If server doesn't support RPL_WELCOME, use RPL_ENDOFMOTD to set status */
	server->status = ConnStatus_connected;
	hist_add(server->history, NULL, msg, params, Activity_status, timestamp, HIST_DFL);
}

/* Expect stuff should probably be moved to serv.c.
 * Also, it might be better to have an enum for all commands and numerics somewhere */
void
handle_expect(struct Server *server, enum Expect cmd, char *about) {
	if (cmd >= Expect_last || cmd < 0)
		return;

	free(server->expect[cmd]);
	server->expect[cmd] = about ? strdup(about) : NULL;
}

char *
handle_expect_get(struct Server *server, enum Expect cmd) {
	if (cmd >= Expect_last || cmd < 0)
		return NULL;
	else
		return server->expect[cmd];
}

void
handle(int rfd, struct Server *server) {
	time_t timestamp;
	char **params;
	char *cmd;
	char *msg;
	char buf[511];
	/* using a buffer size of 511:
	 * - RFC1459 defines the maximum size of a message to be 512
	 * - read_line() doesn't copy the \r\n so this is reduced to 510
	 * - a \0 is needed at the end, so 510 + 1 = 511 */
	int i;

	if (!read_line(rfd, buf, sizeof(buf))) {
		if (buf[0] == EOF || buf[0] == 3 || buf[0] == 4) {
			serv_disconnect(server, 1, "EOF");
			hist_format(server->history, Activity_error, HIST_SHOW,
					"SELF_CONNECTLOST %s %s %s :EOF received",
					server->name, server->host, server->port);
		}
		return;
	}
	msg = buf;

	if (*msg == '!' && strchr(msg, ' ') && *(strchr(msg, ' ')+1) && *(msg+1) != ' ') {
		msg++;
		timestamp = (time_t)strtoll(msg, NULL, 10);
		msg = strchr(msg, ' ') + 1;
	} else {
		timestamp = time(NULL);
	}

	params = param_create(msg);
	if (!*params) {
		free(params);
		return;
	}

	if (**params == ':' || **params == '|')
		cmd = *(params + 1);
	else
		cmd = *(params);

	for (i=0; cmd && handlers[i].cmd; i++) {
		if (strcmp(handlers[i].cmd, cmd) == 0) {
			if (handlers[i].func)
				handlers[i].func(msg, params, server, timestamp);
			/* NULL handlers will stop a message being added to server->history */
			return;
		}
	}

	/* add it to server->history if there is no handler */
	hist_add(server->history, NULL, msg, params, Activity_status, timestamp, HIST_DFL);
}
