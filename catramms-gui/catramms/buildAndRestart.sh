#!/bin/bash

/usr/local/apache-tomcat/bin/shutdown.sh && ./build.sh && rm -rf /usr/local/apache-tomcat/webapps/catramms* && cp ./catramms/target/catramms.war /usr/local/apache-tomcat/webapps && /usr/local/apache-tomcat/bin/startup.sh
