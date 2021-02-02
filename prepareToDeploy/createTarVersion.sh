#!/bin/bash

date

deployDirectory=/opt/catrasoftware/deploy

RED='\033[0;31m'
NC='\033[0m' # No Color

version=$(cat ./version.txt)

currentDir=$(pwd)
moduleName=$(basename $currentDir)

cd $deployDirectory
tarFileName=$moduleName-$version-ubuntu.tar.gz

rm -rf $moduleName-$version
cp -r $moduleName $moduleName-$version
tar cvfz $tarFileName $moduleName-$version
rm -rf $moduleName-$version

cd $currentDir

echo ""
printf "${RED}"
echo "$tarFileName ready"
printf "${NC}"

