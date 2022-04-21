/*
 * src/data/handle.h from hirc
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

#define HANDLER(func) static void func(struct Server *server, struct History *msg)
HANDLER(handle_ERROR);
HANDLER(handle_PING);
HANDLER(handle_PONG);
HANDLER(handle_JOIN);
HANDLER(handle_PART);
HANDLER(handle_KICK);
HANDLER(handle_QUIT);
HANDLER(handle_NICK);
HANDLER(handle_MODE);
HANDLER(handle_TOPIC);
HANDLER(handle_PRIVMSG);
HANDLER(handle_INVITE);
HANDLER(handle_RPL_WELCOME);
HANDLER(handle_RPL_ISUPPORT);
HANDLER(handle_RPL_CHANNELMODEIS);
HANDLER(handle_RPL_NOTOPIC);
HANDLER(handle_RPL_TOPIC);
HANDLER(handle_RPL_TOPICWHOTIME);
HANDLER(handle_RPL_INVITING);
HANDLER(handle_RPL_NAMREPLY);
HANDLER(handle_RPL_ENDOFNAMES);
HANDLER(handle_RPL_MOTD);
HANDLER(handle_RPL_ENDOFMOTD);
HANDLER(handle_ERR_NOSUCHNICK);
HANDLER(handle_ERR_NICKNAMEINUSE);
HANDLER(handle_RPL_AWAY);

struct Ignore *ignores = NULL;
struct Handler handlers[] = {
	{ "ERROR",	handle_ERROR			},
	{ "PING", 	handle_PING			},
	{ "PONG",	handle_PONG			},
	{ "JOIN",	handle_JOIN			},
	{ "PART",	handle_PART			},
	{ "KICK",	handle_KICK			},
	{ "QUIT",	handle_QUIT			},
	{ "NICK",	handle_NICK			},
	{ "MODE",	handle_MODE			},
	{ "TOPIC",	handle_TOPIC			},
	{ "PRIVMSG",	handle_PRIVMSG  		},
	{ "NOTICE",	handle_PRIVMSG	  		},
	{ "INVITE",	handle_INVITE			},
	{ "001",	handle_RPL_WELCOME		},
	{ "005",	handle_RPL_ISUPPORT		},
	{ "301",	handle_RPL_AWAY			},
	{ "324",	handle_RPL_CHANNELMODEIS	},
	{ "331",	handle_RPL_NOTOPIC		},
	{ "329",	NULL				}, /* ignore this:
							    *  - it's nonstandard
							    *  - hirc has no use for it currently
							    *  - it's annoyingly sent after MODE */
	{ "332",	handle_RPL_TOPIC		},
	{ "333",	handle_RPL_TOPICWHOTIME		},
	{ "341",	handle_RPL_INVITING		},
	{ "353",	handle_RPL_NAMREPLY		},
	{ "366",	handle_RPL_ENDOFNAMES		},
	{ "372",	handle_RPL_MOTD			},
	{ "375",	handle_RPL_MOTD			}, /* RPL_MOTDSTART, but handle it the same way as RPL_MOTD */
	{ "376",	handle_RPL_ENDOFMOTD		},
	{ "401",	handle_ERR_NOSUCHNICK		},
	{ "433",	handle_ERR_NICKNAMEINUSE	},
	{ NULL,		NULL 				},
};
