#!/bin/bash

echo "chmod .sh"
chmod u+x /opt/catramms/CatraMMS/scripts/*.sh

echo "crontab"
crontab -u mms /opt/catramms/CatraMMS/conf/crontab.rsi.txt

echo "stopALL"
/opt/catramms/CatraMMS/scripts/stopALL.rsi.sh stop

echo "nginx"
/opt/catramms/CatraMMS/scripts/nginx.sh stop
/opt/catramms/CatraMMS/scripts/nginx.sh status
/opt/catramms/CatraMMS/scripts/nginx.sh start

#encoder before mmsEngineService otherwise mmsEngineService starts to send commands
#to the encoder that it is still down
echo "encoder"
/opt/catramms/CatraMMS/scripts/encoder.rsi.sh stop
/opt/catramms/CatraMMS/scripts/encoder.rsi.sh status
/opt/catramms/CatraMMS/scripts/encoder.rsi.sh start

echo "mmsEngineService"
/opt/catramms/CatraMMS/scripts/mmsEngineService.rsi.sh stop
/opt/catramms/CatraMMS/scripts/mmsEngineService.rsi.sh status
/opt/catramms/CatraMMS/scripts/mmsEngineService.rsi.sh start

echo "api"
/opt/catramms/CatraMMS/scripts/api.rsi.sh stop
/opt/catramms/CatraMMS/scripts/api.rsi.sh status
/opt/catramms/CatraMMS/scripts/api.rsi.sh start

echo "tomcat"
/opt/catramms/CatraMMS/scripts/tomcat.sh stop
/opt/catramms/CatraMMS/scripts/tomcat.sh status
/opt/catramms/CatraMMS/scripts/tomcat.sh start

date

