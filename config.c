/* See LICENSE for copyright details */

#include <ncurses.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include "hirc.h"

int readingconf = 0;

static int config_nicklist_location(long num);
static int config_nicklist_width(long num);
static int config_buflist_location(long num);
static int config_buflist_width(long num);
static int config_nickcolour_self(long num);
static int config_nickcolour_range(long a, long b);
static int config_redrawl(long num);
static int config_redraws(char *str);

char *valname[] = {
	[Val_string] = "a string",
	[Val_bool] = "boolean",
	[Val_colour] = "a number from 0 to 99",
	[Val_signed] = "a numeric value",
	[Val_unsigned] = "positive",
	[Val_nzunsigned] = "greater than zero",
	[Val_pair] = "a pair",
	[Val_colourpair] = "pair with numbers from 0 to 99",
};

struct Config config[] = {
	{"log.dir", 1, Val_string,
		.str = "~/.local/hirc",
		.strhandle = NULL,
		.description = {
		"Directory for hirc to log to.",
		"Can contain ~ to refer to $HOME", NULL}},
	{"log.toggle", 1, Val_bool,
		.num = 1,
		.numhandle = NULL,
		.description = {
		"Simple: to log, or not to log", NULL}},
	{"def.nick", 1, Val_string,
		.str = NULL,
		.strhandle = NULL,
		.description = {
		"Default nickname", NULL}},
	{"def.user", 1, Val_string,
		.str = NULL,
		.strhandle = NULL,
		.description = {
		"Default username (nick!..here..@host), ",
		"may be replaced by identd response", NULL}},
	{"def.real", 1, Val_string,
		.str = NULL,
		.strhandle = NULL,
		.description = {
		"Default \"realname\", seen in /whois", NULL}},
	{"def.chantypes", 1, Val_string,
		.str = "#&!+",
		.strhandle = NULL,
		.description = {
		"You most likely don't want to touch this.",
		"If a server does not supply this in RPL_ISUPPORT,",
		"hirc assumes it will use these channel types.", NULL}},
	{"def.prefixes", 1, Val_string,
		.str = "(ov)@+",
		.strhandle = NULL,
		.description = {
		"You most likely don't want to touch this.",
		"If a server doesn't supply this in the nonstandard",
		"RPL_ISUPPORT, it likely won't support nonstandard",
		"prefixes.", NULL}},
	{"reconnect.interval", 1, Val_nzunsigned,
		.num = 10,
		.numhandle = NULL,
		.description = {
		"Starting reconnect interval in seconds.",
		"In reality, for each attempt this will be multipled",
		"by the number of failed attemps until it reaches",
		"reconnect.maxinterval", NULL}},
	{"reconnect.maxinterval", 1, Val_nzunsigned,
		.num = 600,
		.numhandle = NULL,
		.description = {
		"Maximum reconnect interval in seconds.",
		"See reconnect.interval", NULL}},
	{"nickcolour.self", 1, Val_colour,
		.num = 90,
		.numhandle = config_nickcolour_self,
		.description = {
		"Colour to use for onself.",
		"Must be 0, 99 or anywhere between. 99 is no colours.", NULL}},
	{"nickcolour.range", 1, Val_colourpair,
		.pair = {28, 63},
		.pairhandle = config_nickcolour_range,
		.description = {
		"Range of (mirc extended) colours used to colour nicks",
		"Must be 0, 99 or anywhere between. 99 is no colour",
		"Giving a single value or two identical values will",
		"use that colour only", NULL}},
	{"nicklist.location", 1, Val_unsigned,
		.num = RIGHT,
		.numhandle = config_nicklist_location,
		.description = {
		"Location of nicklist. May be:",
		" 0 - Hidden",
		" 1 - Left",
		" 2 - Right", NULL}},
	{"nicklist.width", 1, Val_nzunsigned,
		.num = 15,
		.numhandle = config_nicklist_width,
		.description = {
		"Number of columns nicklist will take up.", NULL}},
	{"buflist.location", 1, Val_unsigned,
		.num = LEFT,
		.numhandle = config_buflist_location,
		.description = {
		"Location of nicklist. May be:",
		" 0 - Hidden",
		" 1 - Left",
		" 2 - Right", NULL}},
	{"buflist.width", 1, Val_nzunsigned,
		.num = 25,
		.numhandle = config_buflist_width,
		.description = {
		"Number of columns buflist will take up.", NULL}},
	{"misc.topiccolour", 1, Val_colourpair,
		.pair = {99, 89},
		.pairhandle = NULL,
		.description = {
		"Foreground and background colour of topic bar in main window", NULL}},
	{"misc.pingtime", 1, Val_nzunsigned,
		.num = 200,
		.numhandle = NULL,
		.description = {
		"Wait this many seconds since last received message",
		"from server to send PING. If ping.wait seconds",
		"elapses since sending a PING, hirc will consider",
		"the server disconnected.", NULL}},
	{"misc.quitmessage", 1, Val_string,
		.str = "pain is temporary",
		.strhandle = NULL,
		.description = {
		"Message to send on /quit", NULL}},
	{"misc.partmessage", 1, Val_string,
		.str = "pain is temporary",
		.strhandle = NULL,
		.description = {
		"Message to send on /part", NULL}},
	{"divider.toggle", 1, Val_bool,
		.num = 1,
		.numhandle = config_redrawl,
		.description = {
		"Turn divider on/off", NULL}},
	{"divider.margin", 1, Val_nzunsigned,
		.num = 15,
		.numhandle = config_redrawl,
		.description = {
		"Number of columns on the left of the divider", NULL}},
	{"divider.string", 1, Val_string,
		.str = " ",
		.strhandle = config_redraws,
		.description = {
		"String to be used as divider", NULL}},
	{"timestamp.toggle", 1, Val_bool,
		.num = 1,
		.numhandle = config_redrawl,
		.description = {
		"Turn on/off timestamps", NULL}},
	{"format.ui.timestamp", 1, Val_string,
		.str = "%{c:92}${time}%{o} ",
		.strhandle = config_redraws,
		.description = {
		"Format of timestamps",
		"Only shown if timestamp.toggle is on.",
		"This format is special as it is included in others.", NULL}},
	{"format.ui.topic", 1, Val_string,
		.str = "%{c:99,89}${topic}",
		.strhandle = config_redraws,
		.description = {
		"Format of topic at top of main window", NULL}},
	{"format.ui.error", 1, Val_string,
		.str = "%{c:28}%{b}${3} %{b}(at ${1}:${2})",
		.strhandle = config_redraws,
		.description = {
		"Format of SELF_ERROR messages", NULL}},
	{"format.ui.misc", 1, Val_string,
		.str = "${1}",
		.strhandle = config_redraws,
		.description = {
		"Format of SELF_UI messages", NULL}},
	{"format.ui.connectlost", 1, Val_string,
		.str = "Connection to ${1} (${2}:${3}) lost: ${4}",
		.strhandle = config_redraws,
		.description = {
		"Format of SELF_CONNECTLOST messages", NULL}},
	{"format.ui.connecting", 1, Val_string,
		.str = "Connecting to ${1}:${2}",
		.strhandle = config_redraws,
		.description = {
		"Format of SELF_CONNECTING messages", NULL}},
	{"format.ui.connected", 1, Val_string,
		.str = "Connection to ${1} established",
		.strhandle = config_redraws,
		.description = {
		"Format of SELF_CONNECTED messages", NULL}},
	{"format.ui.lookupfail", 1, Val_string,
		.str = "Failed to lookup ${2}: ${4}",
		.strhandle = config_redraws,
		.description = {
		"Format of SELF_LOOKUPFAIL messages", NULL}},
	{"format.ui.connectfail", 1, Val_string,
		.str = "Failed to connect to ${2}:${3}: ${4}",
		.strhandle = config_redraws,
		.description = {
		"Format of SELF_CONNECTFAIL messages", NULL}},
#ifndef TLS
	{"format.ui.tlsnotcompiled", 1, Val_string,
		.str = "TLS not compiled into hirc",
		.strhandle = config_redraws,
		.description = {
		"Format of SELF_TLSNOTCOMPILED messages", NULL}},
#endif /* TLS */
	{"format.privmsg", 1, Val_string,
		.str = "%{nick:${nick}}${nick}%{o}%{=}${2}",
		.strhandle = config_redraws,
		.description = {
		"Format of messages", NULL}},
	{"format.join", 1, Val_string,
		.str = "%{b}%{c:44}+%{o}%{=}%{nick:${nick}}${nick}%{o} (${ident}@${host})",
		.strhandle = config_redraws,
		.description = {
		"Format of JOIN messages", NULL}},
	{"format.quit", 1, Val_string,
		.str = "%{b}%{c:40}<%{o}%{=}%{nick:${nick}}${nick}%{o} (${ident}@${host}): ${1}",
		.strhandle = config_redraws,
		.description = {
		"Format of QUIT messages", NULL}},
	{"format.part", 1, Val_string,
		.str = "%{b}%{c:40}-%{o}%{=}%{nick:${nick}}${nick}%{o} (${ident}@${host}): ${2}",
		.strhandle = config_redraws,
		.description = {
		"Format of PART messages", NULL}},
	{"format.kick", 1, Val_string,
		.str = "%{b}%{c:40}!%{o}%{=}%{nick:${2}}${2}${o} by %{nick:${nick}}${nick}%{o} (${ident}@${host}): ${3}",
		.strhandle = config_redraws,
		.description = {
		"Format of PART messages", NULL}},
	{"format.mode.nick.self", 1, Val_string,
		.str = "${1} set %{c:94}${2-}%{o}",
		.strhandle = config_redraws,
		.description = {
		"Format of modes being set on self by server/self", NULL}},
	{"format.mode.nick", 1, Val_string,
		.str = "${1} set %{c:94}${2-}%{o} by ${nick} (${ident}@${host})",
		.strhandle = config_redraws,
		.description = {
		"Format of modes being on nicks", NULL}},
	{"format.mode.channel", 1, Val_string,
		.str = "mode%{=}%{c:94}${2-}%{o} by %{nick:${nick}}${nick}",
		.strhandle = config_redraws,
		.description = {
		"Format of modes being set on channels", NULL}},
	/* Generic numerics (bit boring) */
	{"format.rpl.welcome", 1, Val_string,
		.str = "${2-}",
		.strhandle = config_redraws,
		.description = {
		"Format of RPL_WELCOME (001) numeric", NULL}},
	{"format.rpl.yourhost", 1, Val_string,
		.str = "${2-}",
		.strhandle = config_redraws,
		.description = {
		"Format of RPL_YOURHOST (002) numeric", NULL}},
	{"format.rpl.created", 1, Val_string,
		.str = "${2-}",
		.strhandle = config_redraws,
		.description = {
		"Format of RPL_CREATED (003) numeric", NULL}},
	{"format.rpl.myinfo", 1, Val_string,
		.str = "${2-}",
		.strhandle = config_redraws,
		.description = {
		"Format of RPL_MYINFO (004) numeric", NULL}},
	{"format.rpl.isupport", 1, Val_string,
		.str = "${2-}",
		.strhandle = config_redraws,
		.description = {
		"Format of RPL_MYSUPPORT (005) numeric", NULL}},
	/* START: misc/rpl-conf-gen.awk */
	{"format.rpl.tracelink", 1, Val_string,
		.str = "${2-}",
		.strhandle = config_redraws,
		.description = {
		"Format of RPL_TRACELINK (200) numeric", NULL}},
	{"format.rpl.traceconnecting", 1, Val_string,
		.str = "${2-}",
		.strhandle = config_redraws,
		.description = {
		"Format of RPL_TRACECONNECTING (201) numeric", NULL}},
	{"format.rpl.tracehandshake", 1, Val_string,
		.str = "${2-}",
		.strhandle = config_redraws,
		.description = {
		"Format of RPL_TRACEHANDSHAKE (202) numeric", NULL}},
	{"format.rpl.traceunknown", 1, Val_string,
		.str = "${2-}",
		.strhandle = config_redraws,
		.description = {
		"Format of RPL_TRACEUNKNOWN (203) numeric", NULL}},
	{"format.rpl.traceoperator", 1, Val_string,
		.str = "${2-}",
		.strhandle = config_redraws,
		.description = {
		"Format of RPL_TRACEOPERATOR (204) numeric", NULL}},
	{"format.rpl.traceuser", 1, Val_string,
		.str = "${2-}",
		.strhandle = config_redraws,
		.description = {
		"Format of RPL_TRACEUSER (205) numeric", NULL}},
	{"format.rpl.traceserver", 1, Val_string,
		.str = "${2-}",
		.strhandle = config_redraws,
		.description = {
		"Format of RPL_TRACESERVER (206) numeric", NULL}},
	{"format.rpl.tracenewtype", 1, Val_string,
		.str = "${2-}",
		.strhandle = config_redraws,
		.description = {
		"Format of RPL_TRACENEWTYPE (208) numeric", NULL}},
	{"format.rpl.traceclass", 1, Val_string,
		.str = "${2-}",
		.strhandle = config_redraws,
		.description = {
		"Format of RPL_TRACECLASS (209) numeric", NULL}},
	{"format.rpl.statslinkinfo", 1, Val_string,
		.str = "${2-}",
		.strhandle = config_redraws,
		.description = {
		"Format of RPL_STATSLINKINFO (211) numeric", NULL}},
	{"format.rpl.statscommands", 1, Val_string,
		.str = "${2-}",
		.strhandle = config_redraws,
		.description = {
		"Format of RPL_STATSCOMMANDS (212) numeric", NULL}},
	{"format.rpl.statscline", 1, Val_string,
		.str = "${2-}",
		.strhandle = config_redraws,
		.description = {
		"Format of RPL_STATSCLINE (213) numeric", NULL}},
	{"format.rpl.statsnline", 1, Val_string,
		.str = "${2-}",
		.strhandle = config_redraws,
		.description = {
		"Format of RPL_STATSNLINE (214) numeric", NULL}},
	{"format.rpl.statsiline", 1, Val_string,
		.str = "${2-}",
		.strhandle = config_redraws,
		.description = {
		"Format of RPL_STATSILINE (215) numeric", NULL}},
	{"format.rpl.statskline", 1, Val_string,
		.str = "${2-}",
		.strhandle = config_redraws,
		.description = {
		"Format of RPL_STATSKLINE (216) numeric", NULL}},
	{"format.rpl.statsyline", 1, Val_string,
		.str = "${2-}",
		.strhandle = config_redraws,
		.description = {
		"Format of RPL_STATSYLINE (218) numeric", NULL}},
	{"format.rpl.endofstats", 1, Val_string,
		.str = "${2-}",
		.strhandle = config_redraws,
		.description = {
		"Format of RPL_ENDOFSTATS (219) numeric", NULL}},
	{"format.rpl.umodeis", 1, Val_string,
		.str = "${2-}",
		.strhandle = config_redraws,
		.description = {
		"Format of RPL_UMODEIS (221) numeric", NULL}},
	{"format.rpl.serviceinfo", 1, Val_string,
		.str = "${2-}",
		.strhandle = config_redraws,
		.description = {
		"Format of RPL_SERVICEINFO (231) numeric", NULL}},
	{"format.rpl.service", 1, Val_string,
		.str = "${2-}",
		.strhandle = config_redraws,
		.description = {
		"Format of RPL_SERVICE (233) numeric", NULL}},
	{"format.rpl.servlistend", 1, Val_string,
		.str = "${2-}",
		.strhandle = config_redraws,
		.description = {
		"Format of RPL_SERVLISTEND (235) numeric", NULL}},
	{"format.rpl.statslline", 1, Val_string,
		.str = "${2-}",
		.strhandle = config_redraws,
		.description = {
		"Format of RPL_STATSLLINE (241) numeric", NULL}},
	{"format.rpl.statsuptime", 1, Val_string,
		.str = "${2-}",
		.strhandle = config_redraws,
		.description = {
		"Format of RPL_STATSUPTIME (242) numeric", NULL}},
	{"format.rpl.statsoline", 1, Val_string,
		.str = "${2-}",
		.strhandle = config_redraws,
		.description = {
		"Format of RPL_STATSOLINE (243) numeric", NULL}},
	{"format.rpl.statshline", 1, Val_string,
		.str = "${2-}",
		.strhandle = config_redraws,
		.description = {
		"Format of RPL_STATSHLINE (244) numeric", NULL}},
	{"format.rpl.luserclient", 1, Val_string,
		.str = "${2-}",
		.strhandle = config_redraws,
		.description = {
		"Format of RPL_LUSERCLIENT (251) numeric", NULL}},
	{"format.rpl.luserop", 1, Val_string,
		.str = "${2-}",
		.strhandle = config_redraws,
		.description = {
		"Format of RPL_LUSEROP (252) numeric", NULL}},
	{"format.rpl.luserunknown", 1, Val_string,
		.str = "${2-}",
		.strhandle = config_redraws,
		.description = {
		"Format of RPL_LUSERUNKNOWN (253) numeric", NULL}},
	{"format.rpl.luserchannels", 1, Val_string,
		.str = "${2-}",
		.strhandle = config_redraws,
		.description = {
		"Format of RPL_LUSERCHANNELS (254) numeric", NULL}},
	{"format.rpl.luserme", 1, Val_string,
		.str = "${2-}",
		.strhandle = config_redraws,
		.description = {
		"Format of RPL_LUSERME (255) numeric", NULL}},
	{"format.rpl.adminme", 1, Val_string,
		.str = "${2-}",
		.strhandle = config_redraws,
		.description = {
		"Format of RPL_ADMINME (256) numeric", NULL}},
	{"format.rpl.adminloc1", 1, Val_string,
		.str = "${2-}",
		.strhandle = config_redraws,
		.description = {
		"Format of RPL_ADMINLOC1 (257) numeric", NULL}},
	{"format.rpl.adminloc2", 1, Val_string,
		.str = "${2-}",
		.strhandle = config_redraws,
		.description = {
		"Format of RPL_ADMINLOC2 (258) numeric", NULL}},
	{"format.rpl.adminemail", 1, Val_string,
		.str = "${2-}",
		.strhandle = config_redraws,
		.description = {
		"Format of RPL_ADMINEMAIL (259) numeric", NULL}},
	{"format.rpl.tracelog", 1, Val_string,
		.str = "${2-}",
		.strhandle = config_redraws,
		.description = {
		"Format of RPL_TRACELOG (261) numeric", NULL}},
	{"format.rpl.none", 1, Val_string,
		.str = "${2-}",
		.strhandle = config_redraws,
		.description = {
		"Format of RPL_NONE (300) numeric", NULL}},
	{"format.rpl.away", 1, Val_string,
		.str = "${2-}",
		.strhandle = config_redraws,
		.description = {
		"Format of RPL_AWAY (301) numeric", NULL}},
	{"format.rpl.userhost", 1, Val_string,
		.str = "${2-}",
		.strhandle = config_redraws,
		.description = {
		"Format of RPL_USERHOST (302) numeric", NULL}},
	{"format.rpl.ison", 1, Val_string,
		.str = "${2-}",
		.strhandle = config_redraws,
		.description = {
		"Format of RPL_ISON (303) numeric", NULL}},
	{"format.rpl.unaway", 1, Val_string,
		.str = "${2-}",
		.strhandle = config_redraws,
		.description = {
		"Format of RPL_UNAWAY (305) numeric", NULL}},
	{"format.rpl.nowaway", 1, Val_string,
		.str = "${2-}",
		.strhandle = config_redraws,
		.description = {
		"Format of RPL_NOWAWAY (306) numeric", NULL}},
	{"format.rpl.whoisuser", 1, Val_string,
		.str = "${2-}",
		.strhandle = config_redraws,
		.description = {
		"Format of RPL_WHOISUSER (311) numeric", NULL}},
	{"format.rpl.whoisserver", 1, Val_string,
		.str = "${2-}",
		.strhandle = config_redraws,
		.description = {
		"Format of RPL_WHOISSERVER (312) numeric", NULL}},
	{"format.rpl.whoisoperator", 1, Val_string,
		.str = "${2-}",
		.strhandle = config_redraws,
		.description = {
		"Format of RPL_WHOISOPERATOR (313) numeric", NULL}},
	{"format.rpl.whowasuser", 1, Val_string,
		.str = "${2-}",
		.strhandle = config_redraws,
		.description = {
		"Format of RPL_WHOWASUSER (314) numeric", NULL}},
	{"format.rpl.endofwho", 1, Val_string,
		.str = "${2-}",
		.strhandle = config_redraws,
		.description = {
		"Format of RPL_ENDOFWHO (315) numeric", NULL}},
	{"format.rpl.whoischanop", 1, Val_string,
		.str = "${2-}",
		.strhandle = config_redraws,
		.description = {
		"Format of RPL_WHOISCHANOP (316) numeric", NULL}},
	{"format.rpl.whoisidle", 1, Val_string,
		.str = "${2-}",
		.strhandle = config_redraws,
		.description = {
		"Format of RPL_WHOISIDLE (317) numeric", NULL}},
	{"format.rpl.endofwhois", 1, Val_string,
		.str = "${2-}",
		.strhandle = config_redraws,
		.description = {
		"Format of RPL_ENDOFWHOIS (318) numeric", NULL}},
	{"format.rpl.whoischannels", 1, Val_string,
		.str = "${2-}",
		.strhandle = config_redraws,
		.description = {
		"Format of RPL_WHOISCHANNELS (319) numeric", NULL}},
	{"format.rpl.liststart", 1, Val_string,
		.str = "${2-}",
		.strhandle = config_redraws,
		.description = {
		"Format of RPL_LISTSTART (321) numeric", NULL}},
	{"format.rpl.list", 1, Val_string,
		.str = "${2-}",
		.strhandle = config_redraws,
		.description = {
		"Format of RPL_LIST (322) numeric", NULL}},
	{"format.rpl.listend", 1, Val_string,
		.str = "${2-}",
		.strhandle = config_redraws,
		.description = {
		"Format of RPL_LISTEND (323) numeric", NULL}},
	{"format.rpl.channelmodeis", 1, Val_string,
		.str = "${2-}",
		.strhandle = config_redraws,
		.description = {
		"Format of RPL_CHANNELMODEIS (324) numeric", NULL}},
	{"format.rpl.notopic", 1, Val_string,
		.str = "${2-}",
		.strhandle = config_redraws,
		.description = {
		"Format of RPL_NOTOPIC (331) numeric", NULL}},
	{"format.rpl.topic", 1, Val_string,
		.str = "${2-}",
		.strhandle = config_redraws,
		.description = {
		"Format of RPL_TOPIC (332) numeric", NULL}},
	{"format.rpl.inviting", 1, Val_string,
		.str = "${2-}",
		.strhandle = config_redraws,
		.description = {
		"Format of RPL_INVITING (341) numeric", NULL}},
	{"format.rpl.summoning", 1, Val_string,
		.str = "${2-}",
		.strhandle = config_redraws,
		.description = {
		"Format of RPL_SUMMONING (342) numeric", NULL}},
	{"format.rpl.version", 1, Val_string,
		.str = "${2-}",
		.strhandle = config_redraws,
		.description = {
		"Format of RPL_VERSION (351) numeric", NULL}},
	{"format.rpl.whoreply", 1, Val_string,
		.str = "${2-}",
		.strhandle = config_redraws,
		.description = {
		"Format of RPL_WHOREPLY (352) numeric", NULL}},
	{"format.rpl.namreply", 1, Val_string,
		.str = "${2-}",
		.strhandle = config_redraws,
		.description = {
		"Format of RPL_NAMREPLY (353) numeric", NULL}},
	{"format.rpl.closing", 1, Val_string,
		.str = "${2-}",
		.strhandle = config_redraws,
		.description = {
		"Format of RPL_CLOSING (362) numeric", NULL}},
	{"format.rpl.links", 1, Val_string,
		.str = "${2-}",
		.strhandle = config_redraws,
		.description = {
		"Format of RPL_LINKS (364) numeric", NULL}},
	{"format.rpl.endoflinks", 1, Val_string,
		.str = "${2-}",
		.strhandle = config_redraws,
		.description = {
		"Format of RPL_ENDOFLINKS (365) numeric", NULL}},
	{"format.rpl.endofnames", 1, Val_string,
		.str = "${2-}",
		.strhandle = config_redraws,
		.description = {
		"Format of RPL_ENDOFNAMES (366) numeric", NULL}},
	{"format.rpl.banlist", 1, Val_string,
		.str = "${2-}",
		.strhandle = config_redraws,
		.description = {
		"Format of RPL_BANLIST (367) numeric", NULL}},
	{"format.rpl.endofbanlist", 1, Val_string,
		.str = "${2-}",
		.strhandle = config_redraws,
		.description = {
		"Format of RPL_ENDOFBANLIST (368) numeric", NULL}},
	{"format.rpl.endofwhowas", 1, Val_string,
		.str = "${2-}",
		.strhandle = config_redraws,
		.description = {
		"Format of RPL_ENDOFWHOWAS (369) numeric", NULL}},
	{"format.rpl.info", 1, Val_string,
		.str = "${2-}",
		.strhandle = config_redraws,
		.description = {
		"Format of RPL_INFO (371) numeric", NULL}},
	{"format.rpl.motd", 1, Val_string,
		.str = "${2-}",
		.strhandle = config_redraws,
		.description = {
		"Format of RPL_MOTD (372) numeric", NULL}},
	{"format.rpl.infostart", 1, Val_string,
		.str = "${2-}",
		.strhandle = config_redraws,
		.description = {
		"Format of RPL_INFOSTART (373) numeric", NULL}},
	{"format.rpl.endofinfo", 1, Val_string,
		.str = "${2-}",
		.strhandle = config_redraws,
		.description = {
		"Format of RPL_ENDOFINFO (374) numeric", NULL}},
	{"format.rpl.motdstart", 1, Val_string,
		.str = "${2-}",
		.strhandle = config_redraws,
		.description = {
		"Format of RPL_MOTDSTART (375) numeric", NULL}},
	{"format.rpl.endofmotd", 1, Val_string,
		.str = "${2-}",
		.strhandle = config_redraws,
		.description = {
		"Format of RPL_ENDOFMOTD (376) numeric", NULL}},
	{"format.rpl.youreoper", 1, Val_string,
		.str = "${2-}",
		.strhandle = config_redraws,
		.description = {
		"Format of RPL_YOUREOPER (381) numeric", NULL}},
	{"format.rpl.rehashing", 1, Val_string,
		.str = "${2-}",
		.strhandle = config_redraws,
		.description = {
		"Format of RPL_REHASHING (382) numeric", NULL}},
	{"format.rpl.time", 1, Val_string,
		.str = "${2-}",
		.strhandle = config_redraws,
		.description = {
		"Format of RPL_TIME (391) numeric", NULL}},
	{"format.rpl.usersstart", 1, Val_string,
		.str = "${2-}",
		.strhandle = config_redraws,
		.description = {
		"Format of RPL_USERSSTART (392) numeric", NULL}},
	{"format.rpl.users", 1, Val_string,
		.str = "${2-}",
		.strhandle = config_redraws,
		.description = {
		"Format of RPL_USERS (393) numeric", NULL}},
	{"format.rpl.endofusers", 1, Val_string,
		.str = "${2-}",
		.strhandle = config_redraws,
		.description = {
		"Format of RPL_ENDOFUSERS (394) numeric", NULL}},
	{"format.rpl.nousers", 1, Val_string,
		.str = "${2-}",
		.strhandle = config_redraws,
		.description = {
		"Format of RPL_NOUSERS (395) numeric", NULL}},
	/* END: misc/rpl-conf-gen.awk */
	{"format.rpl.other", 1, Val_string,
		.str = "${cmd} ${2-}",
		.strhandle = config_redraws,
		.description = {
		"Format of numerics without formats", NULL}},
	{"format.other", 1, Val_string,
		.str = "${raw}",
		.strhandle = config_redraws,
		.description = {
		"Format of other messages without formats", NULL}},
	{NULL},
};

