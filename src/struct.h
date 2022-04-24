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
	char *prefix;
	char *nick;
	char *ident;
	char *host;
	int self;
	struct Nick *next;
};

enum Activity {
	/* There used to be an Activity_ignore = 0, but it served no purpose */
	Activity_none = 1,
	Activity_self = Activity_none,
	Activity_status = 2,
	Activity_notice = Activity_status,
	Activity_error = 3,
	Activity_message = 4,
	Activity_hilight = 5,
};

enum HistOpt {
	/* H - options signal hist functions to perform different actions
	 * C - options are checked later to be handled in different ways
	 * R - options which are read from logs (in HIST_LOGACCEPT) */
	HIST_SHOW = 1,   /* [CR] show in buffer */
	HIST_LOG  = 2,   /* [H]  log to log.dir */
	HIST_MAIN = 4,   /* [H]  copy to &main_buf */
	HIST_SELF = 8,   /* [H]  from = self */
	HIST_TMP  = 16,  /* [C]  purge later */
	HIST_GREP = 32,  /* [C]  generated by /grep */
	HIST_ERR  = 64,  /* [CR] generated by ui_error and friends */
	HIST_SERR = 128, /* [CR] generated by 400-599 numerics (which should be errors) */
	HIST_RLOG = 256, /* [C]  messages read from log, useful for clearing the log */
	HIST_IGN  = 512, /* [CR] added to ignored messages */
	HIST_NIGN = 1024,/* [H]  immune to ignoring (SELF_IGNORES_LIST & SELF_ERROR) */
	HIST_DFL = HIST_SHOW|HIST_LOG,
	HIST_UI = HIST_SHOW|HIST_TMP|HIST_MAIN,
	HIST_LOGACCEPT = HIST_SHOW|HIST_ERR|HIST_SERR|HIST_IGN,
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
	char *format;   /* cached format */
	char *rformat;  /* cached format without mirc codes */
	struct HistInfo *origin;
	struct Nick *from;
	struct History *next;
};

struct HistInfo {
	enum Activity activity;
	int unread;
	int ignored;
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
	/*
	 * The expect system is how a command or handler is able to change the
	 * actions performed by subsequent handlers (without changing
	 * server/chan/nick structs with I prefer to primarily track data, not
	 * actions).
	 *
	 * Each element in the expect array (contained in every server struct)
	 * can be set to either NULL or an arbitrary string (usually a channel
	 * name), as such they aren't really standard - the best way to know
	 * what each does is to grep through the source.
	 *
	 * I think this system works pretty well considering its simplicity.
	 * Alternatively handlers could look back in history - this would
	 * likely be much more robust, whilst being much less effecient as a
	 * handler needs to perform extra logic rather than looking up a
	 * string. There could also be some system utilizing callbacks that
	 * performs logic itself, rather than having handlers perform logic
	 * with the return value of expect_get: this may be more elegant and
	 * "proper", but it seems needlessly complicated to me.
	 *
	 */
	Expect_join,
	Expect_part,
	Expect_pong,
	Expect_names,
	Expect_topic,
	Expect_topicwhotime,
	Expect_channelmodeis,
	Expect_nicknameinuse,
	Expect_nosuchnick, /* currently set by commands that send MODE
			      and subsequently unset by handle_mode */
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
	enum ConnStatus status;
	char *name;
	char *username;
	char *realname;
	char *password;
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
	void (*func)(struct Server *server, struct Channel *channel, char *str);
	int need; /* 0  - nothing
		     1  - server
		     2  - channel (and implicitly server)
		     3+ - implementation defined (pfft. who's gonna reimplement hirc?) */
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
	Val_location,
};

struct Config {
	char *name;
	int isdef;
	enum Valtype valtype;
	char *description[64];
	char *str;
	long num;
	long pair[2];
	int (*strhandle)(struct Config *, char *);
	int (*numhandle)(struct Config *, long);
	int (*pairhandle)(struct Config *, long, long);
};

enum WindowLocation {
	Location_hidden,
	Location_left,
	Location_right,
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

enum Windows {
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
	int showign;
	int hasnicks;
};

#include <wchar.h>
struct Keybind {
	struct Keybind *prev;
	char *binding;
	wchar_t *wbinding;
	char *cmd;
	struct Keybind *next;
};

struct Alias {
	struct Alias *prev;
	char *alias;
	char *cmd;
	struct Alias *next;
};

#include <regex.h>
struct Ignore {
	struct Ignore *prev;
	char *format;
	char *text;
	regex_t regex;
	int regopt;
	int noact;
	char *server;
	struct Ignore *next;
};

#endif /* H_STRUCT */
