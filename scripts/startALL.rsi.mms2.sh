#!/bin/bash


echo "nginx"
/opt/catramms/CatraMMS/scripts/nginx.sh start

#encoder before mmsEngineService otherwise mmsEngineService starts to send commands
#to the encoder that it is still down
echo "mmsEncoder"
/opt/catramms/CatraMMS/scripts/mmsEncoder.rsi.mms2.sh start

echo "mmsEngineService"
/opt/catramms/CatraMMS/scripts/mmsEngineService.rsi.mms2.sh start

echo "mmsEpi"
/opt/catramms/CatraMMS/scripts/mmsEpi.rsi.mms2.sh start

echo "tomcat"
/opt/catramms/CatraMMS/scripts/tomcat.sh start

date

