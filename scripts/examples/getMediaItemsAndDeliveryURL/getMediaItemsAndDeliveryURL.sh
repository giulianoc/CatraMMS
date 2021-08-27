#!/bin/bash


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

curl -k -v -X POST -u 8:1e1h1m1h1o1l1e1809151C1d1q1v1r1p1h1v1d111f1r1p1b1b1V1H1S1b1b0908171:1909171914 -d '{}' -H "Content-Type: application/json" "https://mms-api.restream.ovh/catramms/v1/mediaItems?start=0&rows=10&contentType=video&startIngestionDate=2021-01-01T09:00:00Z&endIngestionDate=2021-05-01T09:00:00Z"


mediaItemKey=1356895
ttlInSeconds=3600
ttlInSeconds=200

curl -k -v -u 8:1e1h1m1h1o1l1e1809151C1d1q1v1r1p1h1v1d111f1r1p1b1b1V1H1S1b1b0908171:1909171914 "https://mms-api.restream.ovh/catramms/v1/delivery/vod/$mediaItemKey/0?encodingProfileLabel=MMS_MP4_H264_2500Kb_medium_720p25_high422_AAC_160&ttlInSeconds=$ttlInSeconds&maxRetries=$maxRetries&authorizationThroughPath=true&redirect=false"


