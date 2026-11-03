#!/bin/bash

# Scarica, compila e installa libvorbis in $DEPLOY_DIR/ffmpeg
# (come tutte le librerie usate da ffmpeg: il pacchetto ffmpeg installato sui server le contiene)
# Ultima versione: https://xiph.org/downloads (https://downloads.xiph.org/releases/vorbis)
# Dipende da libogg (deps/libogg.sh)

set -e

version=1.3.7
name=libvorbis

workDir=$(mktemp -d)
cd "$workDir"

wget -q https://downloads.xiph.org/releases/vorbis/$name-$version.tar.gz
# bsdtar invece di GNU tar: vedi opencore-amr.sh
bsdtar xzf $name-$version.tar.gz
cd $name-$version

./configure --prefix="$DEPLOY_DIR/ffmpeg" --with-ogg="$DEPLOY_DIR/ffmpeg"
make -j "$(nproc)"
make install

cd /
rm -rf "$workDir"
