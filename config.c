/* See LICENSE for copyright details */

#include <ncurses.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include "hirc.h"

static int config_nicklist_location(long num);
static int config_nicklist_width(long num);
static int config_buflist_location(long num);
static int config_buflist_width(long num);
static int config_nickcolour_self(long num);
static int config_nickcolour_range(long a, long b);

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
	{"format.ui.topic", 1, Val_string,
		.str = "%{c:99,89}${topic}",
		.strhandle = NULL,
		.description = {
		"Format of topic at top of main window", NULL}},
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

	while (read_line(fileno(file), buf, sizeof(buf)))
		if (*buf == '/')
			command_eval(buf);
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
