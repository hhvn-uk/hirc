/*
 * src/ui.c from hirc
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

#include <errno.h>
#include <ctype.h>
#include <stdarg.h>
#include <string.h>
#include <stdlib.h>
#include <locale.h>
#include <ncurses.h>
#ifdef TLS
#include <tls.h>
#endif /* TLS */
#include "hirc.h"

int uineedredraw = 0;
int nouich = 0;

#define HIRC_COLOURS 100
static unsigned short colourmap[HIRC_COLOURS] = {
	/* original 16 mirc colours
	 * some clients use the first 16 ansi colours for this,
	 * but here I use the 256 colours to ensure terminal-agnosticism */
	[0] = 255, 16, 19, 46, 124, 88,  127, 184,
	[8] = 208, 46, 45, 51, 21,  201, 240, 255,

	/* extended */
	[16] = 52,  94,  100, 58,  22,  29,  23,  24,  17,  54,  53,  89,
	[28] = 88,  130, 142, 64,  28,  35,  30,  25,  18,  91,  90,  125,
	[40] = 124, 166, 184, 106, 34,  49,  37,  33,  19,  129, 127, 161,
	[52] = 196, 208, 226, 154, 46,  86,  51,  75,  21,  171, 201, 198,
	[64] = 203, 215, 227, 191, 83,  122, 87,  111, 63,  177, 207, 205,
	[76] = 217, 223, 229, 193, 157, 158, 159, 153, 147, 183, 219, 212,
	[88] = 16,  233, 235, 237, 239, 241, 244, 247, 250, 254, 231,

	/* transparency */
	[99] = -1
};

struct Window windows[Win_last] = {
	[Win_dummy]	= {.handler = NULL, .scroll = -1},
	[Win_main]	= {.handler = ui_draw_main, .scroll = -1},
	[Win_input]	= {.handler = ui_draw_input, .scroll = -1},
	[Win_nicklist]	= {.handler = ui_draw_nicklist, .scroll = -1},
	[Win_buflist]	= {.handler = ui_draw_buflist, .scroll = -1},
};

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
	/* Real commands/numerics from server */
	{"PRIVMSG", 		"format.privmsg"},
	{"NOTICE",		"format.notice"},
	{"JOIN",		"format.join"},
	{"PART",		"format.part"},
	{"KICK",		"format.kick"},
	{"QUIT",		"format.quit"},
	{"NICK",		"format.nick"},
	{"TOPIC",		"format.topic"},
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

struct {
	char string[INPUT_MAX];
	unsigned counter;
	char *history[INPUT_HIST_MAX];
} input;

struct Selected selected;
struct Keybind *keybinds = NULL;

void
ui_error_(char *file, int line, const char *func, char *format, ...) {
	char msg[1024];
	va_list ap;

	va_start(ap, format);
	vsnprintf(msg, sizeof(msg), format, ap);
	va_end(ap);

	hist_format(selected.history, Activity_error, HIST_SHOW|HIST_TMP|HIST_MAIN,
			"SELF_ERROR %s %d %s :%s",
			file, line, func, msg);
}

void
ui_perror_(char *file, int line, const char *func, char *str) {
	hist_format(selected.history, Activity_error, HIST_SHOW|HIST_TMP|HIST_MAIN,
			"SELF_ERROR %s %d %s :%s: %s",
			file, line, func, str, strerror(errno));
}

#ifdef TLS
void
ui_tls_config_error_(char *file, int line, const char *func, struct tls_config *config, char *str) {
	hist_format(selected.history, Activity_error, HIST_SHOW|HIST_TMP|HIST_MAIN,
			"SELF_ERROR %s %d %s :%s: %s",
			file, line, func, str, tls_config_error(config));
}

void
ui_tls_error_(char *file, int line, const char *func, struct tls *ctx, char *str) {
	hist_format(selected.history, Activity_error, HIST_SHOW|HIST_TMP|HIST_MAIN,
			"SELF_ERROR %s %d %s :%s: %s",
			file, line, func, str, tls_error(ctx));
}
#endif /* TLS */

void
ui_init(void) {
	setlocale(LC_ALL, "en_US.UTF-8");
	initscr();
	start_color();
	use_default_colors();
	raw();
	noecho();
	nonl(); /* get ^j */

	input.string[0] = '\0';
	memset(input.history, 0, sizeof(input.history));
	input.counter = 0;

	windows[Win_nicklist].location = config_getl("nicklist.location");
	windows[Win_buflist].location = config_getl("buflist.location");

	windows[Win_dummy].window = stdscr;
	windows[Win_main].window = newwin(0, 0, 0, 0);
	windows[Win_input].window = newwin(0, 0, 0, 0);

	windows[Win_dummy].location = HIDDEN;
	windows[Win_main].location = -1;
	windows[Win_input].location = -1;
	if (windows[Win_nicklist].location)
		windows[Win_nicklist].window = newwin(0, 0, 0, 0);
	if (windows[Win_buflist].location)
		windows[Win_buflist].window = newwin(0, 0, 0, 0);

	nodelay(windows[Win_input].window, TRUE);
	keypad(windows[Win_input].window, TRUE);

	ui_redraw();
	ui_select(NULL, NULL);
}

int
ui_get_pair(short fg, short bg) {
	static unsigned short pair_map[HIRC_COLOURS][HIRC_COLOURS];
	static int needinit = 1;
	short j, k;
	int i;

	if (needinit) {
		init_pair(1, -1, -1);
		for (i=2, j=0; j < HIRC_COLOURS; j++) {
			for (k=0; k < HIRC_COLOURS; k++) {
				init_pair(i, colourmap[j], colourmap[k]);
				pair_map[j][k] = i;
				i++;
			}
		}
		needinit = 0;
	}

	if (fg >= HIRC_COLOURS || bg >= HIRC_COLOURS)
		return 1;

	return pair_map[fg][bg];
}

