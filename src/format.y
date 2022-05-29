%{
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <ctype.h>
#include "hirc.h"
#include "data/formats.h"

#define YYSTYPE char *

static void parse_append(char **dest, char *str);
static char *parse_dup(char *str);
static char *parse_printf(char *fmt, ...);
static void yyerror(char *msg);
static int yylex(void);

#define BRACEMAX 16
#define RETURN(s) do {prev = s; return s;} while (0)
#define ISSPECIAL(s) isspecial(s, prev, bracelvl, bracetype, styleseg)


enum {
	PARSE_TIME,
	PARSE_LEFT,
	PARSE_RIGHT,
	PARSE_LAST
};

static int parse_error = 0;
static char *parse_in = NULL;
static char *parse_out[PARSE_LAST] = {NULL, NULL, NULL};
static int parse_pos = PARSE_LEFT;
static char **parse_params = NULL;
static struct Server *parse_server = NULL;

enum {
	var_raw,
	var_cmd,
	var_nick,
	var_ident,
	var_host,
	var_priv,
	var_channel,
	var_topic,
	var_server,
	var_time,
};
static struct {
	char *name;
	char *val;
} vars[] = {
	[var_raw]	= {"raw", NULL},
	[var_cmd]	= {"cmd", NULL},
	[var_nick]	= {"nick", NULL},
	[var_ident]	= {"ident", NULL},
	[var_host]	= {"host", NULL},
	[var_priv]	= {"priv", NULL},
	[var_channel]	= {"channel", NULL},
	[var_topic]	= {"topic", NULL},
	[var_server]	= {"server", NULL},
	[var_time]	= {"time", NULL},
	{NULL, NULL},
};
%}

%token VAR STYLE LBRACE RBRACE COLON COMMA
%token STRING

%%

grammar: /* empty */ { parse_append(&parse_out[parse_pos], ""); }
       	| grammar var { parse_append(&parse_out[parse_pos], $2); }
	| grammar style { parse_append(&parse_out[parse_pos], $2); }
	| grammar STRING { parse_append(&parse_out[parse_pos], $2); }
       	;

var:	VAR LBRACE STRING RBRACE {
   		char buf[8192];
		char *tmp;
     		int i, num;
		if (strisnum($3, 0) && (num = strtoll($3, NULL, 10)) && param_len(parse_params) >= num) {
			if (num == 2 && **(parse_params + num - 1) == 1 &&
					(strcmp_n(vars[var_cmd].val, "PRIVMSG") == 0 || strcmp_n(vars[var_cmd].val, "NOTICE") == 0)) {
				if (strncmp(*(parse_params + num - 1) + 1, "ACTION", CONSTLEN("ACTION")) == 0)
					tmp = parse_dup(*(parse_params + num - 1) + 1 + CONSTLEN("ACTION "));
				else
					tmp = parse_dup(*(parse_params + num - 1) + 1);
				if (tmp[strlen(tmp) - 1] == 1)
					tmp[strlen(tmp) - 1] = '\0';
				$$ = tmp;
			} else $$ = *(parse_params + num - 1);
			goto varfin;
		}
		if (*$3 && *($3 + strlen($3) - 1) == '-') {
			*($3 + strlen($3) - 1) = '\0';
			if (strisnum($3, 0) && (num = strtoll($3, NULL, 10)) && param_len(parse_params) >= num) {
				buf[0] = '\0';
				for (i = num; i <= param_len(parse_params); i++) {
					strlcat(buf, *(parse_params + i - 1), sizeof(buf));
					strlcat(buf, " ", sizeof(buf));
				}
				$$ = parse_dup(buf);
				goto varfin;
			} else {
				*($3 + strlen($3) - 1) = '\0';
			}
		}
     		for (i = 0; vars[i].name; i++) {
			if (vars[i].val && strcmp(vars[i].name, $3) == 0) {
				$$ = parse_printf("%s", vars[i].val);
				goto varfin;
			}
		}
		$$ = parse_printf("${%s}", $3);
varfin:
      		((void)0);
	}
	;

