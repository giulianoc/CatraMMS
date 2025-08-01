#!/bin/bash

declare -a testServers
serverIndex=0
testServers[$((serverIndex*6+0))]=hetzner-test-api-1
testServers[$((serverIndex*6+1))]=88.198.18.153
testServers[$((serverIndex*6+2))]=hetzner-mms-key
testServers[$((serverIndex*6+3))]=9255
testServers[$((serverIndex*6+4))]=api-and-delivery
testServers[$((serverIndex*6+5))]=10.0.1.17

serverIndex=$((serverIndex+1))
testServers[$((serverIndex*6+0))]=hetzner-test-engine-db-2
testServers[$((serverIndex*6+1))]=162.55.245.36
testServers[$((serverIndex*6+2))]=hetzner-mms-key
testServers[$((serverIndex*6+3))]=9255
testServers[$((serverIndex*6+4))]=engine
testServers[$((serverIndex*6+5))]=10.0.1.10

serverIndex=$((serverIndex+1))
testServers[$((serverIndex*6+0))]=hetzner-test-engine-db-4
testServers[$((serverIndex*6+1))]=116.202.118.71
testServers[$((serverIndex*6+2))]=hetzner-mms-key
testServers[$((serverIndex*6+3))]=9255
testServers[$((serverIndex*6+4))]=engine
testServers[$((serverIndex*6+5))]=10.0.1.11

serverIndex=$((serverIndex+1))
testServers[$((serverIndex*6+0))]=hetzner-test-encoder-1
testServers[$((serverIndex*6+1))]=148.251.187.41
testServers[$((serverIndex*6+2))]=hetzner-mms-key
testServers[$((serverIndex*6+3))]=9255
testServers[$((serverIndex*6+4))]=encoder
testServers[$((serverIndex*6+5))]=10.0.1.13

testServersNumber=$((serverIndex+1))

declare -a prodServers

serverIndex=0
prodServers[$((serverIndex*6+0))]=hetzner-api-2
prodServers[$((serverIndex*6+1))]=195.201.58.41
prodServers[$((serverIndex*6+2))]=hetzner-mms-key
prodServers[$((serverIndex*6+3))]=9255
prodServers[$((serverIndex*6+4))]=api
prodServers[$((serverIndex*6+5))]=10.0.1.8

serverIndex=$((serverIndex+1))
prodServers[$((serverIndex*6+0))]=hetzner-api-3
prodServers[$((serverIndex*6+1))]=178.63.22.93
prodServers[$((serverIndex*6+2))]=hetzner-mms-key
prodServers[$((serverIndex*6+3))]=9255
prodServers[$((serverIndex*6+4))]=api
prodServers[$((serverIndex*6+5))]=10.0.1.4

serverIndex=$((serverIndex+1))
prodServers[$((serverIndex*6+0))]=hetzner-api-4
prodServers[$((serverIndex*6+1))]=78.46.101.27
prodServers[$((serverIndex*6+2))]=hetzner-mms-key
prodServers[$((serverIndex*6+3))]=9255
prodServers[$((serverIndex*6+4))]=api
prodServers[$((serverIndex*6+5))]=10.0.1.12

serverIndex=$((serverIndex+1))
prodServers[$((serverIndex*6+0))]=hetzner-delivery-binary-gui-2
prodServers[$((serverIndex*6+1))]=116.202.53.105
prodServers[$((serverIndex*6+2))]=hetzner-mms-key
prodServers[$((serverIndex*6+3))]=9255
prodServers[$((serverIndex*6+4))]=delivery
prodServers[$((serverIndex*6+5))]=10.0.1.7

serverIndex=$((serverIndex+1))
prodServers[$((serverIndex*6+0))]=hetzner-delivery-binary-gui-3
prodServers[$((serverIndex*6+1))]=116.202.172.245
prodServers[$((serverIndex*6+2))]=hetzner-mms-key
prodServers[$((serverIndex*6+3))]=9255
prodServers[$((serverIndex*6+4))]=delivery
prodServers[$((serverIndex*6+5))]=10.0.1.14

