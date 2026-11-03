#!/bin/bash

# Scarica, compila e installa aws-sdk-cpp (core e medialive) in $DEPLOY_DIR/aws-sdk-cpp
# Repository: https://github.com/aws/aws-sdk-cpp (viene usato il tag dell'ultima release)
# Richiede libcurl4-openssl-dev, libssl-dev e zlib1g-dev

set -e

version=1.11.921
name=aws-sdk-cpp

workDir=$(mktemp -d)
cd "$workDir"

git clone --depth 1 --branch $version --recurse-submodules --shallow-submodules \
	https://github.com/aws/aws-sdk-cpp $name
mkdir $name.build
cd $name.build

# core viene sempre compilato perchè è richiesto da ogni servizio: basta un solo passaggio con medialive.
# ENABLE_TESTING=OFF: altrimenti i test unitari vengono compilati ed eseguiti alla fine della compilazione
cmake ../$name -DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_PREFIX_PATH="$DEPLOY_DIR/$name" -DCMAKE_INSTALL_PREFIX="$DEPLOY_DIR/$name" \
	-DBUILD_ONLY="medialive" -DENABLE_TESTING=OFF
make -j "$(nproc)"
make install

cd /
rm -rf "$workDir"
