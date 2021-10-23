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
struct Window mainwindow;
struct Window inputwindow;
struct Window nicklist;
struct Window winlist;

struct Channel *selected_channel = NULL;
struct Server *selected_server = NULL;

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
		winlist.location = LEFT;
		nicklist.location = RIGHT;
	} else {
		winlist.location = winlistlocation;
		nicklist.location = nicklistlocation;
	}

	mainwindow.window = newwin(0, 0, 0, 0);
	inputwindow.window = newwin(0, 0, 0, 0);
	mainwindow.location = -1;
	inputwindow.location = -1;
	if (nicklist.location)
		nicklist.window = newwin(0, 0, 0, 0);
	if (winlist.location)
		winlist.window = newwin(0, 0, 0, 0);

	ui_redraw();

	wprintw(nicklist.window, "nicklist");
	wprintw(winlist.window, "winlist");
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
	case KEY_RESIZE: 
		ui_redraw(); 
		break;
	case KEY_BACKSPACE:
		if (input.counter) {
			memmove(&input.string[input.counter - 1], 
					&input.string[input.counter], 
					strlen(&input.string[input.counter]) + 1);
			input.counter--;
			ui_draw_input();
		}
		break;
	case KEY_LEFT:
		if (input.counter) {
			input.counter--;
			ui_draw_input();
		}
		break;
	case KEY_RIGHT:
		if (input.string[input.counter]) {
			input.counter++;
			ui_draw_input();
		}
		break;
	case '\n':
		if (strcmp(input.string, "/quit") == 0) {
			endwin();
			exit(0);
		}
		wprintw(mainwindow.window, "%s\n", input.string);
		memset(input.string, '\0', sizeof(input.string));
		input.counter = 0;
		ui_draw_input();
		break;
	default:
		if (isprint(key) || iscntrl(key)) {
			memmove(&input.string[input.counter + 1],
					&input.string[input.counter],
					strlen(&input.string[input.counter]));
			input.string[input.counter++] = key;
			ui_draw_input();
		}
	}
}

void
ui_redraw(void) {
	int x = 0, rx = 0;

	if (winlist.location == LEFT) {
		winlist.x = winlist.y = 0;
		winlist.h = LINES;
		winlist.w = winlistwidth;
		x = winlist.w + 1;
	}
	if (nicklist.location == LEFT) {
		nicklist.x = winlist.y = 0;
		nicklist.h = LINES;
		nicklist.w = nicklistwidth;
		x = nicklist.w + 1;
	}
	if (winlist.location == RIGHT) {
		winlist.x = COLS - winlistwidth;
		winlist.y = 0;
		winlist.h = LINES;
		winlist.w = winlistwidth;
		rx = winlistwidth + 1;
	}
	if (nicklist.location == RIGHT) {
		nicklist.x = COLS - nicklistwidth;
		nicklist.y = 0;
		nicklist.h = LINES;
		nicklist.w = nicklistwidth;
		rx = nicklistwidth + 1;
	}

	mainwindow.x = x;
	mainwindow.y = 0;
	mainwindow.h = LINES - 2;
	mainwindow.w = COLS - x - rx;

	inputwindow.x = x;
	inputwindow.y = LINES - 1;
	inputwindow.h = 1;
	inputwindow.w = COLS - x - rx;

	ui_placewindow(&nicklist);
	ui_placewindow(&winlist);
	ui_placewindow(&mainwindow);
	ui_placewindow(&inputwindow);

	if (x)
		mvvline(0, x - 1, '|', LINES);
	if (rx)
		mvvline(0, COLS - rx, '|', LINES);

	mvhline(LINES - 2, x, '-', COLS - x - rx);

	ui_draw_input();
}

void
ui_draw_input(void) {
	char *p;
	char tmp;
	int offset;

	wclear(inputwindow.window);
	/* Round input.counter down to the nearest inputwindow.w.
	 * This gives "pages" that are each as long as the width of the input window */
	offset = ((int) input.counter / inputwindow.w) * inputwindow.w;
	for (p = input.string + offset; p && *p; p++) {
		if (iscntrl(*p)) {
			/* adding 64 will turn ^C into C */
			tmp = *p + 64;
			wattron(inputwindow.window, A_REVERSE);
			waddnstr(inputwindow.window, &tmp, 1);
			wattroff(inputwindow.window, A_REVERSE);
		} else waddnstr(inputwindow.window, p, 1);
	}
	wmove(inputwindow.window, 0, input.counter - offset);
}