long
config_getl(char *name) {
	int i;

	for (i=0; config[i].name; i++) {
		if (strcmp(config[i].name, name) == 0 && (
				config[i].valtype == Val_bool ||
				config[i].valtype == Val_colour ||
				config[i].valtype == Val_signed ||
				config[i].valtype == Val_unsigned ||
				config[i].valtype == Val_nzunsigned))
			return config[i].num;
	}

	return 0;
}

void
config_get_print(char *name) {
	int i;

	for (i=0; config[i].name; i++) {
		if (strcmp(config[i].name, name) == 0) {
			if (config[i].valtype == Val_string)
				hist_format(main_buf, Activity_status, HIST_SHOW, "SELF_UI :%s: %s",
						name, config[i].str);
			else if (config[i].valtype == Val_pair || config[i].valtype == Val_colourpair)
				hist_format(main_buf, Activity_status, HIST_SHOW, "SELF_UI :%s: {%ld, %ld}",
						name, config[i].pair[0], config[i].pair[1]);
			else
				hist_format(main_buf, Activity_status, HIST_SHOW, "SELF_UI :%s: %ld",
						name, config[i].num);
			return;
		}
	}

	ui_error("no such configuration variable: '%s'", name);
}

char *
config_gets(char *name) {
	int i;

	for (i=0; config[i].name; i++) {
		if (strcmp(config[i].name, name) == 0 &&
				config[i].valtype == Val_string)
			return config[i].str;
	}

	return NULL;
}

