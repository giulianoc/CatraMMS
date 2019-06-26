#!/bin/bash


echo "nginx"
/opt/catramms/CatraMMS/scripts/nginx.sh start

#encoder before mmsEngineService otherwise mmsEngineService starts to send commands
#to the encoder that it is still down
echo "encoder"
/opt/catramms/CatraMMS/scripts/encoder.rsi.sh start

echo "mmsEngineService"
/opt/catramms/CatraMMS/scripts/mmsEngineService.rsi.sh start

echo "api"
/opt/catramms/CatraMMS/scripts/api.rsi.sh start

echo "tomcat"
/opt/catramms/CatraMMS/scripts/tomcat.sh start

date

