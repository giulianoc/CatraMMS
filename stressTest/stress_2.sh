#!/bin/bash

url=$1
count=$2

let i=0
tot=0
while [ $i -lt $count ];
do
	res=$(curl -w "$i: %{time_total} %{http_code} %{size_download}\n" -o "/dev/null" -s ${url})
	echo "$(date) - $res"
	val=$(echo $res | cut -f2 -d' ')
	if (( $(echo "$val > 1.0" | bc -l) )); then
		echo "Too long ($i): $val"
	fi
	tot=$(echo "scale=3;${tot}+${val}" | bc)
	let i=i+1

	sleep 1
done

avg=$(echo "scale=3; ${tot}/${count}" | bc)
echo "   ........................."
echo "   AVG: $tot/$count = $avg"

