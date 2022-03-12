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

#define COMMAND(func) static void func(struct Server *server, struct Channel *channel, char *str)

/* IRC commands */
COMMAND(command_away);
COMMAND(command_msg);
COMMAND(command_notice);
COMMAND(command_me);
COMMAND(command_ctcp);
COMMAND(command_quit);
COMMAND(command_join);
COMMAND(command_part);
COMMAND(command_kick);
COMMAND(command_mode);
COMMAND(command_nick);
COMMAND(command_list);
COMMAND(command_whois);
COMMAND(command_who);
COMMAND(command_whowas);
COMMAND(command_ping);
COMMAND(command_quote);
COMMAND(command_connect);
COMMAND(command_disconnect);
COMMAND(command_names);
COMMAND(command_topic);
COMMAND(command_motd);
COMMAND(command_oper);
COMMAND(command_time);
COMMAND(command_stats);
COMMAND(command_kill);
COMMAND(command_links);
COMMAND(command_map);
COMMAND(command_lusers);

/* Channel priviledges (use modelset only) */
COMMAND(command_op);
COMMAND(command_voice);
COMMAND(command_halfop);
COMMAND(command_admin);
COMMAND(command_owner);
COMMAND(command_deop);
COMMAND(command_devoice);
COMMAND(command_dehalfop);
COMMAND(command_deadmin);
COMMAND(command_deowner);
COMMAND(command_ban);
COMMAND(command_unban);

/* UI commands */
COMMAND(command_query);
COMMAND(command_select);
COMMAND(command_set);
COMMAND(command_format);
COMMAND(command_server);
COMMAND(command_bind);
COMMAND(command_help);
COMMAND(command_echo);
COMMAND(command_grep);
COMMAND(command_clear);
COMMAND(command_alias);
COMMAND(command_scroll);
COMMAND(command_source);
COMMAND(command_dump);
COMMAND(command_close);

static char *command_optarg;
enum {
	opt_error = -2,
	opt_done = -1,
	CMD_NARG,
	CMD_ARG,
};

