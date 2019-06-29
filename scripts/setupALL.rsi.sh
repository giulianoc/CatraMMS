#!/bin/bash

#echo "chmod .sh"
#chmod u+x /opt/catramms/CatraMMS/scripts/*.sh

echo "crontab"
crontab -u mms /opt/catramms/CatraMMS/conf/crontab.rsi.txt

date

