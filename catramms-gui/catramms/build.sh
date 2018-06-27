#!/bin/bash

. ~/.profile

mvn clean
mvn --projects catraComponents compile install
if [ $? -ne 0 ]; then
  exit 1
fi

mkdir -p catraComponents/target/jar/META-INF/resources/media

# Copy only the files (.xhtml)
find catraComponents/src/main/webapp/resources/media -maxdepth 1 -type f -exec cp {} catraComponents/target/jar/META-INF/resources/media \;

cp catraComponents/src/main/webapp/resources/faces-config.xml catraComponents/target/jar/META-INF
#cp catraComponents/src/main/webapp/resources/catra.taglib.xml catraComponents/target/jar/META-INF

cp -r catraComponents/target/classes/com catraComponents/target/jar

cd catraComponents/target/jar
jar -cvf catraComponents.jar *
cd ../../..

mvn install:install-file -Dfile=catraComponents/target/jar/catraComponents.jar -DgroupId=com.catra -DartifactId=catraComponents -Dversion=1.0.0-SNAPSHOT -Dpackaging=jar
if [ $? -ne 0 ]; then
  exit 1
fi

mvn --projects catramms clean compile install
if [ $? -ne 0 ]; then
  exit 1
fi

echo "Delivery: ./catramms/target/catramms.war"

exit 0
