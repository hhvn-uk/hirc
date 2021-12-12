/* See LICENSE for copyright details */

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
	[Win_main]	= {.handler = ui_draw_main},
	[Win_input]	= {.handler = ui_draw_input},
	[Win_nicklist]	= {.handler = ui_draw_nicklist},
	[Win_buflist]	= {.handler = ui_draw_buflist},
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
	{"SELF_TLSNOTCOMPILED",	"format.ui.tlsnotcompiled"},
#endif /* TLS */
	{"SELF_KEYBIND_START",	"format.ui.keybind.start"},
	{"SELF_KEYBIND_LIST",	"format.ui.keybind"},
	{"SELF_KEYBIND_END",	"format.ui.keybind.end"},
	{"SELF_GREP_START",	"format.ui.grep.start"},
	{"SELF_GREP_END",	"format.ui.grep.end"},
	{"SELF_ALIAS_START",	"format.ui.alias.start"},
	{"SELF_ALIAS_LIST",	"format.ui.alias"},
	{"SELF_ALIAS_END",	"format.ui.alias.end"},
	/* Real commands/numerics from server */
	{"PRIVMSG", 		"format.privmsg"},
	{"JOIN",		"format.join"},
	{"PART",		"format.part"},
	{"KICK",		"format.kick"},
	{"QUIT",		"format.quit"},
	{"001",			"format.rpl.welcome"},
	{"002",			"format.rpl.yourhost"},
	{"003",			"format.rpl.created"},
	{"004",			"format.rpl.myinfo"},
	{"005",			"format.rpl.isupport"},
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
	/* Pseudo commands for specific formatting */
	{"MODE-NICK-SELF",	"format.mode.nick.self"},
	{"MODE-NICK",		"format.mode.nick"},
	{"MODE-CHANNEL",	"format.mode.channel"},
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
ui_error_(char *file, int line, char *format, ...) {
	char msg[1024];
	va_list ap;

	va_start(ap, format);
	vsnprintf(msg, sizeof(msg), format, ap);
	va_end(ap);

	hist_format(selected.history, Activity_error, HIST_SHOW|HIST_TMP|HIST_MAIN,
			"SELF_ERROR %s %d :%s",
			file, line, msg);
}

void
ui_perror_(char *file, int line, char *str) {
	hist_format(selected.history, Activity_error, HIST_SHOW|HIST_TMP|HIST_MAIN,
			"SELF_ERROR %s %d :%s: %s",
			file, line, str, strerror(errno));
}

#ifdef TLS
void
ui_tls_config_error_(char *file, int line, struct tls_config *config, char *str) {
	hist_format(selected.history, Activity_error, HIST_SHOW|HIST_TMP|HIST_MAIN,
			"SELF_ERROR %s %d :%s: %s",
			file, line, str, tls_config_error(config));
}

void
ui_tls_error_(char *file, int line, struct tls *ctx, char *str) {
	hist_format(selected.history, Activity_error, HIST_SHOW|HIST_TMP|HIST_MAIN,
			"SELF_ERROR %s %d :%s: %s",
			file, line, str, tls_error(ctx));
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

	input.string[0] = '\0';
	memset(input.history, 0, sizeof(input.history));
	input.counter = 0;

	windows[Win_nicklist].location = config_getl("nicklist.location");
	windows[Win_buflist].location = config_getl("buflist.location");

	windows[Win_main].window = newwin(0, 0, 0, 0);
	windows[Win_input].window = newwin(0, 0, 0, 0);
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
						command_eval(kp->cmd);
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
					backup = strdup(input.string);
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
		case '\n':
			command_eval(input.string);
			/* free checks for null */
			free(input.history[INPUT_HIST_MAX - 1]);
			memmove(input.history + 1, input.history, (sizeof(input.history) / INPUT_HIST_MAX) * (INPUT_HIST_MAX - 1));
			input.history[0] = strdup(input.string);
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

	if (x)
		mvvline(0, x - 1, '|', LINES);
	if (rx)
		mvvline(0, COLS - rx, '|', LINES);

	mvhline(LINES - 2, x, '-', COLS - x - rx);
	refresh();

	for (i = 0; i < Win_last; i++) {
		ui_placewindow(&windows[i]);
		windows[i].refresh = 1;
	}
}

