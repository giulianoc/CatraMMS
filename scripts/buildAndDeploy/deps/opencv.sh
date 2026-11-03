#!/bin/bash

# Scarica, compila e installa opencv (con i moduli di opencv_contrib) in $DEPLOY_DIR/opencv
# Repository: https://github.com/opencv/opencv e https://github.com/opencv/opencv_contrib
# viene usato il tag dell'ultima release 4.x: CatraMMS usa le API 4 (es. CascadeClassifier, spostato in contrib nella 5)
# Dipende da ffmpeg (deps/ffmpeg.sh) per videoio (WITH_FFMPEG)

set -e
# il cmake è in pipe con tee
set -o pipefail

version=4.14.0
name=opencv

workDir=$(mktemp -d)
cd "$workDir"

git clone --depth 1 --branch $version https://github.com/opencv/opencv.git opencv
git clone --depth 1 --branch $version https://github.com/opencv/opencv_contrib.git opencv_contrib

# la 4.14.0 non compila con FFmpeg 9 (rimossi AVCodec::pix_fmts e AVCodec::supported_framerates).
# La patch contiene i commit della correzione presente nel branch 4.x (700cd32ff, 83ed22ca2, c0166c617):
# va rimossa quando si passa alla prima release che la include
git -C opencv apply "$(dirname "$0")/patches/opencv-$version-ffmpeg9.patch"

mkdir opencv/build
cd opencv/build

# ffmpeg viene trovato tramite pkg-config (libavcodec, libavformat, ...)
PKG_CONFIG_PATH=$(ls -d "$DEPLOY_DIR"/*/lib/pkgconfig | paste -sd:)
export PKG_CONFIG_PATH
# le librerie di ffmpeg dipendono da quelle in $DEPLOY_DIR (x264, srt, ...): senza LD_LIBRARY_PATH il linker
# non le trova, il test di cmake per ffmpeg fallisce ("Can't build ffmpeg test code") e ffmpeg viene disabilitato
LD_LIBRARY_PATH=$(ls -d "$DEPLOY_DIR"/*/lib | paste -sd:)
export LD_LIBRARY_PATH

# rispetto alla procedura manuale non vengono compilati docs, examples e tests (e quindi non serve opencv_extra,
# che contiene solo i dati dei test): non vengono installati e in emulazione allungherebbero molto la compilazione
cmake -DCMAKE_BUILD_TYPE=Release \
	-DBUILD_DOCS=OFF -DBUILD_EXAMPLES=OFF -DBUILD_TESTS=OFF -DBUILD_PERF_TESTS=OFF \
	-DWITH_FFMPEG=ON -DWITH_LIBV4L=ON \
	-DOPENCV_EXTRA_MODULES_PATH="$workDir/opencv_contrib/modules" \
	-DCMAKE_INSTALL_PREFIX="$DEPLOY_DIR/$name" .. | tee cmake.log

# videoio senza ffmpeg non darebbe errore in compilazione ma CatraMMS usa cv::CAP_FFMPEG
if ! grep -qE "^-- +FFMPEG: +YES" cmake.log
then
	echo "$(date): ffmpeg not found by opencv"

	exit 1
fi

make -j "$(nproc)"
make install

cd /
rm -rf "$workDir"
