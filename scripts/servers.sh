#!/bin/bash

declare -a testServers
serverIndex=0
testServers[$((serverIndex*5+0))]=hetzner-test-api-2
testServers[$((serverIndex*5+1))]=168.119.250.162
testServers[$((serverIndex*5+2))]=hetzner-mms-key
testServers[$((serverIndex*5+3))]=9255
testServers[$((serverIndex*5+4))]=api-and-delivery

serverIndex=$((serverIndex+1))
testServers[$((serverIndex*5+0))]=hetzner-test-engine-db-1
testServers[$((serverIndex*5+1))]=159.69.251.50
testServers[$((serverIndex*5+2))]=hetzner-mms-key
testServers[$((serverIndex*5+3))]=9255
testServers[$((serverIndex*5+4))]=engine

serverIndex=$((serverIndex+1))
testServers[$((serverIndex*5+0))]=hetzner-test-engine-db-5
testServers[$((serverIndex*5+1))]=168.119.105.223
testServers[$((serverIndex*5+2))]=hetzner-mms-key
testServers[$((serverIndex*5+3))]=9255
testServers[$((serverIndex*5+4))]=engine

serverIndex=$((serverIndex+1))
testServers[$((serverIndex*5+0))]=hetzner-test-transcoder-1
testServers[$((serverIndex*5+1))]=148.251.187.41
testServers[$((serverIndex*5+2))]=hetzner-mms-key
testServers[$((serverIndex*5+3))]=9255
testServers[$((serverIndex*5+4))]=encoder

testServersNumber=$((serverIndex+1))

declare -a prodServers
serverIndex=0
prodServers[$((serverIndex*5+0))]=hetzner-api-3
prodServers[$((serverIndex*5+1))]=178.63.22.93
prodServers[$((serverIndex*5+2))]=hetzner-mms-key
prodServers[$((serverIndex*5+3))]=9255
prodServers[$((serverIndex*5+4))]=api

serverIndex=$((serverIndex+1))
prodServers[$((serverIndex*5+0))]=hetzner-api-4
prodServers[$((serverIndex*5+1))]=78.46.101.27
prodServers[$((serverIndex*5+2))]=hetzner-mms-key
prodServers[$((serverIndex*5+3))]=9255
prodServers[$((serverIndex*5+4))]=api

serverIndex=$((serverIndex+1))
prodServers[$((serverIndex*5+0))]=hetzner-delivery-binary-gui-2
prodServers[$((serverIndex*5+1))]=116.202.53.105
prodServers[$((serverIndex*5+2))]=hetzner-mms-key
prodServers[$((serverIndex*5+3))]=9255
prodServers[$((serverIndex*5+4))]=delivery

serverIndex=$((serverIndex+1))
prodServers[$((serverIndex*5+0))]=hetzner-delivery-binary-gui-3
prodServers[$((serverIndex*5+1))]=116.202.172.245
prodServers[$((serverIndex*5+2))]=hetzner-mms-key
prodServers[$((serverIndex*5+3))]=9255
prodServers[$((serverIndex*5+4))]=delivery

serverIndex=$((serverIndex+1))
prodServers[$((serverIndex*5+0))]=hetzner-engine-db-2
prodServers[$((serverIndex*5+1))]=162.55.245.36
prodServers[$((serverIndex*5+2))]=hetzner-mms-key
prodServers[$((serverIndex*5+3))]=9255
prodServers[$((serverIndex*5+4))]=engine

serverIndex=$((serverIndex+1))
prodServers[$((serverIndex*5+0))]=hetzner-engine-db-3
prodServers[$((serverIndex*5+1))]=167.235.14.105
prodServers[$((serverIndex*5+2))]=hetzner-mms-key
prodServers[$((serverIndex*5+3))]=9255
prodServers[$((serverIndex*5+4))]=engine

serverIndex=$((serverIndex+1))
prodServers[$((serverIndex*5+0))]=hetzner-engine-db-4
prodServers[$((serverIndex*5+1))]=116.202.118.71
prodServers[$((serverIndex*5+2))]=hetzner-mms-key
prodServers[$((serverIndex*5+3))]=9255
prodServers[$((serverIndex*5+4))]=engine

serverIndex=$((serverIndex+1))
prodServers[$((serverIndex*5+0))]=cibortv-transcoder-4
prodServers[$((serverIndex*5+1))]=93.58.249.102
prodServers[$((serverIndex*5+2))]=cibortv-transcoder-4
prodServers[$((serverIndex*5+3))]=9255
prodServers[$((serverIndex*5+4))]=externalEncoder

serverIndex=$((serverIndex+1))
prodServers[$((serverIndex*5+0))]=hetzner-transcoder-1
prodServers[$((serverIndex*5+1))]=162.55.235.245
prodServers[$((serverIndex*5+2))]=hetzner-mms-key
prodServers[$((serverIndex*5+3))]=9255
prodServers[$((serverIndex*5+4))]=encoder