struct Command commands[] = {
	/* IRC commands */
	{"away", command_away, 0, {
		"usage: /away [message]",
		"Set yourself as away on the server.",
		"An empty message will unset the away.", NULL}},
	{"msg", command_msg, 1, {
		"usage: /msg <nick|channel> message..",
		"Send a message to a nick or channel.",
		"Will appear in buffers if already open.", NULL}},
	{"notice", command_notice, 1, {
		"usage: /notice <nick|channel> message..",
		"Send a notice to a nick or channel.",
		"Will appear in buffers if already open.", NULL}},
	{"me", command_me, 2, {
		"usage: /me message..",
		"Send a CTCP ACTION to the selected channel/query", NULL}},
	{"ctcp", command_ctcp, 1, {
		"usage: /ctcp [channel|nick] <TYPE>",
		"Send a CTCP request to a channel or nick", NULL}},
	{"quit", command_quit, 0, {
		"usage: /quit",
		"Cleanup and exit", NULL}},
	{"quote", command_quote, 1, {
		"usage: /quote <message>",
		"Send raw message to server", NULL}},
	{"join", command_join, 1, {
		"usage: /join <channel>",
		"Join channel", NULL}},
	{"part", command_part, 1, {
		"usage: /part <channel>",
		"Part channel", NULL}},
	{"kick", command_kick, 1, {
		"usage: /kick [channel] <nick> [reason]",
		"Kick nick from channel", NULL}},
	{"mode", command_mode, 1, {
		"usage: /mode <channel> modes...",
		"Set/unset channel modes", NULL}},
	{"nick", command_nick, 1, {
		"usage: /nick <nick>",
		"Get a new nick", NULL}},
	{"list", command_list, 1, {
		"usage: /list",
		"Get list of channels.", NULL}},
	{"whois", command_whois, 1, {
		"usage: /whois [server] [nick]",
		"Request information on a nick or oneself", NULL}},
	{"who", command_who, 1, {
		"usage: /whois [mask [options]]",
		"Request short information on nicks", NULL}},
	{"whowas", command_whowas, 1, {
		"usage: /whowas [nick [count [server]]]",
		"Request information on old nicks",
		"Count defaults to 5", NULL}},
	{"ping", command_ping, 1, {
		"usage: /ping message...",
		"Send a PING to server.",
		"hirc will do this itself in the background,",
		"but will hide it unless this command is used.", NULL}},
	{"connect", command_connect, 0, {
		"usage: /connect [-network <name>] [-nick <nick>] [-user <user>]",
		"                [-real <comment>] [-tls] [-verify] <host> [port]",
		"Connect to a network/server", NULL}},
	{"disconnect", command_disconnect, 0, {
		"usage: /disconnect [network]",
		"Disconnect from a network/server", NULL}},
	{"names", command_names, 1, {
		"usage: /names <channel>",
		"List nicks in channel (pretty useless with nicklist.", NULL}},
	{"topic", command_topic, 1, {
		"usage: /topic [-clear] [channel] [topic]",
		"Sets, clears, or checks topic in channel.",
		"Provide only channel name to check.", NULL}},
	{"oper", command_oper, 1, {
		"usage: /oper [user] <password>",
		"Authenticate for server operator status.",
		"If a user is not specified, the current nickname is used.", NULL}},
	{"motd", command_motd, 1, {
		"usage: /motd [server/nick]",
		"Get the Message Of The Day for the current server,",
		"specified server, or the server with the specified nickname.", NULL}},
	{"time", command_time, 1, {
		"usage: /time [server/nick]",
		"Get the time and timezone of the current server,",
		"specified server, or the server with the specified nickname.", NULL}},
	{"stats", command_stats, 1, {
		"usage: /stats [type [server]]",
		"Query server statistics. Servers will usually list",
		"types that can be queried if no arguments are given.", NULL}},
	{"kill", command_kill, 1, {
		"usage: /kill <nick> [reason]",
		"Forcefully disconnect a nick from a server.",
		"Uses misc.killmessage if no reason provided.", NULL}},
	{"links", command_links, 1, {
		"usage: /links [[server] mask]",
		"Request list of linked servers from the veiwpoint",
		"of the current or specified server, matching the",
		"specified mask.", NULL}},
	{"lusers", command_lusers, 1, {
		"usage: /lusers",
		"Request a list of users from the server.",
		"This is implemented in all servers, but",
		"only some allow its request via a command.", NULL}},
	{"map", command_map, 1, {
		"usage: /map",
		"Similar to /links but prints an ascii diagram.",
		"Nonstandard feature.", NULL}},
	/* Channel priviledges */
	{"op", command_op, 2, {
		"usage: /op nicks...",
		"Give a nickname +o on the current channel.", NULL}},
	{"voice", command_voice, 2, {
		"usage: /voice nicks...",
		"Give a nickname +v on the current channel.", NULL}},
	{"halfop", command_halfop, 2, {
		"usage: /halfop nicks...",
		"Give a nickname +h on the current channel.", NULL}},
	{"admin", command_admin, 2, {
		"usage: /admin nicks...",
		"Give a nickname +a on the current channel.", NULL}},
	{"owner", command_owner, 2, {
		"usage: /owner nicks...",
		"Give a nickname +q on the current channel.", NULL}},
	{"deop", command_deop, 2, {
		"usage: /deop nicks...",
		"Remove +o for a nick on the current channel.", NULL}},
	{"devoice", command_devoice, 2, {
		"usage: /devoice nicks...",
		"Remove +v for a nick on the current channel.", NULL}},
	{"dehalfop", command_dehalfop, 2, {
		"usage: /dehalfop nicks...",
		"Remove +h for a nick on the current channel.", NULL}},
	{"deadmin", command_deadmin, 2, {
		"usage: /deadmin nicks...",
		"Remove +a for a nick on the current channel.", NULL}},
	{"deowner", command_deowner, 2, {
		"usage: /deowner nicks...",
		"Remove +q for a nick on the current channel.", NULL}},
	{"ban", command_ban, 2, {
		"usage: /ban masks...",
		"Add masks to the +b banlist in the current channel", NULL}},
	{"unban", command_unban, 2, {
		"usage: /unban masks...",
		"Remove masks from the banlist in the current channel", NULL}},
	/* UI commands */
	{"query", command_query, 1, {
		"usage: /query <nick>",
		"Open a buffer for communication with a nick", NULL}},
	{"select", command_select, 0, {
		"usage: /select [-network <name>] [-channel <name>] [buffer id]",
		"Select a buffer", NULL}},
	{"set",	command_set, 0, {
		"usage: /set <variable> [number/range] [end of range]",
		"       /set <variable> string....",
		"Set a configuration variable.",
		"Passing only the name prints content.", NULL}},
	{"format", command_format, 0, {
		"usage: /format <format> string...",
		"Set a formatting variable.",
		"This is equivalent to /set format.<format> string...", NULL}},
	{"server", command_server, 0, {
		"usage: /server [-auto] <server> [cmd....]",
		"       /server [-clear] <server>",
		"Evaluate a cooked command with server as target.",
		" -auto  if supplied with a command, run that command",
		"        automatically when the server connects.",
		"        Otherwise, list autocmds that have been set.",
		" -clear clear autocmds from server",
		"To send a raw command to a server, use:",
		" /server <server> /quote ...", NULL}},
	{"bind", command_bind, 0, {
		"usage: /bind [<keybind> [cmd [..]]]",
		"       /bind -delete <keybind>",
		"Bind command to key.",
		"Accepts caret formatted control characters (eg, ^C).",
		"Accepts multiple characters (alt-c = '^[c'), though",
		"these must be inputted faster than wgetch can read.", NULL}},
	{"help", command_help, 0, {
		"usage: /help [command or variable]",
		"Print help information.",
		"`/help commands` and `/help variables` will list respectively", NULL}},
	{"echo", command_echo, 0, {
		"usage: /echo ...",
		"Print temporarily to selected buffer.", NULL}},
	{"grep", command_grep, 0, {
		"usage: /grep [-iE] [regex]",
		"Search selected buffer",
		" -i: case insensitive",
		" -E: posix extended regex",
		"If no argument supplied, clears previous search",
		"Searches are also cleared after selecting another buffer",
		"See also config variables: regex.extended and regex.icase", NULL}},
	{"clear", command_clear, 0, {
		"usage: /clear [-tmp] [-err] [-serr] [-log]",
		"Clear selected buffer of messages.",
		"By default all messages are cleared.",
		"The following options clear only certain messages:",
		" -tmp:  temporary messages - cleared when switching buffer",
		" -err:  hirc generated errors",
		" -serr: server generated errors",
		" -log:  messages restored from log files", NULL}},
	{"alias", command_alias, 0, {
		"usage: /alias [<alias> [cmd [...]]]",
		"       /alias -delete <alias>",
		"Add or remove an alias that expands to a command.", NULL}},
	{"scroll", command_scroll, 0, {
		"usage: /scroll [-buflist] [-nicklist] [-|+]lines",
		"Scroll a window (main by default).",
		"Positive scrolls up, negative down, 0 resets and tracks",
		"Probably most useful with /bind", NULL}},
	{"source", command_source, 0, {
		"usage: /source <file>",
		"Read a config file. Can be used inside config files.", NULL}},
	{"dump", command_dump, 0, {
		"usage: /dump [-all] [-aliases] [-bindings] [-formats] [-config]",
		"             [-default] [-servers] [-channels] [-queries] <file>",
		"Dumps configuration details into a file.",
		" -autocmds dump commands specified with /server -auto",
		" -aliases  dump /alias commands",
		" -bindings dump /bind commands",
		" -formats  dump /format commands beginning with filter.",
		" -config   dump /format options excluding filters",
		" -servers  dump /server commands",
		" -channels dump /join commands for respective servers",
		" -queries  dump /query commands for respective servers",
		" -default  dump default settings (dump non-default otherwise)",
		"If none (excluding -default) of the above are selected, it is",
		"treated as though all are selected.",
		"If -autocmds and -channels are used together, and there exists",
		"an autocmd to join a channel, then only the autocmd will be dumped.", NULL}},
	{"close", command_close, 0, {
		"usage: /close [id]",
		"Forget about selected buffer, or a buffer by id.", NULL}},
	{NULL, NULL},
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
			ircprintf(sp, format, str);
	} else if (server) {
		ircprintf(server, format, str);
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

	ircprintf(server, "PRIVMSG %s :%s\r\n", target, message);
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

	ircprintf(server, "NOTICE %s :%s\r\n", target, message);
	if (chan) {
		hist_format(chan->history, Activity_self,
				HIST_SHOW|HIST_LOG|HIST_SELF, "NOTICE %s :%s", target, message);
	}
}

