/*
 * src/handle.c from hirc
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
#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include "hirc.h"

#define HANDLER(func) static void func(struct Server *server, struct History *msg)
HANDLER(handle_ERROR);
HANDLER(handle_PING);
HANDLER(handle_PONG);
HANDLER(handle_JOIN);
HANDLER(handle_PART);
HANDLER(handle_KICK);
HANDLER(handle_QUIT);
HANDLER(handle_NICK);
HANDLER(handle_MODE);
HANDLER(handle_TOPIC);
HANDLER(handle_PRIVMSG);
HANDLER(handle_RPL_WELCOME);
HANDLER(handle_RPL_ISUPPORT);
HANDLER(handle_RPL_CHANNELMODEIS);
HANDLER(handle_RPL_NOTOPIC);
HANDLER(handle_RPL_TOPIC);
HANDLER(handle_RPL_TOPICWHOTIME);
HANDLER(handle_RPL_NAMREPLY);
HANDLER(handle_RPL_ENDOFNAMES);
HANDLER(handle_RPL_ENDOFMOTD);
HANDLER(handle_ERR_NICKNAMEINUSE);
HANDLER(handle_RPL_AWAY);
#undef HANDLER

struct Handler handlers[] = {
	{ "ERROR",	handle_ERROR			},
	{ "PING", 	handle_PING			},
	{ "PONG",	handle_PONG			},
	{ "JOIN",	handle_JOIN			},
	{ "PART",	handle_PART			},
	{ "KICK",	handle_KICK			},
	{ "QUIT",	handle_QUIT			},
	{ "NICK",	handle_NICK			},
	{ "MODE",	handle_MODE			},
	{ "TOPIC",	handle_TOPIC			},
	{ "PRIVMSG",	handle_PRIVMSG  		},
	{ "NOTICE",	handle_PRIVMSG	  		},
	{ "001",	handle_RPL_WELCOME		},
	{ "005",	handle_RPL_ISUPPORT		},
	{ "301",	handle_RPL_AWAY			},
	{ "324",	handle_RPL_CHANNELMODEIS	},
	{ "331",	handle_RPL_NOTOPIC		},
	{ "329",	NULL				}, /* ignore this:
							    *  - it's nonstandard
							    *  - hirc has no use for it currently
							    *  - it's annoyingly sent after MODE */
	{ "332",	handle_RPL_TOPIC		},
	{ "333",	handle_RPL_TOPICWHOTIME		},
	{ "353",	handle_RPL_NAMREPLY		},
	{ "366",	handle_RPL_ENDOFNAMES		},
	{ "376",	handle_RPL_ENDOFMOTD		},
	{ "433",	handle_ERR_NICKNAMEINUSE	},
	{ NULL,		NULL 				},
};

static void
handle_PING(struct Server *server, struct History *msg) {
	if (param_len(msg->params) < 2)
		return;

	ircprintf(server, "PONG :%s\r\n", *(msg->params+1));
}

static void
handle_PONG(struct Server *server, struct History *msg) {
	int len;

	if ((len = param_len(msg->params)) < 2)
		return;

	/* RFC1459 says that PONG should have a list of daemons,
	 * but that's not how PONG seems to work in modern IRC. 
	 * Therefore, consider the last parameter as the "message" */
	if (strcmp_n(*(msg->params + len - 1), handle_expect_get(server, Expect_pong)) == 0) {
		hist_addp(server->history, msg, Activity_status, HIST_DFL);
		handle_expect(server, Expect_pong, NULL);
	}
}