void
ui_placewindow(struct Window *window) {
	if (window->location != HIDDEN) {
		wresize(window->window, window->h, window->w);
		mvwin(window->window, window->y, window->x);
		wrefresh(window->window);
	}
}

void
ui_read(void) {
	static int histindex = -1; /* -1 == current input */
	static char *backup = NULL;
	struct Keybind *kp;
	int key;
	int savecounter;

	savecounter = input.counter;

	/* Loop over input, return only if ERR is received.
	 * Normally wgetch exits fast enough that unless something
	 * is being pasted in this won't waste any time that should
	 * be used for other stuff */
	for (;;) {
		switch (key = wgetch(windows[Win_input].window)) {
		case ERR: /* no input received */
			/* Match keybinds here - this allows multikey
			 * bindings such as those with alt, but since
			 * there is no delay with wgetch() it's unlikely
			 * that the user pressing multiple keys will
			 * trigger one. */
			if (input.counter != savecounter) {
				for (kp = keybinds; kp; kp = kp->next) {
					if ((input.counter - savecounter) == strlen(kp->binding) &&
							strncmp(kp->binding, &input.string[savecounter], (input.counter - savecounter)) == 0) {
						command_eval(selected.server, kp->cmd);
						memmove(&input.string[savecounter],
								&input.string[input.counter],
								strlen(&input.string[input.counter]) + 1);
						input.counter = savecounter;
						return;
					}
				}

				if (histindex) {
					free(backup);
					backup = NULL;
					histindex = -1;
				}

			}

			windows[Win_input].handler();
			wrefresh(windows[Win_input].window);
			windows[Win_input].refresh = 0;
			return;
		case KEY_RESIZE:
			ui_redraw();
			break;
		case KEY_BACKSPACE:
			if (input.counter) {
				if (ui_input_delete(1, input.counter) > 0)
					input.counter--;
			}
			break;
		case KEY_UP:
			if (histindex < INPUT_HIST_MAX && input.history[histindex + 1]) {
				if (histindex == -1)
					backup = estrdup(input.string);
				histindex++;
				strlcpy(input.string, input.history[histindex], sizeof(input.string));
				input.counter = strlen(input.string);
			}
			return; /* return so histindex and backup aren't reset */
		case KEY_DOWN:
			if (histindex > -1) {
				histindex--;
				if (histindex == -1) {
					if (backup)
						strlcpy(input.string, backup, sizeof(input.string));
					free(backup);
					backup = NULL;
				} else {
					strlcpy(input.string, input.history[histindex], sizeof(input.string));
				}
				input.counter = strlen(input.string);
			}
			return; /* return so histindex and backup aren't reset */
		case KEY_LEFT:
			if (input.counter)
				input.counter--;
			break;
		case KEY_RIGHT:
			if (input.string[input.counter])
				input.counter++;
			break;
		case KEY_ENTER:
		case '\r':
			command_eval(selected.server, input.string);
			/* free checks for null */
			free(input.history[INPUT_HIST_MAX - 1]);
			memmove(input.history + 1, input.history, (sizeof(input.history) / INPUT_HIST_MAX) * (INPUT_HIST_MAX - 1));
			input.history[0] = estrdup(input.string);
			input.string[0] = '\0';
			input.counter = 0;
			break;
		default:
			if ((key & 0xFF80) == 0x80 || isprint(key) || iscntrl(key)) {
				if (ui_input_insert(key, input.counter) > 0)
					input.counter++;
			}
			break;
		}
	}
}

int
ui_input_insert(char c, int counter) {
	char *p;
	int i, bc;

	for (bc=i=0, p = input.string; i != counter && bc < sizeof(input.string) && *p; p++, bc++) {
		if ((*p & 0xC0) != 0x80)
			i++;
	}
	while ((*p & 0xC0) == 0x80)
		p++;

	if (i != counter)
		return -1;

	if ((strlen(input.string)) > sizeof(input.string))
		return -1;

	memmove(p + 1, p, strlen(p) + 1);
	memcpy(p, &c, 1);
	return ((c & 0xC0) != 0x80);
}


int
ui_input_delete(int num, int counter) {
	char *dest, *p;
	int i, bc;

	if (num < 0)
		return -1;

	for (bc=i=0, dest = input.string; i != counter - 1 && bc < sizeof(input.string) && *dest; dest++, bc++) {
		if ((*dest & 0xC0) != 0x80)
			i++;
	}

	while ((*dest & 0xC0) == 0x80)
		dest++;

	p = dest;
	do {
		p++;
	} while ((*p & 0xC0) == 0x80);

	/* if (i != counter + num) */
	/*      return -1; */

	memmove(dest, p, strlen(p) + 1);
	return num;
}

