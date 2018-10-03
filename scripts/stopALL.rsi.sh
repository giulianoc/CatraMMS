#!/bin/bash

echo "chmod .sh"
chmod u+x /opt/catramms/CatraMMS/scripts/*.sh

/opt/catramms/CatraMMS/scripts/api.rsi.sh stop
/opt/catramms/CatraMMS/scripts/encoder.rsi.sh stop
/opt/catramms/CatraMMS/scripts/mmsEngineService.rsi.sh stop
/opt/catramms/CatraMMS/scripts/tomcat.sh stop

