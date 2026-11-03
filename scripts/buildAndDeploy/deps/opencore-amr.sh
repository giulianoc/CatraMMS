#!/bin/bash

# Scarica, compila e installa opencore-amr in $DEPLOY_DIR/ffmpeg
# (come tutte le librerie usate da ffmpeg: il pacchetto ffmpeg installato sui server le contiene)
# Ultima versione: https://sourceforge.net/projects/opencore-amr/files/opencore-amr

set -e

version=0.1.6
name=opencore-amr

workDir=$(mktemp -d)
cd "$workDir"

wget -q -O $name-$version.tar.gz \
	"https://sourceforge.net/projects/opencore-amr/files/opencore-amr/$name-$version.tar.gz/download"
# bsdtar invece di GNU tar: su Ubuntu 26.04 x86_64 in emulazione (Mac Apple Silicon) GNU tar fallisce
# con "Cannot open: Function not implemented"
bsdtar xzf $name-$version.tar.gz
cd $name-$version

./configure --prefix="$DEPLOY_DIR/ffmpeg"
make -j "$(nproc)"
make install

cd /
rm -rf "$workDir"