void
ui_redraw(void) {
	char *format;
	long nicklistwidth, buflistwidth;
	int x = 0, rx = 0;
	int i;

	nicklistwidth = config_getl("nicklist.width");
	buflistwidth = config_getl("buflist.width");

	/* TODO: what if nicklistwidth or buflistwidth is too big? */
	if (windows[Win_buflist].location == LEFT) {
		windows[Win_buflist].x = windows[Win_buflist].y = 0;
		windows[Win_buflist].h = LINES;
		windows[Win_buflist].w = buflistwidth;
		x = windows[Win_buflist].w + 1;
	}
	if (windows[Win_nicklist].location == LEFT) {
		windows[Win_nicklist].x = windows[Win_buflist].y = 0;
		windows[Win_nicklist].h = LINES;
		windows[Win_nicklist].w = nicklistwidth;
		x = windows[Win_nicklist].w + 1;
	}
	if (windows[Win_buflist].location == RIGHT) {
		windows[Win_buflist].x = COLS - buflistwidth;
		windows[Win_buflist].y = 0;
		windows[Win_buflist].h = LINES;
		windows[Win_buflist].w = buflistwidth;
		rx = buflistwidth + 1;
	}
	if (windows[Win_nicklist].location == RIGHT) {
		windows[Win_nicklist].x = COLS - nicklistwidth;
		windows[Win_nicklist].y = 0;
		windows[Win_nicklist].h = LINES;
		windows[Win_nicklist].w = nicklistwidth;
		rx = nicklistwidth + 1;
	}

	windows[Win_main].x = x;
	windows[Win_main].y = 0;
	windows[Win_main].h = LINES - 2;
	windows[Win_main].w = COLS - x - rx;

	windows[Win_input].x = x;
	windows[Win_input].y = LINES - 1;
	windows[Win_input].h = 1;
	windows[Win_input].w = COLS - x - rx;

	windows[Win_dummy].x = 0;
	windows[Win_dummy].y = 0;
	windows[Win_dummy].h = LINES;
	windows[Win_dummy].w = COLS;

	format = ui_format(NULL, config_gets("format.ui.separator.horizontal"), NULL);
	for (i = x; i <= COLS - rx; i++) {
		wmove(windows[Win_dummy].window, LINES - 2, i);
		ui_wprintc(&windows[Win_dummy], 1, "%s", format);
	}

	if (x) {
		format = ui_format(NULL, config_gets("format.ui.separator.vertical"), NULL);
		for (i = 0; i <= LINES; i++) {
			wmove(windows[Win_dummy].window, i, x - 1);
			ui_wprintc(&windows[Win_dummy], 1, "%s", format);
		}

		format = ui_format(NULL, config_gets("format.ui.separator.split.left"), NULL);
		wmove(windows[Win_dummy].window, LINES - 2, x - 1);
		ui_wprintc(&windows[Win_dummy], 1, "%s", format);
	}

	if (rx) {
		format = ui_format(NULL, config_gets("format.ui.separator.vertical"), NULL);
		for (i = 0; i <= LINES; i++) {
			wmove(windows[Win_dummy].window, i, COLS - rx);
			ui_wprintc(&windows[Win_dummy], 1, "%s", format);
		}

		format = ui_format(NULL, config_gets("format.ui.separator.split.right"), NULL);
		wmove(windows[Win_dummy].window, LINES - 2, COLS - rx);
		ui_wprintc(&windows[Win_dummy], 1, "%s", format);
	}

	refresh();

	for (i = 0; i < Win_last; i++) {
		if (windows[i].location) {
			ui_placewindow(&windows[i]);
			windows[i].refresh = 1;
		}
	}
}

void
ui_draw_input(void) {
	char utfbuf[5];
	char *p;
	int utfc;
	int offset;
	int x;

	werase(windows[Win_input].window);

	/* Round input.counter down to the nearest windows[Win_input].w.
	 * This gives "pages" that are each as long as the width of the input window */
	offset = ((int) input.counter / windows[Win_input].w) * windows[Win_input].w;
	for (x=0, p = input.string + offset; p && *p && x < windows[Win_input].w; p++, x++) {
		if ((*p & 0xC0) == 0xC0) {
			/* see ui_wprintc */
			memset(utfbuf, '\0', sizeof(utfbuf));
			utfbuf[0] = *p;
			for (utfc = 1, p++; (*p & 0xC0) != 0xC0 && (*p & 0x80) == 0x80 && utfc < sizeof(utfbuf); utfc++, p++)
				utfbuf[utfc] = *p;
			waddstr(windows[Win_input].window, utfbuf);
			p--;
		} else if (iscntrl(*p)) {
			/* adding 64 will turn ^C into C */
			wattron(windows[Win_input].window, A_REVERSE);
			waddch(windows[Win_input].window, *p + 64);
			wattroff(windows[Win_input].window, A_REVERSE);
		} else if (!(*p & 0x80)) {
			waddch(windows[Win_input].window, *p);
		}
	}
	wmove(windows[Win_input].window, 0, input.counter - offset);
}

void
ui_draw_nicklist(void) {
	struct Nick *p;
	int y = 0, i;

	werase(windows[Win_nicklist].window);

	if (!selected.channel || !windows[Win_nicklist].location)
		return;

	wmove(windows[Win_nicklist].window, 0, 0);

	nick_sort(&selected.channel->nicks, selected.server);

	for (i=0, p = selected.channel->nicks; p && p->next && p->next->next && i < windows[Win_nicklist].scroll; i++)
		p = p->next;
	if (i != 0) {
		ui_wprintc(&windows[Win_nicklist], 1, "%s\n", ui_format(NULL, config_gets("format.ui.nicklist.more"), NULL));
		y++;
		p = p->next;
		windows[Win_nicklist].scroll = i;
	}

	for (; p && y < windows[Win_nicklist].h - (p->next ? 1 : 0); p = p->next, y++) {
		ui_wprintc(&windows[Win_nicklist], 1, "%c%02d%c%s\n",
				3 /* ^C */, nick_getcolour(p), p->priv, p->nick);
	}

	if (p)
		ui_wprintc(&windows[Win_nicklist], 1, "%s\n", ui_format(NULL, config_gets("format.ui.nicklist.more"), NULL));
}

