/*
 * src/commands.c from hirc
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

#include <ncurses.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>
#include <ctype.h>
#include <regex.h>
#include <errno.h>
#include <pwd.h>
#include <sys/types.h>
#include "hirc.h"

#define command_toofew(cmd) ui_error("/%s: too few arguments", cmd)
#define command_toomany(cmd) ui_error("/%s: too many arguments", cmd)
#define command_needselected(cmd, type) ui_error("/%s: no %s selected", cmd, type)

/*
 * There are some commands that may be useful that I haven't bothered to implement.
 *
 * /notify may be useful but would require storing data in the server, and the
 * ability to perform actions at certain time intervals.
 *
 * I don't think I have ever used /knock
 *
 * A lot of commands related to server administration are nonstandard and/or
 * unwieldy, and as such aren't implemented here. These can be used via
 * aliases, eg: /alias /kline /quote kline.
 *
 */

#include "data/commands.h"

static char *command_optarg;
enum {
	opt_error = -2,
	opt_done = -1,
	CMD_NARG,
	CMD_ARG,
};

struct Alias *aliases = NULL;

COMMAND(
command_away) {
	struct Server *sp;
	char *format;
	int all = 1, ret;
	enum { opt_one };
	static struct CommandOpts opts[] = {
		{"one", CMD_NARG, opt_one},
		{"NULL", 0, 0},
	};

	while ((ret = command_getopt(&str, opts)) != opt_done) {
		switch (ret) {
		case opt_error:
			return;
		case opt_one:
			all = 0;
			break;
		}
	}

	if (str)
		format = "AWAY :%s\r\n";
	else
		format = "AWAY\r\n";

	if (all) {
		for (sp = servers; sp; sp = sp->next)
			serv_write(sp, format, str);
	} else if (server) {
		serv_write(server, format, str);
	} else {
		ui_error("-one specified, but no server selected", NULL);
	}
}

COMMAND(
command_msg) {
	struct Channel *chan = NULL;
	char *target, *message;

	if (!str) {
		command_toofew("msg");
		return;
	}

	target = strtok_r(str, " ", &message);

	if (serv_ischannel(server, target))
		chan = chan_get(&server->channels, target, -1);
	else
		chan = chan_get(&server->privs, target, -1);

	serv_write(server, "PRIVMSG %s :%s\r\n", target, message);
	if (chan) {
		hist_format(chan->history, Activity_self,
				HIST_SHOW|HIST_LOG|HIST_SELF, "PRIVMSG %s :%s", target, message);
	}
}

COMMAND(
command_notice) {
	struct Channel *chan = NULL;
	char *target, *message;

	if (!str) {
		command_toofew("notice");
		return;
	}

	target = strtok_r(str, " ", &message);

	if (serv_ischannel(server, target))
		chan = chan_get(&server->channels, target, -1);
	else
		chan = chan_get(&server->privs, target, -1);

	serv_write(server, "NOTICE %s :%s\r\n", target, message);
	if (chan) {
		hist_format(chan->history, Activity_self,
				HIST_SHOW|HIST_LOG|HIST_SELF, "NOTICE %s :%s", target, message);
	}
}

COMMAND(
command_me) {
	if (!str)
		str = "";

	serv_write(server, "PRIVMSG %s :%cACTION %s%c\r\n", channel->name, 1, str, 1);
	hist_format(channel->history, Activity_self,
			HIST_SHOW|HIST_LOG|HIST_SELF, "PRIVMSG %s :%cACTION %s%c", channel->name, 1, str, 1);
}

COMMAND(
command_ctcp) {
	struct Channel *chan;
	char *target, *ctcp;

	if (!str) {
		command_toofew("ctcp");
		return;
	}

	target = strtok_r(str, " ", &ctcp);

	if (!ctcp) {
		ctcp = target;
		target = channel->name;
	}

	if ((chan = chan_get(&server->channels, target, -1)) == NULL)
		chan = chan_get(&server->privs, target, -1);

	/* XXX: if we CTCP a channel, responses should go to that channel.
	 * This requires more than just expect_set, so might never be
	 * implemented. */
	serv_write(server, "PRIVMSG %s :%c%s%c\r\n", target, 1, ctcp, 1);
	if (chan) {
		hist_format(channel->history, Activity_self,
				HIST_SHOW|HIST_LOG|HIST_SELF, "PRIVMSG %s :%c%s%c",
				target, 1, ctcp, 1);
	}
}

COMMAND(
command_query) {
	struct Channel *priv;

	if (!str) {
		command_toofew("query");
		return;
	}

	if (strchr(str, ' ')) {
		command_toomany("query");
		return;
	}

	if (serv_ischannel(server, str)) {
		ui_error("can't query a channel", NULL);
		return;
	}

	if ((priv = chan_get(&server->privs, str, -1)) == NULL)
		priv = chan_add(server, &server->privs, str, 1);

	if (!nouich)
		ui_select(server, priv);
}

COMMAND(
command_quit) {
	cleanup(str ? str : config_gets("def.quitmessage"));
	exit(EXIT_SUCCESS);
}

COMMAND(
command_join) {
	char msg[512];

	if (!str) {
		command_toofew("join");
		return;
	}

	if (serv_ischannel(server, str))
		snprintf(msg, sizeof(msg), "JOIN %s\r\n", str);
	else
		snprintf(msg, sizeof(msg), "JOIN %c%s\r\n", '#', str);

	if (server->status == ConnStatus_connected)
		serv_write(server, "%s", msg);
	else
		schedule_push(server, "376" /* RPL_ENDOFMOTD */, msg);

	/* Perhaps we should update expect from schedule?
	 * That'd make more sense if different stuff gets
	 * scheduled for events that happen at different times
	 *
	 * Actually, I think that would be a bad idea if schedule gets opened
	 * up to the user. Don't want automatic events triggering ui changes. */
	expect_set(server, Expect_join, str);
}

COMMAND(
command_part) {
	char *chan = NULL, *reason = NULL;
	char msg[512];

	if (str) {
		if (serv_ischannel(server, str))
			chan = strtok_r(str, " ", &reason);
		else
			reason = str;
	}

	if (!chan) {
		if (channel) {
			chan = channel->name;
		} else {
			command_toofew("part");
			return;
		}
	}

	snprintf(msg, sizeof(msg), "PART %s :%s\r\n", chan, reason ? reason : config_gets("def.partmessage"));

	serv_write(server, "%s", msg);
	expect_set(server, Expect_part, chan);
}

COMMAND(
command_cycle) {
	char *chan = NULL;

	if (str && serv_ischannel(server, str))
		chan = strtok(str, " ");
	if (!chan && channel) {
		chan = channel->name;
	} else if (!chan) {
		command_toofew("cycle");
		return;
	}

	command_part(server, channel, str);
	command_join(server, channel, chan);
}