void
ui_draw_input(void) {
	char utfbuf[5];
	char *p;
	int utfc;
	int offset;
	int x;

	ui_wclear(&windows[Win_input]);

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
	int y;

	ui_wclear(&windows[Win_nicklist]);

	if (!selected.channel || !windows[Win_nicklist].location)
		return;

	wmove(windows[Win_nicklist].window, 0, 0);

	nick_sort(&selected.channel->nicks, selected.server);
	/* TODO: more nicks than screen height? */
	for (p = selected.channel->nicks; p; p = p->next) {
		ui_wprintc(&windows[Win_nicklist], 0, "%c%02d%c%s\n",
				3 /* ^C */, nick_getcolour(p), p->priv, p->nick);
	}
}

int
ui_buflist_count(int *ret_servers, int *ret_channels) {
	struct Server *sp;
	struct Channel *chp;
	int sc, cc;

	for (sc = cc = 0, sp = servers; sp; sp = sp->next, sc++)
		for (chp = sp->channels; chp; chp = chp->next, cc++)
			;

	if (ret_servers)
		*ret_servers = sc;
	if (ret_channels)
		*ret_channels = cc;

	return sc + cc + 1;
}

void
ui_buflist_select(int num) {
	struct Server *sp;
	struct Channel *chp;
	int i;

	if (num <= 0) {
		ui_error("buffer index greater than 0 expected", NULL);
		return;
	}

	if (num == 1) {
		ui_select(NULL, NULL);
		return;
	}

	for (i = 2, sp = servers; sp; sp = sp->next) {
		if (i == num) {
			ui_select(sp, NULL);
			return;
		}
		i++; /* increment before moving
			to channel section, not
			int for (;; ..) */

		for (chp = sp->channels; chp; chp = chp->next, i++) {
			if (i == num) {
				ui_select(sp, chp);
				return;
			}
		}
	}

	ui_error("couldn't select buffer with index %d", num);
}

