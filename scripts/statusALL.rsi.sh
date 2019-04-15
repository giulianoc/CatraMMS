#!/bin/bash

echo "MMS API status"
/opt/catramms/CatraMMS/scripts/api.rsi.sh status

echo "MMS Encoder status"
/opt/catramms/CatraMMS/scripts/encoder.rsi.sh status

echo "MMS Engine status"
/opt/catramms/CatraMMS/scripts/mmsEngineService.rsi.sh status

echo "Tomcat status"
/opt/catramms/CatraMMS/scripts/tomcat.sh status

