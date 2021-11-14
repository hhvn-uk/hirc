#include <ncurses.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pwd.h>
#include <sys/types.h>
#include "hirc.h"

static char *command_optarg;
enum {
	opt_error = -2,
	opt_done = -1,
	CMD_NARG,
	CMD_ARG,
};

static struct Command commands[] = {
	{"quit", command_quit, {
		"usage: /quit",
		"Cleanup and exit", NULL}},
	{"quote", command_quote, {
		"usage: /quote <message>",
		"Send raw message to server", NULL}},
	{"join", command_join, {
		"usage: /join <channel>",
		"Join channel", NULL}},
	{"part", command_part, {
		"usage: /part <channel>",
		"Part channel", NULL}},
	{"ping", command_ping, {
		"usage: /ping message...",
		"Send a PING to server.",
		"hirc will do this itself in the background,",
		"but will hide it unless this command is used.", NULL}},
	{"connect", command_connect, {
		"usage: /connect [-network <name>] [-nick <nick>] [-user <user>]",
		"                [-real <comment>] [-tls] [-verify] <host> [port]",
		"Connect to a network/server", NULL}},
	{"select", command_select, {
		"usage: /select [-network <name>] [-channel <name>] [buffer id]",
		"Select a buffer", NULL}},
	{"set",	command_set, {
		"usage: /set <variable> [number/range] [end of range]",
		"       /set <variable> string....",
		"Set a configuration variable.",
		"Passing only the name prints content.", NULL}},
	{"server", command_server, {
		"usage: /server <server> cmd....",
		"Run (non-raw) command with server as target.", NULL}},
	{"help", command_help, {
		"usage: /help [command or variable]",
		"Print help information.",
		"`/help commands` and `/help variables` will list respectively", NULL}},
	{NULL, NULL},
};

void
command_quit(struct Server *server, char *str) {
	cleanup(str ? str : config_gets("misc.quitmessage"));
	exit(EXIT_SUCCESS);
}

void
command_join(struct Server *server, char *str) {
	if (strchr(config_gets("def.chantypes"), *str))
		ircprintf(server, "JOIN %s\r\n", str);
	else
		ircprintf(server, "JOIN #%s\r\n", str);
	handle_expect("JOIN", str);
}

void
command_part(struct Server *server, char *str) {
	if (strchr(config_gets("def.chantypes"), *str))
		ircprintf(server, "PART %s\r\n", str);
	else
		ircprintf(server, "PART #%s\r\n", str);
	handle_expect("PART", str);
}

void
command_ping(struct Server *server, char *str) {
	if (!str) {
		ui_error("/ping requires argument", NULL);
		return;
	}

	ircprintf(server, "PING :%s\r\n", str);
	handle_expect("PONG", str);
}

void
command_quote(struct Server *server, char *str) {
	if (!str) {
		ui_error("/quote requires argument", NULL);
		return;
	}

	if (!server) {
		ui_error("no server selected", NULL);
		return;
	}

	ircprintf(server, "%s\r\n", str);
}

void
command_connect(struct Server *server, char *str) {
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

	if (!host) {
		ui_error("must specify host", NULL);
		return;
	}

	if (!nick) {
		user = getpwuid(geteuid());
		nick = user ? user->pw_name : "null";
	}

	port      = port    ? port     : "6667";
	username = username ? username : nick;
	realname = realname ? realname : nick;
	network  = network  ? network  : host;

	tserver = serv_add(&servers, network, host, port, nick, username, realname, tls, tls_verify);
	serv_connect(tserver);
	ui_select(tserver, NULL);
}

void
command_select(struct Server *server, char *str) {
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
		ui_buflist_select(buf);
	} else ui_error("/select requires argument", NULL);
}

void
command_set(struct Server *server, char *str) {
	char *name, *val;

	if (!str) {
		ui_error("/set requires argument", NULL);
		return;
	}
	name = strtok_r(str, " ", &val);
	config_set(name, val);
}

