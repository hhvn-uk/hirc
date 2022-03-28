/*
 * src/config.c from hirc
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
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <assert.h>
#include <limits.h>
#include "hirc.h"

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
	{"def.modes", 1, Val_signed,
		.num = 1,
		.numhandle = NULL,
		.description = {
		"You most likely don't want to touch this.",
		"If a server doesn't send MODES=... in RPL_ISUPPORT,",
		"use this number instead.", NULL}},
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
	{"regex.extended", 1, Val_bool,
		.num = 0,
		.numhandle = NULL,
		.description = {
		"Use POSIX extended regex at all times.", NULL}},
	{"regex.icase", 1, Val_bool,
		.num = 0,
		.numhandle = NULL,
		.description = {
		"Use case insensitive regex at all times.", NULL}},
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
	{"misc.killmessage", 1, Val_string,
		.str = "no reason",
		.strhandle = NULL,
		.description = {
		"Message to send on /kill", NULL}},
	{"completion.hchar", 1, Val_string,
		.str = ",",
		.strhandle = NULL,
		.description = {
		"Character to place after hilightning a nick",
		"(eg, \",\" -> \"hhvn, hi!\"", NULL}},
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
	{"rdate.short", 1, Val_bool,
		.num = 0,
		.numhandle = config_redrawl,
		.description = {
		"Show short units of time (eg, 1d 2h) vs",
		"long (eg, 1 day 2 hours) units for %{rdate:...}"}},
	{"rdate.averages", 1, Val_bool,
		.num = 1,
		.numhandle = config_redrawl,
		.description = {
		"Months and years are calculated with averages.",
		"Disabling this setting will only use absolute units."}}, /* heh, not intentional */
	{"rdate.verbose", 1, Val_bool,
		.num = 0,
		.numhandle = config_redrawl,
		.description = {
		"Show all units for %{rdate:...}"}},
	{"timestamp.toggle", 1, Val_bool,
		.num = 1,
		.numhandle = config_redrawl,
		.description = {
		"Turn on/off timestamps", NULL}},
	{"format.ui.timestamp", 1, Val_string,
		.str = "%{c:92}%{time:%H:%M:%S,${time}}%{o} ",
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
		.str = "%{c:28}%{b}${4} %{b}(${3} at ${1}:${2})",
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
	{"format.ui.tls.notcompiled", 1, Val_string,
		.str = "TLS not compiled into hirc",
		.strhandle = config_redraws,
		.description = {
		"Format of SELF_TLSNOTCOMPILED messages", NULL}},
#else
	{"format.ui.tls.version", 1, Val_string,
		.str = "Protocol: %{b}${2}%{b} (%{b}${3}%{b} bits, %{b}${4}%{b})",
		.strhandle = config_redraws,
		.description = {
		"TLS version information", NULL}},
	{"format.ui.tls.names", 1, Val_string,
		.str = "SNI name: %{b}${2}%{b}\\nCert subject: %{b}${3}%{b}\\nCert issuer: %{b}${3}%{b}",
		.strhandle = config_redraws,
		.description = {
		"TLS identification", NULL}},
#endif /* TLS */
	{"format.ui.keybind", 1, Val_string,
		.str = " ${1}: ${2}",
		.strhandle = config_redraws,
		.description = {
		"Format of /bind output", NULL}},
	{"format.ui.keybind.start", 1, Val_string,
		.str = "Keybindings:",
		.strhandle = config_redraws,
		.description = {
		"Format of header of /bind output", NULL}},
	{"format.ui.keybind.end", 1, Val_string,
		.str = "",
		.strhandle = config_redraws,
		.description = {
		"Format of footer of /bind output", NULL}},
	{"format.ui.autocmds", 1, Val_string,
		.str = " ${2}",
		.strhandle = config_redraws,
		.description = {
		"Format of /server -auto output", NULL}},
	{"format.ui.autocmds.start", 1, Val_string,
		.str = "Autocmds for ${1}:",
		.strhandle = config_redraws,
		.description = {
		"Format of header of /server -auto output", NULL}},
	{"format.ui.autocmds.end", 1, Val_string,
		.str = "",
		.strhandle = config_redraws,
		.description = {
		"Format of footer of /server -auto output", NULL}},
	{"format.ui.logrestore", 1, Val_string,
		.str = "%{c:93}---%{=}%{c:93}Restored log up until %{b}%{time:%c,${1}}%{b} ---",
		.strhandle = config_redraws,
		.description = {
		"Format of log restore footer.", NULL}},
	{"format.ui.unread", 1, Val_string,
		.str = "%{c:93}---%{=}%{c:93}%{b}${1}%{b} unread (%{b}${2}%{b} ignored) ---",
		.strhandle = config_redraws,
		.description = {
		"Format of unread message indicator.", NULL}},
	{"format.ui.ignores.start", 1, Val_string,
		.str = "Ignoring:",
		.strhandle = config_redraws,
		.description = {
		"Format of ignore list header.", NULL}},
	{"format.ui.ignores", 1, Val_string,
		.str = " %{pad:-3,${1}} ${2}: ${3}",
		.strhandle = config_redraws,
		.description = {
		"Format of ignore list messages.", NULL}},
	{"format.ui.ignores.end", 1, Val_string,
		.str = "",
		.strhandle = config_redraws,
		.description = {
		"Format of ignore list footer.", NULL}},
	{"format.ui.ignores.added", 1, Val_string,
		.str = "Ignore added: ${2} (server: ${1})",
		.strhandle = config_redraws,
		.description = {
		"Format of new ignores.", NULL}},
	{"format.ui.grep.start", 1, Val_string,
		.str = "%{b}%{c:94}Results of ${1}:",
		.strhandle = config_redraws,
		.description = {
		"Format of start of /grep output", NULL}},
	{"format.ui.grep.end", 1, Val_string,
		.str = "",
		.strhandle = config_redraws,
		.description = {
		"Format of end of /grep output", NULL}},
	{"format.ui.alias", 1, Val_string,
		.str = " ${1}: ${2}",
		.strhandle = config_redraws,
		.description = {
		"Format of /alias output", NULL}},
	{"format.ui.alias.start", 1, Val_string,
		.str = "Aliases:",
		.strhandle = config_redraws,
		.description = {
		"Format of header of /alias output", NULL}},
	{"format.ui.alias.end", 1, Val_string,
		.str = "",
		.strhandle = config_redraws,
		.description = {
		"Format of footer of /alias output", NULL}},
	{"format.ui.help", 1, Val_string,
		.str = " ${1}",
		.strhandle = config_redraws,
		.description = {
		"Format of /alias output", NULL}},
	{"format.ui.help.start", 1, Val_string,
		.str = "${1} help:",
		.strhandle = config_redraws,
		.description = {
		"Format of header of /alias output", NULL}},
	{"format.ui.help.end", 1, Val_string,
		.str = "",
		.strhandle = config_redraws,
		.description = {
		"Format of footer of /alias output", NULL}},
	{"format.ui.buflist.old", 1, Val_string,
		.str = "%{c:91}",
		.strhandle = config_redraws,
		.description = {
		"Indicator for disconnected servers or parted channels", NULL}},
	{"format.ui.buflist.activity.none", 1, Val_string,
		.str = "",
		.strhandle = config_redraws,
		.description = {
		"Indicator for buffer with activity of level `none`", NULL}},
	{"format.ui.buflist.activity.status", 1, Val_string,
		.str = "%{c:95}",
		.strhandle = config_redraws,
		.description = {
		"Indicator for buffer with activity of level `status`", NULL}},
	{"format.ui.buflist.activity.error", 1, Val_string,
		.str = "%{c:28}",
		.strhandle = config_redraws,
		.description = {
		"Indicator for buffer with activity of level `error`", NULL}},
	{"format.ui.buflist.activity.message", 1, Val_string,
		.str = "%{c:45}",
		.strhandle = config_redraws,
		.description = {
		"Indicator for buffer with activity of level `message`", NULL}},
	{"format.ui.buflist.activity.hilight", 1, Val_string,
		.str = "%{c:45}%{r}",
		.strhandle = config_redraws,
		.description = {
		"Indicator for buffer with activity of level `hilight`", NULL}},
	{"format.ui.buflist.more", 1, Val_string,
		.str = "%{c:92}...",
		.strhandle = config_redraws,
		.description = {
		"Shown if there are more nicks that must be scrolled to see.", NULL}},
	{"format.ui.nicklist.more", 1, Val_string,
		.str = "%{c:92}...",
		.strhandle = config_redraws,
		.description = {
		"Shown if there are more nicks that must be scrolled to see.", NULL}},
	{"format.ui.separator.vertical", 1, Val_string,
		.str = "│",
		.strhandle = config_redraws,
		.description = {
		"Used for vertical line seperating windows", NULL}},
	{"format.ui.separator.split.left", 1, Val_string,
		.str = "├",
		.strhandle = config_redraws,
		.description = {
		"Joins left vertical separator to input seperator", NULL}},
	{"format.ui.separator.split.right", 1, Val_string,
		.str = "┤",
		.strhandle = config_redraws,
		.description = {
		"Joins right vertical separator to input seperator", NULL}},
	{"format.ui.separator.horizontal", 1, Val_string,
		.str = "─",
		.strhandle = config_redraws,
		.description = {
		"Used for horizontal line separating input and main window", NULL}},
	{"format.privmsg", 1, Val_string,
		.str = "%{nick:${nick}}${priv}${nick}%{o}%{=}${2}",
		.strhandle = config_redraws,
		.description = {
		"Format of messages", NULL}},
	{"format.action", 1, Val_string,
		.str = "%{nick:${nick}}*%{b}${nick}%{b}%{=}${2}",
		.strhandle = config_redraws,
		.description = {
		"Format of actions", NULL}},
	{"format.ctcp.request", 1, Val_string,
		.str = "%{nick:${nick}}${nick}%{o} %{c:94}%{b}q%{o}%{=}${2}",
		.strhandle = config_redraws,
		.description = {
		"Format of CTCP requests", NULL}},
	{"format.ctcp.answer", 1, Val_string,
		.str = "%{nick:${nick}}${nick}%{o} %{c:94}%{b}a%{o}%{=}${2}",
		.strhandle = config_redraws,
		.description = {
		"Format of CTCP answers", NULL}},
	{"format.notice", 1, Val_string,
		.str = "%{nick:${nick}}-${priv}${nick}-%{o}%{=}${2}",
		.strhandle = config_redraws,
		.description = {
		"Format of notices", NULL}},
	{"format.nick",	1, Val_string,
		.str = "%{nick:${nick}}${nick}%{o}%{=}is now known as %{nick:${1}}${1}",
		.strhandle = config_redraws,
		.description = {
		"Format of NICK messages", NULL}},
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
	{"format.topic", 1, Val_string,
		.str = "topic%{=}${2}\\nset by %{nick:${nick}}${nick}%{o} now",
		.strhandle = config_redraws,
		.description = {
		"Format of topic being set", NULL}},
	{"format.invite", 1, Val_string,
		.str = "%{nick:${nick}}${nick}%{o} invited you to ${2}",
		.strhandle = config_redraws,
		.description = {
		"Format of an invitation being received.", NULL}},
	{"format.pong", 1, Val_string,
		.str = "PONG from %{nick:${nick}}${nick}%{o}: ${2}",
		.strhandle = config_redraws,
		.description = {
		"Format of replies to /ping", NULL}},
	{"format.error", 1, Val_string,
		.str = "%{c:28}ERROR:%{o} ${1}",
		.strhandle = config_redraws,
		.description = {
		"Format of generic ERROR messages.",
		"Most commonly seen when being /kill'd.", NULL}},
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
	{"format.rpl.map", 1, Val_string,
		.str = "${2-}",
		.strhandle = config_redraws,
		.description = {
		"Format of RPL_MAP (006) numeric", NULL}},
	{"format.rpl.mapend", 1, Val_string,
		.str = "",
		.strhandle = config_redraws,
		.description = {
		"Format of RPL_MAPEND (007) numeric", NULL}},
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
		.str = "There are ${2} opers online",
		.strhandle = config_redraws,
		.description = {
		"Format of RPL_LUSEROP (252) numeric", NULL}},
	{"format.rpl.luserunknown", 1, Val_string,
		.str = "There are ${2} unknown connections",
		.strhandle = config_redraws,
		.description = {
		"Format of RPL_LUSERUNKNOWN (253) numeric", NULL}},
	{"format.rpl.luserchannels", 1, Val_string,
		.str = "There are ${2} channels formed",
		.strhandle = config_redraws,
		.description = {
		"Format of RPL_LUSERCHANNELS (254) numeric", NULL}},
	{"format.rpl.luserme", 1, Val_string,
		.str = "There are %{split:3, ,${2}} clients and %{split:6, ,${2}} servers connected to this server",
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
		.str = "away%{=}%{nick:${2}}${2}%{o}: ${3}",
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
		.str = "%{c:40}<--%{o}%{=}No longer %{b}away%{b}",
		.strhandle = config_redraws,
		.description = {
		"Format of RPL_UNAWAY (305) numeric", NULL}},
	{"format.rpl.nowaway", 1, Val_string,
		.str = "%{c:32}-->%{o}%{=}Set %{b}away%{b}",
		.strhandle = config_redraws,
		.description = {
		"Format of RPL_NOWAWAY (306) numeric", NULL}},
	{"format.rpl.whoisuser", 1, Val_string,
		.str = "%{b}${2}!${3}@${4}%{b} (${6}):",
		.strhandle = config_redraws,
		.description = {
		"Format of RPL_WHOISUSER (311) numeric", NULL}},
	{"format.rpl.whoisserver", 1, Val_string,
		.str = " %{b}server  %{b}: ${3} (${4})",
		.strhandle = config_redraws,
		.description = {
		"Format of RPL_WHOISSERVER (312) numeric", NULL}},
	{"format.rpl.whoisoperator", 1, Val_string,
		.str = " %{b}oper    %{b}: ${3}",
		.strhandle = config_redraws,
		.description = {
		"Format of RPL_WHOISOPERATOR (313) numeric", NULL}},
	{"format.rpl.whowasuser", 1, Val_string,
		.str = "%{b}${2}!${3}@${4}%{b} (${6}) was on:",
		.strhandle = config_redraws,
		.description = {
		"Format of RPL_WHOWASUSER (314) numeric", NULL}},
	{"format.rpl.endofwho", 1, Val_string,
		.str = "End of WHO results for ${2}",
		.strhandle = config_redraws,
		.description = {
		"Format of RPL_ENDOFWHO (315) numeric", NULL}},
	{"format.rpl.whoisidle", 1, Val_string,
		.str = " %{b}signon  %{b}: %{time:%c,${4}}, idle: %{rdate:${3}}",
		.strhandle = config_redraws,
		.description = {
		"Format of RPL_WHOISIDLE (317) numeric", NULL}},
	{"format.rpl.endofwhois", 1, Val_string,
		.str = "",
		.strhandle = config_redraws,
		.description = {
		"Format of RPL_ENDOFWHOIS (318) numeric", NULL}},
	{"format.rpl.whoischannels", 1, Val_string,
		.str = " %{b}channels%{b}: ${3}",
		.strhandle = config_redraws,
		.description = {
		"Format of RPL_WHOISCHANNELS (319) numeric", NULL}},
	{"format.rpl.liststart", 1, Val_string,
		.str = "%{pad:-15,Channel} %{pad:-5,Nicks} Topic",
		.strhandle = config_redraws,
		.description = {
		"Format of RPL_LISTSTART (321) numeric", NULL}},
	{"format.rpl.list", 1, Val_string,
		.str = "%{pad:-15,${2}} %{pad:-5,${3}} ${4}",
		.strhandle = config_redraws,
		.description = {
		"Format of RPL_LIST (322) numeric", NULL}},
	{"format.rpl.listend", 1, Val_string,
		.str = "",
		.strhandle = config_redraws,
		.description = {
		"Format of RPL_LISTEND (323) numeric", NULL}},
	{"format.rpl.channelmodeis", 1, Val_string,
		.str = "mode%{=}%{c:94}${3-}%{o}",
		.strhandle = config_redraws,
		.description = {
		"Format of RPL_CHANNELMODEIS (324) numeric", NULL}},
	{"format.rpl.notopic", 1, Val_string,
		.str = "topic%{=}no topic set",
		.strhandle = config_redraws,
		.description = {
		"Format of RPL_NOTOPIC (331) numeric", NULL}},
	{"format.rpl.topic", 1, Val_string,
		.str = "topic%{=}${3}",
		.strhandle = config_redraws,
		.description = {
		"Format of RPL_TOPIC (332) numeric", NULL}},
	{"format.rpl.inviting", 1, Val_string,
		.str = "invite%{=}${2}",
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
		.str = "%{b}${6}!${3}@${4}%{b} (%{split:2, ,${8}}): ${7} %{split:1, ,${8}}",
		.strhandle = config_redraws,
		.description = {
		"Format of RPL_WHOREPLY (352) numeric", NULL}},
	{"format.rpl.namreply", 1, Val_string,
		.str = "names%{=}${4-}",
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
		.str = "",
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
	{"format.err.nosuchnick", 1, Val_string,
		.str = "No such nick: ${2}",
		.strhandle = config_redraws,
		.description = {
		"Format of ERR_NOSUCHNICK (401) numeric", NULL}},
	{"format.err.nosuchserver", 1, Val_string,
		.str = "No such server: ${2}",
		.strhandle = config_redraws,
		.description = {
		"Format of ERR_NOSUCHSERVER (402) numeric", NULL}},
	{"format.err.nosuchchannel", 1, Val_string,
		.str = "No such channel: ${2}",
		.strhandle = config_redraws,
		.description = {
		"Format of ERR_NOSUCHCHANNEL (403) numeric", NULL}},
	{"format.err.cannotsendtochan", 1, Val_string,
		.str = "${2-}",
		.strhandle = config_redraws,
		.description = {
		"Format of ERR_CANNOTSENDTOCHAN (404) numeric", NULL}},
	{"format.err.toomanychannels", 1, Val_string,
		.str = "${2-}",
		.strhandle = config_redraws,
		.description = {
		"Format of ERR_TOOMANYCHANNELS (405) numeric", NULL}},
	{"format.err.wasnosuchnick", 1, Val_string,
		.str = "${2-}",
		.strhandle = config_redraws,
		.description = {
		"Format of ERR_WASNOSUCHNICK (406) numeric", NULL}},
	{"format.err.toomanytargets", 1, Val_string,
		.str = "${2-}",
		.strhandle = config_redraws,
		.description = {
		"Format of ERR_TOOMANYTARGETS (407) numeric", NULL}},
	{"format.err.noorigin", 1, Val_string,
		.str = "${2-}",
		.strhandle = config_redraws,
		.description = {
		"Format of ERR_NOORIGIN (409) numeric", NULL}},
	{"format.err.norecipient", 1, Val_string,
		.str = "${2-}",
		.strhandle = config_redraws,
		.description = {
		"Format of ERR_NORECIPIENT (411) numeric", NULL}},
	{"format.err.notexttosend", 1, Val_string,
		.str = "${2-}",
		.strhandle = config_redraws,
		.description = {
		"Format of ERR_NOTEXTTOSEND (412) numeric", NULL}},
	{"format.err.notoplevel", 1, Val_string,
		.str = "${2-}",
		.strhandle = config_redraws,
		.description = {
		"Format of ERR_NOTOPLEVEL (413) numeric", NULL}},
	{"format.err.wildtoplevel", 1, Val_string,
		.str = "${2-}",
		.strhandle = config_redraws,
		.description = {
		"Format of ERR_WILDTOPLEVEL (414) numeric", NULL}},
	{"format.err.unknowncommand", 1, Val_string,
		.str = "${2-}",
		.strhandle = config_redraws,
		.description = {
		"Format of ERR_UNKNOWNCOMMAND (421) numeric", NULL}},
	{"format.err.nomotd", 1, Val_string,
		.str = "${2-}",
		.strhandle = config_redraws,
		.description = {
		"Format of ERR_NOMOTD (422) numeric", NULL}},
	{"format.err.noadmininfo", 1, Val_string,
		.str = "${2-}",
		.strhandle = config_redraws,
		.description = {
		"Format of ERR_NOADMININFO (423) numeric", NULL}},
	{"format.err.fileerror", 1, Val_string,
		.str = "${2-}",
		.strhandle = config_redraws,
		.description = {
		"Format of ERR_FILEERROR (424) numeric", NULL}},
	{"format.err.nonicknamegiven", 1, Val_string,
		.str = "${2-}",
		.strhandle = config_redraws,
		.description = {
		"Format of ERR_NONICKNAMEGIVEN (431) numeric", NULL}},
	{"format.err.erroneusnickname", 1, Val_string,
		.str = "Erroneous nickname: ${2}",
		.strhandle = config_redraws,
		.description = {
		"Format of ERR_ERRONEUSNICKNAME (432) numeric", NULL}},
	{"format.err.nicknameinuse", 1, Val_string,
		.str = "Nickname already in use: ${2}",
		.strhandle = config_redraws,
		.description = {
		"Format of ERR_NICKNAMEINUSE (433) numeric", NULL}},
	{"format.err.nickcollision", 1, Val_string,
		.str = "${2-}",
		.strhandle = config_redraws,
		.description = {
		"Format of ERR_NICKCOLLISION (436) numeric", NULL}},
	{"format.err.usernotinchannel", 1, Val_string,
		.str = "${2-}",
		.strhandle = config_redraws,
		.description = {
		"Format of ERR_USERNOTINCHANNEL (441) numeric", NULL}},
	{"format.err.notonchannel", 1, Val_string,
		.str = "${2-}",
		.strhandle = config_redraws,
		.description = {
		"Format of ERR_NOTONCHANNEL (442) numeric", NULL}},
	{"format.err.useronchannel", 1, Val_string,
		.str = "${2-}",
		.strhandle = config_redraws,
		.description = {
		"Format of ERR_USERONCHANNEL (443) numeric", NULL}},
	{"format.err.nologin", 1, Val_string,
		.str = "${2-}",
		.strhandle = config_redraws,
		.description = {
		"Format of ERR_NOLOGIN (444) numeric", NULL}},
	{"format.err.summondisabled", 1, Val_string,
		.str = "${2-}",
		.strhandle = config_redraws,
		.description = {
		"Format of ERR_SUMMONDISABLED (445) numeric", NULL}},
	{"format.err.usersdisabled", 1, Val_string,
		.str = "${2-}",
		.strhandle = config_redraws,
		.description = {
		"Format of ERR_USERSDISABLED (446) numeric", NULL}},
	{"format.err.notregistered", 1, Val_string,
		.str = "${2-}",
		.strhandle = config_redraws,
		.description = {
		"Format of ERR_NOTREGISTERED (451) numeric", NULL}},
	{"format.err.needmoreparams", 1, Val_string,
		.str = "${2-}",
		.strhandle = config_redraws,
		.description = {
		"Format of ERR_NEEDMOREPARAMS (461) numeric", NULL}},
	{"format.err.alreadyregistred", 1, Val_string,
		.str = "${2-}",
		.strhandle = config_redraws,
		.description = {
		"Format of ERR_ALREADYREGISTRED (462) numeric", NULL}},
	{"format.err.nopermforhost", 1, Val_string,
		.str = "${2-}",
		.strhandle = config_redraws,
		.description = {
		"Format of ERR_NOPERMFORHOST (463) numeric", NULL}},
	{"format.err.passwdmismatch", 1, Val_string,
		.str = "${2-}",
		.strhandle = config_redraws,
		.description = {
		"Format of ERR_PASSWDMISMATCH (464) numeric", NULL}},
	{"format.err.yourebannedcreep", 1, Val_string,
		.str = "${2-}",
		.strhandle = config_redraws,
		.description = {
		"Format of ERR_YOUREBANNEDCREEP (465) numeric", NULL}},
	{"format.err.youwillbebanned", 1, Val_string,
		.str = "${2-}",
		.strhandle = config_redraws,
		.description = {
		"Format of ERR_YOUWILLBEBANNED (466) numeric", NULL}},
	{"format.err.keyset", 1, Val_string,
		.str = "${2-}",
		.strhandle = config_redraws,
		.description = {
		"Format of ERR_KEYSET (467) numeric", NULL}},
	{"format.err.channelisfull", 1, Val_string,
		.str = "${2-}",
		.strhandle = config_redraws,
		.description = {
		"Format of ERR_CHANNELISFULL (471) numeric", NULL}},
	{"format.err.unknownmode", 1, Val_string,
		.str = "${2-}",
		.strhandle = config_redraws,
		.description = {
		"Format of ERR_UNKNOWNMODE (472) numeric", NULL}},
	{"format.err.inviteonlychan", 1, Val_string,
		.str = "${2-}",
		.strhandle = config_redraws,
		.description = {
		"Format of ERR_INVITEONLYCHAN (473) numeric", NULL}},
	{"format.err.bannedfromchan", 1, Val_string,
		.str = "${2-}",
		.strhandle = config_redraws,
		.description = {
		"Format of ERR_BANNEDFROMCHAN (474) numeric", NULL}},
	{"format.err.badchannelkey", 1, Val_string,
		.str = "${2-}",
		.strhandle = config_redraws,
		.description = {
		"Format of ERR_BADCHANNELKEY (475) numeric", NULL}},
	{"format.err.noprivileges", 1, Val_string,
		.str = "${2-}",
		.strhandle = config_redraws,
		.description = {
		"Format of ERR_NOPRIVILEGES (481) numeric", NULL}},
	{"format.err.chanoprivsneeded", 1, Val_string,
		.str = "${2-}",
		.strhandle = config_redraws,
		.description = {
		"Format of ERR_CHANOPRIVSNEEDED (482) numeric", NULL}},
	{"format.err.cantkillserver", 1, Val_string,
		.str = "${2-}",
		.strhandle = config_redraws,
		.description = {
		"Format of ERR_CANTKILLSERVER (483) numeric", NULL}},
	{"format.err.nooperhost", 1, Val_string,
		.str = "${2-}",
		.strhandle = config_redraws,
		.description = {
		"Format of ERR_NOOPERHOST (491) numeric", NULL}},
	{"format.err.noservicehost", 1, Val_string,
		.str = "${2-}",
		.strhandle = config_redraws,
		.description = {
		"Format of ERR_NOSERVICEHOST (492) numeric", NULL}},
	{"format.err.umodeunknownflag", 1, Val_string,
		.str = "${2-}",
		.strhandle = config_redraws,
		.description = {
		"Format of ERR_UMODEUNKNOWNFLAG (501) numeric", NULL}},
	{"format.err.usersdontmatch", 1, Val_string,
		.str = "${2-}",
		.strhandle = config_redraws,
		.description = {
		"Format of ERR_USERSDONTMATCH (502) numeric", NULL}},
	/* END: misc/rpl-conf-gen.awk */
	/* Modern numerics */
	{"format.rpl.localusers", 1, Val_string,
		.str = "There are ${2} current local users, record of ${3}",
		.strhandle = config_redraws,
		.description = {
		"Format of RPLS_LOCALUSERS (265) numeric", NULL}},
	{"format.rpl.globalusers", 1, Val_string,
		.str = "There are ${2} current global users, record of ${3}",
		.strhandle = config_redraws,
		.description = {
		"Format of RPL_GLOBALUSERS (266) numeric", NULL}},
	{"format.rpl.whoisspecial", 1, Val_string,
		.str = " %{b}info    %{b}: ${3}",
		.strhandle = config_redraws,
		.description = {
		"Format of RPL_WHOISSPECIAL (320) numeric", NULL}},
	{"format.rpl.whoisaccount", 1, Val_string,
		.str = " %{b}account %{b}: ${3}",
		.strhandle = config_redraws,
		.description = {
		"Format of RPL_WHOISACCOUNT (330) numeric", NULL}},
	{"format.rpl.topicwhotime", 1, Val_string,
		.str = "set by %{nick:${3}}${3}%{o} at %{time:%Y-%m-%d %H:%M:%S,${4}}",
		.strhandle = config_redraws,
		.description = {
		"Format of RPL_TOPICWHOTIME (333) numeric", NULL}},
	{"format.rpl.whoisactually", 1, Val_string,
		.str = " %{b}actually%{b}: ${3-}",
		.strhandle = config_redraws,
		.description = {
		"Format of RPL_WHOISACTUALLY (338) numeric", NULL}},
	{"format.rpl.whoishost", 1, Val_string,
		.str = " %{b}info    %{b}: ${3}",
		.strhandle = config_redraws,
		.description = {
		"Format of RPL_WHOISHOST (378) numeric", NULL}},
	{"format.rpl.whoismodes", 1, Val_string,
		.str = " %{b}modes   %{b}: ${3}",
		.strhandle = config_redraws,
		.description = {
		"Format of RPL_WHOISMODES (379) numeric", NULL}},
	{"format.rpl.whoissecure", 1, Val_string,
		.str = " %{b}secure  %{b}: ${3}",
		.strhandle = config_redraws,
		.description = {
		"Format of RPL_WHOISSECURE (671) numeric", NULL}},
	/* Default formats */
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
	int i, found;

	for (i = found = 0; config[i].name; i++) {
		if (strncmp(config[i].name, name, strlen(name)) == 0) {
			if (config[i].valtype == Val_string)
				hist_format(selected.history, Activity_status, HIST_UI, "SELF_UI :%s: %s",
						config[i].name, config[i].str);
			else if (config[i].valtype == Val_pair || config[i].valtype == Val_colourpair)
				hist_format(selected.history, Activity_status, HIST_UI, "SELF_UI :%s: {%ld, %ld}",
						config[i].name, config[i].pair[0], config[i].pair[1]);
			else
				hist_format(selected.history, Activity_status, HIST_UI, "SELF_UI :%s: %ld",
						config[i].name, config[i].num);
			found = 1;
		}
	}

	if (!found)
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
				pfree(&config[i].str);
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
	char *str = val ? estrdup(val) : NULL;
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

	pfree(&str);
}

