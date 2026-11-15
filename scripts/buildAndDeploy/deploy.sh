#!/bin/bash

# Installa su un server un pacchetto compilato da build.sh (preso dall'immagine catramms-build:ubuntu-<version>).
#
# usage: deploy.sh <package: ffmpeg | ImageMagick | libpqxx | MMS | aws-sdk-cpp | opencv> <server> [<ubuntu version> (default: 24.04)]
#	- <server> è la destinazione ssh (host, user@host oppure alias di ~/.ssh/config)
#
# Il pacchetto viene copiato in /opt/mms/<package>-<version> e il link /opt/mms/<package> viene spostato
# sulla nuova versione (mmsEnv.sh e gli script dei servizi usano /opt/mms/MMS, /opt/mms/ffmpeg,
# /opt/mms/ImageMagick e /opt/mms/libpqxx).
# La copia viene fatta prima di fermare i servizi: la nuova directory non è ancora usata e
# il fermo dei servizi dura solo il tempo dello spostamento del link.
# I pacchetti senza versione (aws-sdk-cpp, opencv) vengono copiati in /opt/mms/<package>.new e, a servizi fermi,
# sostituiscono /opt/mms/<package>; la directory precedente viene mantenuta in /opt/mms/<package>.old
# Per MMS viene chiesto se fare prima una nuova release: commit (git commit -a), incremento della versione in
# version.txt, tag, push e ricostruzione dell'immagine con il nuovo commit.
# Sull'host il pacchetto viene estratto in <project root>/../target/deploy/ubuntu-<version>/<package>-<version>

set -e

if [ $# -lt 2 -o $# -gt 3 ]
then
	echo "$(date): usage $0 <package, i.e.: ffmpeg | ImageMagick | libpqxx | nginx | MMS | aws-sdk-cpp | opencv> <server> [<ubuntu version, i.e.: 24.04 | 26.04>]"

	exit 1
fi

package=$1
server=$2
ubuntuVersion=${3:-24.04}

# directory del pacchetto all'interno dell'immagine
case "$package" in
	MMS)
		imageDir=/deploy/CatraMMS
		;;
	ffmpeg)
		imageDir=/deploy/ffmpeg
		;;
	ImageMagick)
		imageDir=/deploy/ImageMagick
		;;
	libpqxx)
		imageDir=/deploy/libpqxx
		;;
	aws-sdk-cpp)
		imageDir=/deploy/aws-sdk-cpp
		;;
	opencv)
		imageDir=/deploy/opencv
		;;
	nginx)
		# deps/nginx.sh compila nginx con --prefix=/opt/mms/nginx (il prefix è il path usato sul server, non $DEPLOY_DIR)
		imageDir=/opt/mms/nginx
		;;
	*)
		echo "$(date): package not supported: $package"

		exit 1
		;;
esac

scriptDir=$(cd "$(dirname "$0")" && pwd)
projectDir=$(cd "$scriptDir/../.." && pwd)
imageName="catramms-build:ubuntu-$ubuntuVersion"
# come in build.sh: l'immagine è linux/amd64 (i server di produzione sono x86_64)
platform="linux/amd64"
remoteDir=/opt/mms
# versioni precedenti del pacchetto che vengono mantenute sul server (oltre a quella installata)
previousVersionsToKeep=3

if ! docker image inspect "$imageName" > /dev/null 2>&1
then
	echo "$(date): docker image $imageName not found, run first build.sh $ubuntuVersion"

	exit 1
fi

mkdir -p "$projectDir/../target/deploy/ubuntu-$ubuntuVersion"
localDir=$(cd "$projectDir/../target/deploy/ubuntu-$ubuntuVersion" && pwd)

