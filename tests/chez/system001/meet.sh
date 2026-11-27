#!/bin/sh
# Usage: sh meet.sh OWN OTHER
# Creates build/OWN, then waits about 20 seconds at most for build/OTHER.
touch "build/$1"
tries=0
while [ ! -f "build/$2" ]; do
    tries=$((tries + 1))
    if [ "$tries" -gt 200 ]; then
        exit 1
    fi
    sleep 0.1
done
