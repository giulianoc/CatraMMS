#!/bin/bash

declare -a testServers
serverIndex=0
testServers[$((serverIndex*4+0))]=hetzner-test-api-1
testServers[$((serverIndex*4+1))]=138.201.245.228
testServers[$((serverIndex*4+2))]=hetzner-mms-key
testServers[$((serverIndex*4+3))]=9255

serverIndex=$((serverIndex+1))
testServers[$((serverIndex*4+0))]=hetzner-test-engine-db-2
testServers[$((serverIndex*4+1))]=49.12.74.56
testServers[$((serverIndex*4+2))]=hetzner-mms-key
testServers[$((serverIndex*4+3))]=9255

serverIndex=$((serverIndex+1))
testServers[$((serverIndex*4+0))]=hetzner-test-engine-db-3
testServers[$((serverIndex*4+1))]=142.132.232.196
testServers[$((serverIndex*4+2))]=hetzner-mms-key
testServers[$((serverIndex*4+3))]=9255

serverIndex=$((serverIndex+1))
testServers[$((serverIndex*4+0))]=hetzner-test-transcoder-2
testServers[$((serverIndex*4+1))]=88.198.47.118
testServers[$((serverIndex*4+2))]=hetzner-mms-key
testServers[$((serverIndex*4+3))]=9255

testServersNumber=$((serverIndex+1))

declare -a prodServers
serverIndex=0
prodServers[$((serverIndex*4+0))]=hetzner-api-1
prodServers[$((serverIndex*4+1))]=159.69.251.50
prodServers[$((serverIndex*4+2))]=hetzner-mms-key
prodServers[$((serverIndex*4+3))]=9255

serverIndex=$((serverIndex+1))
prodServers[$((serverIndex*4+0))]=hetzner-api-2
prodServers[$((serverIndex*4+1))]=168.119.250.162
prodServers[$((serverIndex*4+2))]=hetzner-mms-key
prodServers[$((serverIndex*4+3))]=9255

serverIndex=$((serverIndex+1))
prodServers[$((serverIndex*4+0))]=hetzner-delivery-binary-gui-1
prodServers[$((serverIndex*4+1))]=5.9.57.85
prodServers[$((serverIndex*4+2))]=hetzner-mms-key
prodServers[$((serverIndex*4+3))]=9255

serverIndex=$((serverIndex+1))
prodServers[$((serverIndex*4+0))]=hetzner-delivery-binary-gui-5
prodServers[$((serverIndex*4+1))]=136.243.35.105
prodServers[$((serverIndex*4+2))]=hetzner-mms-key
prodServers[$((serverIndex*4+3))]=9255

serverIndex=$((serverIndex+1))
prodServers[$((serverIndex*4+0))]=hetzner-engine-db-1
prodServers[$((serverIndex*4+1))]=167.235.14.44
prodServers[$((serverIndex*4+2))]=hetzner-mms-key
prodServers[$((serverIndex*4+3))]=9255

serverIndex=$((serverIndex+1))
prodServers[$((serverIndex*4+0))]=hetzner-engine-db-3
prodServers[$((serverIndex*4+1))]=167.235.14.105
prodServers[$((serverIndex*4+2))]=hetzner-mms-key
prodServers[$((serverIndex*4+3))]=9255

serverIndex=$((serverIndex+1))
prodServers[$((serverIndex*4+0))]=cibortv-transcoder-4
prodServers[$((serverIndex*4+1))]=93.58.249.102
prodServers[$((serverIndex*4+2))]=cibortv-transcoder-4
prodServers[$((serverIndex*4+3))]=9255

serverIndex=$((serverIndex+1))
prodServers[$((serverIndex*4+0))]=hetzner-transcoder-1
prodServers[$((serverIndex*4+1))]=162.55.235.245
prodServers[$((serverIndex*4+2))]=hetzner-mms-key
prodServers[$((serverIndex*4+3))]=9255

serverIndex=$((serverIndex+1))
prodServers[$((serverIndex*4+0))]=hetzner-transcoder-2
prodServers[$((serverIndex*4+1))]=136.243.34.218
prodServers[$((serverIndex*4+2))]=hetzner-mms-key
prodServers[$((serverIndex*4+3))]=9255

serverIndex=$((serverIndex+1))
prodServers[$((serverIndex*4+0))]=hetzner-transcoder-5
prodServers[$((serverIndex*4+1))]=46.4.98.135
prodServers[$((serverIndex*4+2))]=hetzner-mms-key
prodServers[$((serverIndex*4+3))]=9255

serverIndex=$((serverIndex+1))
prodServers[$((serverIndex*4+0))]=aws-cibortv-transcoder-mil-1
prodServers[$((serverIndex*4+1))]=ec2-15-161-78-89.eu-south-1.compute.amazonaws.com
prodServers[$((serverIndex*4+2))]=aws-cibortv1-key-milan
prodServers[$((serverIndex*4+3))]=22

serverIndex=$((serverIndex+1))
prodServers[$((serverIndex*4+0))]=aws-cibortv-transcoder-mil-2
prodServers[$((serverIndex*4+1))]=ec2-35-152-80-3.eu-south-1.compute.amazonaws.com
prodServers[$((serverIndex*4+2))]=aws-cibortv1-key-milan
prodServers[$((serverIndex*4+3))]=22

serverIndex=$((serverIndex+1))
prodServers[$((serverIndex*4+0))]=aruba-mms-transcoder-2
prodServers[$((serverIndex*4+1))]=ru001941.arubabiz.net
prodServers[$((serverIndex*4+2))]=cibortv-aruba
prodServers[$((serverIndex*4+3))]=9255

serverIndex=$((serverIndex+1))
prodServers[$((serverIndex*4+0))]=aruba-mms-transcoder-3
prodServers[$((serverIndex*4+1))]=ru002148.arubabiz.net
prodServers[$((serverIndex*4+2))]=cibortv-aruba
prodServers[$((serverIndex*4+3))]=9255

serverIndex=$((serverIndex+1))
prodServers[$((serverIndex*4+0))]=serverplan-mms-transcoder-1
prodServers[$((serverIndex*4+1))]=d02c0q-hdea.sphostserver.com
prodServers[$((serverIndex*4+2))]=cibortv-serverplan
prodServers[$((serverIndex*4+3))]=9255

serverIndex=$((serverIndex+1))
prodServers[$((serverIndex*4+0))]=serverplan-mms-transcoder-2
prodServers[$((serverIndex*4+1))]=d02c0q-hdea2.sphostserver.com
prodServers[$((serverIndex*4+2))]=cibortv-serverplan
prodServers[$((serverIndex*4+3))]=9255

prodServersNumber=$((serverIndex+1))

#index=0
#while [ $index -lt $testServersNumber ]
#do
#  serverName=${testServers[$((index*4+0))]}
#  serverAddress=${testServers[$((index*4+1))]}
#  serverKey=${testServers[$((index*4+2))]}
#  serverPort=${testServers[$((index*4+3))]}
#
#  echo $serverName
#  echo $serverAddress
#  echo $serverKey
#  echo $serverPort
#  echo ""
#
#  index=$((index+1))
#done


#index=0
#while [ $index -lt $prodServersNumber ]
#do
#  serverName=${prodServers[$((index*4+0))]}
#  serverAddress=${prodServers[$((index*4+1))]}
#  serverKey=${prodServers[$((index*4+2))]}
#  serverPort=${prodServers[$((index*4+3))]}
#
#  echo $serverName
#  echo $serverAddress
#  echo $serverKey
#  echo $serverPort
#  echo ""
#
#  index=$((index+1))
#done
#
