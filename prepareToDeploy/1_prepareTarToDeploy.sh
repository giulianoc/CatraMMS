#!/bin/bash


if [ $# -ne 2 ]
then
    echo "usage $0 <version tag, i.e.: 1.0.0> <tag message>"
	echo "Reminder to list tags: git tag -n --sort=taggerdate"

    exit
fi

tagName=$1
tagMessage=$2

deployDirectory=/opt/catrasoftware/deploy

currentDir=$(pwd)
moduleName=$(basename $currentDir)

RED='\033[0;31m'
NC='\033[0m' # No Color


echo ""
printf "${RED}"
echo "git commit -am $tagMessage"
printf "${NC}"
git commit -am "$tagMessage"

echo ""
printf "${RED}"
echo "./prepareToDeploy/setTag.sh $tagName $tagMessage"
printf "${NC}"
./prepareToDeploy/setTag.sh $tagName "$tagMessage"

echo ""
printf "${RED}"
echo "git commit -am $tagMessage"
printf "${NC}"
git commit -am "$tagMessage"

echo ""
printf "${RED}"
echo "git push https://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@github.com/giulianoc/CatraMMS.git"
printf "${NC}"
git push https://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@github.com/giulianoc/CatraMMS.git

echo ""
printf "${RED}"
read -n 1 -s -r -p "Press any key to continue making the project"
printf "${NC}"

make
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

