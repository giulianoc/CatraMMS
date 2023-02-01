#!/bin/bash

CONTENT_PATHFILENAME=$1
TAG_NAME=$2

#echo "-------" >> /tmp/promo.log
#echo "$(date)" >> /tmp/promo.log
#echo "CONTENT_PATHFILENAME: $CONTENT_PATHFILENAME" >> /tmp/promo.log
#echo "TAG_NAME: $TAG_NAME" >> /tmp/promo.log

urlencode() {
    # urlencode <string>

    local length="${#1}"
    for (( i = 0; i < length; i++ )); do
        local c="${1:i:1}"
        case $c in
            [a-zA-Z0-9.~_-:/]) printf "$c" ;;
            *) printf '%%%x' \'"$c" ;;
        esac
    done
}

CONTENT_PATHNAME=$(urlencode "$CONTENT_PATHFILENAME")

#echo $CONTENT_PATHNAME >> /tmp/promo.log

curl -k --include -X GET "https://mms-gui.rsi.ch/catramms/rest/api_rsi_v1/materialPromo/$TAG_NAME?ContentPathName=$CONTENT_PATHNAME"

