#!/bin/bash

echo "chmod .sh"
chmod u+x /opt/catramms/CatraMMS/etc/*.sh

/opt/catramms/CatraMMS/etc/api.rsi.sh stop
/opt/catramms/CatraMMS/etc/encoder.rsi.sh stop
/opt/catramms/CatraMMS/etc/mmsEngineService.rsi.sh stop
/opt/catramms/CatraMMS/etc/tomcat.sh stop

