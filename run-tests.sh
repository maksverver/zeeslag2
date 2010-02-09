#!/bin/sh
solve() { ./solver -v "$1" "$2" "$3"; }
while read line; do solve $line; done
