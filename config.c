#include <string.h>
#include <stdlib.h>
#include "hirc.h"

struct Config config[] = {
	{"log.dir", 1, Val_string,
		.str = "~/.local/hirc",
		.strhandle = NULL,
		.description =
		"Directory for hirc to log to."
		"Can contain ~ to refer to $HOME"},
	{"log.toggle", 1, Val_bool,
		.num = 1,
		.numhandle = NULL,
		.description =
		"Simple: to log, or not to log"},
	{"def.nick", 1, Val_string,
		.str = NULL,
		.strhandle = NULL,
		.description =
		"Default nickname"},
	{"def.user", 1, Val_string,
		.str = NULL,
		.strhandle = NULL,
		.description =
		"Default username (nick!..here..@host), "
		"may be replaced by identd response"},
	{"def.real", 1, Val_string,
		.str = NULL,
		.strhandle = NULL,
		.description =
		"Default \"realname\", seen in /whois"},
	{"def.chantypes", 1, Val_string,
		.str = "#&!+",
		.strhandle = NULL,
		.description =
		"You most likely don't want to touch this."
		"If a server does not supply this in RPL_ISUPPORT,"
		"hirc assumes it will use these channel types."},
	{"def.prefixes", 1, Val_string,
		.str = "(ov)@+",
		.strhandle = NULL,
		.description =
		"You most likely don't want to touch this."
		"If a server doesn't supply this in the nonstandard"
		"RPL_ISUPPORT, it likely won't support nonstandard"
		"prefixes."},
	{"reconnect.interval", 1, Val_nzunsigned,
		.num = 10,
		.numhandle = NULL,
		.description =
		"Starting reconnect interval in seconds."
		"In reality, for each attempt this will be multipled"
		"by the number of failed attemps until it reaches"
		"reconnect.maxinterval"},
	{"reconnect.maxinterval", 1, Val_nzunsigned,
		.num = 600,
		.numhandle = NULL,
		.description =
		"Maximum reconnect interval in seconds."
		"See reconnect.interval"},
	{"nickcolour.self", 1, Val_nzunsigned,
		.num = 90,
		.numhandle = config_colour_self,
		.description =
		"Colour to use for onself."
		"Must be 0, 99 or anywhere between. 99 is no colours."},
	{"nickcolour.range", 1, Val_range,
		.range = {28, 63},
		.rangehandle = config_colour_range,
		.description =
		"Range of (mirc extended) colours used to colour nicks"
		"Must be 0, 99 or anywhere between. 99 is no colour"
		"Giving a single value or two identical values will"
		"use that colour only"},
	{"nicklist.location", 1, Val_unsigned,
		.num = RIGHT,
		.numhandle = config_nicklist_location,
		.description =
		"Location of nicklist. May be:"
		" - Hidden (0)"
		" - Left (1)"
		" - Right (2)"},
	{"nicklist.width", 1, Val_nzunsigned,
		.num = 15,
		.numhandle = config_nicklist_width,
		.description =
		"Number of columns nicklist will take up."},
	{"buflist.location", 1, Val_unsigned,
		.num = LEFT,
		.numhandle = config_buflist_location,
		.description =
		"Location of nicklist. May be:"
		" - Hidden (0)"
		" - Left (1)"
		" - Right (2)"},
	{"buflist.width", 1, Val_nzunsigned,
		.num = 25,
		.numhandle = config_buflist_width,
		.description =
		"Number of columns buflist will take up."},
	{"misc.pingtime", 1, Val_nzunsigned,
		.num = 200,
		.numhandle = NULL,
		.description =
		"Wait this many seconds since last received message"
		"from server to send PING. If ping.wait seconds"
		"elapses since sending a PING, hirc will consider"
		"the server disconnected."},
	{"misc.quitmessage", 1, Val_string,
		.str = "pain is temporary",
		.strhandle = NULL,
		.description =
		"Message to send on /quit"},
	{"misc.partmessage", 1, Val_string,
		.str = "pain is temporary",
		.strhandle = NULL,
		.description =
		"Message to send on /part"},
	{NULL},
};

long
config_getl(char *name) {
	int i;

	for (i=0; config[i].name; i++) {
		if (strcmp(config[i].name, name) == 0 && (
				config[i].valtype == Val_bool ||
				config[i].valtype == Val_signed ||
				config[i].valtype == Val_unsigned ||
				config[i].valtype == Val_nzunsigned))
			return config[i].num;
	}

	return 0;
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
		if (strcmp(config[i].name, name) == 0 &&
				config[i].valtype == Val_range) {
			if (a) *a = config[i].range[0];
			if (b) *b = config[i].range[1];
			return;
		}
	}
}

void
config_setl(char *name, long num) {
	int i;

	for (i=0; config[i].name; i++) {
		if (strcmp(config[i].name, name) == 0 && (
				config[i].valtype == Val_bool ||
				config[i].valtype == Val_signed ||
				config[i].valtype == Val_unsigned ||
				config[i].valtype == Val_nzunsigned)) {
			if (config[i].numhandle) {
				config[i].numhandle(num);
			} else {
				config[i].isdef = 0;
				config[i].num = num;
			}
			return;
		}
	}
}

void
config_sets(char *name, char *str) {
	int i;

	for (i=0; config[i].name; i++) {
		if (strcmp(config[i].name, name) == 0 &&
				config[i].valtype == Val_string) {
			if (config[i].strhandle) {
				config[i].strhandle(str);
			} else {
				if (!config[i].isdef)
					free(config[i].str);
				else
					config[i].isdef = 0;
				config[i].str = estrdup(str);
			}
			return;
		}
	}
}

void
config_setr(char *name, long a, long b) {
	int i;

	for (i=0; config[i].name; i++) {
		if (strcmp(config[i].name, name) == 0 &&
				config[i].valtype == Val_range) {
			if (config[i].strhandle) {
				config[i].rangehandle(a, b);
			} else {
				config[i].isdef = 0;
				config[i].range[0] = a;
				config[i].range[1] = b;
			}
			return;
		}
	}
}

void
config_colour_self(long num) {
	return;
}
void
config_colour_range(long a, long b) {
	return;
}
void
config_nicklist_location(long num) {
	return;
}
void
config_nicklist_width(long num) {
	return;
}
void
config_buflist_location(long num) {
	return;
}
void
config_buflist_width(long num) {
	return;
}