COMMAND(
command_me) {
	if (!str)
		str = "";

	ircprintf(server, "PRIVMSG %s :%cACTION %s%c\r\n", channel->name, 1, str, 1);
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
	ircprintf(server, "PRIVMSG %s :%c%s%c\r\n", target, 1, ctcp, 1);
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
	cleanup(str ? str : config_gets("misc.quitmessage"));
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
		ircprintf(server, "%s", msg);
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

	snprintf(msg, sizeof(msg), "PART %s :%s\r\n", chan, reason ? reason : config_gets("misc.partmessage"));

	ircprintf(server, "%s", msg);
	expect_set(server, Expect_part, chan);
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
		ircprintf(server, "KICK %s %s :%s\r\n", chan, nick, reason);
	else
		ircprintf(server, "KICK %s %s\r\n", chan, nick);
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
		ircprintf(server, "MODE %s %s\r\n", chan, modes);
	} else {
		expect_set(server, Expect_channelmodeis, chan);
		ircprintf(server, "MODE %s\r\n", chan);
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

	ircprintf(server, "NICK %s\r\n", str);
	expect_set(server, Expect_nicknameinuse, str);
}

COMMAND(
command_list) {
	if (str) {
		command_toomany("list");
		return;
	}

	ircprintf(server, "LIST\r\n", str);
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
		ircprintf(server, "WHOIS %s :%s\r\n", tserver, nick);
	else
		ircprintf(server, "WHOIS %s\r\n", nick);
}