void
config_read(char *filename) {
	static char **bt;
	static int btoffset = 0;
	char buf[8192];
	char *path;
	FILE *file;
	int save, i;

	if (!filename)
		return;

	path = realpath(filename, NULL);

	/* Check if file is already being read */
	if (bt) {
		for (i = 0; i < btoffset; i++) {
			if (strcmp(path, *(bt + i)) == 0) {
				ui_error("recursive read of '%s' is not allowed", filename);
				pfree(&path);
				return;
			}
		}
	}

	/* Expand bt and add real path */
	if (!bt)
		bt = emalloc((sizeof(char *)) * (btoffset + 1));
	else
		bt = erealloc(bt, (sizeof(char *)) * (btoffset + 1));

	*(bt + btoffset) = path;
	btoffset++;

	/* Read and execute */
	if ((file = fopen(filename, "rb")) == NULL) {
		ui_error("cannot open file '%s': %s", filename, strerror(errno));
		goto shrink;
	}

	save = nouich;
	nouich = 1;
	while (read_line(fileno(file), buf, sizeof(buf)))
		if (*buf == '/')
			command_eval(NULL, buf);
	fclose(file);
	nouich = save;

shrink:
	/* Remove path from bt and shrink */
	pfree(&path);
	btoffset--;
	if (btoffset == 0) {
		pfree(&bt);
		bt = NULL;
	} else {
		bt = erealloc(bt, (sizeof(char *)) * btoffset);
		assert(bt != NULL);
	}
}

static int
config_nicklist_location(long num) {
	int i;

	if (num != HIDDEN && num != LEFT && num != RIGHT) {
		ui_error("nicklist.location must be 0, 1 or 2", NULL);
		return 0;
	}

	if (!selected.hasnicks)
		return 0;

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
	ui_redraw();
	return 1;
}