style:	STYLE LBRACE STRING RBRACE {
     		if (strcmp($3, "b") == 0) {
			$$ = parse_printf("\02"); /* ^B */
     		} else if (strcmp($3, "c") == 0) {
			$$ = parse_printf("\0399,99"); /* ^C */
     		} else if (strcmp($3, "i") == 0) {
			$$ = parse_printf("\011"); /* ^I */
     		} else if (strcmp($3, "o") == 0) {
			$$ = parse_printf("\017"); /* ^O */
     		} else if (strcmp($3, "r") == 0) {
			$$ = parse_printf("\022"); /* ^R */
     		} else if (strcmp($3, "u") == 0) {
			$$ = parse_printf("\025"); /* ^U */
     		} else if (strcmp($3, "=") == 0) {
			if (parse_pos == PARSE_LEFT) {
				parse_pos = PARSE_RIGHT;
				$$ = parse_dup("");
			} else {
				$$ = parse_dup(" ");
			}
		} else if (strcmp($3, "_time") == 0) { /* special style to mark end of timestamp */
			if (parse_pos == PARSE_TIME) {
				parse_pos = PARSE_LEFT; /* let's hope no-one puts it in format.ui.timestamp */
				$$ = parse_dup("");
			} else {
				yyerror("%{_time} used erroneously here: "
					"this style is meant for internal use only, "
					"this isn't a bug if you manually inserted this in a format");
				YYERROR;
			}
		} else {
			$$ = parse_printf("%%{%s}", $3);
		}
	}
	| STYLE LBRACE STRING COLON sstring RBRACE {
		struct Nick *nick;
		if (strcmp($3, "c") == 0) {
			if (strlen($5) <= 2 && isdigit(*$5) && (!*($5+1) || isdigit(*($5+1))))
				$$ = parse_printf("\03%02d" /* ^C */, atoi($5));
		} else if (strcmp($3, "nick") == 0) {
			nick = nick_create($5, ' ', parse_server);
			$$ = parse_printf("\03%02d" /* ^C */, nick_getcolour(nick));
			nick_free(nick);
		} else if (strcmp($3, "rdate") == 0) {
			if (strisnum($5, 0)) {
				$$ = parse_printf("%s", strrdate((time_t)strtoll($5, NULL, 10)));
			} else {
				yyerror("invalid date in invocation of %{rdate:...}");
				YYERROR;
			}
		} else {
			$$ = parse_printf("%%{%s:%s}", $3, $5);
		}
	}
	| STYLE LBRACE STRING COLON sstring COMMA sstring RBRACE {
		char stime[1024];
		time_t dtime;
		if (strcmp($3, "c") == 0) {
			if (strlen($5) <= 2 && isdigit(*$5) && (!*($5+1) || isdigit(*($5+1))) &&
					strlen($7) <= 2 && isdigit(*$7) && (!*($7+1) || isdigit(*($5+1))))
				$$ = parse_printf("\03%02d,%02d" /* ^C */, atoi($5), atoi($7));
			else
				$$ = parse_printf("%%{%s:%s,%s}", $3, $5, $7);
		} else if (strcmp($3, "pad") == 0) {
			if (strisnum($5, 1)) {
				$$ = parse_printf("%1$*2$s", $7, (int)strtoll($5, NULL, 0));
			} else {
				yyerror("second argument to %{pad:...} must be a integer");
				YYERROR;
			}
		} else if (strcmp($3, "time") == 0) {
			if (strisnum($7, 0)) {
				dtime = (time_t)strtoll($7, NULL, 0);
				strftime(stime, sizeof(stime), $5, localtime(&dtime));
				$$ = parse_dup(stime);
			} else {
				yyerror("invalid date in invocation of %{time:...}");
				YYERROR;
			}
		} else {
			$$ = parse_printf("%%{%s:%s,%s}", $3, $5, $7);
		}
	}
	| STYLE LBRACE STRING COLON sstring COMMA sstring COMMA sstring RBRACE {
		int num;
		char *val;
		if (strcmp($3, "split") == 0) {
			num = strtoll($5, NULL, 10);
			val = strntok($9, $7, num);
			if (strisnum($5, 0) && val) {
				$$ = parse_dup(val);
			} else if (strisnum($5, 0)) {
				$$ = parse_dup("");
			} else {
				yyerror("first argument to %{split:...} must be an integer");
				YYERROR;
			}
		} else {
			$$ = parse_printf("%%{%s:%s,%s,%s}", $3, $5, $7, $9);
		}
	}
	;

sstring: STRING
       | var
       | style
       ;

%%

static void
yyerror(char *msg) {
	parse_error = 1;
	ui_error("parsing '%s': %s", parse_in, msg);
}

/* Keep in mind: parse_append doesn't use parse_dup. */
static void
parse_append(char **dest, char *str) {
	size_t size;
	size_t len;

	if (*dest)
		len = strlen(*dest);
	else
		len = 0;

	size = len + strlen(str) + 1;
	if (!*dest)
		*dest = emalloc(size);
	else
		*dest = erealloc(*dest, size);
	
	(len ? strlcat : strlcpy)(*dest, str, size);
}

/* alloc memory to be free'd all at once when parsing is complete
 * pfree() must not be called on anything returned */
