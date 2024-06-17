#!/bin/bash

debugFilename=/tmp/servicesStatus.log

debug=1

source /opt/catrasoftware/CatraMMS/scripts/servicesStatusLibrary.sh

while [ 1 ]
do

	before=$(date +%s)
	echo "" >> $debugFilename

	echo "server_reachable hetzner-api-1" >> $debugFilename
	server_reachable 159.69.251.50 9255 hetzner-api-1

	echo "server_reachable hetzner-api-2" >> $debugFilename
	server_reachable 168.119.250.162 9255 hetzner-api-2

	echo "server_reachable hetzner-delivery-binary-gui-1" >> $debugFilename
	server_reachable 5.9.57.85 9255 hetzner-delivery-binary-gui-1

	echo "server_reachable hetzner-delivery-binary-gui-5" >> $debugFilename
	server_reachable 136.243.35.105 9255 hetzner-delivery-binary-gui-5

	echo "server_reachable hetzner-engine-db-1" >> $debugFilename
	server_reachable 167.235.14.44 9255 hetzner-engine-db-1

	echo "server_reachable hetzner-engine-db-3" >> $debugFilename
	server_reachable 167.235.14.105 9255 hetzner-engine-db-3

	echo "server_reachable cibortv-transcoder-4" >> $debugFilename
	server_reachable 93.58.249.102 9255 cibortv-transcoder-4

	echo "server_reachable hetzner-transcoder-1" >> $debugFilename
	server_reachable 162.55.235.245 9255 hetzner-transcoder-1

	echo "server_reachable hetzner-transcoder-2" >> $debugFilename
	server_reachable 136.243.34.218 9255 hetzner-transcoder-2

	echo "server_reachable hetzner-transcoder-5" >> $debugFilename
	server_reachable 46.4.98.135 9255 hetzner-transcoder-5

	echo "server_reachable aws-cibortv-transcoder-mil-1" >> $debugFilename
	server_reachable ec2-15-161-78-89.eu-south-1.compute.amazonaws.com 22 aws-cibortv-transcoder-mil-1

	echo "server_reachable aws-cibortv-transcoder-mil-2" >> $debugFilename
	server_reachable ec2-35-152-80-3.eu-south-1.compute.amazonaws.com 22 aws-cibortv-transcoder-mil-2

	echo "server_reachable aruba-mms-transcoder-1" >> $debugFilename
	server_reachable ru001940.arubabiz.net 9255 aruba-mms-transcoder-1

	echo "server_reachable aruba-mms-transcoder-2" >> $debugFilename
	server_reachable ru001941.arubabiz.net 9255 aruba-mms-transcoder-2

	echo "server_reachable aruba-mms-transcoder-3" >> $debugFilename
	server_reachable ru002148.arubabiz.net 9255 aruba-mms-transcoder-3

	after=$(date +%s)

	elapsed=$((after-before))

	secondsToSleep=60

	echo "" >> $debugFilename
	echo "$(date +'%Y/%m/%d %H:%M:%S'): script elapsed: $elapsed secs, sleeping $secondsToSleep secs" >> $debugFilename
	sleep $secondsToSleep
done

