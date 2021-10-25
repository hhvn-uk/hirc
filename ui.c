#include <errno.h>
#include <ctype.h>
#include <stdarg.h>
#include <string.h>
#include <stdlib.h>
#include <ncurses.h>
#ifdef TLS
#include <tls.h>
#endif /* TLS */
#include "hirc.h"

struct HistInfo *error_buf;
struct Window windows[Win_last] = {
	[Win_main]	= {.handler = NULL},
	[Win_input]	= {.handler = ui_draw_input},
	[Win_nicklist]	= {.handler = ui_draw_nicklist},
	[Win_winlist]	= {.handler = NULL},
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

	hist_format(NULL, error_buf, Activity_error, HIST_SHOW,
			"SELF_ERROR %s %d :%s", 
			file, line, msg);
}

void
ui_perror_(char *file, int line, char *str) {
	hist_format(NULL, error_buf, Activity_error, HIST_SHOW,
			"SELF_ERROR %s %d :%s: %s",
			file, line, str, strerror(errno));
}

#ifdef TLS
void
ui_tls_config_error_(char *file, int line, struct tls_config *config, char *str) {
	hist_format(NULL, error_buf, Activity_error, HIST_SHOW,
			"SELF_ERROR %s %d :%s: %s",
			file, line, str, tls_config_error(config));
}

void
ui_tls_error_(char *file, int line, struct tls *ctx, char *str) {
	hist_format(NULL, error_buf, Activity_error, HIST_SHOW,
			"SELF_ERROR %s %d :%s: %s",
			file, line, str, tls_error(ctx));
}
#endif /* TLS */

void
ui_init(void) {
	initscr();
	raw();
	noecho();
	nodelay(stdscr, TRUE);
	keypad(stdscr, TRUE);

	memset(input.string, '\0', sizeof(input.string));
	input.counter = 0;

	error_buf = emalloc(sizeof(struct HistInfo));
	error_buf->activity = Activity_ignore;
	error_buf->unread = 0;
	error_buf->server = NULL;
	error_buf->channel = NULL;
	error_buf->history = NULL;

	if (nicklistlocation != 0 && nicklistlocation == winlistlocation) {
		ui_error("nicklist and winlist can't be set to same location in config.h", NULL);
		windows[Win_winlist].location = LEFT;
		windows[Win_nicklist].location = RIGHT;
	} else {
		windows[Win_winlist].location = winlistlocation;
		windows[Win_nicklist].location = nicklistlocation;
	}

	windows[Win_main].window = newwin(0, 0, 0, 0);
	windows[Win_input].window = newwin(0, 0, 0, 0);
	windows[Win_main].location = -1;
	windows[Win_input].location = -1;
	if (windows[Win_nicklist].location)
		windows[Win_nicklist].window = newwin(0, 0, 0, 0);
	if (windows[Win_winlist].location)
		windows[Win_winlist].window = newwin(0, 0, 0, 0);

	ui_redraw();

	wprintw(windows[Win_winlist].window, "winlist");
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
	int key;

	switch (key = wgetch(stdscr)) {
	case ERR:
		/* this happens due to nodelay */
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

	windows[Win_input].redraw = 1;
}

void
ui_redraw(void) {
	int x = 0, rx = 0;

	if (windows[Win_winlist].location == LEFT) {
		windows[Win_winlist].x = windows[Win_winlist].y = 0;
		windows[Win_winlist].h = LINES;
		windows[Win_winlist].w = winlistwidth;
		x = windows[Win_winlist].w + 1;
	}
	if (windows[Win_nicklist].location == LEFT) {
		windows[Win_nicklist].x = windows[Win_winlist].y = 0;
		windows[Win_nicklist].h = LINES;
		windows[Win_nicklist].w = nicklistwidth;
		x = windows[Win_nicklist].w + 1;
	}
	if (windows[Win_winlist].location == RIGHT) {
		windows[Win_winlist].x = COLS - winlistwidth;
		windows[Win_winlist].y = 0;
		windows[Win_winlist].h = LINES;
		windows[Win_winlist].w = winlistwidth;
		rx = winlistwidth + 1;
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
	ui_placewindow(&windows[Win_winlist]);
	ui_placewindow(&windows[Win_main]);
	ui_placewindow(&windows[Win_input]);

	if (x)
		mvvline(0, x - 1, '|', LINES);
	if (rx)
		mvvline(0, COLS - rx, '|', LINES);

	mvhline(LINES - 2, x, '-', COLS - x - rx);

	windows[Win_nicklist].redraw = 1;
	windows[Win_input].redraw = 1;
}

void
ui_draw_input(void) {
	char *p;
	char tmp;
	int offset;

	wclear(windows[Win_input].window);
	/* Round input.counter down to the nearest windows[Win_input].w.
	 * This gives "pages" that are each as long as the width of the input window */
	offset = ((int) input.counter / windows[Win_input].w) * windows[Win_input].w;
	for (p = input.string + offset; p && *p; p++) {
		if (iscntrl(*p)) {
			/* adding 64 will turn ^C into C */
			tmp = *p + 64;
			wattron(windows[Win_input].window, A_REVERSE);
			waddnstr(windows[Win_input].window, &tmp, 1);
			wattroff(windows[Win_input].window, A_REVERSE);
		} else waddnstr(windows[Win_input].window, p, 1);
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

	/* TODO: sort nicks here
	 * TODO: more nicks than screen height? */
	for (p = selected.channel->nicks; p; p = p->next) {
		/* TODO: colourize nicks */
		wprintw(windows[Win_nicklist].window, "%c%s\n", p->priv, p->nick);
	}
}

void
ui_select(struct Server *server, struct Channel *channel) {
	selected.channel = channel;
	selected.server  = server;
	selected.history = channel ? channel->history : server->history;
	selected.name    = channel ? channel->name    : server->name;
}
