#!/bin/bash

curl -k -v -X POST -d @./cutInfo.json -H "Content-Type: application/json" https://mms-gui.catrasoft.cloud/catramms/rest/api_rsi_v1/cutMedia
