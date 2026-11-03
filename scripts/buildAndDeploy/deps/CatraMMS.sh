#!/bin/bash

# Scarica (con i submodules), compila e installa CatraMMS in $DEPLOY_DIR/CatraMMS
# Repository: https://github.com/giulianoc/CatraMMS.git
# usage: CatraMMS.sh [<branch>] (default: master)
# Dipende da tutti gli altri script in deps/

set -e

branch=${1:-master}
name=CatraMMS

workDir=$(mktemp -d)
cd "$workDir"

# alcuni submodules (es. XMLWrapper) hanno come url l'alias ssh git@github-personal: nel container non c'è
# la chiave ssh e vengono scaricati via https (i repository sono pubblici).
# La configurazione passata con -c vale anche per il clone dei submodules
git -c url."https://github.com/".insteadOf="git@github-personal:" \
	clone --branch "$branch" --depth 1 --recurse-submodules --shallow-submodules \
	https://github.com/giulianoc/CatraMMS.git $name

# CMakeLists.txt cerca le librerie di terze parti in ${PROJECT_SOURCE_DIR}/../deploy
ln -s "$DEPLOY_DIR" deploy

# le librerie in $DEPLOY_DIR dipendono a loro volta da altre librerie in $DEPLOY_DIR (es. avcodec da x264):
# il linker le cerca tramite LD_LIBRARY_PATH (vedi opencv.sh)
LD_LIBRARY_PATH=$(ls -d "$DEPLOY_DIR"/*/lib | paste -sd:)
export LD_LIBRARY_PATH
# alcuni eseguibili (es. mmsEngineService) linkano direttamente librerie di $DEPLOY_DIR (-lx264, -lmp3lame, -lsrt, ...)
# di cui i CMakeLists.txt non indicano la directory (link_directories): gcc le cerca anche in LIBRARY_PATH
LIBRARY_PATH=$LD_LIBRARY_PATH
export LIBRARY_PATH

mkdir build
cd build

cmake -DCMAKE_INSTALL_PREFIX="$DEPLOY_DIR/$name" ../$name
make -j "$(nproc)"
make install

cd /
rm -rf "$workDir"
