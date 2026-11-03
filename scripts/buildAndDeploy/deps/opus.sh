#!/bin/bash

# Scarica, compila e installa opus in $DEPLOY_DIR/ffmpeg
# (come tutte le librerie usate da ffmpeg: il pacchetto ffmpeg installato sui server le contiene)
# Ultima versione: https://opus-codec.org/downloads (https://downloads.xiph.org/releases/opus)

set -e

version=1.6.1
name=opus

workDir=$(mktemp -d)
cd "$workDir"

wget -q https://downloads.xiph.org/releases/opus/$name-$version.tar.gz
# bsdtar invece di GNU tar: vedi opencore-amr.sh
bsdtar xzf $name-$version.tar.gz
cd $name-$version

./configure --prefix="$DEPLOY_DIR/ffmpeg"
make -j "$(nproc)"
make install

cd /
rm -rf "$workDir"
