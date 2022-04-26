/*
 * src/data/colours.c from hirc
 *
 * Copyright (c) 2021 hhvn <dev@hhvn.uk>
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

#define HIRC_COLOURS 100
static unsigned short colourmap[HIRC_COLOURS] = {
	/* original 16 mirc colours
	 * some clients use the first 16 ansi colours for this,
	 * but here I use the 256 colours to ensure terminal-agnosticism */
	[0] = 255, 16, 19, 46, 124, 88,  127, 184,
	[8] = 208, 46, 45, 51, 21,  201, 240, 255,

	/* extended */
	[16] = 52,  94,  100, 58,  22,  29,  23,  24,  17,  54,  53,  89,
	[28] = 88,  130, 142, 64,  28,  35,  30,  25,  18,  91,  90,  125,
	[40] = 124, 166, 184, 106, 34,  49,  37,  33,  19,  129, 127, 161,
	[52] = 196, 208, 226, 154, 46,  86,  51,  75,  21,  171, 201, 198,
	[64] = 203, 215, 227, 191, 83,  122, 87,  111, 63,  177, 207, 205,
	[76] = 217, 223, 229, 193, 157, 158, 159, 153, 147, 183, 219, 212,
	[88] = 16,  233, 235, 237, 239, 241, 244, 247, 250, 254, 231,

	/* transparency */
	[99] = -1
};