COMMAND(
command_who) {
	if (!str)
		str = "*"; /* wildcard */

	ircprintf(server, "WHO %s\r\n", str);
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
		ircprintf(server, "WHOWAS %s %s :%s\r\n", nick, count, tserver);
	else if (count)
		ircprintf(server, "WHOWAS %s %s\r\n", nick, count);
	else
		ircprintf(server, "WHOWAS %s 5\r\n", nick);
}

COMMAND(
command_ping) {
	if (!str) {
		command_toofew("ping");
		return;
	}

	ircprintf(server, "PING :%s\r\n", str);
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
		ircprintf(server, "%s\r\n", str);
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
	char *nick	= config_gets("def.nick");
	char *username	= config_gets("def.user");
	char *realname	= config_gets("def.real");
	int tls = 0, tls_verify = 0;
	int ret;
	struct passwd *user;
	enum {
		opt_network,
		opt_nick,
		opt_username,
		opt_realname,
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

	if (!host && network && serv_get(&servers, network)) {
		serv_connect(serv_get(&servers, network));
		return;
	} else if (!host && server) {
		serv_connect(server);
		return;
	} else if (!host) {
		ui_error("must specify host", NULL);
		return;
	}

	if (!nick) {
		user = getpwuid(geteuid());
		nick = user ? user->pw_name : "null";
	}

	if (!port) {
		port = tls  ? "6697" : "6667";
	}

	username = username ? username : nick;
	realname = realname ? realname : nick;
	network  = network  ? network  : host;

	tserver = serv_add(&servers, network, host, port, nick, username, realname, tls, tls_verify);
	serv_connect(tserver);
	if (!nouich)
		ui_select(tserver, NULL);
}

COMMAND(
command_disconnect) {
	struct Server *sp;
	int len;
	char *msg;

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
			return;
		}
	} else sp = server;

	if (!msg || !*msg)
		msg = config_gets("def.quitmessage");

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

		for (sp = servers; sp; sp = sp->next)
			if (strcmp(sp->name, tserver) == 0)
				break;

		if (!sp) {
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

	len = strlen(str) + strlen("format.") + 1;
	newstr = talloc(len);
	snprintf(newstr, len, "format.%s", str);
	command_set(server, channel, newstr);
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
			hist_format(selected.history, Activity_none, HIST_SHOW|HIST_TMP|HIST_MAIN, "SELF_AUTOCMDS_START %s :Autocmds for %s:",
					nserver->name, nserver->name);
			for (acmds = nserver->autocmds; acmds && *acmds; acmds++)
				hist_format(selected.history, Activity_none, HIST_SHOW|HIST_TMP|HIST_MAIN, "SELF_AUTOCMDS_LIST %s :%s",
						nserver->name, *acmds);
			hist_format(selected.history, Activity_none, HIST_SHOW|HIST_TMP|HIST_MAIN, "SELF_AUTOCMDS_END %s :End of autocmds for %s",
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

	ircprintf(server, "NAMES %s\r\n", chan);
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
		ircprintf(server, "TOPIC %s :\r\n", chan);
		return;
	}

	if (!topic) {
		ircprintf(server, "TOPIC %s\r\n", chan);
		expect_set(server, Expect_topic, chan);
	} else ircprintf(server, "TOPIC %s :%s\r\n", chan, topic);
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

	ircprintf(server, "OPER %s %s\r\n", user, pass);
}

