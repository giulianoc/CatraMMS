#!/bin/bash

date

deployDirectory=/opt/catrasoftware/deploy

RED='\033[0;31m'
NC='\033[0m' # No Color

version=$(cat ./version.txt)

currentDir=$(pwd)
moduleName=$(basename $currentDir)

linuxName=$(cat /etc/os-release | grep "^ID=" | cut -d'=' -f2)
#linuxName using centos will be "centos", next remove "
linuxName=$(echo $linuxName | awk '{ if (substr($0, 0, 1) == "\"") printf("%s", substr($0, 2, length($0) - 2)); else printf("%s", $0) }')

cd $deployDirectory
tarFileName=$moduleName-$version-$linuxName.tar.gz

rm -rf $moduleName-$version
cp -r $moduleName $moduleName-$version
tar cvfz $tarFileName $moduleName-$version
rm -rf $moduleName-$version

cd $currentDir

echo ""
printf "${RED}"
echo "$tarFileName ready"
printf "${NC}"

