#!/bin/bash

# Scarica, compila e installa libpqxx in $DEPLOY_DIR/libpqxx
# Repository: https://github.com/jtv/libpqxx (viene usato il tag dell'ultima release)
# Richiede libpq-dev

set -e

#version=8.0.2
version=7.10.7
name=libpqxx

workDir=$(mktemp -d)
cd "$workDir"

git clone --depth 1 --branch $version https://github.com/jtv/libpqxx.git $name
cd $name

# libpqxx va compilata con lo stesso standard C++ dell'applicazione (CMAKE_CXX_STANDARD in CMakeLists.txt di CatraMMS)
# BUILD_TEST=OFF: i test non vengono installati e allungherebbero la compilazione
cmake -DCMAKE_INSTALL_PREFIX="$DEPLOY_DIR/$name" -DBUILD_SHARED_LIBS=on -DCMAKE_CXX_STANDARD=23 -DBUILD_TEST=OFF .
make -j "$(nproc)"
make install

cd /
rm -rf "$workDir"
