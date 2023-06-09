/*
 * src/hirc.h from hirc
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

#ifndef H_HIRC
#define H_HIRC

#include <wchar.h>
#include "struct.h"
#define PARAM_MAX 64
#define INPUT_MAX 8192
#define INPUT_HIST_MAX 64
#define HIST_MAX 8192
	/* real maximum = HIST_MAX * (channels + servers + queries) */
#define CONSTLEN(str) ((size_t)((sizeof(str) - sizeof(str[0])) / sizeof(str[0])))
	/* compile-time char/wchar_t literals */
#define assert(x) ((void)((x) || (die(1, "assertion '%s' failed at %s:%d in %s()\n", #x, __FILE__, __LINE__, __func__),0)))
#ifdef ASSERT_ALWAYS_DIES
#define assert_warn(x,ret) assert(x)
#else
#define assert_warn(x,ret) do {if (!(x)) {ui_error("assertion '%s' failed", #x); return ret;}} while(0)
#endif /* ASSERT_ALWAYS_DIES */

/* strlcpy/wcslcpy.c */
#ifdef HIRC_STRLCPY
#undef strlcpy
size_t		strlcpy(char *, const char *, size_t);
#endif /* HIST_STRLCPY */
#ifdef HIRC_WCSLCPY
#undef wcslcpy
size_t		wcslcpy(wchar_t *, const wchar_t *, size_t);
#endif /* HIST_WCSLCPY */

/* main.c */
void		die(int code, char *format, ...);
void		cleanup(char *quitmsg);

/* params.c */
void		param_free(char **params);
int		param_len(char **params);
char **		param_create(char *msg);

/* str.c */
wchar_t * 	stowc(char *str);
char * 		wctos(wchar_t *str);
char *		homepath(char *path);
int		strcmp_n(const char *s1, const char *s2);
char *		struntil(char *str, char until);
int		strisnum(char *str, int allowneg);
char *		strntok(char *str, char *sep, int n);
char *		strrdate(time_t secs);
char *		smprintf(size_t len, char *fmt, ...);

/* mem.c */
void		pfree_(void **ptr);
#define		pfree(ptr) pfree_((void **)ptr)
void *		emalloc(size_t size);
void *		erealloc(void *ptr, size_t size);
char *		estrdup(const char *str);
void *		talloc(size_t size);
char *		tstrdup(const char *str);
wchar_t * 	ewcsdup(const wchar_t *str);

/* chan.c */
void		chan_free(struct Channel *channel);
void		chan_free_list(struct Channel **head);
struct Channel *chan_create(struct Server *server, char *name, int query);
struct Channel *chan_get(struct Channel **head, char *name, int old);
struct Channel *chan_add(struct Server *server, struct Channel **head, char *name, int query);
int		chan_isold(struct Channel *channel);
void 		chan_setold(struct Channel *channel, int old);
/* struct Channel *chan_dup(struct Channel *channel); */
int		chan_remove(struct Channel **head, char *name);
int		chan_selected(struct Channel *channel);

/* nick.c */
void		prefix_tokenize(char *prefix, char **nick, char **ident, char **host);
short		nick_getcolour(struct Nick *nick);
void		nick_free(struct Nick *nick);
void		nick_free_list(struct Nick **head);
struct Nick *	nick_create(char *prefix, char priv, struct Server *server);
struct Nick *	nick_get(struct Nick **head, char *nick);
struct Nick *	nick_add(struct Nick **head, char *prefix, char priv, struct Server *server);
struct Nick *	nick_dup(struct Nick *nick);
int		nick_isself(struct Nick *nick);
int		nick_isself_server(struct Nick *nick, struct Server *server);
int		nick_remove(struct Nick **head, char *nick);
void		nick_sort(struct Nick **head, struct Server *server);

/* hist.c */
void		hist_free(struct History *history);
void		hist_free_list(struct HistInfo *histinfo);
struct History *hist_create(struct HistInfo *histinfo, struct Nick *from, char *msg,
		enum Activity activity, time_t timestamp, enum HistOpt options);
struct History *hist_addp(struct HistInfo *histinfo, struct History *p,
		enum Activity activity, enum HistOpt options);
struct History *hist_add(struct HistInfo *histinfo,
		char *msg, enum Activity activity,
		time_t timestamp, enum HistOpt options);
struct History *hist_format(struct HistInfo *history, enum Activity activity,
		enum HistOpt options, char *format, ...);
int		hist_len(struct History **history);
int		hist_log(struct History *hist);
struct History *hist_loadlog(struct HistInfo *hist, char *server, char *channel);
void		hist_purgeopt(struct HistInfo *histinfo, enum HistOpt options);

