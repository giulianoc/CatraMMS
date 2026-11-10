#!/bin/bash

# Scarica e compila nginx con i moduli usati da CatraMMS e lo installa in /opt/mms/nginx
# Ultima versione stable: http://nginx.org/en/download.html
# pcre, zlib e openssl vengono scaricati come sorgenti e compilati (staticamente) da nginx stesso
# (--with-pcre, --with-zlib, --with-openssl)
# Moduli:
#	- https://github.com/masterzen/nginx-upload-progress-module
#	- https://github.com/sergey-dryabzhinsky/nginx-rtmp-module
#	- https://github.com/kaltura/nginx-vod-module (compilato senza ffmpeg: vedi sotto)
#	- https://github.com/leev/ngx_http_geoip2_module (richiede libmaxminddb-dev)

set -e

version=1.30.6
pcreVersion=8.45
zlibVersion=1.3.2
opensslVersion=3.6.5
uploadProgressVersion=v0.9.4
rtmpVersion=v1.2.2-r1
vodVersion=1.33
geoip2Version=3.4
name=nginx

workDir=$(mktemp -d)
cd "$workDir"

# bsdtar invece di GNU tar: vedi opencore-amr.sh
wget -q -O pcre-$pcreVersion.tar.gz \
	"https://sourceforge.net/projects/pcre/files/pcre/$pcreVersion/pcre-$pcreVersion.tar.gz/download"
bsdtar xzf pcre-$pcreVersion.tar.gz

# zlib.net contiene solo l'ultima versione, fossils anche le precedenti
wget -q https://zlib.net/fossils/zlib-$zlibVersion.tar.gz
bsdtar xzf zlib-$zlibVersion.tar.gz

wget -q https://github.com/openssl/openssl/releases/download/openssl-$opensslVersion/openssl-$opensslVersion.tar.gz
bsdtar xzf openssl-$opensslVersion.tar.gz

git clone --depth 1 --branch $uploadProgressVersion https://github.com/masterzen/nginx-upload-progress-module.git
git clone --depth 1 --branch $rtmpVersion https://github.com/sergey-dryabzhinsky/nginx-rtmp-module.git
git clone --depth 1 --branch $vodVersion https://github.com/kaltura/nginx-vod-module.git
git clone --depth 1 --branch $geoip2Version https://github.com/leev/ngx_http_geoip2_module.git

wget -q http://nginx.org/download/$name-$version.tar.gz
bsdtar xzf $name-$version.tar.gz
cd $name-$version

# nginx-vod-module viene compilato senza ffmpeg (non vengono passati include e librerie di $DEPLOY_DIR/ffmpeg):
# la 1.33 non compila con FFmpeg 9 (usa avcodec_close, AVCodec::sample_fmts, ... rimossi) e le funzionalità
# che richiedono ffmpeg (thumbnails, volume map, filtri audio) non sono usate da CatraMMS.
# Il packaging VOD (HLS/DASH/MSS) non dipende da ffmpeg.
# nginx compila con -Werror e la 1.33 di nginx-vod-module dà errore con gcc >= 15 e libxml2 >= 2.14 (Ubuntu 26.04):
#	- -std=gnu17: nel C23 (default del gcc 15) 'static void ngx_http_vod_exit_process();' è un prototipo senza
#		parametri, incompatibile con i puntatori a funzione di ngx_module_t (exit_process/exit_master)
#	- -Wno-error=deprecated-declarations: dfxp_format.c usa xmlParserCtxt::recovery, deprecato dalla libxml2 2.14
ccOpt="-O3 -std=gnu17 -Wno-error=deprecated-declarations"
./configure --prefix=/opt/mms/$name \
	--with-pcre=../pcre-$pcreVersion --with-zlib=../zlib-$zlibVersion --with-openssl=../openssl-$opensslVersion \
	--with-http_ssl_module --with-http_v2_module \
	--with-http_auth_request_module --with-http_stub_status_module --with-http_realip_module \
	--add-module=../nginx-upload-progress-module --add-module=../nginx-rtmp-module \
	--add-module=../nginx-vod-module --add-module=../ngx_http_geoip2_module \
	--with-file-aio --with-threads --with-cc-opt="$ccOpt"

make -j "$(nproc)"
make install

cd /
rm -rf "$workDir"
