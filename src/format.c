/*
 * src/format.c from hirc
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

#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include "hirc.h"
#include "data/formats.h"

char *
format_get_bufact(int activity) {
	switch (activity) {
	case Activity_status:
		return format(NULL, config_gets("format.ui.buflist.activity.status"), NULL);
	case Activity_error:
		return format(NULL, config_gets("format.ui.buflist.activity.error"), NULL);
	case Activity_message:
		return format(NULL, config_gets("format.ui.buflist.activity.message"), NULL);
	case Activity_hilight:
		return format(NULL, config_gets("format.ui.buflist.activity.hilight"), NULL);
	default:
		return format(NULL, config_gets("format.ui.buflist.activity.none"), NULL);
	}

	return NULL; /* shouldn't be possible *shrug*/
}

char *
format_get(struct History *hist) {
	char *cmd, *p1, *p2;
	int i;

	assert_warn(hist, NULL);

	if (!hist->params)
		goto raw;

	cmd = *(hist->params);
	p1 = *(hist->params+1);
	p2 = *(hist->params+2);

	if (strcmp_n(cmd, "MODE") == 0) {
		if (p1 && serv_ischannel(hist->origin->server, p1))
			cmd = "MODE-CHANNEL";
		else if (hist->from && nick_isself(hist->from) && strcmp_n(hist->from->nick, p1) == 0)
			cmd = "MODE-NICK-SELF";
		else
			cmd = "MODE-NICK";
	} else if (strcmp_n(cmd, "PRIVMSG") == 0) {
		/* ascii 1 is ^A */
		if (*p2 == 1 && strncmp(p2 + 1, "ACTION", CONSTLEN("ACTION")) == 0)
			cmd = "PRIVMSG-ACTION";
		else if (*p2 == 1)
			cmd = "PRIVMSG-CTCP";
	} else if (strcmp_n(cmd, "NOTICE") == 0 && *p2 == 1) {
		cmd = "NOTICE-CTCP";
	}

	for (i=0; formatmap[i].cmd; i++)
		if (formatmap[i].format && strcmp_n(formatmap[i].cmd, cmd) == 0)
			return formatmap[i].format;

	if (isdigit(*cmd) && isdigit(*(cmd+1)) && isdigit(*(cmd+2)) && !*(cmd+3))
		return "format.rpl.other";

raw:
	return "format.other";
}

static char *
format_get_content(char *sstr, int nesting) {
	static char ret[8192];
	int layer, rc;

	for (layer = 0, rc = 0; sstr && *sstr && rc < sizeof(ret); sstr++) {
		switch (*sstr) {
		case '}':
			if (nesting && layer) {
				ret[rc++] = '}';
				layer--;
			} else {
				goto end;
			}
			break;
		case '{':
			if (nesting)
				layer++;
			ret[rc++] = '{';
			break;
		default:
			ret[rc++] = *sstr;
			break;
		}
	}

end:
	ret[rc] = '\0';
	return ret;
}

