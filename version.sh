#!/bin/bash

set -e

[ $# -eq 1 ] || echo "Wrong parameters count"

file="$1"

[ -n "$file" ]

tr -s "\t" " " < "$file" | while IFS=" " read -r dummy filed number
do
	case "$filed" in
		VERSION_MAJOR)
			version="$number"
		;;
		VERSION_MINOR)
			version="$version.$number"
		;;
		VERSION_BUILD)
			version="$version.$number"
			echo "$version"
			exit 0
		;;
	esac
done
