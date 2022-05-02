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

static int config_window_hide(struct Config *conf, long num);
static int config_window_location(struct Config *conf, long num);
static int config_window_width(struct Config *conf, long num);
static int config_nickcolour_self(struct Config *conf, long num);
static int config_nickcolour_range(struct Config *conf, long a, long b);
static int config_redrawl(struct Config *conf, long num);
static int config_redraws(struct Config *conf, char *str);

static char *strbool[] = { "true", "false", NULL };
static char *strlocation[] = {
	[Location_left] = "left",
	[Location_right] = "right",
	NULL
};

static struct {
	char *name;
	long min, max;
} vals[] = {
	[Val_string]		= {"a string"},
	[Val_bool]		= {"boolean (true/false)",		0, 1},
	[Val_colour]		= {"a number from 0 to 99",		0, 99},
	[Val_signed]		= {"a numeric value",			LONG_MIN, LONG_MAX},
	[Val_unsigned]		= {"positive",				0, LONG_MAX},
	[Val_nzunsigned]	= {"greater than zero",			1, LONG_MAX},
	[Val_pair]		= {"a pair",				LONG_MIN, LONG_MAX},
	[Val_colourpair]	= {"pair with numbers from 0 to 99",	0, 99},
	[Val_location]		= {"a location (left/right)",	Location_left, Location_right},
};

#include "data/config.h"

struct Config *
config_getp(char *name) {
	int i;

	if (!name)
		return NULL;

	for (i = 0; config[i].name; i++)
		if (strcmp(config[i].name, name) == 0)
			return &config[i];
	return NULL;
}

char *
config_get_pretty(struct Config *conf, int pairbrace) {
	static char ret[8192];

	if (conf) {
		if (conf->valtype == Val_string)
			return conf->str;
		else if (conf->valtype == Val_location)
			return strlocation[conf->num];
		else if (conf->valtype == Val_bool)
			return strbool[conf->num];
		else if (conf->valtype == Val_pair || conf->valtype == Val_colourpair)
			snprintf(ret, sizeof(ret), pairbrace ? "{%ld, %ld}" : "%ld %ld", conf->pair[0], conf->pair[1]);
		else
			snprintf(ret, sizeof(ret), "%ld", conf->num);
		return ret;
	} else return NULL;
}

long
config_getl(char *name) {
	struct Config *conf = config_getp(name);
	if (conf && (conf->valtype == Val_bool ||
			conf->valtype == Val_colour ||
			conf->valtype == Val_signed ||
			conf->valtype == Val_unsigned ||
			conf->valtype == Val_nzunsigned ||
			conf->valtype == Val_location))
		return conf->num;

	return 0;
}

char *
config_gets(char *name) {
	struct Config *conf = config_getp(name);
	if (conf && conf->valtype == Val_string)
		return conf->str;

	return NULL;
}

void
config_getr(char *name, long *a, long *b) {
	struct Config *conf = config_getp(name);
	if (conf && (conf->valtype == Val_pair || conf->valtype == Val_colourpair)) {
		if (a) *a = conf->pair[0];
		if (b) *b = conf->pair[1];
	}

	/* return an int for success? */
}

void
config_setl(struct Config *conf, long num) {
	if (!conf)
		return;
	if (num >= vals[conf->valtype].min && num <= vals[conf->valtype].max && (
			conf->valtype == Val_bool ||
			conf->valtype == Val_colour ||
			conf->valtype == Val_signed ||
			conf->valtype == Val_unsigned ||
			conf->valtype == Val_nzunsigned ||
			conf->valtype == Val_location)) {
		if (conf->numhandle)
			if (!conf->numhandle(conf, num))
				return;
		conf->isdef = 0;
		conf->num = num;
	} else {
		ui_error("%s must be %s", conf->name, vals[conf->valtype].name);
	}
}

void
config_sets(struct Config *conf, char *str) {
	if (!conf)
		return;
	if (conf->valtype != Val_string) {
		ui_error("%s must be %s", conf->name, vals[conf->valtype].name);
		return;
	}
	if (conf->strhandle)
		if (!conf->strhandle(conf, str))
			return;
	if (!conf->isdef)
		pfree(&conf->str);
	else
		conf->isdef = 0;
	conf->str = estrdup(str);
}

void
config_setr(struct Config *conf, long a, long b) {
	if (!conf)
		return;
	if (a >= vals[conf->valtype].min && b <= vals[conf->valtype].max &&
			(conf->valtype == Val_pair || conf->valtype == Val_colourpair)) {
		if (conf->pairhandle)
			if (!conf->pairhandle(conf, a, b))
				return;
		conf->isdef = 0;
		conf->pair[0] = a;
		conf->pair[1] = b;
	} else {
		ui_error("%s must be %s", conf->name, vals[conf->valtype].name);
	}
}

