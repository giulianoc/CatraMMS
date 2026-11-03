#!/bin/bash

# Compila CatraMMS all'interno di un container docker Ubuntu.
#
# usage: build.sh <ubuntu version: 24.04 | 26.04> [<branch>]
#	- senza <branch> viene compilato il codice del progetto locale
#	- con <branch> il codice viene clonato (con i submodules) dal repository origin
#
# I files temporanei (sorgenti clonati e directory di build) vengono creati in <project root>/../target
# L'output della build dell'immagine docker viene salvato in <project root>/../target/docker-build-ubuntu-<version>.log

set -e

if [ $# -lt 1 -o $# -gt 2 ]
then
	echo "$(date): usage $0 <ubuntu version, i.e.: 24.04 | 26.04> [<branch>]"

	exit 1
fi

ubuntuVersion=$1
branch=$2

scriptDir=$(cd "$(dirname "$0")" && pwd)
projectDir=$(cd "$scriptDir/../.." && pwd)
dockerFile="$scriptDir/Dockerfile.ubuntu-$ubuntuVersion"

if [ ! -f "$dockerFile" ]
then
	echo "$(date): ubuntu version not supported: $ubuntuVersion (missing $dockerFile)"

	exit 1
fi

mkdir -p "$projectDir/../target"
targetDir=$(cd "$projectDir/../target" && pwd)

# i server di produzione sono x86_64: anche su host arm64 (es. Mac Apple Silicon) si compila per amd64 (in emulazione)
platform="linux/amd64"
imageName="catramms-build:ubuntu-$ubuntuVersion"

# l'output completo della build dell'immagine viene salvato anche in un file di log:
# da un altro terminale si può seguire con tail -f <log>
buildLog="$targetDir/docker-build-ubuntu-$ubuntuVersion.log"
echo "$(date): building docker image $imageName (log: $buildLog)"
# pipefail: altrimenti un errore di docker build verrebbe nascosto dal tee
set -o pipefail
# l'immagine contiene anche CatraMMS compilato e installato in /deploy/CatraMMS (deps/CatraMMS.sh) dal branch
# indicato (master se non indicato)
docker build --progress=plain --platform "$platform" --build-arg CATRAMMS_BRANCH="${branch:-master}" -t "$imageName" -f "$dockerFile" "$scriptDir" 2>&1 | tee "$buildLog"

if [ -z "$branch" ]
then
	sourceDir="$projectDir"
	workDir="$targetDir/ubuntu-$ubuntuVersion/local"
else
	# il nome del branch può contenere '/'
	workDir="$targetDir/ubuntu-$ubuntuVersion/branch-${branch//\//_}"
	sourceDir="$workDir/src"

	repositoryURL=$(git -C "$projectDir" remote get-url origin)

	# il clone viene fatto sull'host perchè origin e alcuni submodules usano l'alias ssh github-personal
	echo "$(date): cloning $repositoryURL, branch $branch"
	rm -rf "$sourceDir"
	mkdir -p "$workDir"
	git clone --branch "$branch" --depth 1 --recurse-submodules --shallow-submodules \
		"$repositoryURL" "$sourceDir"
fi

buildDir="$workDir/build"
mkdir -p "$buildDir"

echo "$(date): compiling $sourceDir in $buildDir (ubuntu $ubuntuVersion)"
# l'utente dell'host evita che i files generati nella directory di build siano di root
docker run --rm \
	--platform "$platform" \
	--user "$(id -u):$(id -g)" \
	-e HOME=/tmp \
	-v "$sourceDir:/src:ro" \
	-v "$buildDir:/build" \
	"$imageName" \
	bash -c 'export LD_LIBRARY_PATH=$(ls -d "$DEPLOY_DIR"/*/lib | paste -sd:) \
		&& export LIBRARY_PATH=$LD_LIBRARY_PATH \
		&& cmake -S /src -B /build && cmake --build /build -j "$(nproc)"'

echo "$(date): build completed, output in $buildDir"
