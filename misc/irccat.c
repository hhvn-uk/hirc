/*
 * misc/irccat.c from hirc - cat(1) that convert mIRC codes to ANSI.
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

#include <stdio.h>
#include <ctype.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>

#define FCOLOUR		"\x1b[38;5;%dm"
#define BCOLOUR		"\x1b[48;5;%dm"
#define BOLD		"\x1b[1m"
#define NBOLD		"\x1b[22m"
#define ITALIC		"\x1b[3m"
#define NITALIC		"\x1b[23m"
#define REVERSE		"\x1b[7m"
#define NREVERSE	"\x1b[27m"
#define UNDERLINE	"\x1b[4m"
#define NUNDERLINE	"\x1b[24m"
#define RESET		"\x1b[0m"

#include "../src/data/colours.h"

void
display(int fd) {
	char buf[BUFSIZ], *s;
	ssize_t ret;
	char colourbuf[2][3];
	int colours[2];
	int rcolour = 0, bg = 0;
	int bold = 0;
	int underline = 0;
	int reverse = 0;
	int italic = 0;

	while ((ret = read(fd, buf, sizeof(buf) - 1)) != 0) {
		buf[ret] = '\0';
		for (s = buf; s && *s; s++) {
			if (rcolour) {
				if (!bg && !colourbuf[0][0] && isdigit(*s)) {
					colourbuf[0][0] = *s;
					continue;
				} else if (!bg && !colourbuf[0][1] && isdigit(*s)) {
					colourbuf[0][1] = *s;
					continue;
				} else if (!bg && *s == ',') {
					bg = 1;
					continue;
				} else if (bg && !colourbuf[1][0] && isdigit(*s)) {
					colourbuf[1][0] = *s;
					continue;
				} else if (bg && !colourbuf[1][1] && isdigit(*s)) {
					colourbuf[1][1] = *s;
					continue;
				} else {
					rcolour = bg = 0;
					colours[0] = colourbuf[0][0] ? atoi(colourbuf[0]) : 99;
					colours[1] = colourbuf[1][0] ? atoi(colourbuf[1]) : 99;

					if (colours[0] != 99)
						printf(FCOLOUR, colourmap[colours[0]]);
					if (colours[1] != 99)
						printf(BCOLOUR, colourmap[colours[1]]);
				}
			}

			switch (*s) {
			case 2: /* ^B */
				if (bold)
					printf(NBOLD);
				else
					printf(BOLD);
				bold = !bold;
				break;
			case 3: /* ^C */
				memset(colourbuf, '\0', sizeof(colourbuf));
				rcolour = 1;
				break;
			case 9: /* ^I */
				if (italic)
					printf(NITALIC);
				else
					printf(ITALIC);
				italic = !italic;
				break;
			case 15: /* ^O */
				bold = underline = reverse = italic = 0;
				printf(RESET);
				break;
			case 18: /* ^R */
				if (reverse)
					printf(NREVERSE);
				else
					printf(REVERSE);
				reverse = !reverse;
				break;
			case 21: /* ^U */
				if (underline)
					printf(NUNDERLINE);
				else
					printf(UNDERLINE);
				underline = !underline;
				break;
			case '\n':
				bold = underline = reverse = italic = 0;
				printf(RESET);
				/* fallthrough */
			default:
				putchar(*s);
				break;
			}
		}
	}
}

int
main(int argc, char *argv[]) {
	int i;
	FILE *f;

	if (argc < 2) {
		display(0);
	} else {
		for (i = 1; i < argc; i++) {
			if (strcmp(argv[i], "-") == 0) {
				display(0);
			} else if ((f = fopen(argv[i], "r")) == NULL) {
				fprintf(stderr, "could not read '%s': %s\n", argv[i], strerror(errno));
			} else {
				display(fileno(f));
				fclose(f);
			}
		}
	}
}