void
config_getr(char *name, long *a, long *b) {
	int i;

	for (i=0; config[i].name; i++) {
		if (strcmp(config[i].name, name) == 0 && (
				config[i].valtype == Val_pair ||
				config[i].valtype == Val_colourpair)) {
			if (a) *a = config[i].pair[0];
			if (b) *b = config[i].pair[1];
			return;
		}
	}
}

void
config_setl(char *name, long num) {
	int i;

	for (i=0; config[i].name; i++) {
		if (strcmp(config[i].name, name) == 0) {
			if ((config[i].valtype == Val_bool && (num == 1 || num == 0)) ||
					(config[i].valtype == Val_colour && num <= 99 && num >= 0) ||
					(config[i].valtype == Val_signed) ||
					(config[i].valtype == Val_unsigned && num >= 0) ||
					(config[i].valtype == Val_nzunsigned && num > 0)) {
				if (config[i].numhandle)
					if (!config[i].numhandle(num))
						return;
				config[i].isdef = 0;
				config[i].num = num;
			} else {
				ui_error("%s must be %s", name, valname[config[i].valtype]);
			}
			return;
		}
	}

	ui_error("no such configuration variable: '%s'", name);
}

void
config_sets(char *name, char *str) {
	int i;

	for (i=0; config[i].name; i++) {
		if (strcmp(config[i].name, name) == 0) {
			if (config[i].valtype != Val_string) {
				ui_error("%s must be %s", name, valname[config[i].valtype]);
				return;
			}
			if (config[i].strhandle)
				if (!config[i].strhandle(str))
					return;
			if (!config[i].isdef)
				free(config[i].str);
			else
				config[i].isdef = 0;
			config[i].str = estrdup(str);
			return;
		}
	}

	ui_error("no such configuration variable: '%s'", name);
}

