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
#include <wchar.h>
#include <wctype.h>
#include <stdarg.h>
#include <string.h>
#include <stdlib.h>
#include <locale.h>
#include <ncurses.h>
#ifdef TLS
#include <tls.h>
#endif /* TLS */
#include "hirc.h"
#include "data/colours.h"

int uineedredraw = 0;
int nouich = 0;

struct Window windows[Win_last] = {
	[Win_dummy]	= {.handler = NULL, .scroll = -1},
	[Win_main]	= {.handler = ui_draw_main, .scroll = -1},
	[Win_input]	= {.handler = ui_draw_input, .scroll = -1},
	[Win_nicklist]	= {.handler = ui_draw_nicklist, .scroll = -1},
	[Win_buflist]	= {.handler = ui_draw_buflist, .scroll = -1},
};

struct {
	wchar_t string[INPUT_MAX];
	unsigned counter;
	char *history[INPUT_HIST_MAX];
	int histindex;
} input;

struct Selected selected;
struct Keybind *keybinds = NULL;

void
ui_error_(char *file, int line, const char *func, char *fmt, ...) {
	char msg[1024];
	va_list ap;

	va_start(ap, fmt);
	vsnprintf(msg, sizeof(msg), fmt, ap);
	va_end(ap);

	hist_format(selected.history, Activity_error, HIST_UI|HIST_ERR|HIST_NIGN,
			"SELF_ERROR %s %d %s :%s",
			file, line, func, msg);
}

void
ui_perror_(char *file, int line, const char *func, char *str) {
	hist_format(selected.history, Activity_error, HIST_UI|HIST_ERR|HIST_NIGN,
			"SELF_ERROR %s %d %s :%s: %s",
			file, line, func, str, strerror(errno));
}

#ifdef TLS
void
ui_tls_config_error_(char *file, int line, const char *func, struct tls_config *config, char *str) {
	hist_format(selected.history, Activity_error, HIST_UI|HIST_ERR|HIST_NIGN,
			"SELF_ERROR %s %d %s :%s: %s",
			file, line, func, str, tls_config_error(config));
}

