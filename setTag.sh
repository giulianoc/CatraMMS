#!/bin/bash

if [ $# -ne 1 ]
then
    echo "usage $0 <version tag, i.e.: v1.0.0>

    exit
fi

tagName=$1

echo "git tag -a $tagName -m \"Version $tagName\""
#git tag -a $tagName -m "Version $tagName"

echo "echo \"$tagName\" > version.txt"
#echo "$tagName" > version.txt