static char *
parse_dup(char *str) {
	static void **mema = NULL;
	static size_t mems = 0;
	void *mem = NULL;
	size_t i;

	if (str) {
		mem = strdup(str);
		if (!mems)
			mema = emalloc((sizeof(char *)) * (mems + 1));
		else
			mema = erealloc(mema, (sizeof(char *)) * (mems + 1));

		*(mema + mems) = mem;
		mems++;
	} else if (mema && mems) {
		for (i = 0; i < mems; i++)
			pfree(mema + i); /* already a double pointer */
		pfree(&mema);
		mems = 0;
		mema = NULL;
	}

	return mem;
}

static char *
parse_printf(char *format, ...) {
	va_list ap;
	char buf[8192];

	va_start(ap, format);
	vsnprintf(buf, sizeof(buf), format, ap);
	va_end(ap);
	return parse_dup(buf);
}

static int
isspecial(char *s, int prev, int bracelvl, int bracetype[static BRACEMAX], int styleseg[static BRACEMAX]) {
	if ((*s == '$' && (*(s+1) == '{' && bracelvl != BRACEMAX)) ||
			(*s == '%' && (*(s+1) == '{' && bracelvl != BRACEMAX)) ||
			(*s == '{' && (prev == VAR || prev == STYLE)) ||
			(*s == '}' && (bracelvl)) ||
			(*s == ',' && (bracelvl && bracetype[bracelvl - 1] == STYLE && styleseg[bracelvl - 1])) ||
			(*s == ':' && (bracelvl && bracetype[bracelvl - 1] == STYLE && !styleseg[bracelvl - 1])))
		return 1;
	else
		return 0;
}

static int
yylex(void) {
	static char *s = NULL, *p = NULL;
	static int bracelvl;
	static int bracetype[BRACEMAX];
	static int styleseg[BRACEMAX];
	static int prev = 0;
	char strlval[8192];
	int i;

	if (!s || prev == 0) {
		s = parse_in;
		bracelvl = 0;
		prev = -1;
	}

	if (!*s)
		RETURN(0);

	if (ISSPECIAL(s)) {
		switch (*s) {
		case '$':
			s++;
			RETURN(VAR);
		case '%':
			s++;
			RETURN(STYLE);
		case '{':
			bracelvl++;
			bracetype[bracelvl - 1] = prev;
			if (prev == STYLE)
				styleseg[bracelvl - 1] = 0;
			s++;
			RETURN(LBRACE);
		case '}':
			bracelvl--;
			s++;
			RETURN(RBRACE);
		case ',':
			styleseg[bracelvl - 1]++;
			s++;
			RETURN(COMMA);
		case ':':
			styleseg[bracelvl - 1]++;
			s++;
			RETURN(COLON);
		}
	}

	/* first char guaranteed due to previous ISSPECIAL() */
	strlval[0] = *s;

	for (p = s + 1, i = 1; *p && i < sizeof(strlval); p++) {
		if (ISSPECIAL(p))
			break;
		if (*p == '\\' && ISSPECIAL(p+1)) {
			strlval[i++] = *(p+1);
			p++;
		} else if (*p == '\\' && *(p+1) == 'n') {
			strlval[i++] = '\n';
			p++;
		} else {
			strlval[i++] = *p;
		}
	}

	strlval[i] = '\0';
	yylval = parse_dup(strlval);
	s = p;
	RETURN(STRING);
}

/*
 * Exposed functions
 */

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

