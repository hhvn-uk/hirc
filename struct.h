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
	Activity_ignore,
	Activity_self = Activity_ignore,
	Activity_status,
	Activity_notice = Activity_status,
	Activity_error,
	Activity_message,
	Activity_hilight,
};

enum HistOpt {
	HIST_SHOW = 1, /* show in buffer */
	HIST_LOG = 2,  /* log to server->logfd */
	HIST_MAIN = 4, /* copy to &main_buf */
	HIST_SELF = 8, /* from = self */
	HIST_DFL = HIST_SHOW|HIST_LOG
};

struct History {
	struct History *prev;
	time_t timestamp;
	enum Activity activity;
	enum HistOpt options;
	char *raw;
	char **params;
	struct Server *origin;
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

struct Server {
	struct Server *prev;
	int wfd;
	int rfd;
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
	int reconnect;
	int connectfail; /* number of failed connections */
	time_t lastconnected; /* last time a connection was lost */
	time_t lastrecv; /* last time a message was received from server */
	time_t pingsent; /* last time a ping was sent to server */
#ifdef TLS
	int tls;
	struct tls *tls_ctx;
#endif /* TLS */
	struct Server *next;
};

struct Handler {
	char *cmd; /* or numeric */
	void (*func)(char *msg, char **params, struct Server *server, time_t timestamp);
};

struct Netconfig {
	char *name;
	char *host;
	char *port;
	char *nick;
	char *user;
	char *real;
	char *join[64];
	int tls;
	int tls_verify;
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
	enum WindowLocation location;
	WINDOW *window;
};

enum {
	Win_main,
	Win_input,
	Win_nicklist,
	Win_winlist,
	Win_last,
};

struct Selected {
	struct Channel *channel;
	struct Server *server;
	struct Channel *oldchannel;
	struct Server *oldserver;
};

#endif /* H_STRUCT */
