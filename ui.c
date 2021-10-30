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

struct Window windows[Win_last] = {
	[Win_main]	= {.handler = NULL},
	[Win_input]	= {.handler = ui_draw_input},
	[Win_nicklist]	= {.handler = ui_draw_nicklist},
	[Win_buflist]	= {.handler = ui_draw_buflist},
};
struct Selected selected;

struct {
	char string[INPUT_MAX];
	unsigned counter;
} input;

void
ui_error_(char *file, int line, char *format, ...) {
	char msg[1024];
	va_list ap;

	va_start(ap, format);
	vsnprintf(msg, sizeof(msg), format, ap);
	va_end(ap);

	hist_format(NULL, main_buf, Activity_error, HIST_SHOW,
			"SELF_ERROR %s %d :%s", 
			file, line, msg);
}

void
ui_perror_(char *file, int line, char *str) {
	hist_format(NULL, main_buf, Activity_error, HIST_SHOW,
			"SELF_ERROR %s %d :%s: %s",
			file, line, str, strerror(errno));
}

#ifdef TLS
void
ui_tls_config_error_(char *file, int line, struct tls_config *config, char *str) {
	hist_format(NULL, main_buf, Activity_error, HIST_SHOW,
			"SELF_ERROR %s %d :%s: %s",
			file, line, str, tls_config_error(config));
}

void
ui_tls_error_(char *file, int line, struct tls *ctx, char *str) {
	hist_format(NULL, main_buf, Activity_error, HIST_SHOW,
			"SELF_ERROR %s %d :%s: %s",
			file, line, str, tls_error(ctx));
}
#endif /* TLS */

void
ui_init(void) {
	setlocale(LC_ALL, "");
	initscr();
	start_color();
	use_default_colors();
	raw();
	noecho();

	memset(input.string, '\0', sizeof(input.string));
	input.counter = 0;

	if (nicklistlocation != 0 && nicklistlocation == buflistlocation) {
		ui_error("nicklist and buflist can't be set to same location in config.h", NULL);
		windows[Win_buflist].location = LEFT;
		windows[Win_nicklist].location = RIGHT;
	} else {
		windows[Win_buflist].location = buflistlocation;
		windows[Win_nicklist].location = nicklistlocation;
	}

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
	static int needredraw;
	int key;

	switch (key = wgetch(windows[Win_input].window)) {
	case ERR: /* no input received */
		if (needredraw) {
			/* Only redraw the input window if there
			 * hasn't been any input received - this
			 * is to avoid forcing a redraw for each
			 * keystroke if they arrive in very fast
			 * succession, i.e. text that is pasted.
			 * KEY_RESIZE will still force a redraw.
			 *
			 * Theoretically this could be done with
			 * bracketed paste stuff, but a solution
			 * that works with all terminals is nice */
			windows[Win_input].redraw = 1;
			needredraw = 0;
		}
		return;
	case KEY_RESIZE: 
		ui_redraw(); 
		return;
	case KEY_BACKSPACE:
		if (input.counter) {
			memmove(&input.string[input.counter - 1], 
					&input.string[input.counter], 
					strlen(&input.string[input.counter]) + 1);
			input.counter--;
		}
		break;
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
		memset(input.string, '\0', sizeof(input.string));
		input.counter = 0;
		break;
	default:
		if (isprint(key) || iscntrl(key)) {
			memmove(&input.string[input.counter + 1],
					&input.string[input.counter],
					strlen(&input.string[input.counter]));
			input.string[input.counter++] = key;
		}
		break;
	}

	needredraw = 1;
}

void
ui_redraw(void) {
	int x = 0, rx = 0;

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

	ui_placewindow(&windows[Win_nicklist]);
	ui_placewindow(&windows[Win_buflist]);
	ui_placewindow(&windows[Win_main]);
	ui_placewindow(&windows[Win_input]);

	if (x)
		mvvline(0, x - 1, '|', LINES);
	if (rx)
		mvvline(0, COLS - rx, '|', LINES);

	mvhline(LINES - 2, x, '-', COLS - x - rx);
	refresh();

	windows[Win_nicklist].redraw = 1;
	windows[Win_buflist].redraw = 1;
	windows[Win_input].redraw = 1;
}