serverIndex=$((serverIndex+1))
prodServers[$((serverIndex*6+0))]=usa-delivery-1
prodServers[$((serverIndex*6+1))]=194.42.206.8
prodServers[$((serverIndex*6+2))]=hetzner-mms-key
prodServers[$((serverIndex*6+3))]=9255
prodServers[$((serverIndex*6+4))]=delivery
prodServers[$((serverIndex*6+5))]=194.42.206.8

serverIndex=$((serverIndex+1))
prodServers[$((serverIndex*6+0))]=usa-delivery-2
prodServers[$((serverIndex*6+1))]=91.222.174.119
prodServers[$((serverIndex*6+2))]=hetzner-mms-key
prodServers[$((serverIndex*6+3))]=9255
prodServers[$((serverIndex*6+4))]=delivery
prodServers[$((serverIndex*6+5))]=91.222.174.119

serverIndex=$((serverIndex+1))
prodServers[$((serverIndex*6+0))]=usa-delivery-3
prodServers[$((serverIndex*6+1))]=91.222.174.80
prodServers[$((serverIndex*6+2))]=hetzner-mms-key
prodServers[$((serverIndex*6+3))]=9255
prodServers[$((serverIndex*6+4))]=delivery
prodServers[$((serverIndex*6+5))]=91.222.174.80

serverIndex=$((serverIndex+1))
prodServers[$((serverIndex*6+0))]=usa-delivery-4
prodServers[$((serverIndex*6+1))]=91.222.174.77
prodServers[$((serverIndex*6+2))]=hetzner-mms-key
prodServers[$((serverIndex*6+3))]=9255
prodServers[$((serverIndex*6+4))]=delivery
prodServers[$((serverIndex*6+5))]=91.222.174.77

serverIndex=$((serverIndex+1))
prodServers[$((serverIndex*6+0))]=usa-delivery-5
prodServers[$((serverIndex*6+1))]=91.222.174.97
prodServers[$((serverIndex*6+2))]=hetzner-mms-key
prodServers[$((serverIndex*6+3))]=9255
prodServers[$((serverIndex*6+4))]=delivery
prodServers[$((serverIndex*6+5))]=91.222.174.97

serverIndex=$((serverIndex+1))
prodServers[$((serverIndex*6+0))]=usa-delivery-6
prodServers[$((serverIndex*6+1))]=91.222.174.81
prodServers[$((serverIndex*6+2))]=hetzner-mms-key
prodServers[$((serverIndex*6+3))]=9255
prodServers[$((serverIndex*6+4))]=delivery
prodServers[$((serverIndex*6+5))]=91.222.174.81

serverIndex=$((serverIndex+1))
prodServers[$((serverIndex*6+0))]=usa-delivery-7
prodServers[$((serverIndex*6+1))]=194.42.196.8
prodServers[$((serverIndex*6+2))]=hetzner-mms-key
prodServers[$((serverIndex*6+3))]=9255
prodServers[$((serverIndex*6+4))]=delivery
prodServers[$((serverIndex*6+5))]=194.42.196.8

serverIndex=$((serverIndex+1))
prodServers[$((serverIndex*6+0))]=eu-delivery-1
prodServers[$((serverIndex*6+1))]=195.160.222.54
prodServers[$((serverIndex*6+2))]=hetzner-mms-key
prodServers[$((serverIndex*6+3))]=9255
prodServers[$((serverIndex*6+4))]=delivery
prodServers[$((serverIndex*6+5))]=192.168.0.1

serverIndex=$((serverIndex+1))
prodServers[$((serverIndex*6+0))]=eu-delivery-2
prodServers[$((serverIndex*6+1))]=195.160.222.53
prodServers[$((serverIndex*6+2))]=hetzner-mms-key
prodServers[$((serverIndex*6+3))]=9255
prodServers[$((serverIndex*6+4))]=delivery
prodServers[$((serverIndex*6+5))]=192.168.0.2

serverIndex=$((serverIndex+1))
prodServers[$((serverIndex*6+0))]=eu-delivery-3
prodServers[$((serverIndex*6+1))]=31.42.176.36
prodServers[$((serverIndex*6+2))]=hetzner-mms-key
prodServers[$((serverIndex*6+3))]=9255
prodServers[$((serverIndex*6+4))]=delivery
prodServers[$((serverIndex*6+5))]=192.168.0.3

