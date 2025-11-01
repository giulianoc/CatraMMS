#!/bin/bash


#if [ $# -ne 1 ]
#then
#    echo "usage $0 <tag message>"
#	echo "Reminder to list tags: git tag -n --sort=taggerdate"
#
#    exit
#fi

deployDirectory=/opt/catrasoftware/deploy

currentDir=$(pwd)
moduleName=$(basename $currentDir)

RED='\033[0;31m'
NC='\033[0m' # No Color

echo ""
printf "${RED}"
echo "git pull"
printf "${NC}"
git submodule foreach "git fetch origin && git checkout main && git pull origin main"
git pull

echo ""
printf "${RED}"
read -n 1 -s -r -p "Press any key to continue making the project"
printf "${NC}"

make -j 8
printf "${RED}"
echo "rm -rf $deployDirectory/$moduleName"
printf "${NC}"
rm -rf $deployDirectory/$moduleName
read -n 1 -s -r -p "Press any key to continue"
make install

echo ""
printf "${RED}"
read -n 1 -s -r -p "Press any key to continue preparing the tar file"
printf "${NC}"

echo ""
printf "${RED}"
echo "./prepareToDeploy/createTarVersion.sh"
printf "${NC}"
./prepareToDeploy/createTarVersion.sh