void
config_set(char *name, char *val) {
	char *str = val ? estrdup(val) : NULL;
	char *tok[3], *save;
	struct Config *conf;
	int i, found;

	tok[0] = strtok_r(val,  " ", &save);
	tok[1] = strtok_r(NULL, " ", &save);
	tok[2] = strtok_r(NULL, " ", &save);

	if (!(conf = config_getp(name))) {
		if (tok[0]) {
			ui_error("no such configuration variable", NULL);
			goto end;
		}
	}

	if (strisnum(tok[0], 1) && strisnum(tok[1], 1) && !tok[2]) {
		config_setr(conf, strtol(tok[0], NULL, 10), strtol(tok[1], NULL, 10));
	} else if (strisnum(tok[0], 1) && !tok[1]) {
		config_setl(conf, strtol(tok[0], NULL, 10));
	} else if (tok[0] && !tok[1] && conf->valtype == Val_bool) {
		if (strcmp(tok[0], "true") == 0)
			config_setl(conf, 1);
		else if (strcmp(tok[0], "false") == 0)
			config_setl(conf, 0);
		else
			goto inval;
	} else if (tok[0] && !tok[1] && conf->valtype == Val_location) {
		if (strcmp(tok[0], "left") == 0)
			config_setl(conf, Location_left);
		else if (strcmp(tok[0], "right") == 0)
			config_setl(conf, Location_right);
		else
			goto inval;
	} else if (tok[0]) {
		config_sets(conf, str);
	} else {
		for (i = found = 0; config[i].name; i++) {
			if (strncmp(config[i].name, name, strlen(name)) == 0) {
				hist_format(selected.history, Activity_status, HIST_UI, "SELF_UI :%s: %s",
						config[i].name, config_get_pretty(&config[i], 1));
				found = 1;
			}
		}

		if (!found)
			ui_error("no such configuration variable", NULL);
	}

end:
	pfree(&str);
	return;

inval:
	ui_error("%s must be %s", name, vals[conf->valtype].name);
	goto end;
}

void
config_read(char *filename) {
	static char **bt = NULL;
	static int btoffset = 0;
	char buf[8192];
	char *path;
	FILE *file;
	int save, i;

	if (!filename)
		return;

	path = realpath(filename, NULL);

	/* Check if file is already being read */
	if (bt && btoffset) {
		for (i = 0; i < btoffset; i++) {
			if (strcmp_n(path, *(bt + i)) == 0) {
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
	while (fgets(buf, sizeof(buf), file)) {
		buf[strlen(buf) - 1] = '\0'; /* remove \n */
		if (*buf == '/')
			command_eval(NULL, buf);
	}
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
config_window_hide(struct Config *conf, long num) {
	enum Windows win;
	enum WindowLocation loc = Location_hidden;
	if (strcmp(conf->name, "buflist.hidden") == 0) {
		win = Win_buflist;
		if (!num)
			loc = config_getl("buflist.location");
	} else if (strcmp(conf->name, "nicklist.hidden") == 0) {
		win = Win_nicklist;
		if (!num)
			loc = config_getl("nicklist.location");
	}
	windows[win].location = loc;
	conf->isdef = 0;
	ui_redraw();
	return 1;
}

static int
config_window_location(struct Config *conf, long num) {
	struct Config *otherloc, *otherhide;
	enum WindowLocation win, otherwin;
	if (strcmp(conf->name, "buflist.location") == 0) {
		win = Win_buflist;
		otherwin = Win_nicklist;
		otherloc = config_getp("nicklist.location");
		otherhide = config_getp("nicklist.hidden");
	} else if (strcmp(conf->name, "nicklist.location") == 0) {
		win = Win_nicklist;
		otherwin = Win_buflist;
		otherloc = config_getp("buflist.location");
		otherhide = config_getp("buflist.hidden");
	}
	if (num == otherloc->num) {
		otherloc->num = (num == Location_left) ? Location_right : Location_left;
		otherloc->isdef = 0;
		if (!otherhide->num)
			windows[otherwin].location = otherloc->num;
	}
	conf->num = windows[win].location = num;
	conf->isdef = 0;
	ui_redraw();
	return 0;
}

static int
config_window_width(struct Config *conf, long num) {
	enum WindowLocation win;
	if (strcmp(conf->name, "buflist.width") == 0)
		win = Win_buflist;
	else if (strcmp(conf->name, "nicklist.width") == 0)
		win = Win_nicklist;
	if (num <= COLS - (windows[win].location ? windows[win].w : 0) - 2) {
		uineedredraw = 1;
		return 1;
	} else {
		ui_error("window will be too big", NULL);
		return 0;
	}
}

static int
config_nickcolour_self(struct Config *conf, long num) {
	windows[Win_nicklist].refresh = 1;
	return 1;
}

static int
config_nickcolour_range(struct Config *conf, long a, long b) {
	windows[Win_nicklist].refresh = 1;
	return 1;
}

static int
config_redraws(struct Config *conf, char *str) {
	ui_redraw();
	return 1;
}

static int
config_redrawl(struct Config *conf, long num) {
	ui_redraw();
	return 1;
}