COMMAND(
command_kick) {
	char *chan, *nick, *reason;
	char *s;

	if (!str) {
		command_toofew("kick");
		return;
	}

	s = strtok_r(str,  " ", &reason);

	if (serv_ischannel(server, s)) {
		chan = s;
		nick = strtok_r(NULL, " ", &reason);
	} else {
		if (channel == NULL) {
			command_needselected("kick", "channel");
			return;
		}

		chan = channel->name;
		nick = s;
	}

	if (reason)
		serv_write(server, "KICK %s %s :%s\r\n", chan, nick, reason);
	else
		serv_write(server, "KICK %s %s\r\n", chan, nick);
}

COMMAND(
command_mode) {
	char *chan, *modes;
	char *s = NULL;

	if (str)
		s = strtok_r(str,  " ", &modes);

	if (serv_ischannel(server, s)) {
		chan = s;
	} else {
		if (channel == NULL) {
			command_needselected("mode", "channel");
			return;
		}

		chan = channel->name;
		if (modes) {
			*(modes - 1) = ' ';
			modes = s;
		}
	}

	if (modes) {
		if (chan == channel->name)
			expect_set(server, Expect_nosuchnick, chan);
		serv_write(server, "MODE %s %s\r\n", chan, modes);
	} else {
		expect_set(server, Expect_channelmodeis, chan);
		serv_write(server, "MODE %s\r\n", chan);
	}
}

COMMAND(
command_nick) {
	if (!str) {
		command_toofew("nick");
		return;
	}

	if (strchr(str, ' ')) {
		command_toomany("nick");
		return;
	}

	serv_write(server, "NICK %s\r\n", str);
	expect_set(server, Expect_nicknameinuse, str);
}

COMMAND(
command_list) {
	if (str) {
		command_toomany("list");
		return;
	}

	serv_write(server, "LIST\r\n", str);
}

COMMAND(
command_whois) {
	char *tserver, *nick;

	if (!str) {
		nick = server->self->nick;
		tserver = NULL;
	} else {
		tserver = strtok_r(str, " ", &nick);
		if (!nick || !*nick) {
			nick = tserver;
			tserver = NULL;
		}
	}

	if (tserver)
		serv_write(server, "WHOIS %s :%s\r\n", tserver, nick);
	else
		serv_write(server, "WHOIS %s\r\n", nick);
}

COMMAND(
command_who) {
	if (!str)
		str = "*"; /* wildcard */

	serv_write(server, "WHO %s\r\n", str);
}

COMMAND(
command_whowas) {
	char *nick, *count, *tserver;

	if (!str) {
		nick = server->self->nick;
		count = tserver = NULL;
	} else {
		nick = strtok_r(str, " ", &tserver);
		count = strtok_r(NULL, " ", &tserver);
	}

	if (tserver)
		serv_write(server, "WHOWAS %s %s :%s\r\n", nick, count, tserver);
	else if (count)
		serv_write(server, "WHOWAS %s %s\r\n", nick, count);
	else
		serv_write(server, "WHOWAS %s 5\r\n", nick);
}

COMMAND(
command_ping) {
	if (!str) {
		command_toofew("ping");
		return;
	}

	serv_write(server, "PING :%s\r\n", str);
	expect_set(server, Expect_pong, str);
}

COMMAND(
command_quote) {
	char msg[512];

	if (!str) {
		command_toofew("quote");
		return;
	}

	if (server->status == ConnStatus_connected) {
		serv_write(server, "%s\r\n", str);
	} else {
		snprintf(msg, sizeof(msg), "%s\r\n", str);
		schedule_push(server, "376" /* RPL_ENDOFMOTD */, msg);
	}
}

COMMAND(
command_connect) {
	struct Server *tserver;
	char *network	= NULL;
	char *host	= NULL;
	char *port	= NULL;
	char *nick	= NULL;
	char *username	= NULL;
	char *realname	= NULL;
	char *password  = NULL;
	int tls = -1, tls_verify = -1; /* tell serv_update not to change */
	int ret;
	struct passwd *user;
	enum {
		opt_network,
		opt_nick,
		opt_username,
		opt_realname,
		opt_password,
#ifdef TLS
		opt_tls,
		opt_tls_verify,
#endif /* TLS */
	};
	static struct CommandOpts opts[] = {
		{"network", CMD_ARG, opt_network},
		{"nick", CMD_ARG, opt_nick},

		{"username", CMD_ARG, opt_username},
		{"user", CMD_ARG, opt_username},

		{"realname", CMD_ARG, opt_realname},
		{"real", CMD_ARG, opt_realname},
		{"comment", CMD_ARG, opt_realname},

		{"pass", CMD_ARG, opt_password},
		{"password", CMD_ARG, opt_password},
		{"auth", CMD_ARG, opt_password},
#ifdef TLS
		{"tls", CMD_NARG, opt_tls},
		{"ssl", CMD_NARG, opt_tls},
		{"verify", CMD_NARG, opt_tls_verify},
#endif /* TLS */
		{NULL, 0, 0},
	};

	while ((ret = command_getopt(&str, opts)) != opt_done) {
		switch (ret) {
		case opt_error:
			return;
		case opt_network:
			network = command_optarg;
			break;
		case opt_nick:
			nick = command_optarg;
			break;
		case opt_username:
			username = command_optarg;
			break;
		case opt_realname:
			realname = command_optarg;
			break;
		case opt_password:
			password = command_optarg;
			break;
#ifdef TLS
		case opt_tls:
			tls = 1;
			break;
		case opt_tls_verify:
			tls_verify = 1;
			break;
#endif /* TLS */
		}
	}

	host = strtok(str,  " ");
	port = strtok(NULL, " ");

	if (!host) {
		if (network) {
			if (!(tserver = serv_get(&servers, network)))
				ui_error("no such network", NULL);
		} else {
			if (!server)
				ui_error("must specify host", NULL);
			else
				tserver = server;
		}
		if (server) {
			serv_update(tserver, nick, username, realname, password, tls, tls_verify);
			serv_connect(tserver);
		}
		return;
	}

	if (tls <= 0)
		tls = 0;
	if (tls_verify <= 0)
		tls_verify = 0;

	if (!nick && !(nick = config_gets("def.nick"))) {
		user = getpwuid(geteuid());
		nick = user ? user->pw_name : "null";
	}
	if (!username && !(username = config_gets("def.user")))
		username = nick;
	if (!realname && !(realname = config_gets("def.real")))
		realname = nick;
	if (!network)
		network = host;
	if (!port) {
		/* no ifdef required, as -tls only works with -DTLS */
		if (tls)
			port = "6697";
		else
			port = "6667";
	}

	tserver = serv_add(&servers, network, host, port, nick,
			username, realname, password, tls, tls_verify);
	serv_connect(tserver);
	if (!nouich)
		ui_select(tserver, NULL);
	return;
}

