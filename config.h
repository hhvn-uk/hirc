#ifndef H_CONFIG
#define H_CONFIG

#include "struct.h"

#define MAIN_NICK "hhvn"
#define MAIN_USERNAME "Fanatic"
#define MAIN_REALNAME "gopher://hhvn.uk"

static char *logdir = "~/.local/hirc";
static struct Netconfig netconfig[] = {
	{
		.name = "test",
		.host = "localhost",
		.port = "6667",
		.nick = MAIN_NICK,
		.user = MAIN_USERNAME,
		.real = MAIN_REALNAME,
		.join = { "#test", "#test2", NULL },
		.tls = 0,
		.tls_verify = 0,
	},
	/*
	{ 	
		.name = "hlircnet",
		.host = "irc.hhvn.uk",
		.port = "6667",
		.nick = MAIN_NICK,
		.user = MAIN_USERNAME,
		.real = MAIN_REALNAME,
		.join = {
			"#hlircnet", "#help", "#gopher", "#vhosts", 
			"#hlfm", "#cgo", "#distrotube", NULL
		},
	},
	{
		.name = "dataswamp",
		.host = "irc.dataswamp.org",
		.port = "6667",
		.nick = MAIN_NICK,
		.user = MAIN_USERNAME,
		.real = MAIN_REALNAME,
		.join = { "#dataswamp", NULL },
	},
	{
		.name = "efnet",
		.host = "efnet.port80.se",
		.port = "6667",
		.nick = MAIN_NICK,
		.user = MAIN_USERNAME,
		.real = MAIN_REALNAME,
		.join = { "#asciiart", "#LRH", "#thepiratebay.org", NULL },
	},
	{
		.name = "ikteam",
		.host = "irc.nk.ax",
		.port = "6667",
		.nick = MAIN_NICK,
		.user = MAIN_USERNAME,
		.real = MAIN_REALNAME,
		.join = { "#chat", NULL },
	},
	{
		.name = "nebulacentre",
		.host = "irc.nebulacentre.net",
		.port = "6667",
		.nick = MAIN_NICK,
		.user = MAIN_USERNAME,
		.real = MAIN_REALNAME,
		.join = { "#general", NULL },
	},
	{
		.name = "sdf",
		.host = "irc.sdf.org",
		.port = "6667",
		.nick = MAIN_NICK,
		.user = MAIN_USERNAME,
		.real = MAIN_REALNAME,
		.join = { "#sdf", "#gopher", "#helpdesk", NULL },
	}
	*/
};

/* real maximum = MAX_HISTORY * (channels + servers + queries) */
#define MAX_HISTORY 1024

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

/* (mIRC) colour for any messages sent by oneself */
static unsigned short selfcolour = 90;

/* (mIRC) inclusive colour range for any messages sent 
 * by others. Use same number twice for constant colour */
static unsigned short othercolour[2] = {28, 63};

/* default channel types */
static char *default_chantypes = "#&!+";

/* default prefixes/priveledges, (symbols)modes */
static char *default_prefixes = "(ov)@+";

/* send ping to server after n seconds of inactivity */
static int pinginact = 200;

/* max seconds to wait between reconnects */
static long maxreconnectinterval = 600;

/* number of seconds between reconnects,
 * multiplied by amount of failed reconnects.
 * Example: reconnectinterval = 10, maxreconnectinterval = 600
 *     1st: 0  * 10 = 0
 *     2nd: 1  * 10 = 10
 *     3rd: 2  * 10 = 20
 *    10th: 10 * 10 = 100
 *    60th: 60 * 10 = 600
 *    61st: 600, maxreconnectinterval reached */
static long reconnectinterval = 10;

/* nicklist location:
 * HIDDEN, LEFT, RIGHT */
static short nicklistlocation = RIGHT;

/* width of nicklist in columns */
static int nicklistwidth = 15;

/* buffer list location:
 * HIDDEN, LEFT, RIGHT */
static short buflistlocation = LEFT;

/* width of buffer list in columns */
static int buflistwidth = 25;

/* default quit message */
static char *quitmessage = "pain is temporary";

#endif /* H_CONFIG */
