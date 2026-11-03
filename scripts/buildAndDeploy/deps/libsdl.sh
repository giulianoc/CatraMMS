#!/bin/bash

# Scarica, compila e installa SDL2 in $DEPLOY_DIR/ffmpeg
# (come tutte le librerie usate da ffmpeg: il pacchetto ffmpeg installato sui server le contiene)
# Ultima versione: https://www.libsdl.org (https://www.libsdl.org/release)
# viene usata SDL2 e non SDL3 perchè ffmpeg (ffplay) usa SDL2 e SDL3 non ha più il configure (solo cmake)

set -e

version=2.32.10
name=libsdl

workDir=$(mktemp -d)
cd "$workDir"

wget -q https://www.libsdl.org/release/SDL2-$version.tar.gz
# bsdtar invece di GNU tar: vedi opencore-amr.sh
bsdtar xzf SDL2-$version.tar.gz
cd SDL2-$version

./configure --prefix="$DEPLOY_DIR/ffmpeg"
make -j "$(nproc)"
make install

cd /
rm -rf "$workDir"