int
ui_buflist_count(int *ret_servers, int *ret_channels, int *ret_privs) {
	struct Server *sp;
	struct Channel *chp;
	int sc, cc, pc;

	for (sc = cc = pc = 0, sp = servers; sp; sp = sp->next, sc++) {
		for (chp = sp->channels; chp; chp = chp->next, cc++)
			;
		for (chp = sp->privs; chp; chp = chp->next, pc++)
			;
	}

	if (ret_servers)
		*ret_servers = sc;
	if (ret_channels)
		*ret_channels = cc;
	if (ret_privs)
		*ret_channels = pc;

	return sc + cc + pc + 1;
}

int
ui_buflist_get(int num, struct Server **server, struct Channel **chan) {
	struct Server *sp;
	struct Channel *chp;
	int i;

	if (num <= 0) {
		ui_error("buffer index greater than 0 expected", NULL);
		return -1;
	}

	if (num == 1) {
		*server = NULL;
		*chan = NULL;
		return 0;
	}

	for (i = 2, sp = servers; sp; sp = sp->next) {
		if (i == num) {
			*server = sp;
			*chan = NULL;
			return 0;
		}
		i++; /* increment before moving
			to channel section, not
			int for (;; ..) */

		for (chp = sp->channels; chp; chp = chp->next, i++) {
			if (i == num) {
				*server = sp;
				*chan = chp;
				return 0;
			}
		}
		for (chp = sp->privs; chp; chp = chp->next, i++) {
			if (i == num) {
				*server = sp;
				*chan = chp;
				return 0;
			}
		}
	}

	ui_error("couldn't find buffer with index %d", num);
	return -1;
}

static char *
ui_format_activity(int activity) {
	switch (activity) {
	case Activity_status:
		return ui_format(NULL, config_gets("format.ui.buflist.activity.status"), NULL);
	case Activity_error:
		return ui_format(NULL, config_gets("format.ui.buflist.activity.error"), NULL);
	case Activity_message:
		return ui_format(NULL, config_gets("format.ui.buflist.activity.message"), NULL);
	case Activity_hilight:
		return ui_format(NULL, config_gets("format.ui.buflist.activity.hilight"), NULL);
	default:
		return ui_format(NULL, config_gets("format.ui.buflist.activity.none"), NULL);
	}

	return NULL; /* shouldn't be possible *shrug*/
}

void
ui_draw_buflist(void) {
	struct Server *sp;
	struct Channel *chp, *prp;
	int i = 1, scroll;
	char *indicator;

	werase(windows[Win_buflist].window);

	if (windows[Win_buflist].scroll < 0)
		scroll = 0;
	else
		scroll = windows[Win_buflist].scroll;

	if (!windows[Win_buflist].location)
		return;

	if (scroll > 0) {
		ui_wprintc(&windows[Win_buflist], 1, "%s\n", ui_format(NULL, config_gets("format.ui.buflist.more"), NULL));
	} else if (scroll < i) {
		if (selected.history == main_buf)
			wattron(windows[Win_buflist].window, A_BOLD);
		ui_wprintc(&windows[Win_buflist], 1, "%02d: %s\n", i, "hirc");
		wattroff(windows[Win_buflist].window, A_BOLD);
	}
	i++;

	for (sp = servers; sp && (i - scroll - 1) < windows[Win_buflist].h; sp = sp->next) {
		if (scroll < i - 1) {
			if (selected.server == sp && !selected.channel)
				wattron(windows[Win_buflist].window, A_BOLD);

			if (sp->status == ConnStatus_notconnected)
				indicator = ui_format(NULL, config_gets("format.ui.buflist.old"), NULL);
			else
				indicator = ui_format_activity(sp->history->activity);

			ui_wprintc(&windows[Win_buflist], 1, "%02d: %s─ %s%s\n", i, sp->next ? "├" : "└", indicator, sp->name);
			wattrset(windows[Win_buflist].window, A_NORMAL);
		}
		i++;

		for (chp = sp->channels; chp && (i - scroll - 1) < windows[Win_buflist].h; chp = chp->next) {
			if (scroll < i - 1) {
				if (selected.channel == chp)
					wattron(windows[Win_buflist].window, A_BOLD);

				if (chp->old)
					indicator = ui_format(NULL, config_gets("format.ui.buflist.old"), NULL);
				else
					indicator = ui_format_activity(chp->history->activity);

				ui_wprintc(&windows[Win_buflist], 1, "%02d: %s  %s─ %s%s\n", i,
						sp->next ? "│" : " ", chp->next || sp->privs ? "├" : "└", indicator, chp->name);
				wattrset(windows[Win_buflist].window, A_NORMAL);
			}
			i++;
		}

		for (prp = sp->privs; prp && (i - scroll - 1) < windows[Win_buflist].h; prp = prp->next) {
			if (scroll < i - 1) {
				if (selected.channel == prp)
					wattron(windows[Win_buflist].window, A_BOLD);

				if (prp->old)
					indicator = ui_format(NULL, config_gets("format.ui.buflist.old"), NULL);
				else
					indicator = ui_format_activity(prp->history->activity);

				ui_wprintc(&windows[Win_buflist], 1, "%02d: %s  %s─ %s%s\n", i,
						sp->next ? "│" : " ", prp->next ? "├" : "└", indicator, prp->name);
				wattrset(windows[Win_buflist].window, A_NORMAL);
			}
			i++;
		}
	}

	if (i <= ui_buflist_count(NULL, NULL, NULL)) {
		wmove(windows[Win_buflist].window, windows[Win_buflist].h - 1, 0);
		ui_wprintc(&windows[Win_buflist], 1, "%s\n", ui_format(NULL, config_gets("format.ui.buflist.more"), NULL));
		wclrtoeol(windows[Win_buflist].window);
	}
}

