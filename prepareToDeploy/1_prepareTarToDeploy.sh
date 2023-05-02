#!/bin/bash


if [ $# -ne 1 ]
then
    echo "usage $0 <tag message>"
	echo "Reminder to list tags: git tag -n --sort=taggerdate"

    exit
fi

#tagName=$1
tagMessage=$1


deployDirectory=/opt/catrasoftware/deploy

currentDir=$(pwd)
moduleName=$(basename $currentDir)


RED='\033[0;31m'
NC='\033[0m' # No Color


#calculate new version, i.e: 1.0.2345
versionpathname=$(dirname $0)
versionpathname=$versionpathname"/../version.txt"
#echo "versionpathname: $versionpathname"
incrementVersion=2
version=$(cat $versionpathname)
arrVersion=(${version//./ })
first=${arrVersion[0]}
second=${arrVersion[1]}
third=${arrVersion[2]}
newThird=$((third+$incrementVersion))
tagName=$first"."$second"."$newThird

echo ""
printf "${RED}"
read -n 1 -s -r -p "new tagname: $tagName, press any key to continue"
printf "${NC}"

echo ""
printf "${RED}"
echo "git pull --tags -f"
printf "${NC}"
git pull --tags -f

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
#githubToken=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
#echo "git push https://$githubToken@github.com/giulianoc/CatraMMS.git"
echo "git push https://github.com/giulianoc/CatraMMS.git"
printf "${NC}"
#git push https://$githubToken@github.com/giulianoc/CatraMMS.git
git push --tags https://github.com/giulianoc/CatraMMS.git
git push https://github.com/giulianoc/CatraMMS.git

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

