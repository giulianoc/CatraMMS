#!/bin/bash

# Scarica, compila e installa libwebm in $DEPLOY_DIR/ffmpeg
# (come tutte le librerie usate da ffmpeg: il pacchetto ffmpeg installato sui server le contiene)
# Repository: https://chromium.googlesource.com/webm/libwebm (viene usato il tag dell'ultima release)

set -e

version=libwebm-1.0.0.32
name=libwebm

workDir=$(mktemp -d)
cd "$workDir"

git clone --depth 1 --branch $version https://chromium.googlesource.com/webm/libwebm $name
cd $name

cmake -DCMAKE_INSTALL_PREFIX="$DEPLOY_DIR/ffmpeg" .
make -j "$(nproc)"
make install

cd /
rm -rf "$workDir"