void
ui_tls_error_(char *file, int line, const char *func, struct tls *ctx, char *str) {
	hist_format(selected.history, Activity_error, HIST_UI|HIST_ERR|HIST_NIGN,
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

	input.string[0] = L'\0';
	memset(input.history, 0, sizeof(input.history));
	input.counter = 0;
	input.histindex = -1;

	windows[Win_nicklist].location = config_getl("nicklist.location");
	windows[Win_buflist].location = config_getl("buflist.location");

	windows[Win_dummy].window = stdscr;
	windows[Win_main].window = newwin(0, 0, 0, 0);
	windows[Win_input].window = newwin(0, 0, 0, 0);

	windows[Win_dummy].location = Location_hidden;
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
	if (window->location != Location_hidden) {
		wresize(window->window, window->h, window->w);
		mvwin(window->window, window->y, window->x);
		wrefresh(window->window);
	}
}

void
ui_read(void) {
	static wchar_t *backup = NULL;
	static int didcomplete = 0;
	struct Keybind *kp;
	char *str;
	wint_t key;
	int ret;
	int savecounter;

	savecounter = input.counter;

	/* Loop over input, return only if ERR is received.
	 * Normally wget_wch exits fast enough that unless something
	 * is being pasted in this won't waste any time that should
	 * be used for other stuff */
	for (;;) {
		ret = wget_wch(windows[Win_input].window, &key);
		if (ret == ERR) {
			/* no input received */
			/* Match keybinds here - this allows multikey
			 * bindings such as those with alt, but since
			 * there is no delay with wgetch() it's unlikely
			 * that the user pressing multiple keys will
			 * trigger one. */
			if (input.counter != savecounter) {
				for (kp = keybinds; kp; kp = kp->next) {
					if ((input.counter - savecounter) == wcslen(kp->wbinding) &&
							wcsncmp(kp->wbinding, &input.string[savecounter], (input.counter - savecounter)) == 0) {
						command_eval(selected.server, kp->cmd);
						memmove(&input.string[savecounter],
								&input.string[input.counter],
								(wcslen(&input.string[input.counter]) + 1) * sizeof(wchar_t));
						input.counter = savecounter;
						return;
					}
				}

				if (input.histindex) {
					pfree(&backup);
					backup = NULL;
					input.histindex = -1;
				}
			}

			windows[Win_input].handler();
			wrefresh(windows[Win_input].window);
			windows[Win_input].refresh = 0;
			return;
		}

		switch (key) {
		case KEY_RESIZE:
			ui_redraw();
			break;
		case KEY_BACKSPACE:
			if (input.counter) {
				memmove(input.string + input.counter - 1,
						input.string + input.counter,
						(wcslen(input.string + input.counter) + 1) * sizeof(wchar_t));
				input.counter--;
			}
			break;
		case KEY_UP:
			if (input.histindex < INPUT_HIST_MAX && input.history[input.histindex + 1]) {
				if (input.histindex == -1)
					backup = ewcsdup(input.string);
				input.histindex++;
				mbstowcs(input.string, input.history[input.histindex], sizeof(input.string));
				input.counter = wcslen(input.string);
			}
			return; /* return so histindex and backup aren't reset */
		case KEY_DOWN:
			if (input.histindex > -1) {
				input.histindex--;
				if (input.histindex == -1) {
					if (backup)
						wcslcpy(input.string, backup, sizeof(input.string));
					pfree(&backup);
					backup = NULL;
				} else {
					mbstowcs(input.string, input.history[input.histindex], sizeof(input.string));
				}
				input.counter = wcslen(input.string);
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
		case '\t':
			complete(input.string, sizeof(input.string), &input.counter);
			break;
		case KEY_ENTER:
		case '\r':
			if (didcomplete && input.string[input.counter - 1] == L' ')
				input.string[input.counter - 1] = '\0';
			if (*input.string != L'\0') {
				/* no need to free str as assigned to input.history[0] */
				str = wctos(input.string);
				command_eval(selected.server, str);
				pfree(&input.history[INPUT_HIST_MAX - 1]);
				memmove(input.history + 1, input.history, (sizeof(input.history) / INPUT_HIST_MAX) * (INPUT_HIST_MAX - 1));
				input.history[0] = str;
				input.string[0] = '\0';
				input.counter = 0;
				input.histindex = -1;
			}
			break;
		default:
			if (iswprint(key) || (iscntrl(key) && input.counter + 1 != sizeof(input.string))) {
					memmove(input.string + input.counter + 1,
							input.string + input.counter,
							(wcslen(input.string + input.counter) + 1) * sizeof(wchar_t));
					input.string[input.counter++] = (wchar_t)key;
			}
			break;
		}

		/* Completion adds a space after the completed text: this
		 * serves the purpose of showing the user that it has been
		 * completely successfully and unambiguously, and allowing them
		 * to begin writing quickly without having to type a space
		 * (this is common in most tab-completion systems).
		 *
		 * The problem is that if something is completed at the end of
		 * a command, and the user then hits enter, that space would be
		 * passed to command_* functions (previous versions of hirc did
		 * this, and put the onus on commands to strip trailing
		 * spaces).
		 *
		 * Instead, this variable is set so that trailing spaces will
		 * be stripped when tab completed, but not ones inserted
		 * manually, inside ui_read() - see the KEY_ENTER case. */
		didcomplete = (key == '\t');
	}
}

void
ui_redraw(void) {
	struct History *p;
	char *fmt;
	long nicklistwidth, buflistwidth;
	int x = 0, rx = 0;
	int i;

	nicklistwidth = config_getl("nicklist.width");
	buflistwidth = config_getl("buflist.width");

	/* TODO: what if nicklistwidth or buflistwidth is too big? */
	if (windows[Win_buflist].location == Location_left) {
		windows[Win_buflist].x = windows[Win_buflist].y = 0;
		windows[Win_buflist].h = LINES;
		windows[Win_buflist].w = buflistwidth;
		x = windows[Win_buflist].w + 1;
	}
	if (windows[Win_nicklist].location == Location_left) {
		windows[Win_nicklist].x = windows[Win_buflist].y = 0;
		windows[Win_nicklist].h = LINES;
		windows[Win_nicklist].w = nicklistwidth;
		x = windows[Win_nicklist].w + 1;
	}
	if (windows[Win_buflist].location == Location_right) {
		windows[Win_buflist].x = COLS - buflistwidth;
		windows[Win_buflist].y = 0;
		windows[Win_buflist].h = LINES;
		windows[Win_buflist].w = buflistwidth;
		rx = buflistwidth + 1;
	}
	if (windows[Win_nicklist].location == Location_right) {
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

	fmt = format(NULL, config_gets("format.ui.separator.horizontal"), NULL);
	for (i = x; i <= COLS - rx; i++) {
		wmove(windows[Win_dummy].window, LINES - 2, i);
		ui_wprintc(&windows[Win_dummy], 1, "%s", fmt);
	}

	if (x) {
		fmt = format(NULL, config_gets("format.ui.separator.vertical"), NULL);
		for (i = 0; i <= LINES; i++) {
			wmove(windows[Win_dummy].window, i, x - 1);
			ui_wprintc(&windows[Win_dummy], 1, "%s", fmt);
		}

		fmt = format(NULL, config_gets("format.ui.separator.split.left"), NULL);
		wmove(windows[Win_dummy].window, LINES - 2, x - 1);
		ui_wprintc(&windows[Win_dummy], 1, "%s", fmt);
	}

	if (rx) {
		fmt = format(NULL, config_gets("format.ui.separator.vertical"), NULL);
		for (i = 0; i <= LINES; i++) {
			wmove(windows[Win_dummy].window, i, COLS - rx);
			ui_wprintc(&windows[Win_dummy], 1, "%s", fmt);
		}

		fmt = format(NULL, config_gets("format.ui.separator.split.right"), NULL);
		wmove(windows[Win_dummy].window, LINES - 2, COLS - rx);
		ui_wprintc(&windows[Win_dummy], 1, "%s", fmt);
	}

	refresh();

	for (i = 0; i < Win_last; i++) {
		if (windows[i].location) {
			ui_placewindow(&windows[i]);
			windows[i].refresh = 1;
		}
	}

	/* Clear format element of history.
	 * Formats need updating if the windows are resized,
	 * or format.* settings are changed. */
	if (selected.history) {
		for (p = selected.history->history; p; p = p->next) {
			if (p->format)
				pfree(&p->format);
			if (p->rformat)
				pfree(&p->rformat);
		}
	}
}

void
ui_draw_input(void) {
	wchar_t *p;
	int offset;
	int x;

	werase(windows[Win_input].window);

	/* Round input.counter down to the nearest windows[Win_input].w.
	 * This gives "pages" that are each as long as the width of the input window */
	offset = ((int) input.counter / windows[Win_input].w) * windows[Win_input].w;
	for (x=0, p = input.string + offset; p && *p && x < windows[Win_input].w; p++, x++) {
		if (iscntrl(*p)) {
			/* adding 64 will turn ^C into C */
			wattron(windows[Win_input].window, A_REVERSE);
			waddch(windows[Win_input].window, *p + 64);
			wattroff(windows[Win_input].window, A_REVERSE);
		} else {
			waddnwstr(windows[Win_input].window, p, 1);
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
		ui_wprintc(&windows[Win_nicklist], 1, "%s\n", format(NULL, config_gets("format.ui.nicklist.more"), NULL));
		y++;
		p = p->next;
		windows[Win_nicklist].scroll = i;
	}

	for (; p && y < windows[Win_nicklist].h - (p->next ? 1 : 0); p = p->next, y++) {
		ui_wprintc(&windows[Win_nicklist], 1, "%c%02d%c%s\n",
				3 /* ^C */, nick_getcolour(p), p->priv, p->nick);
	}

	if (p)
		ui_wprintc(&windows[Win_nicklist], 1, "%s\n", format(NULL, config_gets("format.ui.nicklist.more"), NULL));
}

int
ui_buflist_count(int *ret_servers, int *ret_channels, int *ret_queries) {
	struct Server *sp;
	struct Channel *chp;
	int sc, cc, pc;

	for (sc = cc = pc = 0, sp = servers; sp; sp = sp->next, sc++) {
		for (chp = sp->channels; chp; chp = chp->next, cc++)
			;
		for (chp = sp->queries; chp; chp = chp->next, pc++)
			;
	}

	if (ret_servers)
		*ret_servers = sc;
	if (ret_channels)
		*ret_channels = cc;
	if (ret_queries)
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
		for (chp = sp->queries; chp; chp = chp->next, i++) {
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

void
ui_draw_buflist(void) {
	struct Server *sp;
	struct Channel *chp, *prp;
	int i = 1, scroll;
	char *actind[Activity_last];
	char *oldind, *indicator;

	oldind = estrdup(format(NULL, config_gets("format.ui.buflist.old"), NULL));
	actind[Activity_none] = estrdup(format(NULL, config_gets("format.ui.buflist.activity.none"), NULL));
	actind[Activity_status] = estrdup(format(NULL, config_gets("format.ui.buflist.activity.status"), NULL));
	actind[Activity_error] = estrdup(format(NULL, config_gets("format.ui.buflist.activity.error"), NULL));
	actind[Activity_message] = estrdup(format(NULL, config_gets("format.ui.buflist.activity.message"), NULL));
	actind[Activity_hilight] = estrdup(format(NULL, config_gets("format.ui.buflist.activity.hilight"), NULL));

	werase(windows[Win_buflist].window);

	if (windows[Win_buflist].scroll < 0)
		scroll = 0;
	else
		scroll = windows[Win_buflist].scroll;

	if (!windows[Win_buflist].location)
		return;

	if (scroll > 0) {
		ui_wprintc(&windows[Win_buflist], 1, "%s\n", format(NULL, config_gets("format.ui.buflist.more"), NULL));
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
			indicator = (sp->status == ConnStatus_notconnected) ? oldind : actind[sp->history->activity];
			ui_wprintc(&windows[Win_buflist], 1, "%02d: %s─ %s%s\n", i, sp->next ? "├" : "└", indicator, sp->name);
			wattrset(windows[Win_buflist].window, A_NORMAL);
		}
		i++;

		for (chp = sp->channels; chp && (i - scroll - 1) < windows[Win_buflist].h; chp = chp->next) {
			if (scroll < i - 1) {
				if (selected.channel == chp)
					wattron(windows[Win_buflist].window, A_BOLD);
				indicator = (chp->old) ? oldind : actind[chp->history->activity];
				ui_wprintc(&windows[Win_buflist], 1, "%02d: %s  %s─ %s%s\n", i,
						sp->next ? "│" : " ", chp->next || sp->queries ? "├" : "└", indicator, chp->name);
				wattrset(windows[Win_buflist].window, A_NORMAL);
			}
			i++;
		}

		for (prp = sp->queries; prp && (i - scroll - 1) < windows[Win_buflist].h; prp = prp->next) {
			if (scroll < i - 1) {
				if (selected.channel == prp)
					wattron(windows[Win_buflist].window, A_BOLD);
				indicator = (prp->old) ? oldind : actind[prp->history->activity];
				ui_wprintc(&windows[Win_buflist], 1, "%02d: %s  %s─ %s%s\n", i,
						sp->next ? "│" : " ", prp->next ? "├" : "└", indicator, prp->name);
				wattrset(windows[Win_buflist].window, A_NORMAL);
			}
			i++;
		}
	}

	if (i <= ui_buflist_count(NULL, NULL, NULL)) {
		wmove(windows[Win_buflist].window, windows[Win_buflist].h - 1, 0);
		ui_wprintc(&windows[Win_buflist], 1, "%s\n", format(NULL, config_gets("format.ui.buflist.more"), NULL));
		wclrtoeol(windows[Win_buflist].window);
	}
}

int
ui_wprintc(struct Window *window, int lines, char *fmt, ...) {
	char *str;
	wchar_t *wcs, *s;
	va_list ap;
	int ret;
	attr_t curattr;
	short temp; /* used only for wattr_get,
		     because ncurses is dumb */
	int cc, lc, elc;
	char colourbuf[2][3];
	int colours[2];
	int bold = 0;
	int underline = 0;
	int reverse = 0;
	int italic = 0;

	va_start(ap, fmt);
	ret = vsnprintf(str, 0, fmt, ap) + 1;
	va_end(ap);
	str = emalloc(ret);

	va_start(ap, fmt);
	ret = vsnprintf(str, ret, fmt, ap);
	va_end(ap);
	if (ret < 0)
		return ret;

	if (lines < 0)
		ui_strlenc(window, str, &elc);
	elc -= 1;

	wcs = stowc(str);
	pfree(&str);

	for (ret = cc = lc = 0, s = wcs; s && *s; s++) {
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
				waddnwstr(window->window, s, 1);
				ret++;
				cc++;
			}
			if (cc == window->w || *s == L'\n') {
				lc++;
				cc = 0;
			}
			break;
		}
	}

end:
	pfree(&wcs);
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
			/* Naive utf-8 handling: the 2-nth byte always follows
			 * 10xxxxxxx, so don't count it.
			 *
			 * I figured it would be better to do it this way than
			 * to use widechars, as then there would need to be a
			 * conversion for each call. */
			if ((*s & 0xC0) != 0x80)
				cc++;
			ret++;
			if ((window && cc == window->w) || *s == '\n') {
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
ui_draw_main(void) {
	struct History *p, *hp;
	int y, lines;
	int i;

	werase(windows[Win_main].window);

	for (i=0, p = selected.history->history, hp = NULL; p; p = p->next) {
		if (!(p->options & HIST_SHOW) || ((p->options & HIST_IGN) && !selected.showign))
			continue;
		if (windows[Win_main].scroll > 0 && !hp && !p->next)
			hp = p;
		else if (i == windows[Win_main].scroll && !hp)
			hp = p;

		if (i < windows[Win_main].scroll)
			i++;
		if (!p->format)
			p->format = estrdup(format(&windows[Win_main], NULL, p));
	}

	if (windows[Win_main].scroll > 0)
		windows[Win_main].scroll = i;
	if (!hp)
		hp = selected.history->history;

	for (y = windows[Win_main].h; hp && y > 0; hp = hp->next) {
		if (!(hp->options & HIST_SHOW) || !hp->format || ((hp->options & HIST_IGN) && !selected.showign))
			continue;
		if (ui_strlenc(&windows[Win_main], hp->format, &lines) <= 0)
			continue;
		y = y - lines;
		if (y < lines) {
			y *= -1;
			wmove(windows[Win_main].window, 0, 0);
			ui_wprintc(&windows[Win_main], y, "%s\n", hp->format);
			break;
		}
		wmove(windows[Win_main].window, y, 0);
		ui_wprintc(&windows[Win_main], 0, "%s\n", hp->format);
	}

	if (selected.channel && selected.channel->topic) {
		wmove(windows[Win_main].window, 0, 0);
		ui_wprintc(&windows[Win_main], 0, "%s\n", format(&windows[Win_main], config_gets("format.ui.topic"), NULL));
	}
}

void
ui_select(struct Server *server, struct Channel *channel) {
	struct History *hp, *ind;
	int i, total;

	if (selected.history)
		hist_purgeopt(selected.history, HIST_TMP);

	selected.channel  = channel;
	selected.server   = server;
	selected.history  = channel ? channel->history : server ? server->history : main_buf;
	selected.name     = channel ? channel->name    : server ? server->name    : "hirc";
	selected.hasnicks = channel ? !channel->query && !channel->old : 0;
	selected.showign  = 0;

	if (selected.history->unread || selected.history->ignored) {
		for (i = 0, hp = selected.history->history; hp && hp->next; hp = hp->next, i++);
		if (i == (HIST_MAX-1)) {
			pfree(&hp->next);
			hp->next = NULL;
		}

		total = selected.history->unread + selected.history->ignored;

		for (i = 0, hp = selected.history->history; hp && hp->next && i < total; hp = hp->next)
			if (hp->options & HIST_SHOW)
				i++;
		if (hp) {
			ind = hist_format(NULL, Activity_none, HIST_SHOW|HIST_TMP, "SELF_UNREAD %d %d :unread, ignored",
					selected.history->unread, selected.history->ignored);
			ind->origin = selected.history;
			ind->next = hp;
			ind->prev = hp->prev;
			if (hp->prev)
				hp->prev->next = ind;
			hp->prev = ind;
		}
	}


	selected.history->activity = Activity_none;
	selected.history->unread = selected.history->ignored = 0;

	if (!selected.hasnicks || config_getl("nicklist.hidden"))
		windows[Win_nicklist].location = Location_hidden;
	else
		windows[Win_nicklist].location = config_getl("nicklist.location");
	windows[Win_main].scroll = -1;
	ui_redraw();
}

char *
ui_rectrl(char *str) {
	static char ret[8192];
	static char *rp = NULL;
	int caret, rc;
	char c;

	if (rp) {
		pfree(&rp);
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
		pfree(&rp);
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
	char *tmp, *b;

	assert_warn(binding && cmd, -1);
	b = ui_rectrl(binding);

	for (p = keybinds; p; p = p->next)
		if (strcmp(p->binding, b) == 0)
			return -1;

	p = emalloc(sizeof(struct Keybind));
	p->binding = estrdup(b);
	p->wbinding = stowc(p->binding);
	if (*cmd != '/') {
		tmp = smprintf(strlen(cmd) + 2, "/%s", cmd);
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
	char *b;

	assert_warn(binding, -1);
	b = ui_rectrl(binding);

	for (p=keybinds; p; p = p->next) {
		if (strcmp(p->binding, b) == 0) {
			if (p->prev)
				p->prev->next = p->next;
			else
				keybinds = p->next;

			if (p->next)
				p->next->prev = p->prev;

			pfree(&p->binding);
			pfree(&p->wbinding);
			pfree(&p->cmd);
			pfree(&p);
			return 0;
		}
	}

	return -1;
}