serverIndex=$((serverIndex+1))
prodServers[$((serverIndex*6+0))]=hetzner-engine-db-1
prodServers[$((serverIndex*6+1))]=167.235.10.244
prodServers[$((serverIndex*6+2))]=hetzner-mms-key
prodServers[$((serverIndex*6+3))]=9255
prodServers[$((serverIndex*6+4))]=engine
prodServers[$((serverIndex*6+5))]=10.0.1.3

serverIndex=$((serverIndex+1))
prodServers[$((serverIndex*6+0))]=hetzner-engine-db-3
prodServers[$((serverIndex*6+1))]=116.202.81.159
prodServers[$((serverIndex*6+2))]=hetzner-mms-key
prodServers[$((serverIndex*6+3))]=9255
prodServers[$((serverIndex*6+4))]=engine
prodServers[$((serverIndex*6+5))]=10.0.1.6

serverIndex=$((serverIndex+1))
prodServers[$((serverIndex*6+0))]=hetzner-storage-1
prodServers[$((serverIndex*6+1))]=5.9.65.244
prodServers[$((serverIndex*6+2))]=hetzner-mms-key
prodServers[$((serverIndex*6+3))]=9255
prodServers[$((serverIndex*6+4))]=storage
prodServers[$((serverIndex*6+5))]=10.0.1.16


serverIndex=$((serverIndex+1))
prodServers[$((serverIndex*6+0))]=hetzner-encoder-7
prodServers[$((serverIndex*6+1))]=188.40.73.231
prodServers[$((serverIndex*6+2))]=hetzner-mms-key
prodServers[$((serverIndex*6+3))]=9255
prodServers[$((serverIndex*6+4))]=encoder
prodServers[$((serverIndex*6+5))]=10.0.1.21


serverIndex=$((serverIndex+1))
prodServers[$((serverIndex*6+0))]=aws-encoder-irl-1
prodServers[$((serverIndex*6+1))]=ec2-54-76-8-245.eu-west-1.compute.amazonaws.com
prodServers[$((serverIndex*6+2))]=aws-cibortv1-key-ireland
prodServers[$((serverIndex*6+3))]=22
prodServers[$((serverIndex*6+4))]=externalEncoder
prodServers[$((serverIndex*6+5))]=

serverIndex=$((serverIndex+1))
prodServers[$((serverIndex*6+0))]=aws-encoder-irl-2
prodServers[$((serverIndex*6+1))]=ec2-34-242-128-224.eu-west-1.compute.amazonaws.com
prodServers[$((serverIndex*6+2))]=aws-cibortv1-key-ireland
prodServers[$((serverIndex*6+3))]=22
prodServers[$((serverIndex*6+4))]=externalEncoder
prodServers[$((serverIndex*6+5))]=

serverIndex=$((serverIndex+1))
prodServers[$((serverIndex*6+0))]=aws-encoder-irl-3
prodServers[$((serverIndex*6+1))]=ec2-52-50-208-202.eu-west-1.compute.amazonaws.com
prodServers[$((serverIndex*6+2))]=aws-cibortv1-key-ireland
prodServers[$((serverIndex*6+3))]=22
prodServers[$((serverIndex*6+4))]=externalEncoder
prodServers[$((serverIndex*6+5))]=

serverIndex=$((serverIndex+1))
prodServers[$((serverIndex*6+0))]=aruba-mms-encoder-4
prodServers[$((serverIndex*6+1))]=ru002553.arubabiz.net
prodServers[$((serverIndex*6+2))]=cibortv-aruba
prodServers[$((serverIndex*6+3))]=9255
prodServers[$((serverIndex*6+4))]=externalEncoder
prodServers[$((serverIndex*6+5))]=

serverIndex=$((serverIndex+1))
prodServers[$((serverIndex*6+0))]=aruba-mms-encoder-5
prodServers[$((serverIndex*6+1))]=ru002554.arubabiz.net
prodServers[$((serverIndex*6+2))]=cibortv-aruba
prodServers[$((serverIndex*6+3))]=9255
prodServers[$((serverIndex*6+4))]=externalEncoder
prodServers[$((serverIndex*6+5))]=

