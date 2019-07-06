#!/bin/bash

if [ $# -ne 1 ]
then
        echo "Usage $0 engine | encoder | api"

        exit
fi

component=$1

lastLogFile=$(~/printLogFileName.sh $component)

echo "lastLogFile: $lastLogFile"
tail -f $lastLogFile


