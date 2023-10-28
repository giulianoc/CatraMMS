#!/bin/bash

if [ $# -ne 1 ]
then
        echo "Usage $0 micro-service-name"

        exit
fi

currentDir=$(pwd)

cd /opt/catramms

serviceFileName=$1

if [ -d "$serviceFileName-0.1" ]; then
        rm -rf $serviceFileName-0.1
fi

#remove source
if [ -d "$serviceFileName" ]; then
        rm -rf $serviceFileName
fi

echo "write the bitbucket pwd"
git clone https://giuliano_catrambone@bitbucket.org/giuliano_catrambone/$serviceFileName.git

cd $serviceFileName

./gradlew assemble

cp "build/distributions/$serviceFileName-0.1.tar" ../

cd ..

~/mmsStopALL.sh

rm -rf "$serviceFileName-0.1"
tar -xvf "$serviceFileName-0.1.tar"
rm -rf "$serviceFileName-0.1.tar"

~/mmsStartALL.sh

cd $currentDir

