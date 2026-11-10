#!/bin/bash

# Scarica, compila e installa lame in $DEPLOY_DIR/ffmpeg
# (come tutte le librerie usate da ffmpeg: il pacchetto ffmpeg installato sui server le contiene)
# Ultima versione: https://sourceforge.net/projects/lame/files/lame

set -e

version=4.0
name=lame

workDir=$(mktemp -d)
cd "$workDir"

wget -q -O $name-$version.tar.gz \
	"https://sourceforge.net/projects/lame/files/lame/$version/$name-$version.tar.gz/download"
# bsdtar invece di GNU tar: vedi opencore-amr.sh
bsdtar xzf $name-$version.tar.gz
cd $name-$version

# dalla 4.0 il decoder mp3 richiede libmpg123: viene escluso perchè ffmpeg usa lame solo come encoder
# --disable-frontend: l'eseguibile lame non viene usato (ffmpeg usa solo libmp3lame) e il frontend della 4.0
# non compila con gcc >= 14 (Ubuntu 26.04): parse.c chiama funzioni id3tag_*_ucs2 non più dichiarate
# e passa tipi di puntatore incompatibili, che dal gcc 14 sono errori e non più warning
./configure --prefix="$DEPLOY_DIR/ffmpeg" --disable-decoder --disable-frontend
make -j "$(nproc)"
make install

cd /
rm -rf "$workDir"
