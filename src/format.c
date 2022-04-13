/*
 * src/format.c from hirc
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

#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include "hirc.h"

struct {
	char *cmd;
	char *format;
} formatmap[] = {
	/* SELF_ commands from UI */
	{"SELF_ERROR",		"format.ui.error"},
	{"SELF_UI",		"format.ui.misc"},
	{"SELF_CONNECTLOST",	"format.ui.connectlost"},
	{"SELF_CONNECTING",	"format.ui.connecting"},
	{"SELF_CONNECTED",	"format.ui.connected"},
	{"SELF_LOOKUPFAIL",	"format.ui.lookupfail"},
	{"SELF_CONNECTFAIL",	"format.ui.connectfail"},
#ifndef TLS
	{"SELF_TLSNOTCOMPILED",	"format.ui.tls.notcompiled"},
#else
	{"SELF_TLS_VERSION",	"format.ui.tls.version"},
	{"SELF_TLS_NAMES",	"format.ui.tls.names"},
#endif /* TLS */
	{"SELF_KEYBIND_START",	"format.ui.keybind.start"},
	{"SELF_KEYBIND_LIST",	"format.ui.keybind"},
	{"SELF_KEYBIND_END",	"format.ui.keybind.end"},
	{"SELF_GREP_START",	"format.ui.grep.start"},
	{"SELF_GREP_END",	"format.ui.grep.end"},
	{"SELF_ALIAS_START",	"format.ui.alias.start"},
	{"SELF_ALIAS_LIST",	"format.ui.alias"},
	{"SELF_ALIAS_END",	"format.ui.alias.end"},
	{"SELF_HELP_START",	"format.ui.help.start"},
	{"SELF_HELP",		"format.ui.help"},
	{"SELF_HELP_END",	"format.ui.help.end"},
	{"SELF_AUTOCMDS_START",	"format.ui.autocmds.start"},
	{"SELF_AUTOCMDS_LIST",	"format.ui.autocmds"},
	{"SELF_AUTOCMDS_END",	"format.ui.autocmds.end"},
	{"SELF_LOG_RESTORE",	"format.ui.logrestore"},
	{"SELF_UNREAD",		"format.ui.unread"},
	{"SELF_IGNORES_START",	"format.ui.ignores.start"},
	{"SELF_IGNORES_LIST",	"format.ui.ignores"},
	{"SELF_IGNORES_ADDED",	"format.ui.ignores.added"},
	{"SELF_IGNORES_END",	"format.ui.ignores.end"},
	/* Real commands/numerics from server */
	{"PRIVMSG", 		"format.privmsg"},
	{"NOTICE",		"format.notice"},
	{"JOIN",		"format.join"},
	{"PART",		"format.part"},
	{"KICK",		"format.kick"},
	{"QUIT",		"format.quit"},
	{"NICK",		"format.nick"},
	{"TOPIC",		"format.topic"},
	{"INVITE",		"format.invite"},
	{"PONG",		"format.pong"},
	{"ERROR",		"format.error"},
	/* START: misc/rpl-ui-gen.awk */
	{"200",			"format.rpl.tracelink"},
	{"201",			"format.rpl.traceconnecting"},
	{"202",			"format.rpl.tracehandshake"},
	{"203",			"format.rpl.traceunknown"},
	{"204",			"format.rpl.traceoperator"},
	{"205",			"format.rpl.traceuser"},
	{"206",			"format.rpl.traceserver"},
	{"208",			"format.rpl.tracenewtype"},
	{"209",			"format.rpl.traceclass"},
	{"211",			"format.rpl.statslinkinfo"},
	{"212",			"format.rpl.statscommands"},
	{"213",			"format.rpl.statscline"},
	{"214",			"format.rpl.statsnline"},
	{"215",			"format.rpl.statsiline"},
	{"216",			"format.rpl.statskline"},
	{"218",			"format.rpl.statsyline"},
	{"219",			"format.rpl.endofstats"},
	{"221",			"format.rpl.umodeis"},
	{"231",			"format.rpl.serviceinfo"},
	{"233",			"format.rpl.service"},
	{"235",			"format.rpl.servlistend"},
	{"241",			"format.rpl.statslline"},
	{"242",			"format.rpl.statsuptime"},
	{"243",			"format.rpl.statsoline"},
	{"244",			"format.rpl.statshline"},
	{"251",			"format.rpl.luserclient"},
	{"252",			"format.rpl.luserop"},
	{"253",			"format.rpl.luserunknown"},
	{"254",			"format.rpl.luserchannels"},
	{"255",			"format.rpl.luserme"},
	{"256",			"format.rpl.adminme"},
	{"257",			"format.rpl.adminloc1"},
	{"258",			"format.rpl.adminloc2"},
	{"259",			"format.rpl.adminemail"},
	{"261",			"format.rpl.tracelog"},
	{"300",			"format.rpl.none"},
	{"301",			"format.rpl.away"},
	{"302",			"format.rpl.userhost"},
	{"303",			"format.rpl.ison"},
	{"305",			"format.rpl.unaway"},
	{"306",			"format.rpl.nowaway"},
	{"311",			"format.rpl.whoisuser"},
	{"312",			"format.rpl.whoisserver"},
	{"313",			"format.rpl.whoisoperator"},
	{"314",			"format.rpl.whowasuser"},
	{"315",			"format.rpl.endofwho"},
	{"316",			"format.rpl.whoischanop"},
	{"317",			"format.rpl.whoisidle"},
	{"318",			"format.rpl.endofwhois"},
	{"319",			"format.rpl.whoischannels"},
	{"321",			"format.rpl.liststart"},
	{"322",			"format.rpl.list"},
	{"323",			"format.rpl.listend"},
	{"324",			"format.rpl.channelmodeis"},
	{"331",			"format.rpl.notopic"},
	{"332",			"format.rpl.topic"},
	{"341",			"format.rpl.inviting"},
	{"342",			"format.rpl.summoning"},
	{"351",			"format.rpl.version"},
	{"352",			"format.rpl.whoreply"},
	{"353",			"format.rpl.namreply"},
	{"362",			"format.rpl.closing"},
	{"364",			"format.rpl.links"},
	{"365",			"format.rpl.endoflinks"},
	{"366",			"format.rpl.endofnames"},
	{"367",			"format.rpl.banlist"},
	{"368",			"format.rpl.endofbanlist"},
	{"369",			"format.rpl.endofwhowas"},
	{"371",			"format.rpl.info"},
	{"372",			"format.rpl.motd"},
	{"373",			"format.rpl.infostart"},
	{"374",			"format.rpl.endofinfo"},
	{"375",			"format.rpl.motdstart"},
	{"376",			"format.rpl.endofmotd"},
	{"381",			"format.rpl.youreoper"},
	{"382",			"format.rpl.rehashing"},
	{"391",			"format.rpl.time"},
	{"392",			"format.rpl.usersstart"},
	{"393",			"format.rpl.users"},
	{"394",			"format.rpl.endofusers"},
	{"395",			"format.rpl.nousers"},
	{"401",			"format.err.nosuchnick"},
	{"402",			"format.err.nosuchserver"},
	{"403",			"format.err.nosuchchannel"},
	{"404",			"format.err.cannotsendtochan"},
	{"405",			"format.err.toomanychannels"},
	{"406",			"format.err.wasnosuchnick"},
	{"407",			"format.err.toomanytargets"},
	{"409",			"format.err.noorigin"},
	{"411",			"format.err.norecipient"},
	{"412",			"format.err.notexttosend"},
	{"413",			"format.err.notoplevel"},
	{"414",			"format.err.wildtoplevel"},
	{"421",			"format.err.unknowncommand"},
	{"422",			"format.err.nomotd"},
	{"423",			"format.err.noadmininfo"},
	{"424",			"format.err.fileerror"},
	{"431",			"format.err.nonicknamegiven"},
	{"432",			"format.err.erroneusnickname"},
	{"433",			"format.err.nicknameinuse"},
	{"436",			"format.err.nickcollision"},
	{"441",			"format.err.usernotinchannel"},
	{"442",			"format.err.notonchannel"},
	{"443",			"format.err.useronchannel"},
	{"444",			"format.err.nologin"},
	{"445",			"format.err.summondisabled"},
	{"446",			"format.err.usersdisabled"},
	{"451",			"format.err.notregistered"},
	{"461",			"format.err.needmoreparams"},
	{"462",			"format.err.alreadyregistred"},
	{"463",			"format.err.nopermforhost"},
	{"464",			"format.err.passwdmismatch"},
	{"465",			"format.err.yourebannedcreep"},
	{"466",			"format.err.youwillbebanned"},
	{"467",			"format.err.keyset"},
	{"471",			"format.err.channelisfull"},
	{"472",			"format.err.unknownmode"},
	{"473",			"format.err.inviteonlychan"},
	{"474",			"format.err.bannedfromchan"},
	{"475",			"format.err.badchannelkey"},
	{"481",			"format.err.noprivileges"},
	{"482",			"format.err.chanoprivsneeded"},
	{"483",			"format.err.cantkillserver"},
	{"491",			"format.err.nooperhost"},
	{"492",			"format.err.noservicehost"},
	{"501",			"format.err.umodeunknownflag"},
	{"502",			"format.err.usersdontmatch"},
	/* END: misc/rpl-ui-gen.awk */
	/* Modern stuff */
	{"001",			"format.rpl.welcome"},
	{"002",			"format.rpl.yourhost"},
	{"003",			"format.rpl.created"},
	{"004",			"format.rpl.myinfo"},
	{"005",			"format.rpl.isupport"},
	{"006",			"format.rpl.map"},    /* I'm not so sure if 006 and 007 */
	{"007",			"format.rpl.mapend"}, /* are really exclusive to /map   */
	{"265",			"format.rpl.localusers"},
	{"266",			"format.rpl.globalusers"},
	{"320",			"format.rpl.whoisspecial"},
	{"330",			"format.rpl.whoisaccount"},
	{"333",			"format.rpl.topicwhotime"},
	{"338",			"format.rpl.whoisactually"},
	{"378",			"format.rpl.whoishost"},
	{"379",			"format.rpl.whoismodes"},
	{"671",			"format.rpl.whoissecure"},
	/* Pseudo commands for specific formatting */
	{"MODE-NICK-SELF",	"format.mode.nick.self"},
	{"MODE-NICK",		"format.mode.nick"},
	{"MODE-CHANNEL",	"format.mode.channel"},
	{"PRIVMSG-ACTION",	"format.action"},
	{"PRIVMSG-CTCP",	"format.ctcp.request"},
	{"NOTICE-CTCP",		"format.ctcp.answer"},
	{NULL,			NULL},
};