COMMAND(
command_disconnect) {
	struct Server *sp;
	struct Channel *chp;
	int len;
	char *msg = NULL;

	if (str) {
		len = strcspn(str, " ");
		for (sp = servers; sp; sp = sp->next) {
			if (strlen(sp->name) == len && strncmp(sp->name, str, len) == 0) {
				msg = strchr(str, ' ');
				if (msg && *msg)
					msg++;
				break;
			}
		}

		if (sp == NULL) {
			sp = server;
			msg = str;
		}
	} else sp = server;

	if (!msg || !*msg)
		msg = config_gets("def.quitmessage");

	/* Add fake quit messages to history.
	 * Theoretically, we could send QUIT and then wait for a
	 * reply, but I don't see the advantage. (Unless the
	 * server fucks with our quit messages somehow).
	 *
	 * Since HIST_SELF is used, there is no need to fake a prefix. */
	hist_format(sp->history, Activity_self, HIST_DFL|HIST_SELF, "QUIT :%s", msg);
	for (chp = sp->channels; chp; chp = chp->next)
		hist_format(chp->history, Activity_self, HIST_DFL|HIST_SELF, "QUIT :%s", msg);
	serv_disconnect(sp, 0, msg);
}

COMMAND(
command_select) {
	struct Server *sp;
	struct Channel *chp;
	char *tserver = NULL;
	char *tchannel = NULL;
	int ret, buf = 0;
	enum {
		opt_server,
		opt_channel,
		opt_test,
	};
	static struct CommandOpts opts[] = {
		{"server", CMD_ARG, opt_server},
		{"network", CMD_ARG, opt_server},
		{"channel", CMD_ARG, opt_channel},
		{NULL, 0, 0},
	};

	while ((ret = command_getopt(&str, opts)) != opt_done) {
		switch (ret) {
		case opt_error:
			return;
		case opt_server:
			tserver = command_optarg;
			break;
		case opt_channel:
			tchannel = command_optarg;
			break;
		}
	}

	if (tserver || tchannel) {
		/* TODO: find closest match instead of perfect matches */
		if (!tserver) {
			ui_error("must specify server and channel, or just server", NULL);
			return;
		}

		if (!(sp = serv_get(&servers, tserver))) {
			ui_error("could not find server '%s'", tserver);
			return;
		}

		if (tchannel) {
			for (chp = sp->channels; chp; chp = chp->next)
				if (strcmp(chp->name, tchannel) == 0)
					break;

			if (!chp) {
				ui_error("could not find channel '%s'", tchannel);
				return;
			}
		} else chp = NULL;

		ui_select(sp, chp);

		if (str)
			ui_error("ignoring trailing arguments: '%s'", str);
	} else if (str) {
		buf = atoi(str);
		if (!buf)
			ui_error("invalid buffer index: '%s'", str);
		if (ui_buflist_get(buf, &sp, &chp) != -1)
			ui_select(sp, chp);
	} else {
		command_toofew("select");
	}
}

COMMAND(
command_set) {
	char *name, *val;

	if (!str) {
		command_toofew("set");
		return;
	}
	name = strtok_r(str, " ", &val);
	config_set(name, val);
}

COMMAND(
command_format) {
	char *newstr;
	int len;

	if (!str) {
		command_toofew("format");
		return;
	}

	len = strlen(str) + CONSTLEN("format.") + 1;
	newstr = emalloc(len);
	snprintf(newstr, len, "format.%s", str);
	command_set(server, channel, newstr);
	pfree(&newstr);
}

COMMAND(
command_server) {
	struct Server *nserver;
	char *tserver, *cmd, *arg;
	char **acmds;
	int i, ret, mode;
	enum { opt_norm, opt_auto, opt_clear };
	struct CommandOpts opts[] = {
		{"auto", CMD_NARG, opt_auto},
		{"clear", CMD_NARG, opt_clear},
		{NULL, 0, 0},
	};

	mode = opt_norm;
	while ((ret = command_getopt(&str, opts)) != opt_done) {
		switch (ret) {
		case opt_error:
			return;
		case opt_auto:
		case opt_clear:
			if (mode != opt_norm) {
				ui_error("conflicting flags", NULL);
				return;
			}
			mode = ret;
		}
	}

	tserver = strtok_r(str,  " ", &arg);
	if (mode == opt_norm)
		cmd     = strtok_r(NULL, " ", &arg);

	if (!tserver) {
		command_toofew("server");
		return;
	}

	if ((nserver = serv_get(&servers, tserver)) == NULL) {
		ui_error("no such server: '%s'", tserver);
		return;
	}

	switch (mode) {
	case opt_norm:
		if (!cmd || !*cmd) {
			command_toofew("server");
			return;
		}

		if (*cmd == '/')
			cmd++;

		for (i=0; commands[i].name && commands[i].func; i++) {
			if (strcmp(commands[i].name, cmd) == 0) {
				commands[i].func(nserver, channel, arg);
				return;
			}
		}
		ui_error("no such commands: '%s'", cmd);
		break;
	case opt_auto:
		if (!arg || !*arg) {
			hist_format(selected.history, Activity_none, HIST_UI, "SELF_AUTOCMDS_START %s :Autocmds for %s:",
					nserver->name, nserver->name);
			for (acmds = nserver->autocmds; acmds && *acmds; acmds++)
				hist_format(selected.history, Activity_none, HIST_UI, "SELF_AUTOCMDS_LIST %s :%s",
						nserver->name, *acmds);
			hist_format(selected.history, Activity_none, HIST_UI, "SELF_AUTOCMDS_END %s :End of autocmds for %s",
					nserver->name, nserver->name);
		} else {
			if (*arg == '/') {
				cmd = arg;
			} else {
				cmd = emalloc(strlen(arg) + 2);
				snprintf(cmd, strlen(arg) + 2, "/%s", arg);
			}

			serv_auto_add(nserver, cmd);
		}
		break;
	case opt_clear:
		if (*arg) {
			command_toomany("server");
			break;
		}

		serv_auto_free(nserver);
		break;
	}
}

COMMAND(
command_names) {
	char *chan, *save = NULL;

	chan = strtok_r(str, " ", &save);
	if (!chan)
		chan = channel ? channel->name : NULL;

	if (!chan) {
		command_needselected("names", "channel");
		return;
	}

	if (save && *save) {
		command_toomany("names");
		return;
	}

	serv_write(server, "NAMES %s\r\n", chan);
	expect_set(server, Expect_names, chan);
}

COMMAND(
command_topic) {
	char *chan, *topic = NULL;
	int clear = 0, ret;
	enum { opt_clear, };
	struct CommandOpts opts[] = {
		{"clear", CMD_NARG, opt_clear},
		{NULL, 0, 0},
	};

	while ((ret = command_getopt(&str, opts)) != opt_done) {
		switch (ret) {
		case opt_error:
			return;
		case opt_clear:
			clear = 1;
			break;
		}
	}

	if (str)
		chan = strtok_r(str,  " ", &topic);
	else
		chan = topic = NULL;

	if (chan && !serv_ischannel(server, chan)) {
		topic = chan;
		chan = NULL;
	}

	if (!chan && channel) {
		chan = channel->name;
	} else if (!chan) {
		command_needselected("topic", "channel");
		return;
	}

	if (clear) {
		if (topic) {
			command_toomany("topic");
			return;
		}
		serv_write(server, "TOPIC %s :\r\n", chan);
		return;
	}

	if (!topic) {
		serv_write(server, "TOPIC %s\r\n", chan);
		expect_set(server, Expect_topic, chan);
	} else serv_write(server, "TOPIC %s :%s\r\n", chan, topic);
}

