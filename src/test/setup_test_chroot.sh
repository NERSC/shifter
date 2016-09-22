#!/bin/bash
set -x
chrootdir=$1

if [[ -z "$chrootdir" ]]; then
	exit 1
fi

while read -r path; do
	directory=$(dirname "$path")

	mkdir -p "$chrootdir/$directory"
        cp -p "$path" "$chrootdir/$directory"

	
done < <(find /lib/ /lib64/ -name libnss_files\*)