int
ui_wprintc(struct Window *window, int lines, char *format, ...) {
	char utfbuf[5];
	char str[1024], *s;
	va_list ap;
	int ret;
	attr_t curattr;
	int temp; /* used only for wattr_get,
		     because ncurses is dumb */
	int cc, lc, elc, utfc;
	char colourbuf[2][3];
	int colours[2];
	int colour = 0;
	int bold = 0;
	int underline = 0;
	int reverse = 0;
	int italic = 0;

	va_start(ap, format);
	ret = vsnprintf(str, sizeof(str), format, ap);
	va_end(ap);
	if (ret < 0)
		return ret;

	if (lines < 0)
		ui_strlenc(window, str, &elc);
	elc -= 1;

	for (ret = cc = lc = 0, s = str; s && *s; s++) {
		switch (*s) {
		case 2: /* ^B */
			if (bold)
				wattroff(window->window, A_BOLD);
			else
				wattron(window->window, A_BOLD);
			bold = bold ? 0 : 1;
			break;
		case 3: /* ^C */
			memset(colourbuf, '\0', sizeof(colourbuf));
			/* This section may look a little confusing, but I didn't know
			 * what better way I could do it (a loop for two things? ehm).
			 *
			 * If you want to understand it, I would start with simulating
			 * it on a peice of paper, something like this:
			 *
			 * {   ,   ,'\0'}   ^C01,02
			 * {   ,   ,'\0'}
			 *
			 * Draw a line over *s each time you advance s. */
			if (*s && isdigit(*(s+1))) {
				colourbuf[0][0] = *(s+1);
				s += 1;
			}
			if (*s && isdigit(*(s+1))) {
				colourbuf[0][1] = *(s+1);
				s += 1;
			}
			if (*s && *(s+1) == ',' && isdigit(*(s+2))) {
				colourbuf[1][0] = *(s+2);
				s += 2;
			}
			if (colourbuf[1][0] && *s && isdigit(*(s+1))) {
				colourbuf[1][1] = *(s+1);
				s += 1;
			}

			colours[0] = colourbuf[0][0] ? atoi(colourbuf[0]) : 99;
			colours[1] = colourbuf[1][0] ? atoi(colourbuf[1]) : 99;

			wattr_get(window->window, &curattr, &temp, NULL);
			wattr_set(window->window, curattr, ui_get_pair(colours[0], colours[1]), NULL);
			colour = 1;
			break;
		case 9: /* ^I */
#ifdef A_ITALIC
			if (italic)
				wattroff(window->window, A_ITALIC);
			else
				wattron(window->window, A_ITALIC);
			italic = italic ? 0 : 1;
#endif /* A_ITALIC */
			break;
		case 15: /* ^O */
			colour = 0;
			bold = 0;
			underline = 0;
			reverse = 0;
			italic = 0;
			/* Setting A_NORMAL turns everything off,
			 * without using 5 different attroffs */
			wattrset(window->window, A_NORMAL);
			break;
		case 18: /* ^R */
			if (reverse)
				wattroff(window->window, A_REVERSE);
			else
				wattron(window->window, A_REVERSE);
			reverse = reverse ? 0 : 1;
			break;
		case 21: /* ^U */
			if (underline)
				wattroff(window->window, A_UNDERLINE);
			else
				wattron(window->window, A_UNDERLINE);
			underline = underline ? 0 : 1;
			break;
		default:
			if (lines > 0 && lc >= lines)
				goto end;
			if (!lines || lines > 0 || (lines < 0 && lc >= elc + lines)) {
				if ((*s & 0xC0) == 0xC0) {
					/* Copy a 11xxxxxx byte and
					 * stop when a byte doesn't
					 * match 10xxxxxx, to leave
					 * a full char for writing. */
					memset(utfbuf, '\0', sizeof(utfbuf));
					utfbuf[0] = *s;
					for (utfc = 1, s++; (*s & 0xC0) != 0xC0 && (*s & 0x80) == 0x80 && utfc < sizeof(utfbuf); utfc++, s++)
						utfbuf[utfc] = *s;
					waddstr(window->window, utfbuf);
					s--;
					ret++;
					cc++;
				} else if (!(*s & 0x80)) {
					/* ANDing with 0x80
					 * makes certain we
					 * ignore malformed
					 * utf-8 characters */
					waddch(window->window, *s);
					ret++;
					cc++;
				}
			}
			if (cc == window->w || *s == '\n') {
				lc++;
				cc = 0;
			}
			break;
		}
	}

end:
	colour = 0;
	bold = 0;
	underline =0;
	reverse = 0;
	italic = 0;
	wattrset(window->window, A_NORMAL);

	return ret;
}

int
ui_strlenc(struct Window *window, char *s, int *lines) {
	int ret, cc, lc;

	for (ret = cc = lc = 0; s && *s; s++) {
		switch (*s) {
		case 2:  /* ^B */
		case 9:  /* ^I */
		case 15: /* ^O */
		case 18: /* ^R */
		case 21: /* ^U */
			break;
		case 3:  /* ^C */
			if (*s && isdigit(*(s+1)))
				s += 1;
			if (*s && isdigit(*(s+1)))
				s += 1;
			if (*s && *(s+1) == ',' && isdigit(*(s+2)))
				s += 2;
			if (*s && *(s-1) == ',' && isdigit(*(s+1)))
				s += 1;
			break;
		default:
			/* naive utf-8 handling:
			 * the 2-nth byte always
			 * follows 10xxxxxxx, so
			 * don't count it. */
			if ((*s & 0xC0) != 0x80)
				cc++;
			ret++;
			if (window && cc == window->w || *s == '\n') {
				lc++;
				cc = 0;
			}
			break;
		}
	}

	if (lines)
		*lines = lc + 1;
	return ret;
}

