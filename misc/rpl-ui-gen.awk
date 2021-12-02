#!/bin/awk -f
# Parses RFC1459 for formatmap[]

$1 ~ /^[0-9][0-9][0-9]$/ && $2 ~ /RPL_/ {
	rpl = tolower($2)
	sub(/_/, ".", rpl)
	printf("\t{\"%03d\",\t\t\t\"format.%s\"},\n", $1, rpl);
}
