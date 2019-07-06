#!/bin/bash

export CatraMMS_PATH=/opt/catramms

#echo "chmod .sh"
#chmod u+x $CatraMMS_PATH/CatraMMS/scripts/*.sh

echo "crontab"
crontab -u mms $CatraMMS_PATH/CatraMMS/conf/crontab.txt

date