char *
format(struct Window *window, char *format, struct History *hist) {
	static char *ret;
	char *ts;
	char *rformat;
	int x;
	size_t len, pad;
	int clen[PARSE_LAST]; /* ui_strlenc */
	int alen[PARSE_LAST]; /* strlen */
	int divlen = config_getl("divider.margin");
	int divbool = 0;
	char *divstr = config_gets("divider.string");
	char priv[2];
	size_t i;

	assert_warn(format || hist, NULL);
	if (!format)
		format = config_gets(format_get(hist));
	assert_warn(format, NULL);

	if (hist && config_getl("timestamp.toggle")) {
		ts = config_gets("format.ui.timestamp");
		len = strlen(ts) + strlen(format) + CONSTLEN("%{_time}") + 1;
		rformat = emalloc(len);
		snprintf(rformat, len, "%s%%{_time}%s", ts, format);
		parse_pos = PARSE_TIME;
	} else {
		rformat = strdup(format);
		parse_pos = PARSE_LEFT;
	}

	vars[var_channel].val = selected.channel ? selected.channel->name  : NULL;
	vars[var_topic].val   = selected.channel ? selected.channel->topic : NULL;
	vars[var_server].val  = selected.server  ? selected.server->name   : NULL;

	if (hist) {
		vars[var_raw].val   = hist->raw;
		vars[var_nick].val  = hist->from ? hist->from->nick  : NULL;
		vars[var_ident].val = hist->from ? hist->from->ident : NULL;
		vars[var_host].val  = hist->from ? hist->from->host  : NULL;

		if (hist->from) {
			priv[0] = hist->from->priv;
			priv[priv[0] != ' '] = '\0';
			vars[var_priv].val = priv;
		}

		if (hist->origin) {
			if (hist->origin->channel) {
				divbool = config_getl("divider.toggle");
				vars[var_channel].val = hist->origin->channel->name;
				vars[var_topic].val   = hist->origin->channel->topic;
			}
			parse_server = hist->origin->server;
			if (hist->origin->server) {
				vars[var_server].val  = hist->origin->server->name;
			}
		}

		len = snprintf(vars[var_time].val, 0, "%lld", (long long)hist->timestamp) + 1;
		vars[var_time].val = emalloc(len);
		snprintf(vars[var_time].val, len, "%lld", (long long)hist->timestamp);

		vars[var_cmd].val = *hist->params;
		if (hist->params)
			parse_params = hist->params + 1;
	} else {
		parse_params = NULL;
		parse_server = NULL;
	}

	parse_dup(0); /* free memory in use for last parse_ */
	for (i = 0; i < PARSE_LAST; i++)
		pfree(&parse_out[i]);
	pfree(&ret);
	parse_in = rformat;

	yyparse();
	pfree(&rformat);

	if (parse_error) {
		parse_error = 0;
		return NULL;
	}


	/* If there is no %{=}, then it's on the right */
	if (hist && parse_out[PARSE_LEFT] && !parse_out[PARSE_RIGHT]) {
		parse_out[PARSE_RIGHT] = parse_out[PARSE_LEFT];
		parse_out[PARSE_LEFT] = NULL;
	}
	
	for (i = 0; i < PARSE_LAST; i++) {
		clen[i] = parse_out[i] ? ui_strlenc(&windows[Win_main], parse_out[i], NULL) : 0;
		alen[i] = parse_out[i] ? strlen(parse_out[i]) : 0;
	}

	/* Space for padding is allocated here (see how len is incremented using pad) */
	if (divbool) {
		len = alen[PARSE_TIME] + alen[PARSE_LEFT] + alen[PARSE_RIGHT] + divlen - clen[PARSE_LEFT] + strlen(divstr) + 1;
		pad = clen[PARSE_TIME] + divlen + strlen(divstr) + 1;
		len += (clen[PARSE_RIGHT] / (window->w - pad)) * pad + 1;
		ret = emalloc(len);
		snprintf(ret, len, "%1$s %2$*3$s%4$s%5$s",
				parse_out[PARSE_TIME] ? parse_out[PARSE_TIME] : "",
				parse_out[PARSE_LEFT] ? parse_out[PARSE_LEFT] : "", divlen + alen[PARSE_LEFT] - clen[PARSE_LEFT],
				divstr,
				parse_out[PARSE_RIGHT]);
	} else {
		len = alen[PARSE_TIME] + alen[PARSE_LEFT] + alen[PARSE_RIGHT] + 1;
		if (window) {
			pad = clen[PARSE_TIME];
			len += ((clen[PARSE_LEFT] + clen[PARSE_RIGHT] + 1) / (window->w - pad)) * pad;
		}
		ret = emalloc(len);
		snprintf(ret, len, "%s%s%s",
				parse_out[PARSE_TIME] ? parse_out[PARSE_TIME] : "",
				parse_out[PARSE_LEFT] ? parse_out[PARSE_LEFT] : "",
				parse_out[PARSE_RIGHT] ? parse_out[PARSE_RIGHT] : "");
	}

	if (window) {
		for (i = x = 0; ret[i]; i++) {
			/* taken from ui_strlenc */
			switch (ret[i]) {
			case 2: case 9: case 15: case 18: case 21:
				break;
			case 3:  /* ^C */
				if (ret[i] && isdigit(ret[i+1]))
					i += 1;
				if (ret[i] && isdigit(ret[i+1]))
					i += 1;
				if (ret[i] && ret[i+1] == ',' && isdigit(ret[i+2]))
					i += 2;
				if (ret[i] && i && ret[i-1] == ',' && isdigit(ret[i+1]))
					i += 1;
				break;
			default:
				if ((ret[i] & 0xC0) != 0x80)
					while ((ret[i + 1] & 0xC0) == 0x80)
						i++;
				x++;
			}

			if (x == window->w) {
				x = 0;
				memmove(ret + i + pad, ret + i, strlen(ret + i) + 1);
				if (parse_out[PARSE_TIME])
					memset(ret + i + 1, ' ', clen[PARSE_TIME]);
				if (divbool) {
					memset(ret + i + 1 + clen[PARSE_TIME], ' ', pad - clen[PARSE_TIME]);
					memcpy(ret + i + 1 + pad - strlen(divstr), divstr, strlen(divstr));
				}
				/* no need to increment i, as all the text
				 * inserted here is after ret[i] and will be
				 * counted in this for loop */
			}
		}
	}

	return ret;
}