char *
format_get_bufact(int activity) {
	switch (activity) {
	case Activity_status:
		return format(NULL, config_gets("format.ui.buflist.activity.status"), NULL);
	case Activity_error:
		return format(NULL, config_gets("format.ui.buflist.activity.error"), NULL);
	case Activity_message:
		return format(NULL, config_gets("format.ui.buflist.activity.message"), NULL);
	case Activity_hilight:
		return format(NULL, config_gets("format.ui.buflist.activity.hilight"), NULL);
	default:
		return format(NULL, config_gets("format.ui.buflist.activity.none"), NULL);
	}

	return NULL; /* shouldn't be possible *shrug*/
}

char *
format_get(struct History *hist) {
	char *cmd, *p1, *p2;
	int i;

	if (!hist)
		return NULL;

	if (!hist->params)
		goto raw;

	cmd = *(hist->params);
	p1 = *(hist->params+1);
	p2 = *(hist->params+2);

	if (strcmp_n(cmd, "MODE") == 0) {
		if (p1 && serv_ischannel(hist->origin->server, p1))
			cmd = "MODE-CHANNEL";
		else if (hist->from && nick_isself(hist->from) && strcmp_n(hist->from->nick, p1) == 0)
			cmd = "MODE-NICK-SELF";
		else
			cmd = "MODE-NICK";
	} else if (strcmp_n(cmd, "PRIVMSG") == 0) {
		/* ascii 1 is ^A */
		if (*p2 == 1 && strncmp(p2 + 1, "ACTION", CONSTLEN("ACTION")) == 0)
			cmd = "PRIVMSG-ACTION";
		else if (*p2 == 1)
			cmd = "PRIVMSG-CTCP";
	} else if (strcmp_n(cmd, "NOTICE") == 0 && *p2 == 1) {
		cmd = "NOTICE-CTCP";
	}

	for (i=0; formatmap[i].cmd; i++)
		if (formatmap[i].format && strcmp_n(formatmap[i].cmd, cmd) == 0)
			return formatmap[i].format;

	if (isdigit(*cmd) && isdigit(*(cmd+1)) && isdigit(*(cmd+2)) && !*(cmd+3))
		return "format.rpl.other";

raw:
	return "format.other";
}