static void
handle_JOIN(struct Server *server, struct History *msg) {
	struct Channel *chan;
	struct Nick *nick;
	char *target;

	if (!msg->from || param_len(msg->params) < 2)
		return;

	target = *(msg->params+1);
	if ((chan = chan_get(&server->channels, target, -1)) == NULL)
		chan = chan_add(server, &server->channels, target, 0);
	chan_setold(chan, 0);

	nick = msg->from;
	if (nick_get(&chan->nicks, nick->nick) == NULL)
		nick_add(&chan->nicks, msg->from->prefix, ' ', server);

	hist_addp(server->history, msg, Activity_status, HIST_LOG);
	hist_addp(chan->history, msg, Activity_status, HIST_SHOW);

	if (nick_isself(nick)) {
		if (strcmp_n(target, handle_expect_get(server, Expect_join)) == 0)
			ui_select(server, chan);
		else
			windows[Win_buflist].refresh = 1;
		handle_expect(server, Expect_join, NULL);
	} else if (selected.channel == chan) {
		windows[Win_nicklist].refresh = 1;
	}
}

static void
handle_PART(struct Server *server, struct History *msg) {
	struct Channel *chan;
	struct Nick *nick;
	char *target;

	if (!msg->from || param_len(msg->params) < 2)
		return;

	target = *(msg->params+1);
	if ((chan = chan_get(&server->channels, target, -1)) == NULL)
		return;

	nick = msg->from;
	if (nick_isself(nick)) {
		chan_setold(chan, 1);
		nick_free_list(&chan->nicks);
		if (chan == selected.channel && strcmp_n(target, handle_expect_get(server, Expect_part)) == 0) {
			ui_select(selected.server, NULL);
			handle_expect(server, Expect_part, NULL);
		}
		windows[Win_buflist].refresh = 1;
	} else {
		nick_remove(&chan->nicks, nick->nick);
		if (chan == selected.channel)
			windows[Win_nicklist].refresh = 1;
	}

	hist_addp(server->history, msg, Activity_status, HIST_LOG);
	hist_addp(chan->history, msg, Activity_status, HIST_SHOW);
}

static void
handle_KICK(struct Server *server, struct History *msg) {
	struct Channel *chan;
	struct Nick *nick;
	char *target;

	if (!msg->from || param_len(msg->params) < 3)
		return;

	target = *(msg->params+1);
	if ((chan = chan_get(&server->channels, target, -1)) == NULL)
		chan = chan_add(server, &server->channels, target, 0);

	nick = nick_create(*(msg->params+2), ' ', server);
	if (nick_isself(nick)) {
		chan_setold(chan, 1);
		nick_free_list(&chan->nicks);
		if (chan == selected.channel)
			ui_select(selected.server, NULL);
		windows[Win_buflist].refresh = 1;
	} else {
		nick_remove(&chan->nicks, nick->nick);
		if (chan == selected.channel)
			windows[Win_nicklist].refresh = 1;
	}

	hist_addp(server->history, msg, Activity_status, HIST_LOG);
	hist_addp(chan->history, msg, Activity_status, HIST_SHOW);
	nick_free(nick);
}

static void
handle_ERROR(struct Server *server, struct History *msg) {
	serv_disconnect(server, 0, NULL);
	hist_addp(server->history, msg, Activity_status, HIST_DFL);
}

static void
handle_QUIT(struct Server *server, struct History *msg) {
	struct Channel *chan;
	struct Nick *nick;

	if (!msg->from || param_len(msg->params) < 1)
		return;

	nick = msg->from;
	if (nick_isself(nick)) {
		serv_disconnect(server, 0, NULL);
	}

	hist_addp(server->history, msg, Activity_status, HIST_LOG);
	for (chan = server->channels; chan; chan = chan->next) {
		if (nick_get(&chan->nicks, nick->nick) != NULL) {
			nick_remove(&chan->nicks, nick->nick);
			hist_addp(chan->history, msg, Activity_status, HIST_SHOW);
			if (chan == selected.channel)
				windows[Win_nicklist].refresh = 1;
		}
	}
}

