#!/bin/bash

# Scarica, compila e installa ImageMagick in $DEPLOY_DIR/ImageMagick
# Repository: https://github.com/ImageMagick/ImageMagick (viene usato il tag dell'ultima release)
# https://www.imagemagick.org/script/install-source.php
# I formati delle immagini dipendono dai pacchetti -dev installati (libjpeg-dev, libpng-dev, libpango1.0-dev, ...)
# Viene compilata la configurazione di default (Q16 HDRI): CatraMMS linka Magick++-7.Q16HDRI

set -e

version=7.1.2-34
name=ImageMagick

workDir=$(mktemp -d)
cd "$workDir"

git clone --depth 1 --branch $version https://github.com/ImageMagick/ImageMagick.git $name
cd $name

./configure --enable-delegate-build --prefix="$DEPLOY_DIR/$name"
make -j "$(nproc)"
make install

cd /
rm -rf "$workDir"
