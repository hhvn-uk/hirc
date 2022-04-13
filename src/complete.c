/*
 * src/complete.c from hirc
 *
 * Copyright (c) 2022 hhvn <dev@hhvn.uk>
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
#include <libgen.h>
#include <dirent.h>
#include "hirc.h"

void complete_stitch(wchar_t *dest, size_t dsize, unsigned *counter, unsigned coff,
		wchar_t **stoks, size_t slen,
		wchar_t *str,
		wchar_t **etoks, size_t elen,
		int fullcomplete);
void complete_add(char **ret, char *str, int *fullcomplete);
void complete_cmds(char *str, size_t len, char **ret, int *fullcomplete);
void complete_settings(char *str, size_t len, char **ret, int *fullcomplete);
void complete_nicks(struct Channel *chan, char *str, size_t len, char **ret, int *fullcomplete);
void complete_servers(struct Server **head, char *str, size_t len, char **ret, int *fullcomplete);
void complete_files(char *str, size_t len, char **ret, int *fullcomplete);

void
complete_stitch(wchar_t *dest, size_t dsize, unsigned *counter, unsigned coff,
		wchar_t **stoks, size_t slen,
		wchar_t *str,
		wchar_t **etoks, size_t elen,
		int fullcomplete) {
	wchar_t **wp;
	size_t i, dc = 0;

	for (wp = stoks, i = 0; i < slen; i++, wp++)
		dc += swprintf(dest + dc, dsize - dc, L"%ls%s", *wp, (i != slen - 1 || str || elen) ? " " : "");

	if (str)
		dc += swprintf(dest + dc, dsize - dc, L"%ls%s", str, (fullcomplete || elen) ? " " : "");

	if (!fullcomplete && elen)
		coff -= 1;
	*counter = dc + coff;

	for (wp = etoks, i = 0; i < elen; i++, wp++)
		dc += swprintf(dest + dc, dsize - dc, L"%ls%s", *wp, i != elen - 1 ? " " : "");
}

void
complete_add(char **ret, char *str, int *fullcomplete) {
	int i;

	if ((*ret)) {
		(*fullcomplete) = 0;
		for (i = 0; (*ret)[i] && str[i] && (*ret)[i] == str[i]; i++);
		(*ret)[i] = '\0';
	} else (*ret) = estrdup(str);
}

void
complete_cmds(char *str, size_t len, char **ret, int *fullcomplete) {
	int i;
	for (i = 0; commands[i].name; i++)
		if (strncmp(commands[i].name, str, len) == 0)
			complete_add(ret, commands[i].name, fullcomplete);
}

void
complete_settings(char *str, size_t len, char **ret, int *fullcomplete) {
	int i;
	for (i = 0; config[i].name; i++)
		if (strncmp(config[i].name, str, len) == 0)
			complete_add(ret, config[i].name, fullcomplete);
}

void
complete_nicks(struct Channel *chan, char *str, size_t len, char **ret, int *fullcomplete) {
	struct Nick *np;
	for (np = chan->nicks; np; np = np->next)
		if (!np->self && strncmp(np->nick, str, len) == 0)
			complete_add(ret, np->nick, fullcomplete);
}

void
complete_servers(struct Server **head, char *str, size_t len, char **ret, int *fullcomplete) {
	struct Server *sp;
	for (sp = *head; sp; sp = sp->next)
		if (strncmp(sp->name, str, len) == 0)
			complete_add(ret, sp->name, fullcomplete);
}

void
complete_files(char *str, size_t len, char **ret, int *fullcomplete) {
	char *cpy[2], *dir, *file;
	struct dirent **dirent;
	int dirs, i;

	cpy[0] = estrdup(str);
	cpy[1] = estrdup(str);
	dir = dirname(cpy[0]);
	file = basename(cpy[1]);

	if ((dirs = scandir(dir, &dirent, NULL, alphasort)) >= 0) {
		for (i = 0; i < dirs; i++) {
			if (strncmp(dirent[i]->d_name, str, len) == 0)
				complete_add(ret, dirent[i]->d_name, fullcomplete);
			pfree(&dirent[i]);
		}
		pfree(&dirent);
	}

	pfree(&cpy[0]);
	pfree(&cpy[1]);
}

void
complete(wchar_t *str, size_t size, unsigned *counter) {
	wchar_t *wstem = NULL;
	char *stem = NULL;
	static int pctok, prcnt;
	wchar_t **_toks;
	wchar_t **toks;
	wchar_t *cmd;
	size_t tokn, i, j, len;
	wchar_t *wp, *dup, *save;
	char *found = NULL, *p, *hchar;
	int ctok = -1, rcnt = -1; /* toks[ctok] + rcnt == char before cursor */
	unsigned coff = 0; /* str + coff == *counter */
	int fullcomplete = 1;
	int type;

	/* start at 1: 'a b c' -> 2 spaces, but 3 tokens */
	for (wp = str, tokn = 1, i = j = 0; wp && *wp; wp++, i++, j++) {
		if (i == *counter || (*(wp+1) == '\0' && rcnt == -1)) {
			if (*wp == ' ' && *(wp+1) == '\0' && i + 1 == *counter) {
				ctok = tokn;
				rcnt = 0;
			} else {
				ctok = tokn - 1;
				rcnt = j;
				if (i != *counter)
					rcnt++;
			}
		}
		if (*wp == L' ') {
			j = -1;
			tokn++;
		}
	}

	_toks = toks = emalloc(tokn * sizeof(wchar_t *));
	dup = ewcsdup(str);
	wp = NULL;
	i = 0;
	memset(toks, 0, tokn * sizeof(wchar_t *));
	while ((wp = wcstok(!wp ? dup : NULL, L" ", &save)) && i < tokn)
		*(_toks + i++) = wp;