void
ui_draw_input(void) {
	char *p;
	int offset;
	int y;

	wmove(windows[Win_input].window, 0, 0);
	for (y = 0; y < windows[Win_input].w; y++)
		waddch(windows[Win_input].window, ' ');
	wmove(windows[Win_input].window, 0, 0);

	/* Round input.counter down to the nearest windows[Win_input].w.
	 * This gives "pages" that are each as long as the width of the input window */
	offset = ((int) input.counter / windows[Win_input].w) * windows[Win_input].w;
	for (y=0, p = input.string + offset; p && *p && y < windows[Win_input].w; p++, y++) {
		if (iscntrl(*p)) {
			/* adding 64 will turn ^C into C */
			wattron(windows[Win_input].window, A_REVERSE);
			waddch(windows[Win_input].window, *p + 64);
			wattroff(windows[Win_input].window, A_REVERSE);
		} else waddch(windows[Win_input].window, *p);
	}
	wmove(windows[Win_input].window, 0, input.counter - offset);
}

void
ui_draw_nicklist(void) {
	struct Nick *p;

	wclear(windows[Win_nicklist].window);
	if (!selected.channel || !windows[Win_nicklist].location)
		return;

	wmove(windows[Win_nicklist].window, 0, 0);

	nick_sort(&selected.channel->nicks, selected.server);
	/* TODO: more nicks than screen height? */
	for (p = selected.channel->nicks; p; p = p->next) {
		/* TODO: colourize nicks */
		wprintw(windows[Win_nicklist].window, "%c%s\n", p->priv, p->nick);
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

	return sc + cc;
}

void
ui_buflist_select(int num) {
	struct Server *sp;
	struct Channel *chp;
	int i;

	if (num == 0) {
		ui_select(NULL, NULL);
		return;
	}

	for (i = 1, sp = servers; sp; sp = sp->next, i++) {
		if (i == num) {
			ui_select(sp, NULL);
			return;
		}

		for (i++, chp = sp->channels; chp; chp = chp->next, i++) {
			if (i == num) {
				ui_select(sp, chp);
				return;
			}
		}
	}
}

void
ui_draw_buflist(void) {
	struct Server *sp;
	struct Channel *chp;
	int i = 0, len, tmp;
	int sc, cc;

	wclear(windows[Win_buflist].window);
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
ui_wprintc(WINDOW *window, char *format, ...) {
	char str[1024], *s;
	va_list ap;
	int ret;
	attr_t curattr;
	int temp; /* used only for wattr_get, 
		     because ncurses is dumb */
	char colourbuf[2][3];
	int colours[2];
	int colour = 0;
	int bold = 0;
	int underline = 0;
	int reverse = 0;
	int italic = 0;

	va_start(ap, format);
	if ((ret = vsnprintf(str, sizeof(str), format, ap)) < 0) {
		va_end(ap);
		return ret;
	}

	for (s = str; s && *s; s++) {
		switch (*s) {
		case 2: /* ^B */
			if (bold)
				wattroff(window, A_BOLD);
			else
				wattron(window, A_BOLD);
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

			wattr_get(window, &curattr, &temp, NULL);
			wattr_set(window, curattr, ui_get_pair(colours[0], colours[1]), NULL);
			colour = 1;
			break;
		case 9: /* ^I */
			if (italic)
				wattroff(window, A_ITALIC);
			else
				wattron(window, A_ITALIC);
			italic = italic ? 0 : 1;
			break;
		case 15: /* ^O */
			colour = 0;
			bold = 0;
			underline =0;
			reverse = 0;
			italic = 0;
			/* Setting A_NORMAL turns everything off, 
			 * without using 5 different attroffs */
			wattrset(window, A_NORMAL);
			break;
		case 18: /* ^R */
			if (reverse)
				wattroff(window, A_REVERSE);
			else
				wattron(window, A_REVERSE);
			reverse = reverse ? 0 : 1;
			break;
		case 21: /* ^U */
			if (underline)
				wattroff(window, A_UNDERLINE);
			else
				wattron(window, A_UNDERLINE);
			underline = underline ? 0 : 1;
			break;
		default:
			waddch(window, *s);
			break;
		}
	}

	colour = 0;
	bold = 0;
	underline =0;
	reverse = 0;
	italic = 0;
	wattrset(window, A_NORMAL);

	va_end(ap);
	return ret;
}

void
ui_select(struct Server *server, struct Channel *channel) {
	selected.channel = channel;
	selected.server  = server;
	selected.history = channel ? channel->history : server ? server->history : main_buf;
	selected.name    = channel ? channel->name    : server ? server->name    : "hirc";
}
