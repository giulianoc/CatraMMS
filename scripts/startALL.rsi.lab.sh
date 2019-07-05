#!/bin/bash


echo "nginx"
/opt/catramms/CatraMMS/scripts/nginx.sh start

#encoder before mmsEngineService otherwise mmsEngineService starts to send commands
#to the encoder that it is still down
echo "mmsEncoder"
/opt/catramms/CatraMMS/scripts/mmsEncoder.rsi.lab.sh start

echo "mmsEngineService"
/opt/catramms/CatraMMS/scripts/mmsEngineService.rsi.lab.sh start

echo "mmsApi"
/opt/catramms/CatraMMS/scripts/mmsApi.rsi.lab.sh start

echo "tomcat"
/opt/catramms/CatraMMS/scripts/tomcat.sh start

date

