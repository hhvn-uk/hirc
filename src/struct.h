/*
 * src/struct.h from hirc
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

#ifndef H_STRUCT
#define H_STRUCT

#include <time.h>
#include <sys/time.h>
#include <poll.h>

struct Nick {
	struct Nick *prev;
	char priv;    /* [~&@%+ ] */
	char *prefix; /* don't read from this, nick, ident, host are
			 pointers to segments of this.
			 printf(":%s!%s@%s\n", nick, ident, host); */
	char *nick;
	char *ident;
	char *host;
	int self;
	struct Nick *next;
};

enum Activity {
	Activity_ignore = 0,
	Activity_self = Activity_ignore,
	Activity_none = 1,
	Activity_status = 2,
	Activity_notice = Activity_status,
	Activity_error = 3,
	Activity_message = 4,
	Activity_hilight = 5,
};

enum HistOpt {
	HIST_SHOW = 1,  /* show in buffer */
	HIST_LOG = 2,   /* log to server->logfd */
	HIST_MAIN = 4,  /* copy to &main_buf */
	HIST_SELF = 8,  /* from = self */
	HIST_TMP = 16,  /* purge later */
	HIST_GREP = 32, /* generated by /grep */
	HIST_DFL = HIST_SHOW|HIST_LOG,
	HIST_ALL = 0xFFFF
};

struct History {
	struct History *prev;
	time_t timestamp;
	enum Activity activity;
	enum HistOpt options;
	char *raw;
	char **_params; /* contains all params, free from here */
	char **params;  /* contains params without perfix, don't free */
	char *format;   /* cached ui_format */
	struct HistInfo *origin;
	struct Nick *from;
	struct History *next;
};

struct HistInfo {
	enum Activity activity;
	int unread;
	struct Server *server;
	struct Channel *channel;
	struct History *history;
};

struct Channel {
	struct Channel *prev;
	int old; /* are we actually in this channel,
		    or just keeping it for memory */
	char *name;
	char *mode;
	char *topic;
	int priv;
	struct Nick *nicks;
	struct HistInfo *history;
	struct Server *server;
	struct Channel *next;
};

enum ConnStatus {
	ConnStatus_notconnected,
	ConnStatus_connecting,
	ConnStatus_connected,
	ConnStatus_file,
};

struct Support {
	struct Support *prev;
	char *key;
	char *value;
	struct Support *next;
};

enum Expect {
	Expect_join,
	Expect_part,
	Expect_pong,
	Expect_names,
	Expect_topic,
	Expect_topicwhotime,
	Expect_channelmodeis,
	Expect_nicknameinuse,
	Expect_last,
};

struct Schedule {
	struct Schedule *prev;
	char *tmsg;
	char *msg;
	struct Schedule *next;
};

#define SERVER_INPUT_SIZE 16384
struct Server {
	struct Server *prev;
	int wfd;
	int rfd;
	char inputbuf[SERVER_INPUT_SIZE];
	int inputlen;
	struct pollfd *rpollfd;
	int logfd;
	enum ConnStatus status;
	char *name;
	char *username;
	char *realname;
	char *host;
	char *port;
	struct Support *supports;
	struct Nick *self;
	struct HistInfo *history;
	struct Channel *channels;
	struct Channel *privs;
	struct Schedule *schedule;
	int reconnect;
	char *expect[Expect_last];
	char **autocmds;
	int connectfail; /* number of failed connections */
	time_t lastconnected; /* last time a connection was lost */
	time_t lastrecv; /* last time a message was received from server */
	time_t pingsent; /* last time a ping was sent to server */
#ifdef TLS
	int tls;
	int tls_verify;
	struct tls *tls_ctx;
#endif /* TLS */
	struct Server *next;
};

/* messages received from server */
struct Handler {
	char *cmd; /* or numeric */
	void (*func)(struct Server *server, struct History *msg);
};

/* commands received from user */
struct Command {
	char *name;
	void (*func)(struct Server *server, char *str);
	int needserver;
	char *description[64];
};

struct CommandOpts {
	char *opt;
	int arg;
	int ret;
};

enum Valtype {
	Val_string,
	Val_bool,
	Val_colour,
	Val_signed,
	Val_unsigned,
	Val_nzunsigned,
	Val_pair,
	Val_colourpair,
};

struct Config {
	char *name;
	int isdef;
	enum Valtype valtype;
	char *description[64];
	char *str;
	long num;
	long pair[2];
	int (*strhandle)(char *string);
	int (*numhandle)(long num);
	int (*pairhandle)(long a, long b);
};

enum WindowLocation {
	HIDDEN,
	LEFT,
	RIGHT,
};

#include <ncurses.h>
struct Window {
	int x, y;
	int h, w;
	int refresh;
	int scroll;
	enum WindowLocation location;
	void (*handler)(void);
	WINDOW *window;
};

enum {
	Win_dummy,
	Win_main,
	Win_nicklist,
	Win_buflist,
	Win_input, /* should always be
		     last to refresh */
	Win_last,
};

struct Selected {
	struct Channel *channel;
	struct Server *server;
	struct HistInfo *history;
	char *name;
	int hasnicks;
};

struct Keybind {
	struct Keybind *prev;
	char *binding;
	char *cmd;
	struct Keybind *next;
};

struct Alias {
	struct Alias *prev;
	char *alias;
	char *cmd;
	struct Alias *next;
};

#endif /* H_STRUCT */
