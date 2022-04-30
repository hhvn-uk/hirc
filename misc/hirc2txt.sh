#!/bin/sh
# Select logfile, and view with hirc2txt/irccat

dir="$HOME/.local/hirc"

file=$(find "$dir" -type f -name "*.log" | dmenu -i -p 'Log file:' | head -n 1)

hirc2txt < $file | irccat | less -R