char *
format_(struct Window *window, char *format, struct History *hist, int recursive) {
	static char *ret;
	struct Nick *nick;
	size_t rs = BUFSIZ;
	size_t rc, pc;
	int escape, i;
	long long pn;
	int rhs = 0;
	int divider = 0;
	char **params;
	char *content, *p, *p2;
	char *ts, *save;
	char colourbuf[2][3];
	char priv[2];
	char chs[2];
	size_t len;
	enum {
		sub_raw,
		sub_cmd,
		sub_nick,
		sub_ident,
		sub_host,
		sub_priv,
		sub_channel,
		sub_topic,
		sub_server,
		sub_time,
	};
	struct {
		char *name;
		char *val;
	} subs[] = {
		[sub_raw]	= {"raw", NULL},
		[sub_cmd]	= {"cmd", NULL},
		[sub_nick]	= {"nick", NULL},
		[sub_ident]	= {"ident", NULL},
		[sub_host]	= {"host", NULL},
		[sub_priv]      = {"priv", NULL},
		[sub_channel]	= {"channel", NULL},
		[sub_topic]	= {"topic", NULL},
		[sub_server]	= {"server", NULL},
		[sub_time]	= {"time", NULL},
		{NULL, NULL},
	};

	if (!format)
		format = config_gets(format_get(hist));
	assert_warn(format, NULL);

	pfree(&ret);
	ret = emalloc(rs);

	subs[sub_channel].val = selected.channel ? selected.channel->name  : NULL;
	subs[sub_topic].val   = selected.channel ? selected.channel->topic : NULL;
	subs[sub_server].val  = selected.server  ? selected.server->name   : NULL;

	if (hist) {
		subs[sub_raw].val   = hist->raw;
		subs[sub_nick].val  = hist->from ? hist->from->nick  : NULL;
		subs[sub_ident].val = hist->from ? hist->from->ident : NULL;
		subs[sub_host].val  = hist->from ? hist->from->host  : NULL;

		if (hist->from) {
			priv[0] = hist->from->priv;
			priv[priv[0] != ' '] = '\0';
			subs[sub_priv].val = priv;
		}

		if (hist->origin) {
			if (hist->origin->channel) {
				if (!recursive)
					divider = config_getl("divider.toggle");
				subs[sub_channel].val = hist->origin->channel->name;
				subs[sub_topic].val   = hist->origin->channel->topic;
			}
			if (hist->origin->server) {
				subs[sub_server].val  = hist->origin->server->name;
			}
		}

		len = snprintf(subs[sub_time].val, 0, "%lld", (long long)hist->timestamp) + 1;
		subs[sub_time].val = emalloc(len);
		snprintf(subs[sub_time].val, len, "%lld", (long long)hist->timestamp);

		params = hist->params;
		subs[sub_cmd].val = *params;
		params++;
	}

	if (!recursive && hist && config_getl("timestamp.toggle")) {
		ts = estrdup(format_(NULL, config_gets("format.ui.timestamp"), hist, 1));
	} else {
		ts = "";
	}

	for (escape = 0, rc = 0; format && *format && rc < rs; ) {
outcont:
		if (rc > rs / 2) {
			rs *= 2;
			ret = erealloc(ret, rs);
		}

		if (!escape && *format == '$' && *(format+1) == '{' && strchr(format, '}')) {
			escape = 0;
			content = format_get_content(format+2, 0);

			for (p = content; *p && isdigit(*p); p++);
			/* If all are digits, *p == '\0' */
			if (!*p && hist) {
				pn = strtol(content, NULL, 10) - 1;
				if (pn >= 0 && param_len(params) > pn) {
					if (**(params+pn) == 1 && strncmp((*(params+pn))+1, "ACTION", CONSTLEN("ACTION")) == 0 && strchr(*(params+pn), ' '))
						rc += snprintf(&ret[rc], rs - rc, "%s", struntil(strchr(*(params+pn), ' ') + 1, 1));
					else if (**(params+pn) == 1)
						rc += snprintf(&ret[rc], rs - rc, "%s", struntil((*(params+pn)) + 1, 1));
					else
						rc += snprintf(&ret[rc], rs - rc, "%s", *(params+pn));
					format = strchr(format, '}') + 1;
					continue;
				}
			}
			/* All are digits except a trailing '-' */
			if (*p == '-' && *(p+1) == '\0' && hist) {
				pn = strtol(content, NULL, 10) - 1;
				if (pn >= 0 && param_len(params) > pn) {
					for (; *(params+pn) != NULL; pn++) {
						if (**(params+pn) == 1 && strncmp((*(params+pn))+1, "ACTION", CONSTLEN("ACTION")) == 0 && strchr(*(params+pn), ' ')) {
							rc += snprintf(&ret[rc], rs - rc, "%s%s",
									struntil(strchr(*(params+pn), ' ') + 1, 1),
									*(params+pn+1) ? " " : "");
						} else if (**(params+pn) == 1) {
							rc += snprintf(&ret[rc], rs - rc, "%s%s",
									struntil((*(params+pn)) + 1, 1),
									*(params+pn+1) ? " " : "");
						} else {
							rc += snprintf(&ret[rc], rs - rc, "%s%s",
									*(params+pn), *(params+pn+1) ? " " : "");
						}
					}
					format = strchr(format, '}') + 1;
					continue;
				}
			}

			for (i=0; subs[i].name; i++) {
				if (strcmp_n(subs[i].name, content) == 0) {
					if (subs[i].val)
						rc += snprintf(&ret[rc], rs - rc, "%s", subs[i].val);
					format = strchr(format, '}') + 1;
					goto outcont; /* unfortunately, need to use a goto as we are already in a loop */
				}
			}
		}

		if (!escape && *format == '%' && *(format+1) == '{' && strchr(format, '}')) {
			escape = 0;
			content = format_get_content(format+2, 0);

			switch (*content) {
			case 'b':
			case 'B':
				ret[rc++] = 2; /* ^B */
				format = strchr(format, '}') + 1;
				continue;
			case 'c':
			case 'C':
				if (*(content+1) == ':' && isdigit(*(content+2))) {
					content += 2;
					memset(colourbuf, 0, sizeof(colourbuf));
					colourbuf[0][0] = *content;
					content++;
					if (isdigit(*content)) {
						colourbuf[0][1] = *content;
						content += 1;
					}
					if (*content == ',' && isdigit(*(content+1))) {
						colourbuf[1][0] = *(content+1);
						content += 2;
					}
					if (colourbuf[1][0] && isdigit(*content)) {
						colourbuf[1][1] = *(content);
						content += 1;
					}
					if (*content == '\0') {
						rc += snprintf(&ret[rc], rs - rc, "%c%02d,%02d", 3 /* ^C */,
								atoi(colourbuf[0]), colourbuf[1][0] ? atoi(colourbuf[1]) : 99);
						format = strchr(format, '}') + 1;
						continue;
					}
				}
				break;
			case 'i':
			case 'I':
				if (*(content+1) == '\0') {
					ret[rc++] = 9; /* ^I */
					format = strchr(format, '}') + 1;
					continue;
				}
				break;
			case 'o':
			case 'O':
				if (*(content+1) == '\0') {
					ret[rc++] = 15; /* ^O */
					format = strchr(format, '}') + 1;
					continue;
				}
				break;
			case 'r':
			case 'R':
				if (*(content+1) == '\0') {
					ret[rc++] = 18; /* ^R */
					format = strchr(format, '}') + 1;
					continue;
				}
				break;
			case 'u':
			case 'U':
				if (*(content+1) == '\0') {
					ret[rc++] = 21; /* ^U */
					format = strchr(format, '}') + 1;
					continue;
				}
				break;
			case '=':
				if (*(content+1) == '\0' && divider) {
					rhs = 1;
					ret[rc] = '\0';
					/* strlen(ret) - ui_strlenc(window, ret, NULL) should get
					 * the length of hidden characters. Add this onto the
					 * margin to pad out properly. */
					/* Save ret for use in snprintf */
					save = estrdup(ret);
					rc = snprintf(ret, rs, "%1$*3$s%2$s", save, config_gets("divider.string"),
							(int)(config_getl("divider.margin") + (strlen(ret) - ui_strlenc(window, ret, NULL))));
					pfree(&save);
					format = strchr(format, '}') + 1;
					continue;
				} else if (*(content+1) == '\0') {
					ret[rc++] = ' ';
					format = strchr(format, '}') + 1;
					continue;
				}
				break;
			}

			/* pad, nick, split, rdate and time, must then continue as they modify content
			 *
			 * These styling formatters are quite ugly and repetitive. 
			 * %{nick:...} was implemented first, and has the most (all of them :)) comments */
			if (strncmp(content, "pad:", CONSTLEN("pad:")) == 0 && strchr(content, ',')) {
				pn = strtol(content + CONSTLEN("pad:"), NULL, 10);
				content = estrdup(format_get_content(strchr(format+2+CONSTLEN("pad:"), ',') + 1, 1));
				save = ret;
				ret = NULL;
				format_(NULL, content, hist, 1);
				rc += snprintf(&save[rc], rs - rc, "%1$*2$s", ret, (int)pn);
				pfree(&ret);
				ret = save;
				format = strchr(format+2+CONSTLEN("pad:"), ',') + strlen(content) + 2;

				pfree(&content);
				continue;
			}

			if (strncmp(content, "rdate:", CONSTLEN("rdate:")) == 0) {
				content = estrdup(format_get_content(format+2+CONSTLEN("rdate:"), 1));
				save = ret;
				ret = NULL;
				format_(NULL, content, hist, 1);
				pn = strtoll(ret, NULL, 10);
				rc += snprintf(&save[rc], rs - rc, "%s", strrdate((time_t)pn));
				format += 3 + CONSTLEN("rdate:") + strlen(content);

				pfree(&ret);
				ret = save;
				pfree(&content);
				continue;
			}

			if (strncmp(content, "time:", CONSTLEN("time:")) == 0 && strchr(content, ',')) {
				content = estrdup(format_get_content(strchr(format+2+CONSTLEN("time:"), ',') + 1, 1));
				save = ret;
				ret = NULL;
				format_(NULL, content, hist, 1);
				pn = strtoll(ret, NULL, 10);
				p = struntil(format+2+CONSTLEN("time:"), ',');

				rc += strftime(&save[rc], rs - rc, p, localtime((time_t *)&pn));
				format = strchr(format+2+CONSTLEN("time:"), ',') + strlen(content) + 2;

				pfree(&ret);
				ret = save;
				pfree(&content);
				continue;
			}

			/* second comma ptr - second comma ptr = distance.
			 * If the distance is 2, then there is one non-comma char between. */
			p = strchr(content, ',');
			if (p)
				p2 = strchr(p + 1, ',');
			if (strncmp(content, "split:", CONSTLEN("split:")) == 0 && p2 - p == 2) {
				pn = strtol(content + CONSTLEN("split:"), NULL, 10);
				chs[0] = *(strchr(content, ',') + 1);
				chs[1] = '\0';
				content = estrdup(format_get_content(
							strchr(
								strchr(format+2+CONSTLEN("split:"), ',') + 1,
								',') + 1,
							1));
				save = ret;
				ret = NULL;
				format_(NULL, content, hist, 1);
				rc += snprintf(&save[rc], rs - rc, "%s", strntok(ret, chs, pn));
				format = strchr(
					strchr(format+2+CONSTLEN("split:"), ',') + 1,
					',') + strlen(content) + 2;

				pfree(&ret);
				ret = save;
				pfree(&content);
				continue;
			}

			if (hist && !recursive && strncmp(content, "nick:", CONSTLEN("nick:")) == 0) {
				content = estrdup(format_get_content(format+2+CONSTLEN("nick:"), 1));
				save = ret;
				ret = NULL;
				format_(NULL, content, hist, 1);
				nick = nick_create(ret, ' ', hist->origin ? hist->origin->server : NULL);
				rc += snprintf(&save[rc], rs - rc, "%c%02d", 3 /* ^C */, nick_getcolour(nick));
				format += 3 + CONSTLEN("nick:") + strlen(content);

				pfree(&ret);
				ret = save;
				nick_free(nick);
				pfree(&content);
				continue;
			}
		}

		if (escape && *format == 'n') {
			ret[rc++] = '\n';
			rc += snprintf(&ret[rc], rs - rc, "%1$*3$s%2$s", "", config_gets("divider.string"),
					(int)(ui_strlenc(NULL, ts, NULL) + config_getl("divider.margin")));
			escape = 0;
			format++;
			continue;
		}

		if (escape && (*format == '%' || *format == '$') && *(format+1) == '{' && strchr(format, '}'))
			escape = 0;

		if (escape) {
			ret[rc++] = '\\';
			escape = 0;
		}

		if (*format == '\\') {
			escape = 1;
			format++;
		} else {
			ret[rc++] = *format;
			format++;
		}
	}

	ret[rc] = '\0';
	if (!recursive && divider && !rhs) {
		save = estrdup(ret);
		rc = snprintf(ret, rs, "%1$*4$s%2$s%3$s", "", config_gets("divider.string"), save, (int)config_getl("divider.margin"));
		pfree(&save);
	}

	save = estrdup(ret);
	rc = snprintf(ret, rs, "%s%s", ts, save);
	pfree(&save);

	if (!recursive && window) {
		for (p = ret, pc = 0; p && p <= (ret + rs); p++) {
			/* lifted from ui_strlenc */
			switch (*p) {
			case 2:  /* ^B */
			case 9:  /* ^I */
			case 15: /* ^O */
			case 18: /* ^R */
			case 21: /* ^U */
				break;
			case 3:  /* ^C */
				if (*p && isdigit(*(p+1)))
					p += 1;
				if (*p && isdigit(*(p+1)))
					p += 1;
				if (*p && *(p+1) == ',' && isdigit(*(p+2)))
					p += 2;
				if (*p && *(p-1) == ',' && isdigit(*(p+1)))
					p += 1;
				break;
			default:
				/* naive utf-8 handling:
				 * the 2-nth byte always
				 * follows 10xxxxxxx, so
				 * don't count it. */
				if ((*p & 0xC0) != 0x80)
					pc++;

				if (*p == '\n') {
					p++;
					pc = 0;
				}

				if (pc == window->w) {
					save = estrdup(p);

					if (divider) {
						p += snprintf(p, rs - ((size_t)(p - ret)), "%1$*4$s %2$s%3$s",
								"", config_gets("divider.string"), save,
								(int)config_getl("divider.margin") + ui_strlenc(NULL, ts, NULL));
					} else {
						p += snprintf(p, rs - ((size_t)(p - ret)), "%1$*3$s %2$s", "", save, ui_strlenc(NULL, ts, NULL));
					}

					pfree(&save);
					pc = 0;
				}
			}
		}
	}

	if (subs[sub_time].val)
		pfree(&subs[sub_time].val);
	if (ts[0] != '\0')
		pfree(&ts);

	return ret;
}