serverIndex=$((serverIndex+1))
prodServers[$((serverIndex*6+0))]=serverplan-mms-encoder-3
prodServers[$((serverIndex*6+1))]=d02c0q-hdea3.sphostserver.com
prodServers[$((serverIndex*6+2))]=cibortv-serverplan
prodServers[$((serverIndex*6+3))]=9255
prodServers[$((serverIndex*6+4))]=externalEncoder
prodServers[$((serverIndex*6+5))]=

serverIndex=$((serverIndex+1))
prodServers[$((serverIndex*6+0))]=serverplan-mms-encoder-4
prodServers[$((serverIndex*6+1))]=d02c0q-hdea4.sphostserver.com
prodServers[$((serverIndex*6+2))]=cibortv-serverplan
prodServers[$((serverIndex*6+3))]=9255
prodServers[$((serverIndex*6+4))]=externalEncoder
prodServers[$((serverIndex*6+5))]=

serverIndex=$((serverIndex+1))
prodServers[$((serverIndex*6+0))]=aws-integration-5
prodServers[$((serverIndex*6+1))]=ec2-18-200-160-66.eu-west-1.compute.amazonaws.com
prodServers[$((serverIndex*6+2))]=aws-hdea-key-integration-ireland
prodServers[$((serverIndex*6+3))]=22
prodServers[$((serverIndex*6+4))]=integration
prodServers[$((serverIndex*6+5))]=

serverIndex=$((serverIndex+1))
prodServers[$((serverIndex*6+0))]=aws-integration-6
prodServers[$((serverIndex*6+1))]=ec2-63-34-124-54.eu-west-1.compute.amazonaws.com
prodServers[$((serverIndex*6+2))]=aws-hdea-key-integration-ireland
prodServers[$((serverIndex*6+3))]=22
prodServers[$((serverIndex*6+4))]=integration
prodServers[$((serverIndex*6+5))]=

serverIndex=$((serverIndex+1))
prodServers[$((serverIndex*6+0))]=aws-integration-7
prodServers[$((serverIndex*6+1))]=ec2-54-78-165-54.eu-west-1.compute.amazonaws.com
prodServers[$((serverIndex*6+2))]=aws-hdea-key-integration-ireland
prodServers[$((serverIndex*6+3))]=22
prodServers[$((serverIndex*6+4))]=integration
prodServers[$((serverIndex*6+5))]=

serverIndex=$((serverIndex+1))
prodServers[$((serverIndex*6+0))]=aws-integration-8
prodServers[$((serverIndex*6+1))]=ec2-18-202-82-214.eu-west-1.compute.amazonaws.com
prodServers[$((serverIndex*6+2))]=aws-hdea-key-integration-ireland
prodServers[$((serverIndex*6+3))]=22
prodServers[$((serverIndex*6+4))]=integration
prodServers[$((serverIndex*6+5))]=

serverIndex=$((serverIndex+1))
prodServers[$((serverIndex*6+0))]=aws-integration-9
prodServers[$((serverIndex*6+1))]=ec2-34-248-209-52.eu-west-1.compute.amazonaws.com
prodServers[$((serverIndex*6+2))]=aws-hdea-key-integration-ireland
prodServers[$((serverIndex*6+3))]=22
prodServers[$((serverIndex*6+4))]=integration
prodServers[$((serverIndex*6+5))]=

serverIndex=$((serverIndex+1))
prodServers[$((serverIndex*6+0))]=aws-integration-10
prodServers[$((serverIndex*6+1))]=ec2-52-51-54-221.eu-west-1.compute.amazonaws.com
prodServers[$((serverIndex*6+2))]=aws-hdea-key-integration-ireland
prodServers[$((serverIndex*6+3))]=22
prodServers[$((serverIndex*6+4))]=integration
prodServers[$((serverIndex*6+5))]=

serverIndex=$((serverIndex+1))
prodServers[$((serverIndex*6+0))]=aws-integration-11
prodServers[$((serverIndex*6+1))]=ec2-34-241-24-37.eu-west-1.compute.amazonaws.com
prodServers[$((serverIndex*6+2))]=aws-hdea-key-integration-ireland
prodServers[$((serverIndex*6+3))]=22
prodServers[$((serverIndex*6+4))]=integration
prodServers[$((serverIndex*6+5))]=

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
