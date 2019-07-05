#!/bin/bash

echo "MMS API status"
/opt/catramms/CatraMMS/scripts/mmsApi.rsi.lab.sh status

echo "MMS Encoder status"
/opt/catramms/CatraMMS/scripts/mmsAncoder.rsi.lab.sh status

echo "MMS Engine status"
/opt/catramms/CatraMMS/scripts/mmsEngineService.rsi.lab.sh status

echo "Tomcat status"
/opt/catramms/CatraMMS/scripts/tomcat.sh status

