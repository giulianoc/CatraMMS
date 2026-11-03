#!/bin/bash

# Scarica, compila e installa fdk-aac in $DEPLOY_DIR/ffmpeg
# (come tutte le librerie usate da ffmpeg: il pacchetto ffmpeg installato sui server le contiene)
# Ultima versione: https://sourceforge.net/projects/opencore-amr/files/fdk-aac

set -e

version=3.0.0
name=fdk-aac

workDir=$(mktemp -d)
cd "$workDir"

wget -q -O $name-$version.tar.gz \
	"https://sourceforge.net/projects/opencore-amr/files/fdk-aac/$name-$version.tar.gz/download"
# bsdtar invece di GNU tar: vedi opencore-amr.sh
bsdtar xzf $name-$version.tar.gz
cd $name-$version

./configure --prefix="$DEPLOY_DIR/ffmpeg"
make -j "$(nproc)"
make install

cd /
rm -rf "$workDir"
