/*
 * src/data/commands.h from hirc
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
COMMAND(command_invite);
COMMAND(command_cycle);

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
COMMAND(command_ignore);

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
		"usage: /part <channel> [reason]",
		"Part channel", NULL}},
	{"cycle", command_cycle, 1, {
		"usage: /cycle <channel> [reason]",
		"Part channel and rejoin", NULL}},
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
		"                [-real <comment>] [-tls] [-verify] [host] [port]",
		"Connect to a network/server.",
		"If no host is given, it will attempt to connect to the\n",
		"selected server if it is disconnected\n",
		NULL}},
	{"disconnect", command_disconnect, 0, {
		"usage: /disconnect [network] [msg]",
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
		"Uses def.killmessage if no reason provided.", NULL}},
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
	{"invite", command_invite, 1, {
		"usage: /invite <nick> [channel]",
		"Invite a nick to the current or specified channel.", NULL}},
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
		" -i   case insensitive",
		" -E   posix extended regex",
		" -raw search raw message rather than displayed text",
		"Displays any lines that match the regex in the current buffer,",
		"unless -raw is specified. For convenience, all whitespace is",
		"squeezed down to one space.",
		"If no argument is supplied, clears previous search.",
		"Searches are also cleared after selecting another buffer.",
		"See also variables: regex.extended and regex.icase", NULL}},
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
		"             [-default] [-servers] [-channels] [-queries] [-ignores] <file>",
		"Dumps configuration details into a file.",
		" -autocmds dump commands specified with /server -auto",
		" -aliases  dump /alias commands",
		" -bindings dump /bind commands",
		" -formats  dump /format commands beginning with filter.",
		" -config   dump /format options excluding filters",
		" -servers  dump /server commands",
		" -channels dump /join commands for respective servers",
		" -queries  dump /query commands for respective servers",
		" -ignores  dump /ignore commands",
		" -default  dump default settings (dump non-default otherwise)",
		"If none (excluding -default) of the above are selected, it is",
		"treated as though all are selected.",
		"If -autocmds and -channels are used together, and there exists",
		"an autocmd to join a channel, then only the autocmd will be dumped.", NULL}},
	{"close", command_close, 0, {
		"usage: /close [id]",
		"Forget about selected buffer, or a buffer by id.", NULL}},
	{"ignore", command_ignore, 0, {
		"usage: /ignore [[-server] [-noact] [-format format] regex]",
		"       /ignore -delete id",
		"       /ignore -hide|-show",
		"Hide future messages matching regex.",
		"Regexes should match a raw IRC message.",
		"Display all rules if no argument given.",
		" -show   show ignored messages",
		" -hide   hide ignored messages",
		" -delete delete rule with specified ID",
		" -E      use extended POSIX regex",
		" -i      case insensitive match",
		" -server only ignore for the current server",
		"         or server provided by /server.",
		" -noact  set activity to Activity_ignore,",
		"         but don't hide the message.",
		" -format only ignore messages with >format<",
		"See also: regex.extended, regex.icase", NULL}},
	{NULL, NULL},
};