COMMAND(
command_oper) {
	char *user, *pass;

	if (!str) {
		command_toofew("oper");
		return;
	}

	user = strtok_r(str, " ", &pass);
	if (pass && strchr(pass, ' ')) {
		command_toomany("oper");
		return;
	}
	if (!pass) {
		pass = user;
		user = server->self->nick;
	}

	serv_write(server, "OPER %s %s\r\n", user, pass);
}

static void
command_send0(struct Server *server, char *cmd, char *cmdname, char *str) {
	if (str)
		command_toomany(cmdname);
	else
		serv_write(server, "%s\r\n", cmd);
}

COMMAND(
command_lusers) {
	command_send0(server, "LUSERS", "lusers", str);
}

COMMAND(
command_map) {
	command_send0(server, "MAP", "map", str);
}

static void
command_send1(struct Server *server, char *cmd, char *cmdname, char *str) {
	if (str && strchr(str, ' '))
		command_toomany(cmdname);
	else if (str)
		serv_write(server, "%s %s\r\n", cmd, str);
	else
		serv_write(server, "%s\r\n", cmd);
}

COMMAND(
command_motd) {
	command_send1(server, "MOTD", "motd", str);
}

COMMAND(
command_time) {
	command_send1(server, "TIME", "time", str);
}

static void
command_send2(struct Server *server, char *cmd, char *cmdname, char *str) {
	if (str && strchr(str, ' ') != strrchr(str, ' '))
		command_toomany(cmdname);
	else if (str)
		serv_write(server, "%s %s\r\n", cmd, str);
	else
		serv_write(server, "%s\r\n", cmd);
}

COMMAND(
command_links) {
	command_send2(server, "LINKS", "links", str);
}

COMMAND(
command_stats) {
	command_send2(server, "STATS", "stats", str);
}

COMMAND(
command_kill) {
	char *nick, *reason;

	if (!str) {
		command_toofew("kill");
		return;
	}

	nick = strtok_r(str, " ", &reason);
	if (!reason)
		reason = config_gets("def.killmessage");
	serv_write(server, "KILL %s :%s\r\n", nick, reason);
}

COMMAND(
command_bind) {
	struct Keybind *p;
	char *binding = NULL, *cmd = NULL;
	int delete = 0, ret;
	enum { opt_delete, };
	struct CommandOpts opts[] = {
		{"delete", CMD_NARG, opt_delete},
		{NULL, 0, 0},
	};

	while ((ret = command_getopt(&str, opts)) != opt_done) {
		switch (ret) {
		case opt_error:
			return;
		case opt_delete:
			delete = 1;
			break;
		}
	}

	if (str)
		binding = strtok_r(str, " ", &cmd);

	if (delete) {
		if (ui_unbind(binding) == -1)
			ui_error("no such keybind: '%s'", binding);
		return;
	}

	if (!binding) {
		hist_format(selected.history, Activity_none, HIST_UI, "SELF_KEYBIND_START :Keybindings:");
		for (p = keybinds; p; p = p->next)
			hist_format(selected.history, Activity_none, HIST_UI, "SELF_KEYBIND_LIST %s :%s", ui_unctrl(p->binding), p->cmd);
		hist_format(selected.history, Activity_none, HIST_UI, "SELF_KEYBIND_END :End of keybindings");
	} else if (!cmd) {
		for (p = keybinds; p; p = p->next) {
			if (strcmp(p->binding, binding) == 0) {
				hist_format(selected.history, Activity_none, HIST_UI, "SELF_KEYBIND_START :Keybindings:");
				hist_format(selected.history, Activity_none, HIST_UI, "SELF_KEYBIND_LIST %s :%s", ui_unctrl(p->binding), p->cmd);
				hist_format(selected.history, Activity_none, HIST_UI, "SELF_KEYBIND_END :End of keybindings");
				return;
			}
		}

		ui_error("no such keybind: '%s'", binding);
	} else {
		if (ui_bind(binding, cmd) == -1)
			ui_error("keybind already exists: '%s'", binding);
	}
}

COMMAND(
command_alias) {
	struct Alias *p;
	char *alias = NULL, *cmd = NULL;
	int delete = 0, ret;
	enum { opt_delete, };
	struct CommandOpts opts[] = {
		{"delete", CMD_NARG, opt_delete},
		{NULL, 0, 0},
	};

	while ((ret = command_getopt(&str, opts)) != opt_done) {
		switch (ret) {
		case opt_error:
			return;
		case opt_delete:
			delete = 1;
			break;
		}
	}

	if (str)
		alias = strtok_r(str, " ", &cmd);

	if (delete) {
		if (alias_remove(alias) == -1)
			ui_error("no such alias: '%s'", alias);
		return;
	}

	if (!alias) {
		hist_format(selected.history, Activity_none, HIST_UI, "SELF_ALIAS_START :Aliases:");
		for (p = aliases; p; p = p->next)
			hist_format(selected.history, Activity_none, HIST_UI, "SELF_ALIAS_LIST %s :%s", p->alias, p->cmd);
		hist_format(selected.history, Activity_none, HIST_UI, "SELF_ALIAS_END :End of aliases");
	} else if (!cmd) {
		for (p = aliases; p; p = p->next) {
			if (strcmp(p->alias, alias) == 0) {
				hist_format(selected.history, Activity_none, HIST_UI, "SELF_ALIAS_START :Aliases:");
				hist_format(selected.history, Activity_none, HIST_UI, "SELF_ALIAS_LIST %s :%s", p->alias, p->cmd);
				hist_format(selected.history, Activity_none, HIST_UI, "SELF_ALIAS_END :End of aliases");
				return;
			}
		}

		ui_error("no such alias: '%s'", alias);
	} else {
		if (alias_add(alias, cmd) == -1)
			ui_error("alias already exists: '%s'", alias);
	}
}


