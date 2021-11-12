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

	hist_format(main_buf, Activity_error, HIST_SHOW,
			"SELF_ERROR %s %d :%s", 
			file, line, msg);
}

void
ui_perror_(char *file, int line, char *str) {
	hist_format(main_buf, Activity_error, HIST_SHOW,
			"SELF_ERROR %s %d :%s: %s",
			file, line, str, strerror(errno));
}

#ifdef TLS
void
ui_tls_config_error_(char *file, int line, struct tls_config *config, char *str) {
	hist_format(main_buf, Activity_error, HIST_SHOW,
			"SELF_ERROR %s %d :%s: %s",
			file, line, str, tls_config_error(config));
}

void
ui_tls_error_(char *file, int line, struct tls *ctx, char *str) {
	hist_format(main_buf, Activity_error, HIST_SHOW,
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
	static int needrefresh;
	int key;

	switch (key = wgetch(windows[Win_input].window)) {
	case ERR: /* no input received */
		if (needrefresh) {
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
			windows[Win_input].refresh = 1;
			needrefresh = 0;
		}
		return;
	case KEY_RESIZE:
		ui_redraw();
		return;
	case KEY_BACKSPACE:
		if (input.counter) {
			if (ui_input_delete(1, input.counter) > 0)
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
		if ((key & 0xFF80) == 0x80 || isprint(key) || iscntrl(key)) {
			if (ui_input_insert(key, input.counter) > 0)
				input.counter++;
		}
		break;
	}

	needrefresh = 1;
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
			if (italic)
				wattroff(window->window, A_ITALIC);
			else
				wattron(window->window, A_ITALIC);
			italic = italic ? 0 : 1;
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
			if (cc == window->w) {
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

void
ui_draw_main(void) {
	struct History *p;
	int y, lines;

	ui_wclear(&windows[Win_main]);

	y = windows[Win_main].h;
	for (p = selected.history->history; p && y > 0; p = p->next) {
		ui_strlenc(&windows[Win_main], p->raw, &lines);
		y = y - lines;
		if (y < lines) {
			y *= -1;
			wmove(windows[Win_main].window, 0, 0);
			ui_wprintc(&windows[Win_main], y, "%s\n", p->raw);
		}
		wmove(windows[Win_main].window, y, 0);
		ui_wprintc(&windows[Win_main], 0, "%s\n", p->raw);
	}
}

void
ui_select(struct Server *server, struct Channel *channel) {
	selected.channel = channel;
	selected.server  = server;
	selected.history = channel ? channel->history : server ? server->history : main_buf;
	selected.name    = channel ? channel->name    : server ? server->name    : "hirc";
}