getcmd:
	if (*str == L'/' && tokn)
		cmd = toks[0] + 1;
	else
		cmd = NULL;

	/* /server network /comman<cursor --> /comman<cursor>
	 * /server [-auto] network<cursor> --> ... nah */
	if (cmd && ((wcscmp(cmd, L"server") == 0 && ctok != 1 && (tokn <= 2 || wcscmp(toks[1], L"-auto") != 0 || ctok != 2)) ||
			wcscmp(cmd, L"alias") == 0 ||
			wcscmp(cmd, L"bind") == 0) && tokn >= 2) {
		if (tokn > 2 && wcscmp(toks[1], L"-auto") == 0)
			i = 3;
		else if (tokn > 2)
			i = 2;
		else
			i = 1;
		j = 1 + wcslen(toks[0]);
		if (i >= 2)
			j += 1 + wcslen(toks[1]);
		if (i >= 3)
			j += 1 + wcslen(toks[2]);
		str += j;
		coff += j;
		size -= j;

		toks += i;
		tokn -= i;
		ctok -= i;
		goto getcmd;
	}

	/* complete commands */
	if (cmd && ctok == 0) {
		wstem = toks[0] + 1;

		stem = wctos(wstem);
		len = strlen(stem);

		complete_cmds(stem, len, &found, &fullcomplete);
		pfree(&stem);

		if (found) {
			len = strlen(found) + 2;
			p = emalloc(len);
			snprintf(p, len, "/%s", found);
			pfree(&found);
			found = p;

			wp = stowc(found);
			pfree(&found);
			complete_stitch(str, size, counter, coff,
					NULL, 0,
					wp,
					toks + 1, tokn - 1,
					fullcomplete);
			pfree(&wp);
			goto end;
		}
		pfree(&found);
	}

	/* complete commands/variables as arguments */
	type = 0;
	if (cmd) {
		if (wcscmp(cmd, L"help") == 0)
			type = 1;
		else if (wcscmp(cmd, L"set") == 0)
			type = 2;
		else if (wcscmp(cmd, L"format") == 0)
			type = 3;
		if (type && ctok == 1 && toks[1]) {
			wstem = toks[1];

			p = wctos(wstem);
			if (type == 3) {
				len = strlen(p) + CONSTLEN("format.") + 1;
				stem = emalloc(len);
				snprintf(stem, len, "format.%s", p);
			} else stem = p;
			len = strlen(stem);

			if (type == 1)
				complete_cmds(stem, len, &found, &fullcomplete);
			complete_settings(stem, len, &found, &fullcomplete);
			pfree(&stem);

			if (found) {
				if (type == 3)
					p = found + CONSTLEN("format.");
				else
					p = found;
				wp = stowc(p);
				complete_stitch(str, size, counter, coff,
						toks, 1,
						wp,
						toks + 2, tokn - 2,
						fullcomplete);
				pfree(&wp);
				pfree(&found);
				goto end;
			}
			pfree(&found);
		}
	}

	/* complete nicks */
	if (selected.channel && ctok > -1 && toks[ctok] && *toks[ctok]) {
		wstem = toks[ctok];
		stem = wctos(wstem);
		len = strlen(stem);

		complete_nicks(selected.channel, stem, len, &found, &fullcomplete);
		pfree(&stem);

		if (found) {
			if (ctok == 0 && fullcomplete) {
				hchar = config_gets("completion.hchar");
				len = strlen(found) + strlen(hchar) + 1;
				p = emalloc(len);
				snprintf(p, len, "%s%s", found, hchar);
				pfree(&found);
				found = p;
			}
			wp = stowc(found);
			complete_stitch(str, size, counter, coff,
					toks, ctok,
					wp,
					toks + ctok + 1, tokn - ctok - 1,
					fullcomplete);
			pfree(&wp);
			pfree(&found);
			goto end;
		}
		pfree(&found);
	}

	/* complete filenames with /source and /dump */
	if (cmd && (wcscmp(cmd, L"source") == 0 || wcscmp(cmd, L"dump") == 0) && ctok > 0 &&
			toks[ctok] && *toks[ctok] && *toks[ctok] != L'-' && ctok == tokn - 1) {
		wstem = toks[ctok];
		stem = wctos(wstem);
		len = strlen(stem);

		complete_files(stem, len, &found, &fullcomplete);
		pfree(&stem);

		if (found) {
			wp = stowc(found);
			complete_stitch(str, size, counter, coff,
					toks, tokn - 1,
					wp,
					NULL, 0,
					fullcomplete);
			pfree(&wp);
			pfree(&found);
			goto end;
		}
		pfree(&found);
	}

	/* complete servers with /server */
	if (cmd && wcscmp(cmd, L"server") == 0) {
		if (toks[1] && wcscmp(toks[1], L"-auto") == 0)
			i = 2;
		else
			i = 1;
		if (i == ctok && toks[ctok] && *toks[ctok] != L'-' && *toks[ctok]) {
			wstem = toks[ctok];
			stem = wctos(wstem);
			len = strlen(stem);

			complete_servers(&servers, stem, len, &found, &fullcomplete);
			pfree(&stem);

			if (found) {
				wp = stowc(found);
				complete_stitch(str, size, counter, coff,
						toks, ctok,
						wp,
						toks + ctok + 1, tokn - ctok - 1,
						fullcomplete);
				pfree(&wp);
				pfree(&found);
				goto end;
			}
			pfree(&found);
		}
	}

end:
	pfree(&_toks);
	/* elements in _toks are pointers to dup */
	pfree(&dup);
	return;
}