static char *
ui_get_pseudocmd(struct History *hist) {
	char *cmd, *p1, *p2;

	cmd = *(hist->params);
	p1 = *(hist->params+1);
	p2 = *(hist->params+2);

	if (strcmp_n(cmd, "MODE") == 0) {
		if (p1 && serv_ischannel(hist->origin->server, p1))
			return "MODE-CHANNEL";
		else if (hist->from && nick_isself(hist->from) && strcmp_n(hist->from->nick, p1) == 0)
			return "MODE-NICK-SELF";
		else
			return "MODE-NICK";
	}

	if (strcmp_n(cmd, "PRIVMSG") == 0) {
		/* ascii 1 is ^A */
		if (*p2 == 1 && strncmp(p2 + 1, "ACTION", strlen("ACTION")) == 0)
			return "PRIVMSG-ACTION";
		else if (*p2 == 1)
			return "PRIVMSG-CTCP";
	}

	if (strcmp_n(cmd, "NOTICE") == 0 && *p2 == 1)
		return "NOTICE-CTCP";

	return cmd;
}

int
ui_hist_print(struct Window *window, int lines, struct History *hist) {
	char *cmd, *p1, *p2;
	int i;

	if (!hist)
		return -1;

	if (!hist->params)
		goto raw;

	cmd = ui_get_pseudocmd(hist);

	for (i=0; formatmap[i].cmd; i++)
		if (formatmap[i].format && strcmp_n(formatmap[i].cmd, cmd) == 0)
			return ui_wprintc(window, lines, "%s\n", ui_format(window, config_gets(formatmap[i].format), hist));

	if (isdigit(*cmd) && isdigit(*(cmd+1)) && isdigit(*(cmd+2)) && !*(cmd+3))
		return ui_wprintc(window, lines, "%s\n", ui_format(window, config_gets("format.rpl.other"), hist));

raw:
	return ui_wprintc(window, lines, "%s\n", ui_format(window, config_gets("format.other"), hist));
}

int
ui_hist_len(struct Window *window, struct History *hist, int *lines) {
	char *cmd;
	int i;

	if (!hist)
		return -1;

	if (!hist->params)
		goto raw;

	cmd = ui_get_pseudocmd(hist);

	for (i=0; formatmap[i].cmd; i++)
		if (formatmap[i].format && strcmp_n(formatmap[i].cmd, cmd) == 0)
			return ui_strlenc(window, ui_format(window, config_gets(formatmap[i].format), hist), lines);

	if (isdigit(*cmd) && isdigit(*(cmd+1)) && isdigit(*(cmd+2)) && !*(cmd+3))
		return ui_strlenc(window, ui_format(window, config_gets("format.rpl.other"), hist), lines);

raw:
	return ui_strlenc(window, ui_format(window, config_gets("format.other"), hist), lines);
}

void
ui_draw_main(void) {
	struct History *p;
	int y, lines;
	int i;

	werase(windows[Win_main].window);

	for (i=0, p = selected.history->history; p && p->next && i < windows[Win_main].scroll; i++)
		p = p->next;

	if (i)
		windows[Win_main].scroll = i;

	y = windows[Win_main].h;
	for (; p && y > 0; p = p->next) {
		if (!(p->options & HIST_SHOW))
			continue;
		if (ui_hist_len(&windows[Win_main], p, &lines) <= 0)
			continue;
		y = y - lines;
		if (y < lines) {
			y *= -1;
			wmove(windows[Win_main].window, 0, 0);
			ui_hist_print(&windows[Win_main], y, p);
			break;
		}
		wmove(windows[Win_main].window, y, 0);
		ui_hist_print(&windows[Win_main], 0, p);
	}

	if (selected.channel && selected.channel->topic) {
		wmove(windows[Win_main].window, 0, 0);
		ui_wprintc(&windows[Win_main], 0, "%s\n", ui_format(&windows[Win_main], config_gets("format.ui.topic"), NULL));
	}
}

void
ui_select(struct Server *server, struct Channel *channel) {
	selected.channel  = channel;
	selected.server   = server;
	selected.history  = channel ? channel->history : server ? server->history : main_buf;
	selected.name     = channel ? channel->name    : server ? server->name    : "hirc";
	selected.hasnicks = channel ? !channel->priv && !channel->old : 0;

	selected.history->activity = Activity_none;
	selected.history->unread = 0;

	hist_purgeopt(selected.history, HIST_TMP);
	if (!selected.hasnicks)
		windows[Win_nicklist].location = HIDDEN;
	else
		windows[Win_nicklist].location = config_getl("nicklist.location");
	windows[Win_main].scroll = -1;
	ui_redraw();
}