/* serv.c */
void		serv_free(struct Server *server);
void		serv_connect(struct Server *server);
void		serv_read(struct Server *sp);
int		serv_write(struct Server *server, enum Sched when, char *format, ...);
struct Server *	serv_create(char *name, char *host, char *port, char *nick,
		char *username, char *realname, char *password, int tls, int tls_verify);
void		serv_update(struct Server *sp, char *nick, char *username,
		char *realname, char *password, int tls, int tls_verify);
struct Server * serv_get(struct Server **head, char *name);
struct Server * serv_add(struct Server **head, char *name, char *host, char *port, char *nick,
		char *username, char *realname, char *password, int tls, int tls_verify);
int		serv_len(struct Server **head);
int		serv_poll(struct Server **head, int timeout);
int		serv_remove(struct Server **head, char *name);
int		serv_selected(struct Server *server);
void		serv_disconnect(struct Server *server, int reconnect, char *msg);
int		serv_ischannel(struct Server *server, char *str);
void		serv_auto_add(struct Server *server, char *cmd);
void		serv_auto_free(struct Server *server);
void		serv_auto_send(struct Server *server);
int		serv_auto_haschannel(struct Server *server, char *chan);
char *		support_get(struct Server *server, char *key);
void		support_set(struct Server *server, char *key, char *value);
void		schedule(struct Server *server, enum Sched when, char *msg);
void		schedule_send(struct Server *server, enum Sched when);
void		expect_set(struct Server *server, enum Expect cmd, char *about);
char *		expect_get(struct Server *server, enum Expect cmd);

/* handle.c */
void		handle(struct Server *server, char *msg);

/* ui.c */
void		ui_init(void);
#define		ui_deinit() endwin()
void		ui_read(void);
void		ui_complete(wchar_t *str, size_t size);
int		ui_input_insert(char c, int counter);
int		ui_input_delete(int num, int counter);
void		ui_redraw(void);
void		ui_draw_input(void);
void		ui_draw_nicklist(void);
void		ui_draw_buflist(void);
void		ui_draw_main(void);
int		ui_buflist_count(int *ret_servers, int *ret_channels, int *ret_queries);
int		ui_buflist_get(int num, struct Server **server, struct Channel **chan);
int		ui_get_pair(short fg, short bg);
int		ui_wprintc(struct Window *window, int lines, char *format, ...);
int		ui_strlenc(struct Window *window, char *s, int *lines);
void		ui_select(struct Server *server, struct Channel *channel);
char *		ui_rectrl(char *str);
char *		ui_unctrl(char *str);
int		ui_bind(char *binding, char *cmd);
int		ui_unbind(char *binding);
void		ui_error_(char *file, int line, const char *func, char *format, ...);
#define		ui_error(format, ...) ui_error_(__FILE__, __LINE__, __func__, format, __VA_ARGS__)
void		ui_perror_(char *file, int line, const char *func, char *str);
#define		ui_perror(str) ui_perror_(__FILE__, __LINE__, __func__, str)
#ifdef TLS
#include <tls.h>
void		ui_tls_config_error_(char *file, int line, const char *func, struct tls_config *config, char *str);
#define		ui_tls_config_error(config, str) ui_tls_config_error_(__FILE__, __LINE__, __func__, config, str)
void		ui_tls_error_(char *file, int line, const char *func, struct tls *ctx, char *str);
#define		ui_tls_error(ctx, str) ui_tls_error_(__FILE__, __LINE__, __func__, ctx, str)
#endif /* TLS */

/* format.c */
char *		format_get_bufact(int activity);
char *		format_get(struct History *hist);
char *		format(struct Window *window, char *format, struct History *hist);

/* commands.c */
void		command_eval(struct Server *server, char *str);
int		command_getopt(char **str, struct CommandOpts *opts);
int		alias_add(char *binding, char *cmd);
int		alias_remove(char *binding);
char *		alias_eval(char *cmd);

/* config.c */
char *		config_get_pretty(struct Config *conf, int pairbrace);
long		config_getl(char *name);
char *		config_gets(char *name);
void		config_getr(char *name, long *a, long *b);
struct Config *	config_getp(char *name);
void		config_set(char *name, char *str);
void		config_setl(struct Config *conf, long num);
void		config_sets(struct Config *conf, char *str);
void		config_setr(struct Config *conf, long a, long b);
int		config_read(char *filename);

/* complete.c */
void		complete(wchar_t *str, size_t size, unsigned *counter);

/* main.c */
extern struct Server *servers;
extern struct HistInfo *main_buf;

/* handle.c */
extern struct Ignore *ignores;

/* ui.c */
extern struct Selected selected;
extern struct Keybind *keybinds;
extern struct Window windows[Win_last];
extern int uineedredraw;
extern int nouich;

/* config.c */
extern struct Config config[];

/* commands.c */
extern struct Command commands[];
extern struct Alias *aliases;

#endif /* H_HIRC */