COMMAND(
command_help) {
	char *p;
	int cmdonly = 0;
	int found = 0;
	int i, j;

	if (!str) {
		command_help(server, channel, "/help");
		return;
	}

	p = strrchr(str, ' ');
	if (p && *(p+1) == '\0')
		*p = '\0';

	if (strcmp(str, "commands") == 0) {
		hist_format(selected.history, Activity_none, HIST_UI, "SELF_HELP_START :%s", str);
		for (i=0; commands[i].name && commands[i].func; i++)
			hist_format(selected.history, Activity_none, HIST_UI, "SELF_HELP : /%s", commands[i].name);
		hist_format(selected.history, Activity_none, HIST_UI, "SELF_HELP_END :end of help");
		return;
	}

	if (strcmp(str, "variables") == 0) {
		hist_format(selected.history, Activity_none, HIST_UI, "SELF_HELP_START :%s", str);
		for (i=0; config[i].name; i++)
			hist_format(selected.history, Activity_none, HIST_UI, "SELF_UI : %s", config[i].name);
		hist_format(selected.history, Activity_none, HIST_UI, "SELF_HELP_END :end of help");
		return;
	}

	if (*str == '/') {
		cmdonly = 1;
		str++;
	}

	for (i=0; commands[i].name && commands[i].func; i++) {
		if (strncmp(commands[i].name, str, strlen(str)) == 0) {
			found = 1;
			hist_format(selected.history, Activity_none, HIST_UI, "SELF_HELP_START :%s", commands[i].name);
			for (j=0; commands[i].description[j]; j++)
				hist_format(selected.history, Activity_none, HIST_UI, "SELF_HELP :%s", commands[i].description[j]);
			if (strcmp(commands[i].name, str) == 0)
				goto end; /* only print one for an exact match, i,e, /help format should only print the command, not all formats. */
		}
	}

	if (!cmdonly) {
		for (i=0; config[i].name; i++) {
			if (strncmp(config[i].name, str, strlen(str)) == 0) {
				found = 1;
				hist_format(selected.history, Activity_none, HIST_UI, "SELF_HELP_START :%s", config[i].name);
				for (j=0; config[i].description[j]; j++)
					hist_format(selected.history, Activity_none, HIST_UI, "SELF_HELP :%s", config[i].description[j]);
				if (strcmp(config[i].name, str) == 0)
					goto end;
			}
		}
	}

end:
	if (found)
		hist_format(selected.history, Activity_none, HIST_UI, "SELF_HELP_END :end of help");
	else
		ui_error("no help on '%s'", str);
}

COMMAND(
command_echo) {
	if (!str)
		str = "";

	hist_format(selected.history, Activity_none, HIST_SHOW|HIST_TMP, "SELF_UI :%s", str);
}

COMMAND(
command_grep) {
	struct History *p;
	regex_t re;
	size_t i;
	int regopt = 0, ret, raw = 0;
	char errbuf[1024], *s;
	enum { opt_extended, opt_icase, opt_raw };
	static struct CommandOpts opts[] = {
		{"E", CMD_NARG, opt_extended},
		{"i", CMD_NARG, opt_icase},
		{"raw", CMD_NARG, opt_raw},
		{NULL, 0, 0},
	};

	hist_purgeopt(selected.history, HIST_GREP);
	windows[Win_main].refresh = 1;
	if (!str) {
		return;
	}

	while ((ret = command_getopt(&str, opts)) != opt_done) {
		switch (ret) {
		case opt_error:
			return;
		case opt_extended:
			regopt |= REG_EXTENDED;
			break;
		case opt_icase:
			regopt |= REG_ICASE;
			break;
		case opt_raw:
			raw = 1;
			break;
		}
	}

	if (config_getl("regex.extended"))
		regopt |= REG_EXTENDED;
	if (config_getl("regex.icase"))
		regopt |= REG_ICASE;

	if ((ret = regcomp(&re, str, regopt)) != 0) {
		regerror(ret, &re, errbuf, sizeof(errbuf));
		regfree(&re);
		ui_error("unable to compile regex '%s': %s", str, errbuf);
		return;
	}

	hist_format(selected.history, Activity_none, HIST_SHOW|HIST_TMP|HIST_GREP, "SELF_GREP_START :%s", str);

	/* Get oldest, but don't set p to NULL */
	for (p = selected.history->history; p && p->next; p = p->next);

	/* Traverse until we hit a message already generated by /grep */
	for (; p && !(p->options & HIST_GREP); p = p->prev) {
		if (!raw && !p->format)
			p->format = estrdup(format(&windows[Win_main], NULL, p));
		if (!raw && !p->rformat) {
			p->rformat = emalloc(strlen(p->format) + 1);
			/* since only one or zero characters are added to
			 * rformat for each char in format, and both are the
			 * same sie, there is no need to expand rformat or
			 * truncate it. */
			for (i = 0, s = p->format; s && *s; s++) {
				switch (*s) {
				case 2: case 9: case 15: case 18: case 21:
					break;
				case 3:
					if (*s && isdigit(*(s+1)))
						s += 1;
					if (*s && isdigit(*(s+1)))
						s += 1;
					if (*s && *(s+1) == ',' && isdigit(*(s+2)))
						s += 2;
					else
						break; /* break here to avoid needing to check back for comma in next if */
					if (isdigit(*(s+1)))
						s += 1;
					break;
				case '\n': case ' ':
					while (*(s+1) == ' ')
						s++;
					/* fallthrough */
				default:
					p->rformat[i++] = *s != '\n' ? *s : ' ';
				}
			}
		}
		if (regexec(&re, raw ? p->raw : p->rformat, 0, NULL, 0) == 0)
			hist_addp(selected.history, p, p->activity, p->options | HIST_GREP | HIST_TMP);
	}

	hist_format(selected.history, Activity_none, HIST_SHOW|HIST_TMP|HIST_GREP, "SELF_GREP_END :end of /grep command");
}

COMMAND(
command_clear) {
	int ret, cleared = 0;
	enum { opt_tmp, opt_err, opt_serr, opt_log };
	static struct CommandOpts opts[] = {
		{"tmp", CMD_NARG, opt_tmp},
		{"err", CMD_NARG, opt_err},
		{"serr", CMD_NARG, opt_serr},
		{"log", CMD_NARG, opt_log},
		{NULL, 0, 0},
	};

	if (str) {
		while ((ret = command_getopt(&str, opts)) != opt_done) {
			switch (ret) {
			case opt_error:
				return;
			case opt_tmp:
				hist_purgeopt(selected.history, HIST_TMP);
				cleared = 1;
				break;
			case opt_err:
				hist_purgeopt(selected.history, HIST_ERR);
				cleared = 1;
				break;
			case opt_serr:
				hist_purgeopt(selected.history, HIST_SERR);
				cleared = 1;
				break;
			case opt_log:
				hist_purgeopt(selected.history, HIST_RLOG);
				cleared = 1;
				break;
			}
		}

		if (*str) {
			command_toomany("clear");
			return;
		}
	}

	if (!cleared)
		hist_purgeopt(selected.history, HIST_ALL);
	windows[Win_main].refresh = 1;
}

