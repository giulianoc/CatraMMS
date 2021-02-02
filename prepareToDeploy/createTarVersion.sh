#!/bin/bash

date

version=$(cat ./version.txt)

currentDir=$(pwd)
moduleName=$(basename $currentDir)

cd /opt/catrasoftware/deploy
tarFileName=$moduleName-$version-ubuntu.tar.gz

rm -rf $moduleName-$version
cp -r $moduleName $moduleName-$version
tar cvfz $tarFileName $moduleName-$version
rm -rf $moduleName-$version

cd $currentDir

echo ""
echo "$tarFileName ready"

