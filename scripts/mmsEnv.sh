#!/bin/bash

CatraMMS_PATH=/opt/catramms

export LD_LIBRARY_PATH=$CatraMMS_PATH/CatraLibraries/lib:$CatraMMS_PATH/CatraMMS/lib:$CatraMMS_PATH/ImageMagick-7.0.8-49/lib:$CatraMMS_PATH/curlpp/lib64:$CatraMMS_PATH/curlpp/lib:$CatraMMS_PATH/ffmpeg-4.2.2/lib:$CatraMMS_PATH/ffmpeg-4.2.2/lib64:$CatraMMS_PATH/jsoncpp/lib:$CatraMMS_PATH/opencv/lib64

export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:$CatraMMS_PATH/ffmpeg-4.2.2/lib:$CatraMMS_PATH/ffmpeg-4.2.2/lib64
export PATH=$PATH:$CatraMMS_PATH/ffmpeg-4.2.2/bin