void
ui_draw_buflist(void) {
	struct Server *sp;
	struct Channel *chp;
	int i = 1, len, tmp;
	int sc, cc, y;

	ui_wclear(&windows[Win_buflist]);

	if (!windows[Win_buflist].location)
		return;

	if (selected.history == main_buf)
		wattron(windows[Win_buflist].window, A_BOLD);
	len = wprintw(windows[Win_buflist].window, "%02d: %s\n", i++, "hirc");
	wattroff(windows[Win_buflist].window, A_BOLD);

	for (sc = cc = 0, sp = servers; sp; sp = sp->next, sc++) {
		if (selected.server == sp && !selected.channel)
			wattron(windows[Win_buflist].window, A_BOLD);
		else if (sp->status != ConnStatus_connected)
			wattron(windows[Win_buflist].window, A_DIM);

		len = wprintw(windows[Win_buflist].window, "%02d: %s─ %s\n", i++, sp->next ? "├" : "└", sp->name);
		wattroff(windows[Win_buflist].window, A_BOLD);
		wattroff(windows[Win_buflist].window, A_DIM);

		for (chp = sp->channels; chp; chp = chp->next, cc++) {
			if (selected.channel == chp)
				wattron(windows[Win_buflist].window, A_BOLD);
			else if (chp->old)
				wattron(windows[Win_buflist].window, A_DIM);

			len = wprintw(windows[Win_buflist].window, "%02d: %s  %s─ %s\n", i++,
					sp->next ? "│" : " ", chp->next ? "├" : "└", chp->name);
			wattroff(windows[Win_buflist].window, A_BOLD);
			wattroff(windows[Win_buflist].window, A_DIM);
		}
	}

	/* One could use ui_buflist_count here (and I have tested it: works) but
	 * it requires two passes over the servers and channels, whilst only one
	 * when integrated to the loop above. */
	wmove(windows[Win_buflist].window, windows[Win_buflist].h - 1, 0);
	len = wprintw(windows[Win_buflist].window, "[S: %02d | C: %02d]", sc, cc);
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
			if (cc == window->w) {
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
			if (window && cc == window->w) {
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

void
ui_filltoeol(struct Window *window, char c) {
	int y, x;

	getyx(window->window, y, x);
	for (; x < window->w; x++)
		waddch(window->window, c);
}

void
ui_wclear(struct Window *window) {
	int y;

	for (y = 0; y <= window->h; y++) {
		wmove(window->window, y, 0);
		ui_filltoeol(window, ' ');
	}
	wmove(window->window, 0, 0);
}

int
ui_hist_print(struct Window *window, int lines, struct History *hist) {
	char *cmd, *p1, *chantypes;
	int i;

	if (!hist)
		return -1;

	if (!hist->params || !*(hist->params+1))
		goto raw;

	if (**(hist->params) == ':') {
		cmd = *(hist->params+1);
		p1 = *(hist->params+2);
	} else {
		cmd = *(hist->params);
		p1 = *(hist->params+1);
	}

	if (strcmp_n(cmd, "MODE") == 0) {
		if (hist->origin && hist->origin->server)
			chantypes = support_get(hist->origin->server, "CHANTYPES");
		else
			chantypes = config_gets("def.chantypes");

		if (p1 && strchr(chantypes, *p1))
			cmd = "MODE-CHANNEL";
		else if (hist->from && nick_isself(hist->from) && strcmp_n(hist->from->nick, p1) == 0)
			cmd = "MODE-NICK-SELF";
		else
			cmd = "MODE-NICK";
	}

	for (i=0; formatmap[i].cmd; i++)
		if (formatmap[i].format && strcmp_n(formatmap[i].cmd, cmd) == 0)
			return ui_wprintc(window, lines, "%s\n", ui_format(config_gets(formatmap[i].format), hist));

	if (isdigit(*cmd) && isdigit(*(cmd+1)) && isdigit(*(cmd+2)) && !*(cmd+3))
		return ui_wprintc(window, lines, "%s\n", ui_format(config_gets("format.rpl.other"), hist));

raw:
	return ui_wprintc(window, lines, "%s\n", ui_format(config_gets("format.other"), hist));
}

int
ui_hist_len(struct Window *window, struct History *hist, int *lines) {
	char *cmd;
	int i;

	if (!hist)
		return -1;

	if (!hist->params || !*(hist->params+2))
		goto raw;

	if (**(hist->params) == ':')
		cmd = *(hist->params+2);
	else
		cmd = *(hist->params);

	for (i=0; formatmap[i].cmd; i++)
		if (formatmap[i].format && strcmp_n(formatmap[i].cmd, cmd) == 0)
			return ui_strlenc(window, ui_format(config_gets(formatmap[i].format), hist), lines);

	if (isdigit(*cmd) && isdigit(*(cmd+1)) && isdigit(*(cmd+2)) && !*(cmd+3))
		return ui_strlenc(window, ui_format(config_gets("format.rpl.other"), hist), lines);

raw:
	return ui_strlenc(window, ui_format(config_gets("format.other"), hist), lines);
}

void
ui_draw_main(void) {
	struct History *p;
	int y, lines;

	ui_wclear(&windows[Win_main]);

	y = windows[Win_main].h;
	for (p = selected.history->history; p && y > 0; p = p->next) {
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
		ui_wprintc(&windows[Win_main], 0, "%s\n", ui_format(config_gets("format.ui.topic"), NULL));
	}
}

void
ui_select(struct Server *server, struct Channel *channel) {
	selected.channel = channel;
	selected.server  = server;
	selected.history = channel ? channel->history : server ? server->history : main_buf;
	selected.name    = channel ? channel->name    : server ? server->name    : "hirc";

	hist_purgeopt(selected.history, HIST_TMP);
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
ui_format(char *format, struct History *hist) {
	static char ret[8192];
	static int recursive = 0;
	struct Nick *nick;
	int rc, escape, pn, i;
	int rhs = 0;
	int divider = 0;
	char **params;
	char *content, *p;
	char *ts, *save;
	char colourbuf[2][3];
	char printformat[64];
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

		if (**(params = hist->params) == ':')
			params++;

		subs[sub_cmd].val = *params;
		params++;
	}

	if (!recursive && hist && config_getl("timestamp.toggle")) {
		recursive = 1;
		ts = strdup(ui_format(config_gets("format.ui.timestamp"), hist));
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
				if (pn >= 0 && param_len(params) >= pn) {
					rc += snprintf(&ret[rc], sizeof(ret) - rc, "%s", *(params+pn));
					format = strchr(format, '}') + 1;
					continue;
				}
			}
			/* All are digits except a trailing '-' */
			if (*p == '-' && *(p+1) == '\0' && hist) {
				pn = strtol(content, NULL, 10) - 1;
				if (pn >= 0 && param_len(params) >= pn) {
					for (; *(params+pn) != NULL; pn++)
						rc += snprintf(&ret[rc], sizeof(ret) - rc, "%s%s", *(params+pn), *(params+pn+1) ? " " : "");
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
					/* strlen(ret) - ui_strlenc(NULL, ret, NULL) should get
					 * the length of hidden characters. Add this onto the
					 * margin to pad out properly. */
					snprintf(printformat, sizeof(printformat), "%%%lds%%s",
							config_getl("divider.margin") + (strlen(ret) - ui_strlenc(NULL, ret, NULL)));
					/* Save ret for use in snprintf */
					save = strdup(ret);
					rc = snprintf(ret, sizeof(ret), printformat, save, config_gets("divider.string"));
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

			/* This bit must come last as it modifies content */
			if (hist && !recursive && strncmp(content, "nick:", strlen("nick:")) == 0) {
				content = strdup(ui_format_get_content(format+2+strlen("nick:"), 1));
				save = strdup(ret); /* save ret, as this will be modified by recursing */
				recursive = 1;
				p = strdup(ui_format(content, hist));
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

		if (escape) {
			ret[rc++] = '\\';
			escape = 0;
		}

		if (*format == '\\') {
			escape = 1;
		} else {
			ret[rc++] = *format;
			format++;
		}
	}

	ret[rc] = '\0';
	if (!recursive && divider && !rhs) {
		snprintf(printformat, sizeof(printformat), "%%%lds%%s%%s", config_getl("divider.margin"));
		save = strdup(ret);
		rc = snprintf(ret, sizeof(ret), printformat, "", config_gets("divider.string"), save);
		free(save);
	}

	save = strdup(ret);
	rc = snprintf(ret, sizeof(ret), "%s%s", ts, save);
	free(save);

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

	for (rc = 0; str && *str; str++) {
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

	ret[rc] = '\0';
	rp = strdup(ret);

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
	rp = strdup(ret);

	return rp;
}

int
ui_bind(char *binding, char *cmd) {
	struct Keybind *p;
	char *tmp;

	if (!binding || !cmd)
		return -1;

	p = malloc(sizeof(struct Keybind));
	p->binding = strdup(ui_rectrl(binding));
	if (*cmd != '/') {
		tmp = malloc(strlen(cmd) + 2);
		snprintf(tmp, strlen(cmd) + 2, "/%s", cmd);
		p->cmd = tmp;
	} else {
		p->cmd = strdup(cmd);
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
