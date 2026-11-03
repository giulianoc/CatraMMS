#!/bin/bash

# Scarica, compila e installa ffmpeg in $DEPLOY_DIR/ffmpeg
# Ultima versione: https://www.ffmpeg.org/download.html (https://ffmpeg.org/releases)
# Dipende dalle librerie compilate dagli altri script in deps/

set -e

version=9.0.2
name=ffmpeg

workDir=$(mktemp -d)
cd "$workDir"

wget -q https://ffmpeg.org/releases/$name-$version.tar.xz
# bsdtar invece di GNU tar: vedi opencore-amr.sh
bsdtar xJf $name-$version.tar.xz
cd $name-$version

# i .pc delle librerie vengono presi direttamente dalle directory di installazione in $DEPLOY_DIR
PKG_CONFIG_PATH=$(ls -d "$DEPLOY_DIR"/*/lib/pkgconfig | paste -sd:)
export PKG_CONFIG_PATH

# lame e xvidcore vengono cercate da configure senza pkg-config: sono installate in $DEPLOY_DIR/$name
# come tutte le librerie usate da ffmpeg (deps/<libreria>.sh)
extraCFlags="-I$DEPLOY_DIR/$name/include"
extraLDFlags="-L$DEPLOY_DIR/$name/lib"

./configure --prefix="$DEPLOY_DIR/$name" \
	--extra-cflags="$extraCFlags" --extra-ldflags="$extraLDFlags" \
	--enable-indev=alsa --enable-outdev=alsa \
	--enable-libfreetype --enable-libharfbuzz --enable-libfontconfig --enable-libfribidi \
	--enable-libmp3lame --enable-libopus --enable-libx265 --enable-libaom --enable-gpl \
	--enable-libxml2 --enable-libfdk-aac --enable-libvpx --enable-libx264 --enable-nonfree \
	--enable-shared --enable-libopencore-amrnb --enable-libopencore-amrwb --enable-libvorbis \
	--enable-libxvid --enable-version3 --enable-openssl --enable-libsrt --enable-ffplay

# ffplay viene escluso silenziosamente da configure se manca qualcosa (es. sdl2)
if ! grep -q "^CONFIG_FFPLAY=yes" ffbuild/config.mak
then
	echo "$(date): ffplay is not enabled in the configure output"

	exit 1
fi

make -j "$(nproc)"
make install

cd /
rm -rf "$workDir"
