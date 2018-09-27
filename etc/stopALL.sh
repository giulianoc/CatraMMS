#!/bin/bash

echo "chmod .sh"
chmod u+x /opt/catramms/CatraMMS/etc/*.sh

/opt/catramms/CatraMMS/etc/api.sh stop
/opt/catramms/CatraMMS/etc/encoder.sh stop
/opt/catramms/CatraMMS/etc/mmsEngineService.sh stop
/opt/catramms/CatraMMS/etc/tomcat.sh stop

