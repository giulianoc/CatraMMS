#!/bin/bash

pgrep -f "mmsCloudLoop.sh prod" > /dev/null
if [ $? -ne 0 ]; then
  TELEGRAM_GROUPALARMS_BOT_TOKEN=6212616142:AAGeDfAYfKK0S4O2LixolBpb11yppWymLlY TELEGRAM_GROUPALARMS_ID=-1001837678428 nohup /opt/catrasoftware/CatraMMS/scripts/mmsCloudLoop.sh prod > /dev/null &
fi;

pgrep -f "mmsCloudLoop.sh test" > /dev/null
if [ $? -ne 0 ]; then
  TELEGRAM_GROUPALARMS_BOT_TOKEN=6212616142:AAGeDfAYfKK0S4O2LixolBpb11yppWymLlY TELEGRAM_GROUPALARMS_ID=-1001837678428 nohup /opt/catrasoftware/CatraMMS/scripts/mmsCloudLoop.sh test > /dev/null &
fi;

