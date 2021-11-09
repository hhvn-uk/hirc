#ifndef H_HIRC
#define H_HIRC

#include "struct.h"
#include "config.h"
#define PARAM_MAX 64
#define INPUT_MAX 8192
#define COMMANDARG_MAX (INPUT_MAX / 5)
        /* Theoretical max: -a o -b o -c o *
	 *                  12345          */

/* main.c */
void *		emalloc(size_t size);
char *		estrdup(const char *str);
void		cleanup(char *quitmsg);
void		param_free(char **params);
int		param_len(char **params);
char **		param_create(char *msg);
int		read_line(int fd, char *buf, size_t buf_len);
int		ircprintf(struct Server *server, char *format, ...);
char *		homepath(char *path);
char		chrcmp(char c, char *s);
char *		struntil(char *str, char until);

/* chan.c */
void		chan_free(struct Channel *channel);
void		chan_free_list(struct Channel **head);
struct Channel *chan_create(struct Server *server, char *name);
struct Channel *chan_get(struct Channel **head, char *name, int old);
struct Channel *chan_add(struct Server *server, struct Channel **head, char *name);
int		chan_isold(struct Channel *channel);
void 		chan_setold(struct Channel *channel, int old);
/* struct Channel *chan_dup(struct Channel *channel); */
int		chan_remove(struct Channel **head, char *name);
int		chan_selected(struct Channel *channel);

/* nick.c */
void		prefix_tokenize(char *prefix, char **nick, char **ident, char **host);
void		nick_free(struct Nick *nick);
void		nick_free_list(struct Nick **head);
struct Nick *	nick_create(char *prefix, char priv, struct Server *server);
struct Nick *	nick_get(struct Nick **head, char *nick);
struct Nick *	nick_add(struct Nick **head, char *prefix, char priv, struct Server *server);
struct Nick *	nick_dup(struct Nick *nick, struct Server *server);
int		nick_isself(struct Nick *nick);
int		nick_isself_server(struct Nick *nick, struct Server *server);
int		nick_remove(struct Nick **head, char *nick);
char *		nick_strprefix(struct Nick *nick);
void		nick_sort(struct Nick **head, struct Server *server);

/* hist.c */
void		hist_free(struct History *history);
void		hist_free_list(struct HistInfo *histinfo);
struct History *hist_create(struct HistInfo *histinfo, struct Nick *from, char *msg,
		char **params, enum Activity activity, time_t timestamp, enum HistOpt options);
struct History *hist_add(struct HistInfo *histinfo, 
		struct Nick *from, char *msg, char **params, enum Activity activity, 
		time_t timestamp, enum HistOpt options);
struct History *hist_format(struct HistInfo *history, enum Activity activity, 
		enum HistOpt options, char *format, ...);
int		hist_len(struct History **history);
int		hist_log(char *msg, struct Nick *from, time_t timestamp, struct Server *server);

/* serv.c */
void		serv_free(struct Server *server);
void		serv_connect(struct Server *server);
struct Server *	serv_create(char *name, char *host, char *port, char *nick,
		char *username, char *realname, int tls, int tls_verify);
struct Server * serv_get(struct Server **head, char *name);
struct Server * serv_get_byrfd(struct Server **head, int rfd);
struct Server * serv_add(struct Server **head, char *name, char *host, 
		char *port, char *nick, char *username, char *realname, int tls, int tls_verify);
int		serv_len(struct Server **head);
int		serv_poll(struct Server **head, int timeout);
int		serv_remove(struct Server **head, char *name);
int		serv_selected(struct Server *server);
void		serv_disconnect(struct Server *server, int reconnect, char *msg);
char *		support_get(struct Server *server, char *key);
void		support_set(struct Server *server, char *key, char *value);

/* handle.c */
void		handle(int rfd, struct Server *server);
void		handle_PING(char *msg, char **params, struct Server *server, time_t timestamp);
void		handle_JOIN(char *msg, char **params, struct Server *server, time_t timestamp);
void		handle_PART(char *msg, char **params, struct Server *server, time_t timestamp);
void		handle_QUIT(char *msg, char **params, struct Server *server, time_t timestamp);
void		handle_PRIVMSG(char *msg, char **params, struct Server *server, time_t timestamp);
void		handle_ISUPPORT(char *msg, char **params, struct Server *server, time_t timestamp);
void		handle_NAMREPLY(char *msg, char **params, struct Server *server, time_t timestamp);
void		handle_NICKNAMEINUSE(char *msg, char **params, struct Server *server, time_t timestamp);
void		handle_NICK(char *msg, char **params, struct Server *server, time_t timestamp);

/* ui.c */
void		ui_init(void);
#define		ui_deinit() endwin()
void		ui_read(void);
int		ui_input_insert(char c, int counter);
int		ui_input_delete(int num, int counter);
void		ui_redraw(void);
void		ui_draw_input(void);
void		ui_draw_nicklist(void);
void		ui_draw_buflist(void);
void		ui_draw_main(void);
int		ui_buflist_count(int *ret_servers, int *ret_channels);
void		ui_buflist_select(int num);
int		ui_get_pair(short fg, short bg);
int		ui_wprintc(struct Window *window, int lines, char *format, ...);
int		ui_strlenc(struct Window *window, char *s, int *lines);
void		ui_select(struct Server *server, struct Channel *channel);
void		ui_filltoeol(struct Window *window, char c);
void		ui_wclear(struct Window *window);
void		ui_error_(char *file, int line, char *format, ...);
#define		ui_error(format, ...) ui_error_(__FILE__, __LINE__, format, __VA_ARGS__);
void		ui_perror_(char *file, int line, char *str);
#define		ui_perror(str) ui_perror_(__FILE__, __LINE__, str);
#ifdef TLS
#include <tls.h>
void		ui_tls_config_error_(char *file, int line, struct tls_config *config, char *str);
#define		ui_tls_config_error(config, str) ui_tls_config_error_(__FILE__, __LINE__, config, str);
void		ui_tls_error_(char *file, int line, struct tls *ctx, char *str);
#define		ui_tls_error(ctx, str) ui_tls_error_(__FILE__, __LINE__, ctx, str);
#endif /* TLS */

/* commands.c */
void		command_eval(char *str);
int		command_getopt(char **str, struct CommandOpts *opts);
void		command_quit(char *str);
void		command_connect(char *str);
void		command_select(char *str);

/* main.c */
extern struct Server *servers;
extern struct HistInfo *main_buf;

/* ui.c */
extern struct Selected selected;
extern struct Window windows[Win_last];

#endif /* H_HIRC */