static void
handle_MODE(struct Server *server, struct History *msg) {
	struct Channel *chan;

	if (!msg->from || param_len(msg->params) < 3)
		return;

	if (serv_ischannel(server, *(msg->params+1))) {
		if ((chan = chan_get(&server->channels, *(msg->params+1), -1)) == NULL)
			chan = chan_add(server, &server->channels, *(msg->params+1), 0);

		hist_addp(chan->history, msg, Activity_status, HIST_DFL);
		ircprintf(server, "MODE %s\r\n", chan->name); /* Get full mode via RPL_CHANNELMODEIS
						               * instead of concatenating manually */
		ircprintf(server, "NAMES %s\r\n", chan->name); /* Also get updated priviledges */
	} else {
		hist_addp(server->history, msg, Activity_status, HIST_DFL);
	}
}

static void
handle_PRIVMSG(struct Server *server, struct History *msg) {
	int act_direct = Activity_hilight, act_regular = Activity_message;
	struct Channel *chan;
	struct Channel *priv;
	struct Nick *nick;
	char *target;

	if (!msg->from || param_len(msg->params) < 3)
		return;

	if (strcmp(*msg->params, "NOTICE") == 0)
		act_direct = act_regular = Activity_notice;

	target = *(msg->params + 1);
	nick = msg->from;
	if (strchr(nick->nick, '.')) {
		/* it's a server */
		hist_addp(server->history, msg, Activity_status, HIST_DFL);
	} else if (strcmp(target, server->self->nick) == 0) {
		/* it's messaging me */
		if ((priv = chan_get(&server->privs, nick->nick, -1)) == NULL)
			priv = chan_add(server, &server->privs, nick->nick, 1);
		chan_setold(priv, 0);

		hist_addp(priv->history, msg, act_direct, HIST_DFL);
	} else if (nick_isself(nick) && !chrcmp(*target, "#&!+")) {
		/* i'm messaging someone */
		if ((priv = chan_get(&server->privs, target, -1)) == NULL)
			priv = chan_add(server, &server->privs, target, 1);
		chan_setold(priv, 0);

		hist_addp(priv->history, msg, act_regular, HIST_DFL);
	} else {
		/* message to a channel */
		if ((chan = chan_get(&server->channels, target, -1)) == NULL)
			chan = chan_add(server, &server->channels, target, 0);

		hist_addp(chan->history, msg, nick_isself(nick) ? act_direct : act_regular, HIST_DFL);
	}
}