COMMAND(
command_scroll) {
	int ret, winid = Win_main;
	long diff;
	enum { opt_buflist, opt_nicklist };
	static struct CommandOpts opts[] = {
		{"buflist", CMD_NARG, opt_buflist},
		{"nicklist", CMD_NARG, opt_nicklist},
		{NULL, 0, 0},
	};

	if (!str)
		goto narg;

	while (!(*str == '-' && isdigit(*(str+1))) && (ret = command_getopt(&str, opts)) != opt_done) {
		switch (ret) {
		case opt_error:
			return;
		case opt_buflist:
			winid = Win_buflist;
			break;
		case opt_nicklist:
			winid = Win_nicklist;
			break;
		}

		if (*str == '-' && isdigit(*(str+1)))
			break;
	}

	if (!*str)
		goto narg;

	diff = strtol(str, NULL, 10);
	if (diff == 0 || diff == LONG_MIN)
		windows[winid].scroll = -1; /* no scroll, tracking */
	else if (windows[winid].scroll >= 0)
		windows[winid].scroll += diff;
	else
		windows[winid].scroll = diff;

	windows[winid].refresh = 1;
	return;

narg:
	command_toofew("scroll");
}

COMMAND(
command_source) {
	char *p;
	if (!str) {
		command_toofew("source");
		return;
	}
	p = strrchr(str, ' ');
	if (p && *(p+1) == '\0')
		*p = '\0'; /* remove trailing spaces */
	config_read(str);
}

COMMAND(
command_dump) {
	FILE *file;
	int selected = 0;
	int def = 0, ret;
	int i;
	char **aup, *p;
	struct Server *sp;
	struct Channel *chp;
	struct Alias *ap;
	struct Keybind *kp;
	struct Ignore *ip;
	enum {
		opt_aliases = 1,
		opt_bindings = 2,
		opt_formats = 4,
		opt_config = 8,
		opt_servers = 16,
		opt_channels = 32,
		opt_queries = 64,
		opt_autocmds = 128,
		opt_ignores = 256,
		opt_default = 512,
	};
	static struct CommandOpts opts[] = {
		{"aliases", CMD_NARG, opt_aliases},
		{"bindings", CMD_NARG, opt_bindings},
		{"formats", CMD_NARG, opt_formats},
		{"config", CMD_NARG, opt_config},
		{"servers", CMD_NARG, opt_servers},
		{"autocmds", CMD_NARG, opt_autocmds},
		{"channels", CMD_NARG, opt_channels},
		{"queries", CMD_NARG, opt_queries},
		{"ignores", CMD_NARG, opt_ignores},
		{"default", CMD_NARG, opt_default},
		{NULL, 0, 0},
	};

	while ((ret = command_getopt(&str, opts)) != opt_done) {
		switch (ret) {
		case opt_error:
			return;
		case opt_aliases:
		case opt_bindings:
		case opt_formats:
		case opt_config:
		case opt_servers:
		case opt_channels:
		case opt_autocmds:
		case opt_ignores:
			selected |= ret;
			break;
		case opt_default:
			def = 1;
			break;
		}
	}

	if (!selected)
		selected = opt_default - 1;

	if (!str || !*str) {
		command_toofew("dump");
		return;
	}
	p = strrchr(str, ' ');
	if (p && *(p+1) == '\0')
		*p = '\0';

	if ((file = fopen(str, "wb")) == NULL) {
		ui_error("cannot open file '%s': %s", str, strerror(errno));
		return;
	}

	if (selected & (opt_servers|opt_channels|opt_queries|opt_autocmds) && servers) {
		if (selected & opt_servers)
			fprintf(file, "Network connections\n");

		for (sp = servers; sp; sp = sp->next) {
			if (selected & opt_servers) {
				fprintf(file, "/connect -network %s ", sp->name);
				if (strcmp_n(sp->self->nick, config_gets("def.nick")) != 0)
					fprintf(file, "-nick %s ", sp->self->nick);
				if (strcmp_n(sp->username, config_gets("def.user")) != 0)
					fprintf(file, "-user %s ", sp->username);
				if (strcmp_n(sp->realname, config_gets("def.real")) != 0)
					fprintf(file, "-real %s ", sp->realname);
#ifdef TLS
				if (sp->tls)
					fprintf(file, "-tls ");
#endif /* TLS */
				fprintf(file, "%s %s\n", sp->host, sp->port);
			}
			if (selected & opt_autocmds) {
				for (aup = sp->autocmds; *aup; aup++)
					fprintf(file, "/server -auto %s %s\n", sp->name, *aup);
			}
			if (selected & opt_channels) {
				for (chp = sp->channels; chp; chp = chp->next)
					if (!(selected & opt_autocmds) || !serv_auto_haschannel(sp, chp->name))
						fprintf(file, "/server %s /join %s\n", sp->name, chp->name);
			}
			if (selected & opt_queries) {
				for (chp = sp->privs; chp; chp = chp->next)
					fprintf(file, "/server %s /query %s\n", sp->name, chp->name);
			}
			fprintf(file, "\n");
		}
	}

	if (selected & opt_aliases && aliases) {
		fprintf(file, "Aliases\n");
		for (ap = aliases; ap; ap = ap->next)
			fprintf(file, "/alias %s %s\n", ap->alias, ap->cmd);
		fprintf(file, "\n");
	}

	if (selected & opt_bindings && keybinds) {
		fprintf(file, "Keybindings\n");
		for (kp = keybinds; kp; kp = kp->next)
			fprintf(file, "/bind %s %s\n", ui_unctrl(kp->binding), kp->cmd);
		fprintf(file, "\n");
	}

	if (selected & opt_formats || selected & opt_config) {
		fprintf(file, "Configuration variables\n");
		for (i = 0; config[i].name; i++) {
			if (!config[i].isdef || def) {
				if (selected & opt_formats && strncmp(config[i].name, "format.", CONSTLEN("format.")) == 0) {
					fprintf(file, "/format %s %s\n", config[i].name + CONSTLEN("format."), config[i].str);
				} else if (selected & opt_config && strncmp(config[i].name, "format.", CONSTLEN("format.")) != 0) {
					fprintf(file, "/set %s %s\n", config[i].name, config_get_pretty(&config[i], 0));
				}
			}
		}
		fprintf(file, "\n");
	}

	if (selected & opt_ignores && ignores) {
		fprintf(file, "Ignore rules\n");
		for (ip = ignores; ip; ip = ip->next) {
			if (ip->server)
				fprintf(file, "/server %s /ignore -server ", ip->server);
			else
				fprintf(file, "/ignore ");

			if (ip->format)
				fprintf(file, "-format %s ", ip->format);
			if (ip->regopt & REG_EXTENDED)
				fprintf(file, "-E ");
			if (ip->regopt & REG_ICASE)
				fprintf(file, "-i ");

			fprintf(file, "%s\n", ip->text);
		}
		fprintf(file, "\n");
	}

	fclose(file);
}