static void
command_send0(struct Server *server, char *cmd, char *cmdname, char *str) {
	if (str)
		command_toomany(cmdname);
	else
		ircprintf(server, "%s\r\n", cmd);
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
		ircprintf(server, "%s %s\r\n", cmd, str);
	else
		ircprintf(server, "%s\r\n", cmd);
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
		ircprintf(server, "%s %s\r\n", cmd, str);
	else
		ircprintf(server, "%s\r\n", cmd);
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
		reason = config_gets("misc.killmessage");
	ircprintf(server, "KILL %s :%s\r\n", nick, reason);
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
		hist_format(selected.history, Activity_none, HIST_SHOW|HIST_TMP|HIST_MAIN, "SELF_KEYBIND_START :Keybindings:");
		for (p = keybinds; p; p = p->next)
			hist_format(selected.history, Activity_none, HIST_SHOW|HIST_TMP|HIST_MAIN, "SELF_KEYBIND_LIST %s :%s", ui_unctrl(p->binding), p->cmd);
		hist_format(selected.history, Activity_none, HIST_SHOW|HIST_TMP|HIST_MAIN, "SELF_KEYBIND_END :End of keybindings");
	} else if (!cmd) {
		for (p = keybinds; p; p = p->next) {
			if (strcmp(p->binding, binding) == 0) {
				hist_format(selected.history, Activity_none, HIST_SHOW|HIST_TMP|HIST_MAIN, "SELF_KEYBIND_START :Keybindings:");
				hist_format(selected.history, Activity_none, HIST_SHOW|HIST_TMP|HIST_MAIN, "SELF_KEYBIND_LIST %s :%s", ui_unctrl(p->binding), p->cmd);
				hist_format(selected.history, Activity_none, HIST_SHOW|HIST_TMP|HIST_MAIN, "SELF_KEYBIND_END :End of keybindings");
				return;
			}
		}

		ui_error("no such keybind: '%s'", binding);
	} else {
		ui_bind(binding, cmd);
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
		hist_format(selected.history, Activity_none, HIST_SHOW|HIST_TMP|HIST_MAIN, "SELF_ALIAS_START :Aliases:");
		for (p = aliases; p; p = p->next)
			hist_format(selected.history, Activity_none, HIST_SHOW|HIST_TMP|HIST_MAIN, "SELF_ALIAS_LIST %s :%s", p->alias, p->cmd);
		hist_format(selected.history, Activity_none, HIST_SHOW|HIST_TMP|HIST_MAIN, "SELF_ALIAS_END :End of aliases");
	} else if (!cmd) {
		for (p = aliases; p; p = p->next) {
			if (strcmp(p->alias, alias) == 0) {
				hist_format(selected.history, Activity_none, HIST_SHOW|HIST_TMP|HIST_MAIN, "SELF_ALIAS_START :Aliases:");
				hist_format(selected.history, Activity_none, HIST_SHOW|HIST_TMP|HIST_MAIN, "SELF_ALIAS_LIST %s :%s", p->alias, p->cmd);
				hist_format(selected.history, Activity_none, HIST_SHOW|HIST_TMP|HIST_MAIN, "SELF_ALIAS_END :End of aliases");
				return;
			}
		}

		ui_error("no such alias: '%s'", alias);
	} else {
		alias_add(alias, cmd);
	}
}


