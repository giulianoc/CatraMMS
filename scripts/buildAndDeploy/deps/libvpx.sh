#!/bin/bash

# Scarica, compila e installa libvpx in $DEPLOY_DIR/ffmpeg
# (come tutte le librerie usate da ffmpeg: il pacchetto ffmpeg installato sui server le contiene)
# Repository: https://chromium.googlesource.com/webm/libvpx (viene usato il tag dell'ultima release)
# Richiede nasm per le ottimizzazioni assembler x86_64

set -e

version=v1.17.0
name=libvpx

workDir=$(mktemp -d)
cd "$workDir"

git clone --depth 1 --branch $version https://chromium.googlesource.com/webm/libvpx $name
cd $name

./configure --prefix="$DEPLOY_DIR/ffmpeg" --enable-pic
make -j "$(nproc)"
make install

cd /
rm -rf "$workDir"