static void
handle_RPL_ISUPPORT(struct Server *server, struct History *msg) {
	char *key, *value;
	char **params = msg->params;

	hist_addp(server->history, msg, Activity_status, HIST_DFL);
	if (param_len(msg->params) < 4)
		return;

	params += 2;

	/* skip the last param ".... :are supported by this server" */
	for (; *params && *(params+1); params++) {
		key = tstrdup(*params);
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

static void
handle_RPL_AWAY(struct Server *server, struct History *msg) {
	struct Channel *priv;
	struct HistInfo *history;

	if ((priv = chan_get(&server->privs, *(msg->params+2), -1)) != NULL)
		history = priv->history;
	else
		history = server->history;
	hist_addp(history, msg, Activity_status, HIST_DFL);
}

static void
handle_RPL_CHANNELMODEIS(struct Server *server, struct History *msg) {
	struct Channel *chan;

	if (param_len(msg->params) < 4)
		return;

	if ((chan = chan_get(&server->channels, *(msg->params+2), -1)) == NULL)
		chan = chan_add(server, &server->channels, *(msg->params+2), 0);

	free(chan->mode);
	chan->mode = estrdup(*(msg->params+3));

	if (handle_expect_get(server, Expect_channelmodeis)) {
		hist_addp(chan->history, msg, Activity_status, HIST_DFL);
		handle_expect(server, Expect_channelmodeis, NULL);
	} else {
		hist_addp(chan->history, msg, Activity_status, HIST_LOG);
	}
}

static void
handle_RPL_NAMREPLY(struct Server *server, struct History *msg) {
	struct Channel *chan;
	struct Nick *oldnick;
	char **params = msg->params;
	char *nick, priv, *target;
	char **nicks, **nicksref;
	char *supportedprivs;

	if (param_len(params) < 5)
		return;

	params += 3;
	target = *params;

	if ((chan = chan_get(&server->channels, target, -1)) == NULL)
		chan = chan_add(server, &server->channels, target, 0);

	if (strcmp_n(target, handle_expect_get(server, Expect_names)) == 0)
		hist_addp(chan->history, msg, Activity_status, HIST_DFL);
	else
		hist_addp(chan->history, msg, Activity_status, HIST_LOG);

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

static void
handle_RPL_ENDOFNAMES(struct Server *server, struct History *msg) {
	char *target;

	hist_addp(server->history, msg, Activity_status, HIST_LOG);
	if (param_len(msg->params) < 3)
		return;

	target = *(msg->params+2);
	if (strcmp_n(target, handle_expect_get(server, Expect_names)) == 0)
		handle_expect(server, Expect_names, NULL);
}

static void
handle_ERR_NICKNAMEINUSE(struct Server *server, struct History *msg) {
	char nick[64]; /* should be limited to 9 chars, but newer servers *shrug*/

	hist_addp(server->history, msg, Activity_status, HIST_DFL);

	if (handle_expect_get(server, Expect_nicknameinuse) == NULL) {
		snprintf(nick, sizeof(nick), "%s_", server->self->nick);
		nick_free(server->self);
		server->self = nick_create(nick, ' ', server);
		server->self->self = 1;
		ircprintf(server, "NICK %s\r\n", nick);
	} else {
		handle_expect(server, Expect_nicknameinuse, NULL);
	}
}

static void
handle_NICK(struct Server *server, struct History *msg) {
	struct Nick *nick, *chnick;
	struct Channel *chan;
	char prefix[128];
	char *newnick;
	char priv;

	if (!msg->from || !*msg->params || !*msg->params+1)
		return;

	nick = msg->from;
	hist_addp(server->history, msg, Activity_status, HIST_DFL);
	newnick = *(msg->params+1);

	if (strcmp(nick->nick, newnick) == 0)
		return;

	if (nick_isself(nick)) {
		nick_free(server->self);
		server->self = nick_create(newnick, ' ', server);
		server->self->self = 1;
		handle_expect(server, Expect_nicknameinuse, NULL);
	}

	for (chan = server->channels; chan; chan = chan->next) {
		if ((chnick = nick_get(&chan->nicks, nick->nick)) != NULL) {
			snprintf(prefix, sizeof(prefix), ":%s!%s@%s",
					newnick, chnick->ident, chnick->host);
			priv = chnick->priv;
			nick_remove(&chan->nicks, nick->nick);
			nick_add(&chan->nicks, prefix, priv, server);
			hist_addp(chan->history, msg, Activity_status, HIST_SHOW);
			if (selected.channel == chan)
				windows[Win_nicklist].refresh = 1;
		}
	}
}

static void
handle_TOPIC(struct Server *server, struct History *msg) {
	struct Channel *chan;

	if (param_len(msg->params) < 3 || !msg->from)
		return;

	if ((chan = chan_get(&server->channels, *(msg->params+1), -1)) != NULL) {
		hist_addp(chan->history, msg, Activity_status, HIST_DFL);
		free(chan->topic);
		chan->topic = *(msg->params+2) ? estrdup(*(msg->params+2)) : NULL;
	}
}

static void
handle_RPL_NOTOPIC(struct Server *server, struct History *msg) {
	struct Channel *chan;
	char *target;

	if (param_len(msg->params) < 4)
		return;

	target = *(msg->params+2);

	if ((chan = chan_get(&server->channels, target, -1)) == NULL)
		return;

	if (strcmp_n(target, handle_expect_get(server, Expect_topic)) == 0) {
		hist_addp(chan->history, msg, Activity_status, HIST_DFL);
		handle_expect(server, Expect_topic, NULL);
	} else {
		hist_addp(chan->history, msg, Activity_status, HIST_LOG);
	}
}

static void
handle_RPL_TOPIC(struct Server *server, struct History *msg) {
	struct Channel *chan;
	char *target, *topic;

	if (param_len(msg->params) < 4)
		return;

	target = *(msg->params+2);
	topic = *(msg->params+3);

	if ((chan = chan_get(&server->channels, target, -1)) == NULL)
		return;

	free(chan->topic);
	chan->topic = topic ? estrdup(topic) : NULL;

	if (strcmp_n(target, handle_expect_get(server, Expect_topic)) == 0) {
		hist_addp(chan->history, msg, Activity_status, HIST_DFL);
		handle_expect(server, Expect_topic, NULL);
		handle_expect(server, Expect_topicwhotime, target);
	} else {
		hist_addp(chan->history, msg, Activity_status, HIST_LOG);
	}
}

static void
handle_RPL_TOPICWHOTIME(struct Server *server, struct History *msg) {
	struct Channel *chan;
	char *target;

	if (param_len(msg->params) < 5)
		return;

	target = *(msg->params+2);

	if ((chan = chan_get(&server->channels, target, -1)) == NULL)
		return;

	if (strcmp_n(target, handle_expect_get(server, Expect_topicwhotime)) == 0) {
		hist_addp(chan->history, msg, Activity_status, HIST_DFL);
		handle_expect(server, Expect_topicwhotime, NULL);
	} else {
		hist_addp(chan->history, msg, Activity_status, HIST_LOG);
	}
}

static void
handle_RPL_WELCOME(struct Server *server, struct History *msg) {
	server->status = ConnStatus_connected;
	hist_addp(server->history, msg, Activity_status, HIST_DFL);
}

static void
handle_RPL_ENDOFMOTD(struct Server *server, struct History *msg) {
	/* If server doesn't support RPL_WELCOME, use RPL_ENDOFMOTD to set status */
	server->status = ConnStatus_connected;
	hist_addp(server->history, msg, Activity_status, HIST_DFL);
}

/* Expect stuff should probably be moved to serv.c.
 * Also, it might be better to have an enum for all commands and numerics somewhere */
void
handle_expect(struct Server *server, enum Expect cmd, char *about) {
	if (cmd >= Expect_last || cmd < 0 || readingconf)
		return;

	free(server->expect[cmd]);
	server->expect[cmd] = about ? estrdup(about) : NULL;
}

char *
handle_expect_get(struct Server *server, enum Expect cmd) {
	if (cmd >= Expect_last || cmd < 0)
		return NULL;
	else
		return server->expect[cmd];
}

void
handle_logonly(struct Server *server, struct History *msg) {
	hist_addp(server->history, msg, Activity_status, HIST_LOG);
}

void
handle(struct Server *server, char *msg) {
	struct History *hist;
	time_t timestamp;
	char **params;
	char *cmd;
	char *schmsg;
	int i;

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

	/* Fire off any scheduled events for the current cmd */
	while ((schmsg = schedule_pull(server, cmd)) != NULL)
		ircprintf(server, "%s", schmsg);

	for (i=0; cmd && handlers[i].cmd; i++) {
		if (strcmp(handlers[i].cmd, cmd) == 0) {
			if (handlers[i].func) {
				/* histinfo set to the server's history
				 * currently, but not actually appended */
				hist = hist_create(server->history, NULL, msg, 0, timestamp, 0);
				handlers[i].func(server, hist);
				hist_free(hist);
			}
			/* NULL handlers will stop a message being added to server->history */
			return;
		}
	}

	/* add it to server->history if there is no handler */
	hist_add(server->history, NULL, msg, Activity_status, timestamp, HIST_DFL);
	param_free(params);
}