serverIndex=$((serverIndex+1))
prodServers[$((serverIndex*5+0))]=hetzner-transcoder-2
prodServers[$((serverIndex*5+1))]=136.243.34.218
prodServers[$((serverIndex*5+2))]=hetzner-mms-key
prodServers[$((serverIndex*5+3))]=9255
prodServers[$((serverIndex*5+4))]=encoder

serverIndex=$((serverIndex+1))
prodServers[$((serverIndex*5+0))]=hetzner-transcoder-5
prodServers[$((serverIndex*5+1))]=46.4.98.135
prodServers[$((serverIndex*5+2))]=hetzner-mms-key
prodServers[$((serverIndex*5+3))]=9255
prodServers[$((serverIndex*5+4))]=encoder

serverIndex=$((serverIndex+1))
prodServers[$((serverIndex*5+0))]=aws-cibortv-transcoder-mil-1
prodServers[$((serverIndex*5+1))]=ec2-15-161-78-89.eu-south-1.compute.amazonaws.com
prodServers[$((serverIndex*5+2))]=aws-cibortv1-key-milan
prodServers[$((serverIndex*5+3))]=22
prodServers[$((serverIndex*5+4))]=externalEncoder

serverIndex=$((serverIndex+1))
prodServers[$((serverIndex*5+0))]=aws-cibortv-transcoder-mil-2
prodServers[$((serverIndex*5+1))]=ec2-35-152-80-3.eu-south-1.compute.amazonaws.com
prodServers[$((serverIndex*5+2))]=aws-cibortv1-key-milan
prodServers[$((serverIndex*5+3))]=22
prodServers[$((serverIndex*5+4))]=externalEncoder

serverIndex=$((serverIndex+1))
prodServers[$((serverIndex*5+0))]=aruba-mms-transcoder-2
prodServers[$((serverIndex*5+1))]=ru001941.arubabiz.net
prodServers[$((serverIndex*5+2))]=cibortv-aruba
prodServers[$((serverIndex*5+3))]=9255
prodServers[$((serverIndex*5+4))]=externalEncoder

serverIndex=$((serverIndex+1))
prodServers[$((serverIndex*5+0))]=aruba-mms-transcoder-3
prodServers[$((serverIndex*5+1))]=ru002148.arubabiz.net
prodServers[$((serverIndex*5+2))]=cibortv-aruba
prodServers[$((serverIndex*5+3))]=9255
prodServers[$((serverIndex*5+4))]=externalEncoder

serverIndex=$((serverIndex+1))
prodServers[$((serverIndex*5+0))]=serverplan-mms-transcoder-1
prodServers[$((serverIndex*5+1))]=d02c0q-hdea.sphostserver.com
prodServers[$((serverIndex*5+2))]=cibortv-serverplan
prodServers[$((serverIndex*5+3))]=9255
prodServers[$((serverIndex*5+4))]=externalEncoder

serverIndex=$((serverIndex+1))
prodServers[$((serverIndex*5+0))]=serverplan-mms-transcoder-2
prodServers[$((serverIndex*5+1))]=d02c0q-hdea2.sphostserver.com
prodServers[$((serverIndex*5+2))]=cibortv-serverplan
prodServers[$((serverIndex*5+3))]=9255
prodServers[$((serverIndex*5+4))]=externalEncoder

serverIndex=$((serverIndex+1))
prodServers[$((serverIndex*5+0))]=aws-integration-1
prodServers[$((serverIndex*5+1))]=ec2-54-76-8-245.eu-west-1.compute.amazonaws.com
prodServers[$((serverIndex*5+2))]=aws-hdea-key-integration-ireland
prodServers[$((serverIndex*5+3))]=22
prodServers[$((serverIndex*5+4))]=integration

serverIndex=$((serverIndex+1))
prodServers[$((serverIndex*5+0))]=aws-integration-2
prodServers[$((serverIndex*5+1))]=ec2-18-202-82-214.eu-west-1.compute.amazonaws.com
prodServers[$((serverIndex*5+2))]=aws-hdea-key-integration-ireland
prodServers[$((serverIndex*5+3))]=22
prodServers[$((serverIndex*5+4))]=integration

serverIndex=$((serverIndex+1))
prodServers[$((serverIndex*5+0))]=aws-integration-3
prodServers[$((serverIndex*5+1))]=ec2-54-78-165-54.eu-west-1.compute.amazonaws.com
prodServers[$((serverIndex*5+2))]=aws-hdea-key-integration-ireland
prodServers[$((serverIndex*5+3))]=22
prodServers[$((serverIndex*5+4))]=integration

serverIndex=$((serverIndex+1))
prodServers[$((serverIndex*5+0))]=aws-integration-4
prodServers[$((serverIndex*5+1))]=ec2-63-34-124-54.eu-west-1.compute.amazonaws.com
prodServers[$((serverIndex*5+2))]=aws-hdea-key-integration-ireland
prodServers[$((serverIndex*5+3))]=22
prodServers[$((serverIndex*5+4))]=integration

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
