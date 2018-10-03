#!/bin/bash

DATE=`date +%Y-%m-%d`
tail -f logs/mmsAPI/mms*$DATE* logs/mmsEngineService/mms*$DATE* logs/mmsEncoder/mms*$DATE* 


