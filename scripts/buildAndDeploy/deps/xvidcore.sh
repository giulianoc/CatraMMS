#!/bin/bash

# Scarica, compila e installa xvidcore in $DEPLOY_DIR/ffmpeg
# (come tutte le librerie usate da ffmpeg: il pacchetto ffmpeg installato sui server le contiene)
# Ultima versione: https://labs.xvid.com/source

set -e

version=1.3.7
name=xvidcore

workDir=$(mktemp -d)
cd "$workDir"

wget -q https://downloads.xvid.com/downloads/$name-$version.tar.gz
# bsdtar invece di GNU tar: vedi opencore-amr.sh
bsdtar xzf $name-$version.tar.gz
cd $name/build/generic

./configure --prefix="$DEPLOY_DIR/ffmpeg"
make -j "$(nproc)"
make install

cd /
rm -rf "$workDir"
