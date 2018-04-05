#!/bin/bash

PID=$(cat /usr/local/nginx/conf/nginx.conf | grep -Ev '^\s*#' | awk 'BEGIN { RS="[;{}]" } { if ($1 == "pid") print $2 }' | head -n1)
#echo $PID
sudo start-stop-daemon --stop --quiet  --retry=TERM/30/KILL/5 --pidfile $PID --name nginx

