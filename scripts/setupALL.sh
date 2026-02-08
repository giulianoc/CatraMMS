#!/bin/bash

export MMS_PATH=/opt/mms

#echo "chmod .sh"
#chmod u+x $MMS_PATH/MMS/scripts/*.sh

echo "crontab"
crontab -u mms $MMS_PATH/MMS/conf/crontab.txt

date