void
config_setr(char *name, long a, long b) {
	int i;

	for (i=0; config[i].name; i++) {
		if (strcmp(config[i].name, name) == 0 ) {
			if ((config[i].valtype != Val_pair && config[i].valtype != Val_colourpair)||
					(config[i].valtype == Val_colourpair &&
					(a > 99 || a < 0 || b > 99 || b < 0))) {
				ui_error("%s must be %s", name, valname[config[i].valtype]);
				return;
			}
			if (config[i].pairhandle)
				if (!config[i].pairhandle(a, b))
					return;
			config[i].isdef = 0;
			config[i].pair[0] = a;
			config[i].pair[1] = b;
			return;
		}
	}

	ui_error("no such configuration variable: '%s'", name);
}

void
config_set(char *name, char *val) {
	char *str = val ? strdup(val) : NULL;
	char *tok[3], *save, *p;

	tok[0] = strtok_r(val,  " ", &save);
	tok[1] = strtok_r(NULL, " ", &save);
	tok[2] = strtok_r(NULL, " ", &save);

	if (strisnum(tok[0]) && strisnum(tok[1]) && !tok[2])
		config_setr(name, strtol(tok[0], NULL, 10), strtol(tok[1], NULL, 10));
	else if (strisnum(tok[0]) && !tok[1])
		config_setl(name, strtol(tok[0], NULL, 10));
	else if (tok[0])
		config_sets(name, str);
	else
		config_get_print(name);

	free(str);
}