void
command_server(struct Server *server, char *str) {
	struct Server *nserver;
	char *tserver, *cmd, *arg;
	int i;

	tserver = strtok_r(str,  " ", &arg);
	cmd     = strtok_r(NULL, " ", &arg);

	if (!tserver || !cmd) {
		ui_error("/server requires 2 arguments", NULL);
		return;
	}

	if ((nserver = serv_get(&servers, tserver)) == NULL) {
		ui_error("no such server: '%s'", tserver);
		return;
	}

	if (*cmd == '/')
		cmd++;

	for (i=0; commands[i].name && commands[i].func; i++) {
		if (strcmp(commands[i].name, cmd) == 0) {
			commands[i].func(nserver, arg);
			return;
		}
	}

	ui_error("no such commands: '%s'", cmd);
}

void
command_help(struct Server *server, char *str) {
	int cmdonly = 0;
	int i, j;

	if (!str) {
		command_help(server, "/help");
		return;
	}

	if (strcmp(str, "commands") == 0) {
		hist_format(main_buf, Activity_status, HIST_SHOW, "SELF_UI :Commands:");
		for (i=0; commands[i].name && commands[i].func; i++)
			hist_format(main_buf, Activity_status, HIST_SHOW, "SELF_UI : %s", commands[i].name);
		return;
	}

	if (strcmp(str, "variables") == 0) {
		hist_format(main_buf, Activity_status, HIST_SHOW, "SELF_UI :Variables:");
		for (i=0; config[i].name; i++)
			hist_format(main_buf, Activity_status, HIST_SHOW, "SELF_UI : %s", config[i].name);
		return;
	}

	if (*str == '/') {
		cmdonly = 1;
		str++;
	}

	for (i=0; commands[i].name && commands[i].func; i++) {
		if (strcmp(commands[i].name, str) == 0) {
			hist_format(main_buf, Activity_status, HIST_SHOW, "SELF_UI :%s", str);
			hist_format(main_buf, Activity_status, HIST_SHOW, "SELF_UI :===");
			for (j=0; commands[i].description[j]; j++)
				hist_format(main_buf, Activity_status, HIST_SHOW, "SELF_UI :%s", commands[i].description[j]);
			hist_format(main_buf, Activity_status, HIST_SHOW, "SELF_UI :");
			return;
		}
	}

	if (!cmdonly) {
		for (i=0; config[i].name; i++) {
			if (strcmp(config[i].name, str) == 0) {
				hist_format(main_buf, Activity_status, HIST_SHOW, "SELF_UI :%s", str);
				hist_format(main_buf, Activity_status, HIST_SHOW, "SELF_UI :===");
				for (j=0; config[i].description[j]; j++)
					hist_format(main_buf, Activity_status, HIST_SHOW, "SELF_UI :%s", config[i].description[j]);
				hist_format(main_buf, Activity_status, HIST_SHOW, "SELF_UI :");
				return;
			}
		}
	}

	ui_error("no help on '%s'", str);
}

int
command_getopt(char **str, struct CommandOpts *opts) {
	char *opt;

	if (!str || !*str || **str != '-')
		return opt_done;

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
				*((*str)-1) = '\0';
			}

			return opts->ret;
		}
	}

	ui_error("no such option '%s'", opt);
	return opt_error;
}

void
command_eval(char *str) {
	struct Command *cmdp;
	char msg[512];
	char *cmd;
	char *s;

	if (*str != '/' || strncmp(str, "/ /", sizeof("/ /")) == 0) {
		/* Provide a way to escape commands
		 *      "/ /cmd" --> "/cmd"      */
		if (strncmp(str, "/ /", sizeof("/ /")) == 0)
			str += 3;

		if (selected.channel && selected.server) {
			// TODO: message splitting
			snprintf(msg, sizeof(msg), "PRIVMSG %s :%s", selected.channel->name, str);
			ircprintf(selected.server, "%s\r\n", msg);
			hist_format(selected.channel->history, Activity_self, HIST_SHOW|HIST_LOG|HIST_SELF, msg);
		} else
			ui_error("channel not selected, message ignored", NULL);

		return;
	} else {
		str++;
		cmd = str;
		str = strchr(str, ' ');
		if (str && *str) {
			*str = '\0';
			str++;
			if (*str == '\0')
				str = NULL;
		}

		for (cmdp = commands; cmdp->name && cmdp->func; cmdp++) {
			if (strcmp(cmdp->name, cmd) == 0) {
				cmdp->func(selected.server, str);
				return;
			}
		}

		ui_error("no such command: '%s'", cmd);
	}
}
