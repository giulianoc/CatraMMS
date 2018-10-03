#!/bin/bash

echo "chmod .sh"
chmod u+x /opt/catramms/CatraMMS/scripts/*.sh

/opt/catramms/CatraMMS/scripts/api.sh stop
/opt/catramms/CatraMMS/scripts/encoder.sh stop
/opt/catramms/CatraMMS/scripts/mmsEngineService.sh stop
/opt/catramms/CatraMMS/scripts/tomcat.sh stop