void
config_read(char *filename) {
	char buf[8192];
	FILE *file;

	if ((file = fopen(filename, "rb")) == NULL) {
		ui_error("cannot open file '%s': %s", filename, strerror(errno));
		return;
	}

	readingconf = 1;
	while (read_line(fileno(file), buf, sizeof(buf)))
		if (*buf == '/')
			command_eval(buf);
	readingconf = 0;
}

static int
config_nicklist_location(long num) {
	int i;

	if (num != HIDDEN && num != LEFT && num != RIGHT) {
		ui_error("nicklist.location must be 0, 1 or 2", NULL);
		return 0;
	}

	if (num == windows[Win_buflist].location != HIDDEN)
		windows[Win_buflist].location = num == LEFT ? RIGHT : LEFT;
	windows[Win_nicklist].location = num;

	ui_redraw();

	for (i=0; config[i].name; i++) {
		if (strcmp(config[i].name, "nicklist.location") == 0)
			config[i].num = num;
		if (strcmp(config[i].name, "buflist.location") == 0)
			config[i].num = windows[Win_buflist].location;
	}

	return 0;
}

static int
config_nicklist_width(long num) {
	if (num <= COLS - (windows[Win_buflist].location ? windows[Win_buflist].w : 0) - 2) {
		uineedredraw = 1;
		return 1;
	}

	ui_error("nicklist will be too big", NULL);
	return 0;
}

