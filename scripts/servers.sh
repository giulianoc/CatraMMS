#!/bin/bash

declare -a testServers
serverIndex=0
testServers[$((serverIndex*6+0))]=hetzner-test-api-2
testServers[$((serverIndex*6+1))]=168.119.250.162
testServers[$((serverIndex*6+2))]=hetzner-mms-key
testServers[$((serverIndex*6+3))]=9255
testServers[$((serverIndex*6+4))]=api-and-delivery
testServers[$((serverIndex*6+5))]=10.0.0.5

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
prodServers[$((serverIndex*6+0))]=hetzner-engine-db-5
prodServers[$((serverIndex*6+1))]=5.9.81.10
prodServers[$((serverIndex*6+2))]=hetzner-mms-key
prodServers[$((serverIndex*6+3))]=9255
prodServers[$((serverIndex*6+4))]=engine
prodServers[$((serverIndex*6+5))]=10.0.1.5

#serverIndex=$((serverIndex+1))
#prodServers[$((serverIndex*6+0))]=cibortv-encoder-4
#prodServers[$((serverIndex*6+1))]=93.58.249.102
#prodServers[$((serverIndex*6+2))]=cibortv-transcoder-4
#prodServers[$((serverIndex*6+3))]=9255
#prodServers[$((serverIndex*6+4))]=externalEncoder
#prodServers[$((serverIndex*6+5))]=

#serverIndex=$((serverIndex+1))
#prodServers[$((serverIndex*6+0))]=hetzner-encoder-1
#prodServers[$((serverIndex*6+1))]=157.90.129.10
#prodServers[$((serverIndex*6+2))]=hetzner-mms-key
#prodServers[$((serverIndex*6+3))]=9255
#prodServers[$((serverIndex*6+4))]=encoder
#prodServers[$((serverIndex*6+5))]=10.0.1.16

serverIndex=$((serverIndex+1))
prodServers[$((serverIndex*6+0))]=hetzner-encoder-3
prodServers[$((serverIndex*6+1))]=23.88.12.230
prodServers[$((serverIndex*6+2))]=hetzner-mms-key
prodServers[$((serverIndex*6+3))]=9255
prodServers[$((serverIndex*6+4))]=encoder
prodServers[$((serverIndex*6+5))]=10.0.1.2

serverIndex=$((serverIndex+1))
prodServers[$((serverIndex*6+0))]=hetzner-encoder-4
prodServers[$((serverIndex*6+1))]=23.88.12.229
prodServers[$((serverIndex*6+2))]=hetzner-mms-key
prodServers[$((serverIndex*6+3))]=9255
prodServers[$((serverIndex*6+4))]=encoder
prodServers[$((serverIndex*6+5))]=10.0.1.9

serverIndex=$((serverIndex+1))
prodServers[$((serverIndex*6+0))]=hetzner-encoder-6
prodServers[$((serverIndex*6+1))]=23.88.13.17
prodServers[$((serverIndex*6+2))]=hetzner-mms-key
prodServers[$((serverIndex*6+3))]=9255
prodServers[$((serverIndex*6+4))]=encoder
prodServers[$((serverIndex*6+5))]=10.0.1.15

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