# solo per MMS: eventuale nuova release (commit, nuova versione in version.txt, tag e push).
# L'immagine compila CatraMMS clonandolo da github (deps/CatraMMS.sh): dopo il push l'immagine viene ricostruita
# (viene ricompilato solo il layer di CatraMMS) altrimenti verrebbe installato il codice precedente al commit
releaseVersion=
if [ "$package" == "MMS" ]
then
	read -r -p "$(date): do you want to commit, tag and push a new release? (y/N) " answer
	if [ "$answer" == "y" -o "$answer" == "Y" ]
	then
		branch=$(git -C "$projectDir" branch --show-current)
		if [ -z "$branch" ]
		then
			echo "$(date): the project is not on a branch (detached HEAD)"

			exit 1
		fi

		# la build clona anche i submodules da github: le loro modifiche devono essere già committate e pushate
		notReadySubmodules=$(git -C "$projectDir" submodule foreach --quiet --recursive \
			'if [ -n "$(git status --porcelain --untracked-files=no)" ]; then echo "$displaypath: uncommitted changes";
			elif [ -z "$(git branch -r --contains HEAD)" ]; then echo "$displaypath: commit not pushed"; fi')
		if [ -n "$notReadySubmodules" ]
		then
			echo "$(date): submodules to be committed/pushed before the release:"
			echo "$notReadySubmodules"

			exit 1
		fi

		git -C "$projectDir" fetch --tags origin
		if [ "$(git -C "$projectDir" rev-list --count "HEAD..origin/$branch")" -gt 0 ]
		then
			echo "$(date): the branch $branch is behind origin/$branch, run first git pull"

			exit 1
		fi

		# nuova versione: viene incrementato il terzo numero (es. 1.0.7180 -> 1.0.7181)
		IFS=. read -r major minor patch < "$projectDir/version.txt"
		releaseVersion="$major.$minor.$((patch + 1))"
		if git -C "$projectDir" rev-parse -q --verify "refs/tags/$releaseVersion" > /dev/null
		then
			echo "$(date): tag $releaseVersion already exists"

			exit 1
		fi

		read -r -p "$(date): commit message: " commitMessage
		if [ -z "$commitMessage" ]
		then
			echo "$(date): commit message is empty"

			exit 1
		fi

		# git commit -a: vengono committati tutti i files tracciati modificati (non i nuovi files)
		echo "$(date): files to be committed in the release $releaseVersion (branch $branch):"
		git -C "$projectDir" status --short --untracked-files=no
		read -r -p "$(date): continue? (y/N) " answer
		if [ "$answer" != "y" -a "$answer" != "Y" ]
		then
			exit 1
		fi

		echo "$releaseVersion" > "$projectDir/version.txt"
		git -C "$projectDir" commit -a -m "$commitMessage"
		git -C "$projectDir" tag -a "$releaseVersion" -m "$commitMessage"
		git -C "$projectDir" push origin "$branch"
		git -C "$projectDir" push origin "$releaseVersion"

		# come in build.sh
		buildLog="$projectDir/../target/docker-build-ubuntu-$ubuntuVersion.log"
		echo "$(date): building docker image $imageName with the branch $branch (log: $buildLog)"
		set -o pipefail
		docker build --progress=plain --platform "$platform" --build-arg CATRAMMS_BRANCH="$branch" -t "$imageName" \
			-f "$scriptDir/Dockerfile.ubuntu-$ubuntuVersion" "$scriptDir" 2>&1 | tee "$buildLog"
	fi
fi

# il pacchetto viene copiato dall'immagine tramite un container creato e non avviato
extractDir="$localDir/$package"
rm -rf "$extractDir"
containerId=$(docker create --platform "$platform" "$imageName")
trap 'docker rm "$containerId" > /dev/null' EXIT
echo "$(date): extracting $imageDir from $imageName"
docker cp "$containerId:$imageDir" "$extractDir"

# nginx già installato: /opt/mms/nginx sul server contiene anche la configurazione (link a MMS/conf/nginx.conf,
# sites-enabled, snippets, certificati), il link logs e le directory temporanee (vedi installServer.sh),
# per cui viene sostituito solo l'eseguibile (il precedente viene mantenuto in sbin/nginx.old).
# Prima installazione (installServer.sh): viene copiata tutta la directory (logica generale sotto)
if [ "$package" == "nginx" ] && ssh "$server" test -x "$remoteDir/nginx/sbin/nginx"
then
	echo "$(date): copying nginx executable to $server:$remoteDir/nginx/sbin/nginx.new"
	rsync -a "$extractDir/sbin/nginx" "$server:$remoteDir/nginx/sbin/nginx.new"

	echo "$(date): installing nginx executable on $server"
	ssh "$server" bash -s -- "$remoteDir" << 'EOF'
set -e

sbinDir=$1/nginx/sbin

# verifica che il nuovo eseguibile parta (es. librerie mancanti) prima di sostituire quello attuale
"$sbinDir/nginx.new" -V

cp -p "$sbinDir/nginx" "$sbinDir/nginx.old"
mv "$sbinDir/nginx.new" "$sbinDir/nginx"
EOF

	echo "$(date): deploy of nginx on $server completed, restart nginx to use the new executable (previous one in $remoteDir/nginx/sbin/nginx.old)"

	exit 0
fi

# versioned: 1 se il pacchetto viene installato in /opt/mms/<package>-<version> con il link /opt/mms/<package>
versioned=1
if [ "$package" == "MMS" ]
then
	# version.txt viene installato da CMakeLists.txt
	version=$(tr -d '[:space:]' < "$extractDir/version.txt")
elif [ "$package" == "ffmpeg" ]
then
	version=$(sed -n 's/^#define FFMPEG_VERSION "\(.*\)"$/\1/p' "$extractDir/include/libavutil/ffversion.h")
elif [ "$package" == "ImageMagick" ]
then
	# versione completa, es. 7.1.2-34: MagickLibVersionText (7.1.2) + MagickLibAddendum (-34)
	versionHeader="$extractDir/include/ImageMagick-7/MagickCore/version.h"
	versionText=$(sed -n 's/^#[[:space:]]*define[[:space:]]*MagickLibVersionText[[:space:]]*"\(.*\)"$/\1/p' "$versionHeader")
	versionAddendum=$(sed -n 's/^#[[:space:]]*define[[:space:]]*MagickLibAddendum[[:space:]]*"\(.*\)"$/\1/p' "$versionHeader")
	# senza MagickLibVersionText la versione resta vuota (e lo script si ferma più sotto)
	version=${versionText:+$versionText$versionAddendum}
elif [ "$package" == "libpqxx" ]
then
	version=$(sed -n 's/^#[[:space:]]*define[[:space:]]*PQXX_VERSION[[:space:]]*"\(.*\)"$/\1/p' "$extractDir/include/pqxx/version.hxx")
else
	versioned=0
fi

if [ $versioned -eq 1 ]
then
	if [ -z "$version" ]
	then
		echo "$(date): version of $package not found"

		exit 1
	fi

	# l'immagine potrebbe non contenere il commit della release (es. ref non ancora aggiornato dall'API di github)
	if [ -n "$releaseVersion" -a "$version" != "$releaseVersion" ]
	then
		echo "$(date): the image contains $package $version instead of the release $releaseVersion, run again build.sh $ubuntuVersion"

		exit 1
	fi

	packageDir="$package-$version"
	rm -rf "$localDir/$packageDir"
	mv "$extractDir" "$localDir/$packageDir"
	localPackageDir="$localDir/$packageDir"
	# la directory definitiva viene copiata direttamente: non è ancora usata dai servizi
	remotePackageDir="$packageDir"
else
	packageDir="$package"
	localPackageDir="$extractDir"
	# /opt/mms/<package> è usata dai servizi: viene sostituita solo a servizi fermi
	remotePackageDir="$package.new"
fi
#echo "$(date): local installation in $localPackageDir"

echo "$(date): copying $packageDir to $server:$remoteDir/$remotePackageDir"
rsync -a --delete "$localPackageDir/" "$server:$remoteDir/$remotePackageDir/"

echo "$(date): installing $packageDir on $server"
ssh "$server" bash -s -- "$remoteDir" "$package" "$packageDir" "$previousVersionsToKeep" "$versioned" << 'EOF'
set -e

remoteDir=$1
package=$2
packageDir=$3
previousVersionsToKeep=$4
versioned=$5

# lo stdin di questa shell è lo script stesso (bash -s): i comandi non devono leggerlo, quindi < /dev/null
stopAll()
{
	~/mmsStopALL.sh < /dev/null
}

# i servizi avviati ereditano stdout/stderr: se fossero quelli della sessione ssh, ssh non terminerebbe finchè
# i servizi sono attivi (ed i loro messaggi, es. un crash, finirebbero nell'output del deploy).
# L'output di mmsStartALL.sh viene quindi scritto in un file e poi mostrato
startAll()
{
	local startLog=~/mmsStartALL.log
	local startStatus=0

	~/mmsStartALL.sh < /dev/null > "$startLog" 2>&1 || startStatus=$?
	cat "$startLog"

	return $startStatus
}

if [ $versioned -eq 0 ]
then
	echo ""
	echo "------------------------------------------------------------------------------------------------------------------------------------------"
	echo "mmsStopALL.sh"
	#stopAll

	echo ""
	echo "------------------------------------------------------------------------------------------------------------------------------------------"
	echo "replace $remoteDir/$package (previous one in $remoteDir/$package.old)"
	rm -rf "${remoteDir:?}/$package.old"
	if [ -e "$remoteDir/$package" ]
	then
		mv "$remoteDir/$package" "$remoteDir/$package.old"
	fi
	mv "$remoteDir/$package.new" "$remoteDir/$package"

	echo ""
	echo "------------------------------------------------------------------------------------------------------------------------------------------"
	echo "mmsStartALL.sh"
	#startAll

	exit 0
fi

# se /opt/mms/<package> fosse una directory (e non un link) ln -sfn creerebbe il link al suo interno
if [ -e "$remoteDir/$package" -a ! -L "$remoteDir/$package" ]
then
	echo "$(date): $remoteDir/$package is not a symbolic link, move it manually (i.e.: mv $remoteDir/$package $remoteDir/$package-old)"

	exit 1
fi

echo ""
echo "------------------------------------------------------------------------------------------------------------------------------------------"
echo "mmsStopALL.sh"
#stopAll

echo ""
echo "------------------------------------------------------------------------------------------------------------------------------------------"
echo "ln -sfn $packageDir $remoteDir/$package"
ln -sfn "$packageDir" "$remoteDir/$package"

echo ""
echo "------------------------------------------------------------------------------------------------------------------------------------------"
echo "mmsStartALL.sh"
#startAll

# vengono mantenute le ultime $previousVersionsToKeep versioni precedenti (ordinate per numero di versione).
# -type d esclude i link (es. /opt/mms/MMS) e i vecchi pacchetti MMS-<version>.tar.gz
echo ""
echo "------------------------------------------------------------------------------------------------------------------------------------------"
echo "remove previous versions of $package (keeping the last $previousVersionsToKeep)"
find "$remoteDir" -mindepth 1 -maxdepth 1 -type d -name "$package-[0-9]*" ! -name "$packageDir" -printf '%f\n' \
	| sort -V | head -n -"$previousVersionsToKeep" \
	| while read -r previousPackageDir
	do
		echo "rm -rf $remoteDir/$previousPackageDir"
		rm -rf "${remoteDir:?}/$previousPackageDir"
	done
EOF

echo "$(date): deploy of $packageDir on $server completed"