static char *
ui_format_get_content(char *sstr, int nesting) {
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
ui_format(struct Window *window, char *format, struct History *hist) {
	static char ret[8192];
	static int recursive = 0;
	struct Nick *nick;
	size_t rc, pc;
	int escape, i;
	long pn;
	int rhs = 0;
	int divider = 0;
	char **params;
	char *content, *p, *p2;
	char *ts, *save;
	char colourbuf[2][3];
	char chs[2];
	enum {
		sub_raw,
		sub_cmd,
		sub_nick,
		sub_ident,
		sub_host,
		sub_channel,
		sub_topic,
		sub_server,
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
		[sub_channel]	= {"channel", NULL},
		[sub_topic]	= {"topic", NULL},
		[sub_server]	= {"server", NULL},
		{NULL, NULL},
	};

	/* ${time} is implemented specially so doesn't appear in list */

	subs[sub_channel].val = selected.channel ? selected.channel->name  : NULL;
	subs[sub_topic].val   = selected.channel ? selected.channel->topic : NULL;
	subs[sub_server].val  = selected.server  ? selected.server->name   : NULL;

	if (hist) {
		subs[sub_raw].val   = hist->raw;
		subs[sub_nick].val  = hist->from ? hist->from->nick  : NULL;
		subs[sub_ident].val = hist->from ? hist->from->ident : NULL;
		subs[sub_host].val  = hist->from ? hist->from->host  : NULL;

		if (hist->origin) {
			if (hist->origin->channel) {
				divider = config_getl("divider.toggle");
				subs[sub_channel].val = hist->origin->channel->name;
				subs[sub_topic].val   = hist->origin->channel->topic;
			}
			if (hist->origin->server) {
				subs[sub_server].val  = hist->origin->server->name;
			}
		}

		params = hist->params;
		subs[sub_cmd].val = *params;
		params++;
	}

	if (!recursive && hist && config_getl("timestamp.toggle")) {
		recursive = 1;
		ts = estrdup(ui_format(NULL, config_gets("format.ui.timestamp"), hist));
		recursive = 0;
	} else {
		ts = "";
	}

	for (escape = 0, rc = 0; format && *format && rc < sizeof(ret); ) {
		if (!escape && *format == '$' && *(format+1) == '{' && strchr(format, '}')) {
			escape = 0;
			content = ui_format_get_content(format+2, 0);

			for (p = content; *p && isdigit(*p); p++);
			/* If all are digits, *p == '\0' */
			if (!*p && hist) {
				pn = strtol(content, NULL, 10) - 1;
				if (pn >= 0 && param_len(params) > pn) {
					if (**(params+pn) == 1 && strncmp((*(params+pn))+1, "ACTION", strlen("ACTION")) == 0 && strchr(*(params+pn), ' '))
						rc += snprintf(&ret[rc], sizeof(ret) - rc, "%s", struntil(strchr(*(params+pn), ' ') + 1, 1));
					else if (**(params+pn) == 1)
						rc += snprintf(&ret[rc], sizeof(ret) - rc, "%s", struntil((*(params+pn)) + 1, 1));
					else
						rc += snprintf(&ret[rc], sizeof(ret) - rc, "%s", *(params+pn));
					format = strchr(format, '}') + 1;
					continue;
				}
			}
			/* All are digits except a trailing '-' */
			if (*p == '-' && *(p+1) == '\0' && hist) {
				pn = strtol(content, NULL, 10) - 1;
				if (pn >= 0 && param_len(params) > pn) {
					for (; *(params+pn) != NULL; pn++) {
						if (**(params+pn) == 1 && strncmp((*(params+pn))+1, "ACTION", strlen("ACTION")) == 0 && strchr(*(params+pn), ' ')) {
							rc += snprintf(&ret[rc], sizeof(ret) - rc, "%s%s",
									struntil(strchr(*(params+pn), ' ') + 1, 1),
									*(params+pn+1) ? " " : "");
						} else if (**(params+pn) == 1) {
							rc += snprintf(&ret[rc], sizeof(ret) - rc, "%s%s",
									struntil((*(params+pn)) + 1, 1),
									*(params+pn+1) ? " " : "");
						} else {
							rc += snprintf(&ret[rc], sizeof(ret) - rc, "%s%s",
									*(params+pn), *(params+pn+1) ? " " : "");
						}
					}
					format = strchr(format, '}') + 1;
					continue;
				}
			}

			if (hist && content && strncmp(content, "time:", strlen("time:")) == 0 || strcmp(content, "time") == 0) {
				/* This always continues, so okay to modify content */
				content = strtok(content, ":");
				content = strtok(NULL, ":");

				if (!content)
					rc += strftime(&ret[rc], sizeof(ret) - rc, "%H:%M", gmtime(&hist->timestamp));
				else
					rc += strftime(&ret[rc], sizeof(ret) - rc, content, gmtime(&hist->timestamp));
				format = strchr(format, '}') + 1;
				continue;
			}

			for (i=0; subs[i].name; i++) {
				if (strcmp_n(subs[i].name, content) == 0) {
					if (subs[i].val)
						rc += snprintf(&ret[rc], sizeof(ret) - rc, "%s", subs[i].val);
					format = strchr(format, '}') + 1;
					continue;
				}
			}
		}

		if (!escape && *format == '%' && *(format+1) == '{' && strchr(format, '}')) {
			escape = 0;
			content = ui_format_get_content(format+2, 0);

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
						rc += snprintf(&ret[rc], sizeof(ret) - rc, "%c%02d,%02d", 3 /* ^C */,
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
					rc = snprintf(ret, sizeof(ret), "%1$*3$s%2$s", save, config_gets("divider.string"),
							config_getl("divider.margin") + (strlen(ret) - ui_strlenc(window, ret, NULL)));
					free(save);
					format = strchr(format, '}') + 1;
					continue;
				} else if (*(content+1) == '\0') {
					ret[rc++] = ' ';
					format = strchr(format, '}') + 1;
					continue;
				}
				break;
			}

			/* pad, nick and split must then continue as they modify content */
			if (strncmp(content, "pad:", strlen("pad:")) == 0 && strchr(content, ',')) {
				pn = strtol(content + strlen("pad:"), NULL, 10);
				content = estrdup(ui_format_get_content(strchr(format+2+strlen("pad:"), ',') + 1, 1));
				save = estrdup(ret);
				recursive = 1;
				p = estrdup(ui_format(NULL, content, hist));
				recursive = 0;
				memcpy(ret, save, rc);
				rc += snprintf(&ret[rc], sizeof(ret) - rc, "%1$*2$s", p, pn);
				format = strchr(format+2+strlen("pad:"), ',') + strlen(content) + 2;

				free(content);
				free(save);
				free(p);
				continue;
			}

			/* second comma ptr - second comma ptr = distance.
			 * If the distance is 2, then there is one non-comma char between. */
			p = strchr(content, ',');
			if (p)
				p2 = strchr(p + 1, ',');
			if (strncmp(content, "split:", strlen("split:")) == 0 && p2 - p == 2) {
				pn = strtol(content + strlen("split:"), NULL, 10);
				chs[0] = *(strchr(content, ',') + 1);
				chs[1] = '\0';
				content = estrdup(ui_format_get_content(
							strchr(
								strchr(format+2+strlen("split:"), ',') + 1,
								',') + 1,
							1));
				save = estrdup(ret);
				recursive = 1;
				p = estrdup(ui_format(NULL, content, hist));
				recursive = 0;
				memcpy(ret, save, rc);
				rc += snprintf(&ret[rc], sizeof(ret) - rc, "%s", strntok(p, chs, pn));
				format = strchr(
					strchr(format+2+strlen("split:"), ',') + 1,
					',') + strlen(content) + 2;

				free(content);
				free(save);
				free(p);
				continue;
			}

			if (hist && !recursive && strncmp(content, "nick:", strlen("nick:")) == 0) {
				content = estrdup(ui_format_get_content(format+2+strlen("nick:"), 1));
				save = estrdup(ret); /* save ret, as this will be modified by recursing */
				recursive = 1;
				p = estrdup(ui_format(NULL, content, hist));
				recursive = 0;
				memcpy(ret, save, rc); /* copy saved value back into ret, we don't 
							  need strlcpy as we don't use null byte */
				nick = nick_create(p, ' ', hist->origin ? hist->origin->server : NULL);
				rc += snprintf(&ret[rc], sizeof(ret) - rc, "%c%02d", 3 /* ^C */, nick_getcolour(nick));
				format += 3 + strlen("nick:") + strlen(content);

				nick_free(nick);
				free(content);
				free(save);
				free(p);
				continue;
			}
		}

		if (escape && *format == 'n') {
			ret[rc++] = '\n';
			rc += snprintf(&ret[rc], sizeof(ret) - rc, "%1$*3$s%2$s", "", config_gets("divider.string"),
					ui_strlenc(NULL, ts, NULL) + config_getl("divider.margin"));
			escape = 0;
			format++;
			continue;
		}

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
		rc = snprintf(ret, sizeof(ret), "%1$*4$s%2$s%3$s", "", config_gets("divider.string"), save, config_getl("divider.margin"));
		free(save);
	}

	save = estrdup(ret);
	rc = snprintf(ret, sizeof(ret), "%s%s", ts, save);
	free(save);

	if (!recursive && window) {
		for (p = ret, pc = 0; p && p <= (ret + sizeof(ret)); p++) {
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
						p += snprintf(p, sizeof(ret) - ((size_t)(p - ret)), "%1$*4$s %2$s%3$s",
								"", config_gets("divider.string"), save,
								config_getl("divider.margin") + ui_strlenc(NULL, ts, NULL));
					} else {
						p += snprintf(p, sizeof(ret) - ((size_t)(p - ret)), "%1$*3$s %2$s", "", save, ui_strlenc(NULL, ts, NULL));
					}

					free(save);
					pc = 0;
				}
			}
		}
	}

	if (ts[0] != '\0')
		free(ts);

	return ret;
}

