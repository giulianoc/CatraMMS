#!/bin/bash

echo "chmod .sh"
chmod u+x /home/mms/catramms/CatraMMS/etc/*.sh

./api.sh stop
./encoder.sh stop
./mmsEngineService.sh stop
./tomcat.sh stop

