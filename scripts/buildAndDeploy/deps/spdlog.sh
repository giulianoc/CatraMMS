#!/bin/bash

# Scarica spdlog e ne installa gli headers in $DEPLOY_DIR/spdlog/include
# Repository: https://github.com/gabime/spdlog.git (viene usato il tag dell'ultima release)
# CatraMMS usa spdlog header-only (con fmt bundled): non serve compilare la libreria

set -e

version=v1.17.0
name=spdlog

workDir=$(mktemp -d)
cd "$workDir"

git clone --depth 1 --branch $version https://github.com/gabime/spdlog.git $name

mkdir -p "$DEPLOY_DIR/$name"
cp -r $name/include "$DEPLOY_DIR/$name/"

cd /
rm -rf "$workDir"