char *
ui_rectrl(char *str) {
	static char ret[8192];
	static char *rp = NULL;
	int caret, rc;
	char c;

	if (rp) {
		free(rp);
		rp = NULL;
	}

	for (rc = 0, caret = 0; str && *str; str++) {
		if (caret) {
			c = toupper(*str) - 64;
			if (c <= 31 && c >= 0) {
				ret[rc++] = c;
			} else {
				ret[rc++] = '^';
				ret[rc++] = *str;
			}
			caret = 0;
		} else if (*str == '^') {
			caret = 1;
		} else {
			ret[rc++] = *str;
		}
	}

	if (caret)
		ret[rc++] = '^';
	ret[rc] = '\0';
	rp = estrdup(ret);

	return rp;
}

char *
ui_unctrl(char *str) {
	static char ret[8192];
	static char *rp = NULL;;
	int rc;

	if (rp) {
		free(rp);
		rp = NULL;
	}

	for (rc = 0; str && *str; str++) {
		if (*str <= 31 && *str >= 0) {
			ret[rc++] = '^';
			ret[rc++] = (*str) + 64;
		} else {
			ret[rc++] = *str;
		}
	}

	ret[rc] = '\0';
	rp = estrdup(ret);

	return rp;
}

int
ui_bind(char *binding, char *cmd) {
	struct Keybind *p;
	char *tmp;

	if (!binding || !cmd)
		return -1;

	p = emalloc(sizeof(struct Keybind));
	p->binding = estrdup(ui_rectrl(binding));
	if (*cmd != '/') {
		tmp = emalloc(strlen(cmd) + 2);
		snprintf(tmp, strlen(cmd) + 2, "/%s", cmd);
		p->cmd = tmp;
	} else {
		p->cmd = estrdup(cmd);
	}
	p->prev = NULL;
	p->next = keybinds;
	if (keybinds)
		keybinds->prev = p;
	keybinds = p;

	return 0;
}

int
ui_unbind(char *binding) {
	struct Keybind *p;

	if (!binding)
		return -1;

	for (p=keybinds; p; p = p->next) {
		if (strcmp(p->binding, binding) == 0) {
			if (p->prev)
				p->prev->next = p->next;
			else
				keybinds = p->next;

			if (p->next)
				p->next->prev = p->prev;

			free(p->binding);
			free(p->cmd);
			free(p);
			return 0;
		}
	}

	return -1;
}