COMMAND(
command_close) {
	struct Server *sp;
	struct Channel *chp;
	int buf;

	if (str) {
		buf = atoi(str);
		if (!buf)
			ui_error("invalid buffer index: '%s'", str);
		if (ui_buflist_get(buf, &sp, &chp) == -1)
			return;
	} else {
		sp = selected.server;
		chp = selected.channel;
	}

	if (!sp) {
		ui_error("cannot close main buffer", NULL);
		return;
	}

	if (chp) {
		if (serv_ischannel(sp, chp->name) && !chp->old) {
			serv_write(sp, "PART %s\r\n", chp->name);
			chan_remove(&sp->channels, chp->name);
		} else {
			chan_remove(&sp->privs, chp->name);
		}
		ui_select(sp, NULL);
	} else {
		if (sp->status != ConnStatus_notconnected) {
			ui_error("can't close connected server", NULL);
			return;
		} else {
			serv_remove(&servers, sp->name);
		}
		ui_select(NULL, NULL);
	}
}

COMMAND(
command_ignore) {
	struct Ignore *ign, *p;
	char errbuf[BUFSIZ], *format = NULL;
	size_t len;
	long id;
	int ret, noact = 0, i, regopt = 0, serv = 0;
	enum { opt_show, opt_hide, opt_extended, opt_icase,
		opt_server, opt_delete, opt_format, opt_noact };
	static struct CommandOpts opts[] = {
		{"E", CMD_NARG, opt_extended},
		{"i", CMD_NARG, opt_icase},
		{"show", CMD_NARG, opt_show},
		{"hide", CMD_NARG, opt_hide},
		{"server", CMD_NARG, opt_server},
		{"noact", CMD_NARG, opt_noact},
		{"delete", CMD_NARG, opt_delete},
		{"format", CMD_ARG, opt_format},
		{NULL, 0, 0},
	};

	if (!str) {
		hist_format(selected.history, Activity_none, HIST_UI, "SELF_IGNORES_START :Ignoring:");
		for (p = ignores, i = 1; p; p = p->next, i++)
			if (!serv || !p->server || strcmp(server->name, p->server) == 0)
				hist_format(selected.history, Activity_none, HIST_UI|HIST_NIGN, "SELF_IGNORES_LIST %d %s %s %s :%s",
						i, p->server ? p->server : "ANY", p->noact ? "yes" : "no", p->format ? p->format : "ANY", p->text);
		hist_format(selected.history, Activity_none, HIST_UI, "SELF_IGNORES_END :End of ignore list");
		return;
	}

	while ((ret = command_getopt(&str, opts)) != opt_done) {
		switch (ret) {
		case opt_error:
			return;
		case opt_show: case opt_hide:
			if (*str) {
				command_toomany("ignore");
			} else {
				selected.showign = ret == opt_show;
				windows[Win_main].refresh = 1;
			}
			return;
		case opt_delete:
			if (!strisnum(str, 0)) {
				ui_error("invalid id: %s", str);
				return;
			}
			id = strtol(str, NULL, 10);
			if (!id || id > INT_MAX || id < INT_MIN)
				goto idrange;
			for (p = ignores, i = 1; p; p = p->next, i++) {
				if (i == id) {
					if (i == 1)
						ignores = p->next;
					if (p->next)
						p->next->prev = p->prev;
					if (p->prev)
						p->prev->next = p->next;
					regfree(&p->regex);
					pfree(&p->text);
					pfree(&p->server);
					free(p);
					return;
				}
			}
idrange:
			ui_error("id out of range: %s", str);
			return;
		case opt_format:
			if (strncmp(command_optarg, "format.", CONSTLEN("format.")) == 0) {
				format = strdup(command_optarg);
			} else {
				len = strlen(command_optarg) + 8;
				format = emalloc(len);
				snprintf(format, len, "format.%s", command_optarg);
			}

			if (!config_gets(format)) {
				ui_error("no such format: %s", format + CONSTLEN("format"));
				free(format);
				return;
			}
			break;
		case opt_noact:
			noact = 1;
			break;
		case opt_extended:
			regopt |= REG_EXTENDED;
			break;
		case opt_icase:
			regopt |= REG_ICASE;
			break;
		case opt_server:
			serv = 1;
			break;
		}
	}

	if (config_getl("regex.extended"))
		regopt |= REG_EXTENDED;
	if (config_getl("regex.icase"))
		regopt |= REG_ICASE;

	if (!*str) {
		command_toofew("ignore");
		return;
	}

	ign = emalloc(sizeof(struct Ignore));
	ign->next = ign->prev = NULL;
	if ((ret = regcomp(&ign->regex, str, regopt)) != 0) {
		regerror(ret, &ign->regex, errbuf, sizeof(errbuf));
		ui_error("%s: %s", errbuf, str);
		free(ign);
		return;
	}
	ign->text   = strdup(str);
	ign->format = format;
	ign->regopt = regopt;
	ign->noact  = noact;
	ign->server = serv ? strdup(server->name) : NULL;

	if (!nouich)
		hist_format(selected.history, Activity_none, HIST_UI, "SELF_IGNORES_ADDED %s %s %s :%s",
				serv ? server->name : "ANY", noact ? "yes" : "no", format ? format : "ANY", str);

	if (!ignores) {
		ignores = ign;
	} else {
		for (p = ignores; p && p->next; p = p->next);
		ign->prev = p;
		p->next = ign;
	}
}

static void
modelset(char *cmd, struct Server *server, struct Channel *channel,
		int remove, char mode, char *args) {
	char *percmds, *p;
	char *modes;
	int percmd;
	int i;
	size_t len;

	if (!args) {
		command_toofew(cmd);
		return;
	}

	percmds = support_get(server, "MODES");
	if (percmds)
		percmd = atoi(percmds);
	else
		percmd = config_getl("def.modes");

	/*
	 * Now, I'd hope that servers do something clever like determining this
	 * value from the max length of channels, nicks and IRC messages
	 * overall, so that it is impossible to send messages that are too
	 * long, but I doubt it.
	 * TODO: some maths for limiting percmd based on lengths.
	 * 
	 * It'd be useful to be able to check if the desired priviledge is
	 * supported by the server. Theoretically some servers could also use
	 * different modes for priviledges, in this case it would make sense
	 * that 'char mode' be the symbol, i,e, ~&@%+ and then we would look up
	 * the mode in PREFIX.
	 * TODO: check PREFIX=...
	 *
	 */

	/* 2 = null byte and +/- */
	len = percmd + 2;
	modes = emalloc(len);
	*modes = remove ? '-' : '+';
	for (i = 0; i < percmd; i++)
		*(modes + i + 1) = mode;
	*(modes + len - 1) = '\0';

	while (*args) {
		for (i = 0, p = args; *p && i < percmd; p++)
			if (*p == ' ' && *(p+1) != ' ')
				i++;
		if (*p)
			*(p - 1) = '\0';
		else
			i++;
		*(modes + i + 1) = '\0';

		serv_write(server, "MODE %s %s %s\r\n", channel->name, modes, args);

		args = p;
	}

	expect_set(server, Expect_nosuchnick, channel->name);
	pfree(&modes);
}

COMMAND(
command_op) {
	modelset("op", server, channel, 0, 'o', str);
}