COMMAND(
command_help) {
	int cmdonly = 0;
	int found = 0;
	int i, j;

	if (!str) {
		command_help(server, channel, "/help");
		return;
	}

	if (strcmp(str, "commands") == 0) {
		hist_format(selected.history, Activity_none, HIST_SHOW|HIST_TMP|HIST_MAIN, "SELF_HELP_START :%s", str);
		for (i=0; commands[i].name && commands[i].func; i++)
			hist_format(selected.history, Activity_none, HIST_SHOW|HIST_TMP|HIST_MAIN, "SELF_HELP : /%s", commands[i].name);
		hist_format(selected.history, Activity_none, HIST_SHOW|HIST_TMP|HIST_MAIN, "SELF_HELP_END :end of help");
		return;
	}

	if (strcmp(str, "variables") == 0) {
		hist_format(selected.history, Activity_none, HIST_SHOW|HIST_TMP|HIST_MAIN, "SELF_HELP_START :%s", str);
		for (i=0; config[i].name; i++)
			hist_format(selected.history, Activity_none, HIST_SHOW|HIST_TMP|HIST_MAIN, "SELF_UI : %s", config[i].name);
		hist_format(selected.history, Activity_none, HIST_SHOW|HIST_TMP|HIST_MAIN, "SELF_HELP_END :end of help");
		return;
	}

	if (*str == '/') {
		cmdonly = 1;
		str++;
	}

	for (i=0; commands[i].name && commands[i].func; i++) {
		if (strncmp(commands[i].name, str, strlen(str)) == 0) {
			found = 1;
			hist_format(selected.history, Activity_none, HIST_SHOW|HIST_TMP|HIST_MAIN, "SELF_HELP_START :%s", commands[i].name);
			for (j=0; commands[i].description[j]; j++)
				hist_format(selected.history, Activity_none, HIST_SHOW|HIST_TMP|HIST_MAIN, "SELF_HELP :%s", commands[i].description[j]);
		}
	}

	if (!cmdonly) {
		for (i=0; config[i].name; i++) {
			if (strncmp(config[i].name, str, strlen(str)) == 0) {
				found = 1;
				hist_format(selected.history, Activity_none, HIST_SHOW|HIST_TMP|HIST_MAIN, "SELF_HELP_START :%s", config[i].name);
				for (j=0; config[i].description[j]; j++)
					hist_format(selected.history, Activity_none, HIST_SHOW|HIST_TMP|HIST_MAIN, "SELF_UI :%s", config[i].description[j]);
			}
		}
	}

	if (found)
		hist_format(selected.history, Activity_none, HIST_SHOW|HIST_TMP|HIST_MAIN, "SELF_HELP_END :end of help");
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
	int regopt = 0, ret;
	char errbuf[1024];
	enum { opt_extended, opt_icase };
	static struct CommandOpts opts[] = {
		{"E", CMD_NARG, opt_extended},
		{"i", CMD_NARG, opt_icase},
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

	/* Traverse until we hit a message generated by us */
	for (; p && !(p->options & HIST_GREP); p = p->prev) {
		/* TODO: matching ui_format result by default,
		 *       option for matching raw */
		if (regexec(&re, p->raw, 0, NULL, 0) == 0)
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
	if (!str) {
		command_toofew("source");
		return;
	}

	config_read(str);
}

COMMAND(
command_dump) {
	FILE *file;
	int selected = 0;
	int def = 0, ret;
	int i;
	char **aup;
	struct Server *sp;
	struct Channel *chp;
	struct Alias *ap;
	struct Keybind *kp;
	enum {
		opt_aliases = 1,
		opt_bindings = 2,
		opt_formats = 4,
		opt_config = 8,
		opt_servers = 16,
		opt_channels = 32,
		opt_queries = 64,
		opt_autocmds = 128,
		opt_default = 256,
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

	if ((file = fopen(str, "wb")) == NULL) {
		ui_error("cannot open file '%s': %s", str, strerror(errno));
		return;
	}

	if (selected & (opt_servers|opt_channels|opt_queries|opt_autocmds)) {
		if (selected & opt_servers)
			fprintf(file, "Network connections\n");

		for (sp = servers; sp; sp = sp->next) {
			if (selected & opt_servers) {
				fprintf(file, "/connect -network %s -nick %s -user %s -real %s %s%s %s\n",
						sp->name,
						sp->self->nick,
						sp->username,
						sp->realname,
#ifdef TLS
						sp->tls ? "-tls " : "",
#else
						"",
#endif /* TLS */
						sp->host,
						sp->port);
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

	if (selected & opt_aliases) {
		fprintf(file, "ALiases\n");
		for (ap = aliases; ap; ap = ap->next)
			fprintf(file, "/alias %s %s\n", ap->alias, ap->cmd);
		fprintf(file, "\n");
	}

	if (selected & opt_bindings) {
		fprintf(file, "Keybindings\n");
		for (kp = keybinds; kp; kp = kp->next)
			fprintf(file, "/bind %s %s\n", ui_unctrl(kp->binding), kp->cmd);
		fprintf(file, "\n");
	}

	if (selected & opt_formats || selected & opt_config) {
		fprintf(file, "Configuration variables\n");
		for (i = 0; config[i].name; i++) {
			if (!config[i].isdef || def) {
				if (selected & opt_formats && strncmp(config[i].name, "format.", strlen("format.")) == 0) {
					fprintf(file, "/format %s %s\n", config[i].name + strlen("format."), config[i].str);
				} else if (selected & opt_config && strncmp(config[i].name, "format.", strlen("format.")) != 0) {
					if (config[i].valtype == Val_string)
						fprintf(file, "/set %s %s\n", config[i].name, config[i].str);
					else if (config[i].valtype == Val_pair || config[i].valtype == Val_colourpair)
						fprintf(file, "/set %s %ld %ld\n", config[i].name, config[i].pair[0], config[i].pair[1]);
					else
						fprintf(file, "/set %s %ld\n", config[i].name, config[i].num);
				}
			}
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
		if (serv_ischannel(sp, chp->name)) {
			ircprintf(sp, "PART %s\r\n", chp->name);
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
	modes = talloc(len);
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

		ircprintf(server, "MODE %s %s %s\r\n", channel->name, modes, args);

		args = p;
	}

	expect_set(server, Expect_nosuchnick, channel->name);
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

int
command_getopt(char **str, struct CommandOpts *opts) {
	char *opt;

	if (!str || !*str || **str != '-') {
		if (**str == '\\' && *((*str)+1) == '-')
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

	p = emalloc(sizeof(struct Alias));
	if (*alias != '/') {
		tmp = emalloc(strlen(alias) + 2);
		snprintf(tmp, strlen(alias) + 2, "/%s", alias);
		p->alias = tmp;
	} else p->alias = estrdup(alias);
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

			free(p->alias);
			free(p->cmd);
			free(p);
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
	char *s;

	s = tstrdup(alias_eval(str));

	if (*s != '/' || strncmp(s, "/ /", sizeof("/ /")) == 0) {
		/* Provide a way to escape commands
		 *      "/ /cmd" --> "/cmd"      */
		if (strncmp(s, "/ /", sizeof("/ /")) == 0)
			s += 3;

		if (selected.channel && selected.server) {
			// TODO: message splitting
			snprintf(msg, sizeof(msg), "PRIVMSG %s :%s", selected.channel->name, s);
			ircprintf(selected.server, "%s\r\n", msg);
			hist_format(selected.channel->history, Activity_self, HIST_SHOW|HIST_LOG|HIST_SELF, "%s", msg);
		} else {
			ui_error("channel not selected, message ignored", NULL);
		}
		return;
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

				return;
			}
		}

		ui_error("no such command: '%s'", cmd);
	}
}
