#!/bin/bash
if [[ "$#" -lt 1 ]]; then
	echo "Usage me log.txt"
	exit
fi
log="$1"
clean="$1.clean"
# 71763 345 374.90 365.89 364.37 363.65 363.65
# 90030 266 273.81 274.70 273.55 273.92 275.35 265.61
(echo "Review...";
 echo;
 cgrepp -nav '^\d+\s\d+(\s\d+\.\d+){6}$' "$log") 2>&1 | less
grepp '^\d+\s\d+(\s\d+\.\d+){6}$' "$log" > "$clean"
echo "Wrote clean version to: $clean"
printf 'Lines: %s\n' "$(wc -l "$clean")"