static int
config_buflist_location(long num) {
	int i;

	if (num != HIDDEN && num != LEFT && num != RIGHT) {
		ui_error("buflist.location must be 0, 1 or 2", NULL);
		return 0;
	}

	if (num == windows[Win_buflist].location != HIDDEN)
		windows[Win_nicklist].location = num == LEFT ? RIGHT : LEFT;
	windows[Win_buflist].location = num;

	ui_redraw();

	for (i=0; config[i].name; i++) {
		if (strcmp(config[i].name, "buflist.location") == 0)
			config[i].num = num;
		if (strcmp(config[i].name, "nicklist.location") == 0)
			config[i].num = windows[Win_nicklist].location;
	}

	return 0;
}

static int
config_buflist_width(long num) {
	if (num <= COLS - (windows[Win_nicklist].location ? windows[Win_nicklist].w : 0) - 2) {
		uineedredraw = 1;
		return 1;
	}

	ui_error("buflist will be too big", NULL);
	return 0;
}

static int
config_nickcolour_self(long num) {
	windows[Win_nicklist].refresh = 1;
	return 1;
}

static int
config_nickcolour_range(long a, long b) {
	windows[Win_nicklist].refresh = 1;
	return 1;
}

static int
config_redraws(char *str) {
	ui_redraw();
	return 1;
}

static int
config_redrawl(long num) {
	windows[Win_main].refresh = 1;
	return 1;
}
