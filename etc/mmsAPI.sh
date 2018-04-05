#!/bin/bash

export LD_LIBRARY_PATH=/home/giuliano/catramms/lib:/home/giuliano/ffmpeg/lib
export MMS_CONFIGPATHNAME=/home/giuliano/catramms/cfg/api.cfg

spawn-fcgi -p 8000 -n /home/giuliano/dev/usr_local/bin/cgi/api.fcgi

