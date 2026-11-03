#!/bin/bash

# Scarica nlohmann json e ne installa gli headers in $DEPLOY_DIR/nlohmann/include
# Repository: https://github.com/nlohmann/json.git (viene usato il tag dell'ultima release)
# Libreria header-only: non serve compilare nulla

set -e

version=v3.12.0
name=nlohmann

workDir=$(mktemp -d)
cd "$workDir"

git clone --depth 1 --branch $version https://github.com/nlohmann/json.git $name

mkdir -p "$DEPLOY_DIR/$name"
cp -r $name/include "$DEPLOY_DIR/$name/"

cd /
rm -rf "$workDir"
