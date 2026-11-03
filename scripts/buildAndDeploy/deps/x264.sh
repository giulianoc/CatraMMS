#!/bin/bash

# Scarica, compila e installa x264 in $DEPLOY_DIR/ffmpeg
# (come tutte le librerie usate da ffmpeg: il pacchetto ffmpeg installato sui server le contiene)
# Repository: https://code.videolan.org/videolan/x264.git
# x264 non ha tag di release: viene usato un commit fissato del branch stable, così la build è riproducibile.
# Per aggiornare: git ls-remote https://code.videolan.org/videolan/x264.git refs/heads/stable
# Richiede nasm per le ottimizzazioni assembler x86_64

set -e

# branch stable, commit del 2025-06-08
version=b35605ace3ddf7c1a5d67a2eb553f034aef41d55
name=x264

workDir=$(mktemp -d)
cd "$workDir"

# git clone --branch non accetta un commit: si scarica solo quel commit con fetch
git init -q $name
cd $name
git fetch -q --depth 1 https://code.videolan.org/videolan/x264.git $version
git checkout -q FETCH_HEAD

./configure --prefix="$DEPLOY_DIR/ffmpeg" --enable-shared --enable-pic
make -j "$(nproc)"
make install

cd /
rm -rf "$workDir"
