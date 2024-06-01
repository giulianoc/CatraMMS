#!/bin/bash

if [ $# -ne 1 -a $# -ne 2 ]
then
        echo "Usage $0 micro-service-name (i.e.: cibortv, catrammswebservices) conf in case of catrammswebservices (<empty>, cibortv, prod, test)"

        exit
fi

currentDir=$(pwd)

cd /opt/catramms

serviceFileName=$1
conf=$2

if [ -d "$serviceFileName-0.1" ]; then
        rm -rf $serviceFileName-0.1
fi

#remove source
if [ -d "$serviceFileName" ]; then
        rm -rf $serviceFileName
fi

echo "write the bitbucket pwd"
git clone https://giuliano_catrambone@bitbucket.org/giuliano_catrambone/$serviceFileName.git

if [ ! -d "$serviceFileName" ]; then
	echo "no service directory found: $serviceFileName"
	exit 1
fi

cd $serviceFileName

#next if is for the catrammswebservices microservice
if [[ "$conf" == *"test"* ]]; then
	if [ -f "./src/main/resources/application-test.yml" ]; then
		echo "application-test.yml is used"
		cp ./src/main/resources/application-test.yml ./src/main/resources/application.yml
	fi
elif [[ "$conf" == *"cibortv"* ]]; then
	if [ -f "./src/main/resources/application-cibortv.yml" ]; then
		echo "application-cibortv.yml is used"
		cp ./src/main/resources/application-cibortv.yml ./src/main/resources/application.yml
	fi
elif [[ "$conf" == *"prod"* ]]; then
	if [ -f "./src/main/resources/application-prod.yml" ]; then
		echo "application-prod.yml is used"
		cp ./src/main/resources/application-prod.yml ./src/main/resources/application.yml
	fi
else
	echo "application.yml is used"
fi
read -n 1 -s -r -p "premi un tasto"
echo ""

./gradlew assemble

cp "build/distributions/$serviceFileName-0.1.tar" ../

cd ..

~/mmsStopALL.sh
sleep 1

rm -rf "$serviceFileName-0.1"
tar -xvf "$serviceFileName-0.1.tar"
rm -rf "$serviceFileName-0.1.tar"

~/mmsStartALL.sh

cd $currentDir

