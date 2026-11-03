#!/bin/bash

# Scarica, compila e installa libsrt in $DEPLOY_DIR/ffmpeg
# (come tutte le librerie usate da ffmpeg: il pacchetto ffmpeg installato sui server le contiene)
# Repository: https://github.com/Haivision/srt (viene usato il tag dell'ultima release)
# Richiede libssl-dev per la crittografia (ENABLE_ENCRYPTION, attiva di default)

set -e

version=v1.5.7
name=libsrt

workDir=$(mktemp -d)
cd "$workDir"

git clone --depth 1 --branch $version https://github.com/Haivision/srt.git srt
mkdir srt/build
cd srt/build

cmake -DCMAKE_INSTALL_PREFIX="$DEPLOY_DIR/ffmpeg" -DENABLE_STDCXX_SYNC=ON -DENABLE_SHARED=ON -DENABLE_STATIC=ON ..
make -j "$(nproc)"
make install

cd /
rm -rf "$workDir"
