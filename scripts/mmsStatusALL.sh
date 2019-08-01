#!/bin/bash

echo "MMS API status"
~/mmsApi.sh status

echo "MMS Encoder status"
~/mmsEncoder.sh status

echo "MMS Engine status"
~/mmsEngineService.sh status

echo "Tomcat status"
~/tomcat.sh status

