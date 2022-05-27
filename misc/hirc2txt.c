#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>

char *topic = NULL;

void
convert(char *line) {
	char *tok[8];
	char *msg, *p;
	char time[64];
	time_t timestamp;
	int i;

	if (*line == 'v')
		line = strchr(line, '\t');
	tok[0] = strtok_r(line, "\t", &msg);
	for (i = 1; i < (sizeof(tok) / sizeof(tok[0])); i++)
		tok[i] = strtok_r(NULL, "\t", &msg);

	if (!tok[0] || !tok[1] || !tok[2] ||
			!tok[3] || !tok[4] || !tok[5] ||
			!tok[6] || !tok[7] || !msg)
		return;

	timestamp = (time_t)strtoll(tok[0], NULL, 10);

	strftime(time, sizeof(time), "[%Y-%m-%d %H:%M:%S]", localtime(&timestamp));

	if (strncmp(msg, "PRIVMSG ", 8) == 0 && strstr(msg, ":\01ACTION")) {
		if (msg[strlen(msg) - 1] == 1)
			msg[strlen(msg) - 1] = '\0';
		p = strchr(msg, ':') + 9;
		printf("%s *%s %s\n", time, tok[5], p);
	} else if (strncmp(msg, "PRIVMSG ", 8) == 0 && strstr(msg, ":\01")) {
		if (msg[strlen(msg) - 1] == 1)
			msg[strlen(msg) - 1] = '\0';
		p = strchr(msg, ':') + 2;
		printf("%s %s requested %s via CTCP\n", time, tok[5], p);
	} else if (strncmp(msg, "NOTICE ", 7) == 0 && strstr(msg, ":\01")) {
		if (msg[strlen(msg) - 1] == 1)
			msg[strlen(msg) - 1] = '\0';
		p = strchr(msg, ':') + 2;
		if (msg = strchr(p, ' ')) {
			*msg = '\0';
			msg++;
		} else msg = "";
		printf("%s %s replied to the CTCP request for %s: %s\n", time, tok[5], p, msg);
	} else if (strncmp(msg, "NOTICE ", 7) == 0 && (p = strchr(msg, ':'))) {
		printf("%s -%s%s- %s\n", time, *tok[4] != ' ' ? tok[4] : "", tok[5], p + 1);
	} else if (strncmp(msg, "PRIVMSG ", 8) == 0 && (p = strchr(msg, ':'))) {
		printf("%s <%s%s> %s\n", time, *tok[4] != ' ' ? tok[4] : "", tok[5], p + 1);
	} else if (strncmp(msg, "JOIN ", 5) == 0) {
		printf("%s %s (%s@%s) joined.\n", time, tok[5], tok[6], tok[7]);
	} else if (strncmp(msg, "PART ", 5) == 0) {
		printf("%s %s (%s@%s) parted.\n", time, tok[5], tok[6], tok[7]);
	} else if (strncmp(msg, "QUIT ", 5) == 0) {
		printf("%s %s (%s@%s) quit.\n", time, tok[5], tok[6], tok[7]);
	} else if (strncmp(msg, "332 ", 4) == 0 && (p = strchr(msg, ':'))) {
		free(topic);
		topic = strdup(p + 1);
	} else if (strncmp(msg, "TOPIC ", 6) == 0 && (p = strchr(msg, ':'))) {
		if (topic)
			printf("%s %s changed the topic from \"%s\" to \"%s\"\n", time, tok[5], topic, p + 1);
		else
			printf("%s %s set the topic to \"%s\"\n", time, tok[5], p + 1);
		free(topic);
		topic = strdup(p + 1);
	} else if (strncmp(msg, "NICK ", 5) == 0 && (p = strchr(msg, ':'))) {
		printf("%s %s is now known as %s\n", time, tok[5], p + 1);
	} else if (strncmp(msg, "MODE ", 5) == 0 && (p = msg + 5)) {
		if ((p = strchr(p, ' ')))
			printf("%s %s set mode(s) %s\n", time, tok[5], p + 1);
	}

}

int
main(void) {
	char buf[2048], *p;

	while (fgets(buf, sizeof(buf), stdin)) {
		if ((p = strchr(buf, '\n')) && *(p+1) == '\0')
			*p = '\0';
		convert(buf);
	}
}
