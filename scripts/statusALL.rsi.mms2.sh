#!/bin/bash

echo "MMS API status"
/opt/catramms/CatraMMS/scripts/mmsApi.rsi.mms2.sh status

echo "MMS Encoder status"
/opt/catramms/CatraMMS/scripts/mmsEncoder.rsi.mms2.sh status

echo "MMS Engine status"
/opt/catramms/CatraMMS/scripts/mmsEngineService.rsi.mms2.sh status

echo "Tomcat status"
/opt/catramms/CatraMMS/scripts/tomcat.sh status

