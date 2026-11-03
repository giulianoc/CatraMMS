#!/bin/bash

# Scarica, compila e installa x265 in $DEPLOY_DIR/ffmpeg
# (come tutte le librerie usate da ffmpeg: il pacchetto ffmpeg installato sui server le contiene)
# Repository ufficiale: https://bitbucket.org/multicoreware/x265_git (viene usato il tag dell'ultima release)
# il mirror https://github.com/videolan/x265 non è aggiornato (fermo alla 3.5)
# Richiede nasm per le ottimizzazioni assembler x86_64

set -e

version=4.2
name=x265

workDir=$(mktemp -d)
cd "$workDir"

git clone --depth 1 --branch $version https://bitbucket.org/multicoreware/x265_git.git $name
cd $name/build

cmake -DCMAKE_INSTALL_PREFIX="$DEPLOY_DIR/ffmpeg" ../source
make -j "$(nproc)"
make install

cd /
rm -rf "$workDir"
