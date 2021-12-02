#!/bin/awk -f
# Parses RFC1459 for config[]

$1 ~ /^[0-9][0-9][0-9]$/ && $2 ~ /RPL_/ {
	rpl = tolower($2);
	sub(/_/, ".", rpl);
	printf("\t{\"format.%s\", 1, Val_string,\n", rpl);
	printf("\t\t.str = \"${2-}\",\n");
	printf("\t\t.strhandle = config_redraws,\n");
	printf("\t\t.description = {\n");
	printf("\t\t\"Format of %s (%03d) numeric\", NULL}},\n", $2, $1)
}
