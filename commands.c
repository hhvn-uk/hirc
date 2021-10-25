#include <ncurses.h>
#include <stdlib.h>
#include <string.h>
#include "hirc.h"

void
command_eval(char *str) {
	char msg[512];

	if (*str != '/' || strncmp(str, "/ /", sizeof("/ /")) == 0) {
		if (strncmp(str, "/ /", sizeof("/ /")) == 0)
			str += 3;

		if (selected.channel && selected.server) {
			// TODO: message splitting
			snprintf(msg, sizeof(msg), "PRIVMSG %s :%s", selected.channel->name, str);
			ircprintf(selected.server, "%s\r\n", msg);
			hist_format(selected.server, selected.channel->history, Activity_self, HIST_SHOW|HIST_LOG|HIST_SELF, msg);
		} else
			ui_error("channel not selected, message ignored", NULL);

		return;
	}

	if (strcmp(str, "/quit") == 0) {
		endwin();
		exit(0);
	}

	str++;
	ircprintf(selected.server, "%s\r\n", str);
}
