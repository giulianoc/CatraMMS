#!/bin/bash


echo "nginx"
~/nginx.sh start

#encoder before mmsEngineService otherwise mmsEngineService starts to send commands
#to the encoder that it is still down
echo "mmsEncoder"
~/mmsEncoder.sh start

echo "mmsEngineService"
~/mmsEngineService.sh start

echo "mmsApi"
~/mmsApi.sh start

echo "tomcat"
~/tomcat.sh start

date