COMMAND(
command_voice) {
	modelset("voice", server, channel, 0, 'v', str);
}

COMMAND(
command_halfop) {
	modelset("halfop", server, channel, 0, 'h', str);
}

COMMAND(
command_admin) {
	modelset("admin", server, channel, 0, 'a', str);
}

COMMAND(
command_owner) {
	modelset("owner", server, channel, 0, 'q', str);
}

COMMAND(
command_deop) {
	modelset("deop", server, channel, 1, 'o', str);
}

COMMAND(
command_devoice) {
	modelset("devoice", server, channel, 1, 'v', str);
}

COMMAND(
command_dehalfop) {
	modelset("dehalfop", server, channel, 1, 'h', str);
}

COMMAND(
command_deadmin) {
	modelset("deadmin", server, channel, 1, 'a', str);
}

COMMAND(
command_deowner) {
	modelset("deowner", server, channel, 1, 'q', str);
}

/* In most IRC clients /ban would create a mask from a nickname. I decided
 * against doing this as there would have to be some way of templating a mask,
 * eg, ${nick}*!*@*.{host}. This could've been done through splitting the
 * variable handling out of ui_format and using that here as well, however,
 * all though it could simplify ui_format by processing variables first, then
 * formats, it could cause problems as the variables themselves could contain
 * text that needs to be escaped or dealt with. Of course, there's nothing
 * stopping me from leaving the one in ui_format and creating another, but at
 * that point I decided it wasn't worth it for one command. Another approach I
 * though of was having the ability to create templates with combinatorial
 * options, but I think the mental overhead for doing so could be spent
 * constructing the masks manually instead. *shrug*/
COMMAND(
command_ban) {
	modelset("ban", server, channel, 0, 'b', str);
}

COMMAND(
command_unban) {
	modelset("unban", server, channel, 1, 'b', str);
}

COMMAND(
command_invite) {
	char *nick, *chan;

	if (!str) {
		command_toofew("invite");
		return;
	}

	nick = strtok_r(str, " ", &chan);
	if (!chan)
		chan = channel->name;

	serv_write(server, "INVITE %s %s\r\n", nick, chan);
}

int
command_getopt(char **str, struct CommandOpts *opts) {
	char *opt;

	if (!str || !*str || **str != '-') {
		if (str && *str && **str == '\\' && *((*str)+1) == '-')
			(*str)++;
		return opt_done;
	}

	opt = struntil((*str)+1, ' ');

	for (; opts->opt; opts++) {
		if (strcmp(opts->opt, opt) == 0) {
			*str = strchr(*str, ' ');
			if (*str)
				(*str)++;

			if (opts->arg) {
				command_optarg = *str;
				if (*str)
					*str = strchr(*str, ' ');
				if (*str && **str) {
					**str = '\0';
					(*str)++;
				}
			} else {
				if (*str && **str)
					*((*str)-1) = '\0';
			}

			/* Always return something that's not NULL */
			if (!*str)
				*str = "";

			return opts->ret;
		}
	}

	ui_error("no such option '%s'", opt);
	return opt_error;
}

int
alias_add(char *alias, char *cmd) {
	struct Alias *p;
	char *tmp;

	if (!alias || !cmd)
		return -1;

	if (*alias != '/') {
		tmp = emalloc(strlen(alias) + 2);
		snprintf(tmp, strlen(alias) + 2, "/%s", alias);
	}

	for (p = aliases; p; p = p->next)
		if (strcmp(p->alias, tmp) == 0)
			return -1;

	p = emalloc(sizeof(struct Alias));
	if (*alias != '/')
		p->alias = tmp;
	else
		p->alias = estrdup(alias);

	if (*cmd != '/') {
		tmp = emalloc(strlen(cmd) + 2);
		snprintf(tmp, strlen(cmd) + 2, "/%s", cmd);
		p->cmd = tmp;
	} else p->cmd = estrdup(cmd);
	p->prev = NULL;
	p->next = aliases;
	if (aliases)
		aliases->prev = p;
	aliases = p;

	return 0;
}

int
alias_remove(char *alias) {
	struct Alias *p;

	if (!alias)
		return -1;

	for (p=aliases; p; p = p->next) {
		if (strcmp(p->alias, alias) == 0) {
			if (p->prev)
				p->prev->next = p->next;
			else
				aliases = p->next;

			if (p->next)
				p->next->prev = p->prev;

			pfree(&p->alias);
			pfree(&p->cmd);
			pfree(&p);
			return 0;
		}
	}

	return -1;
}

char *
alias_eval(char *cmd) {
	static char ret[8192];
	struct Alias *p;
	int len, rc = 0;
	char *s;

	if ((s = strchr(cmd, ' ')) != NULL)
		len = s - cmd;
	else
		len = strlen(cmd);

	for (p = aliases; p; p = p->next) {
		if (p->cmd && strlen(p->alias) == len && strncmp(p->alias, cmd, len) == 0) {
			rc += strlcpy(&ret[rc], p->cmd, sizeof(ret) - rc);
			break;
		}
	}

	if (!rc)
		return cmd;

	rc += strlcpy(&ret[rc], cmd + len, sizeof(ret) - rc);
	return ret;
}

void
command_eval(struct Server *server, char *str) {
	struct Command *cmdp;
	char msg[512];
	char *cmd;
	char *s, *dup;

	s = dup = estrdup(alias_eval(str));

	if (*s != '/' || strncmp(s, "/ /", sizeof("/ /")) == 0) {
		/* Provide a way to escape commands
		 *      "/ /cmd" --> "/cmd"      */
		if (strncmp(s, "/ /", sizeof("/ /")) == 0)
			s += 3;

		if (selected.channel && selected.server) {
			// TODO: message splitting
			snprintf(msg, sizeof(msg), "PRIVMSG %s :%s", selected.channel->name, s);
			serv_write(selected.server, "%s\r\n", msg);
			hist_format(selected.channel->history, Activity_self, HIST_SHOW|HIST_LOG|HIST_SELF, "%s", msg);
		} else {
			ui_error("channel not selected, message ignored", NULL);
		}
		goto end;
	} else {
		s++;
		cmd = s;
		s = strchr(s, ' ');
		if (s && *s) {
			*s = '\0';
			s++;
			if (*s == '\0')
				s = NULL;
		}

		for (cmdp = commands; cmdp->name && cmdp->func; cmdp++) {
			if (strcmp(cmdp->name, cmd) == 0) {
				if (cmdp->need == 2 && !selected.channel)
					ui_error("/%s requires a channel to be selected", cmdp->name);
				else if (cmdp->need == 2 && selected.channel->server != server)
					ui_error("/%s cannot be run with /server", cmdp->name);
				else if (cmdp->need == 1 && !server)
					ui_error("/%s requires a server to be selected or provided by /server", cmdp->name);
				else
					cmdp->func(server, selected.channel, s);


				goto end;
			}
		}

		ui_error("no such command: '%s'", cmd);
	}
end:
	pfree(&dup);
}
