#!/bin/bash


echo "mmsApi"
~/mmsApi.sh stop

echo "mmsEncoder"
~/mmsEncoder.sh stop

echo "mmsEngineService"
~/mmsEngineService.sh stop

echo "tomcat"
~/tomcat.sh stop

echo "nginx"
~/nginx.sh stop


echo "mmsStatus to check all is down"
~/mmsStatusALL.sh

date
