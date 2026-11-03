#!/bin/bash

# Scarica, compila e installa libaom in $DEPLOY_DIR/ffmpeg
# (come tutte le librerie usate da ffmpeg: il pacchetto ffmpeg installato sui server le contiene)
# Repository: https://aomedia.googlesource.com/aom (viene usato il tag dell'ultima release)
# Richiede nasm per le ottimizzazioni assembler x86_64

set -e

version=v3.15.2
name=libaom

workDir=$(mktemp -d)
cd "$workDir"

git clone --depth 1 --branch $version https://aomedia.googlesource.com/aom aom
mkdir -p aom_build
cd aom_build

# ENABLE_TESTS=0: i test unitari non servono e ne raddoppierebbero il tempo di compilazione
cmake -DCMAKE_INSTALL_PREFIX="$DEPLOY_DIR/ffmpeg" -DBUILD_SHARED_LIBS=1 -DENABLE_TESTS=0 ../aom
make -j "$(nproc)"
make install

cd /
rm -rf "$workDir"