static char *
format_get_content(char *sstr, int nesting) {
	static char ret[8192];
	int layer, rc;

	for (layer = 0, rc = 0; sstr && *sstr && rc < sizeof(ret); sstr++) {
		switch (*sstr) {
		case '}':
			if (nesting && layer) {
				ret[rc++] = '}';
				layer--;
			} else {
				goto end;
			}
			break;
		case '{':
			if (nesting)
				layer++;
			ret[rc++] = '{';
			break;
		default:
			ret[rc++] = *sstr;
			break;
		}
	}

end:
	ret[rc] = '\0';
	return ret;
}

char *
format_(struct Window *window, char *format, struct History *hist, int recursive) {
	static char *ret;
	struct Nick *nick;
	size_t rs = BUFSIZ;
	size_t rc, pc;
	int escape, i;
	long long pn;
	int rhs = 0;
	int divider = 0;
	char **params;
	char *content, *p, *p2;
	char *ts, *save;
	char colourbuf[2][3];
	char priv[2];
	char chs[2];
	size_t len;
	enum {
		sub_raw,
		sub_cmd,
		sub_nick,
		sub_ident,
		sub_host,
		sub_priv,
		sub_channel,
		sub_topic,
		sub_server,
		sub_time,
	};
	struct {
		char *name;
		char *val;
	} subs[] = {
		[sub_raw]	= {"raw", NULL},
		[sub_cmd]	= {"cmd", NULL},
		[sub_nick]	= {"nick", NULL},
		[sub_ident]	= {"ident", NULL},
		[sub_host]	= {"host", NULL},
		[sub_priv]      = {"priv", NULL},
		[sub_channel]	= {"channel", NULL},
		[sub_topic]	= {"topic", NULL},
		[sub_server]	= {"server", NULL},
		[sub_time]	= {"time", NULL},
		{NULL, NULL},
	};

	if (!format)
		format = config_gets(format_get(hist));
	if (!format)
		return NULL;

	pfree(&ret);
	ret = emalloc(rs);

	subs[sub_channel].val = selected.channel ? selected.channel->name  : NULL;
	subs[sub_topic].val   = selected.channel ? selected.channel->topic : NULL;
	subs[sub_server].val  = selected.server  ? selected.server->name   : NULL;

	if (hist) {
		subs[sub_raw].val   = hist->raw;
		subs[sub_nick].val  = hist->from ? hist->from->nick  : NULL;
		subs[sub_ident].val = hist->from ? hist->from->ident : NULL;
		subs[sub_host].val  = hist->from ? hist->from->host  : NULL;

		if (hist->from) {
			priv[0] = hist->from->priv;
			priv[priv[0] != ' '] = '\0';
			subs[sub_priv].val = priv;
		}

		if (hist->origin) {
			if (hist->origin->channel) {
				if (!recursive)
					divider = config_getl("divider.toggle");
				subs[sub_channel].val = hist->origin->channel->name;
				subs[sub_topic].val   = hist->origin->channel->topic;
			}
			if (hist->origin->server) {
				subs[sub_server].val  = hist->origin->server->name;
			}
		}

		len = snprintf(subs[sub_time].val, 0, "%lld", (long long)hist->timestamp) + 1;
		subs[sub_time].val = emalloc(len);
		snprintf(subs[sub_time].val, len, "%lld", (long long)hist->timestamp);

		params = hist->params;
		subs[sub_cmd].val = *params;
		params++;
	}

	if (!recursive && hist && config_getl("timestamp.toggle")) {
		ts = estrdup(format_(NULL, config_gets("format.ui.timestamp"), hist, 1));
	} else {
		ts = "";
	}

	for (escape = 0, rc = 0; format && *format && rc < rs; ) {
outcont:
		if (rc > rs / 2) {
			rs *= 2;
			ret = erealloc(ret, rs);
		}

		if (!escape && *format == '$' && *(format+1) == '{' && strchr(format, '}')) {
			escape = 0;
			content = format_get_content(format+2, 0);

			for (p = content; *p && isdigit(*p); p++);
			/* If all are digits, *p == '\0' */
			if (!*p && hist) {
				pn = strtol(content, NULL, 10) - 1;
				if (pn >= 0 && param_len(params) > pn) {
					if (**(params+pn) == 1 && strncmp((*(params+pn))+1, "ACTION", CONSTLEN("ACTION")) == 0 && strchr(*(params+pn), ' '))
						rc += snprintf(&ret[rc], rs - rc, "%s", struntil(strchr(*(params+pn), ' ') + 1, 1));
					else if (**(params+pn) == 1)
						rc += snprintf(&ret[rc], rs - rc, "%s", struntil((*(params+pn)) + 1, 1));
					else
						rc += snprintf(&ret[rc], rs - rc, "%s", *(params+pn));
					format = strchr(format, '}') + 1;
					continue;
				}
			}
			/* All are digits except a trailing '-' */
			if (*p == '-' && *(p+1) == '\0' && hist) {
				pn = strtol(content, NULL, 10) - 1;
				if (pn >= 0 && param_len(params) > pn) {
					for (; *(params+pn) != NULL; pn++) {
						if (**(params+pn) == 1 && strncmp((*(params+pn))+1, "ACTION", CONSTLEN("ACTION")) == 0 && strchr(*(params+pn), ' ')) {
							rc += snprintf(&ret[rc], rs - rc, "%s%s",
									struntil(strchr(*(params+pn), ' ') + 1, 1),
									*(params+pn+1) ? " " : "");
						} else if (**(params+pn) == 1) {
							rc += snprintf(&ret[rc], rs - rc, "%s%s",
									struntil((*(params+pn)) + 1, 1),
									*(params+pn+1) ? " " : "");
						} else {
							rc += snprintf(&ret[rc], rs - rc, "%s%s",
									*(params+pn), *(params+pn+1) ? " " : "");
						}
					}
					format = strchr(format, '}') + 1;
					continue;
				}
			}

			for (i=0; subs[i].name; i++) {
				if (strcmp_n(subs[i].name, content) == 0) {
					if (subs[i].val)
						rc += snprintf(&ret[rc], rs - rc, "%s", subs[i].val);
					format = strchr(format, '}') + 1;
					goto outcont; /* unfortunately, need to use a goto as we are already in a loop */
				}
			}
		}

		if (!escape && *format == '%' && *(format+1) == '{' && strchr(format, '}')) {
			escape = 0;
			content = format_get_content(format+2, 0);

			switch (*content) {
			case 'b':
			case 'B':
				ret[rc++] = 2; /* ^B */
				format = strchr(format, '}') + 1;
				continue;
			case 'c':
			case 'C':
				if (*(content+1) == ':' && isdigit(*(content+2))) {
					content += 2;
					memset(colourbuf, 0, sizeof(colourbuf));
					colourbuf[0][0] = *content;
					content++;
					if (isdigit(*content)) {
						colourbuf[0][1] = *content;
						content += 1;
					}
					if (*content == ',' && isdigit(*(content+1))) {
						colourbuf[1][0] = *(content+1);
						content += 2;
					}
					if (colourbuf[1][0] && isdigit(*content)) {
						colourbuf[1][1] = *(content);
						content += 1;
					}
					if (*content == '\0') {
						rc += snprintf(&ret[rc], rs - rc, "%c%02d,%02d", 3 /* ^C */,
								atoi(colourbuf[0]), colourbuf[1][0] ? atoi(colourbuf[1]) : 99);
						format = strchr(format, '}') + 1;
						continue;
					}
				}
				break;
			case 'i':
			case 'I':
				if (*(content+1) == '\0') {
					ret[rc++] = 9; /* ^I */
					format = strchr(format, '}') + 1;
					continue;
				}
				break;
			case 'o':
			case 'O':
				if (*(content+1) == '\0') {
					ret[rc++] = 15; /* ^O */
					format = strchr(format, '}') + 1;
					continue;
				}
				break;
			case 'r':
			case 'R':
				if (*(content+1) == '\0') {
					ret[rc++] = 18; /* ^R */
					format = strchr(format, '}') + 1;
					continue;
				}
				break;
			case 'u':
			case 'U':
				if (*(content+1) == '\0') {
					ret[rc++] = 21; /* ^U */
					format = strchr(format, '}') + 1;
					continue;
				}
				break;
			case '=':
				if (*(content+1) == '\0' && divider) {
					rhs = 1;
					ret[rc] = '\0';
					/* strlen(ret) - ui_strlenc(window, ret, NULL) should get
					 * the length of hidden characters. Add this onto the
					 * margin to pad out properly. */
					/* Save ret for use in snprintf */
					save = estrdup(ret);
					rc = snprintf(ret, rs, "%1$*3$s%2$s", save, config_gets("divider.string"),
							config_getl("divider.margin") + (strlen(ret) - ui_strlenc(window, ret, NULL)));
					pfree(&save);
					format = strchr(format, '}') + 1;
					continue;
				} else if (*(content+1) == '\0') {
					ret[rc++] = ' ';
					format = strchr(format, '}') + 1;
					continue;
				}
				break;
			}

			/* pad, nick, split, rdate and time, must then continue as they modify content
			 *
			 * These styling formatters are quite ugly and repetitive. 
			 * %{nick:...} was implemented first, and has the most (all of them :)) comments */
			if (strncmp(content, "pad:", CONSTLEN("pad:")) == 0 && strchr(content, ',')) {
				pn = strtol(content + CONSTLEN("pad:"), NULL, 10);
				content = estrdup(format_get_content(strchr(format+2+CONSTLEN("pad:"), ',') + 1, 1));
				save = ret;
				ret = NULL;
				format_(NULL, content, hist, 1);
				rc += snprintf(&save[rc], rs - rc, "%1$*2$s", ret, pn);
				pfree(&ret);
				ret = save;
				format = strchr(format+2+CONSTLEN("pad:"), ',') + strlen(content) + 2;

				pfree(&content);
				continue;
			}

			if (strncmp(content, "rdate:", CONSTLEN("rdate:")) == 0) {
				content = estrdup(format_get_content(format+2+CONSTLEN("rdate:"), 1));
				save = ret;
				ret = NULL;
				format_(NULL, content, hist, 1);
				pn = strtoll(ret, NULL, 10);
				rc += snprintf(&save[rc], rs - rc, "%s", strrdate((time_t)pn));
				format += 3 + CONSTLEN("rdate:") + strlen(content);

				pfree(&ret);
				ret = save;
				pfree(&content);
				continue;
			}

			if (strncmp(content, "time:", CONSTLEN("time:")) == 0 && strchr(content, ',')) {
				content = estrdup(format_get_content(strchr(format+2+CONSTLEN("time:"), ',') + 1, 1));
				save = ret;
				ret = NULL;
				format_(NULL, content, hist, 1);
				pn = strtoll(ret, NULL, 10);
				p = struntil(format+2+CONSTLEN("time:"), ',');

				rc += strftime(&save[rc], rs - rc, p, localtime((time_t *)&pn));
				format = strchr(format+2+CONSTLEN("time:"), ',') + strlen(content) + 2;

				pfree(&ret);
				ret = save;
				pfree(&content);
				continue;
			}

			/* second comma ptr - second comma ptr = distance.
			 * If the distance is 2, then there is one non-comma char between. */
			p = strchr(content, ',');
			if (p)
				p2 = strchr(p + 1, ',');
			if (strncmp(content, "split:", CONSTLEN("split:")) == 0 && p2 - p == 2) {
				pn = strtol(content + CONSTLEN("split:"), NULL, 10);
				chs[0] = *(strchr(content, ',') + 1);
				chs[1] = '\0';
				content = estrdup(format_get_content(
							strchr(
								strchr(format+2+CONSTLEN("split:"), ',') + 1,
								',') + 1,
							1));
				save = ret;
				ret = NULL;
				format_(NULL, content, hist, 1);
				rc += snprintf(&save[rc], rs - rc, "%s", strntok(ret, chs, pn));
				format = strchr(
					strchr(format+2+CONSTLEN("split:"), ',') + 1,
					',') + strlen(content) + 2;

				pfree(&ret);
				ret = save;
				pfree(&content);
				continue;
			}

			if (hist && !recursive && strncmp(content, "nick:", CONSTLEN("nick:")) == 0) {
				content = estrdup(format_get_content(format+2+CONSTLEN("nick:"), 1));
				save = ret;
				ret = NULL;
				format_(NULL, content, hist, 1);
				nick = nick_create(ret, ' ', hist->origin ? hist->origin->server : NULL);
				rc += snprintf(&save[rc], rs - rc, "%c%02d", 3 /* ^C */, nick_getcolour(nick));
				format += 3 + CONSTLEN("nick:") + strlen(content);

				pfree(&ret);
				ret = save;
				nick_free(nick);
				pfree(&content);
				continue;
			}
		}

		if (escape && *format == 'n') {
			ret[rc++] = '\n';
			rc += snprintf(&ret[rc], rs - rc, "%1$*3$s%2$s", "", config_gets("divider.string"),
					ui_strlenc(NULL, ts, NULL) + config_getl("divider.margin"));
			escape = 0;
			format++;
			continue;
		}

		if (escape && (*format == '%' || *format == '$') && *(format+1) == '{' && strchr(format, '}'))
			escape = 0;

		if (escape) {
			ret[rc++] = '\\';
			escape = 0;
		}

		if (*format == '\\') {
			escape = 1;
			format++;
		} else {
			ret[rc++] = *format;
			format++;
		}
	}

	ret[rc] = '\0';
	if (!recursive && divider && !rhs) {
		save = estrdup(ret);
		rc = snprintf(ret, rs, "%1$*4$s%2$s%3$s", "", config_gets("divider.string"), save, config_getl("divider.margin"));
		pfree(&save);
	}

	save = estrdup(ret);
	rc = snprintf(ret, rs, "%s%s", ts, save);
	pfree(&save);

	if (!recursive && window) {
		for (p = ret, pc = 0; p && p <= (ret + rs); p++) {
			/* lifted from ui_strlenc */
			switch (*p) {
			case 2:  /* ^B */
			case 9:  /* ^I */
			case 15: /* ^O */
			case 18: /* ^R */
			case 21: /* ^U */
				break;
			case 3:  /* ^C */
				if (*p && isdigit(*(p+1)))
					p += 1;
				if (*p && isdigit(*(p+1)))
					p += 1;
				if (*p && *(p+1) == ',' && isdigit(*(p+2)))
					p += 2;
				if (*p && *(p-1) == ',' && isdigit(*(p+1)))
					p += 1;
				break;
			default:
				/* naive utf-8 handling:
				 * the 2-nth byte always
				 * follows 10xxxxxxx, so
				 * don't count it. */
				if ((*p & 0xC0) != 0x80)
					pc++;

				if (*p == '\n') {
					p++;
					pc = 0;
				}

				if (pc == window->w) {
					save = estrdup(p);

					if (divider) {
						p += snprintf(p, rs - ((size_t)(p - ret)), "%1$*4$s %2$s%3$s",
								"", config_gets("divider.string"), save,
								config_getl("divider.margin") + ui_strlenc(NULL, ts, NULL));
					} else {
						p += snprintf(p, rs - ((size_t)(p - ret)), "%1$*3$s %2$s", "", save, ui_strlenc(NULL, ts, NULL));
					}

					pfree(&save);
					pc = 0;
				}
			}
		}
	}

	if (subs[sub_time].val)
		pfree(&subs[sub_time].val);
	if (ts[0] != '\0')
		pfree(&ts);

	return ret;
}
