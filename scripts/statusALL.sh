#!/bin/bash

echo "MMS API status"
/opt/catramms/CatraMMS/scripts/api.sh status

echo "MMS Encoder status"
/opt/catramms/CatraMMS/scripts/encoder.sh status

echo "MMS Engine status"
/opt/catramms/CatraMMS/scripts/mmsEngineService.sh status

echo "Tomcat status"
/opt/catramms/CatraMMS/scripts/tomcat.sh status

