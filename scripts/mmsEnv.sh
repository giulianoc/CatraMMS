#!/bin/bash

CatraMMS_PATH=/opt/catramms

export LD_LIBRARY_PATH=$CatraMMS_PATH/CatraLibraries/lib:$CatraMMS_PATH/CatraMMS/lib:$CatraMMS_PATH/ImageMagick/lib:$CatraMMS_PATH/curlpp/lib64:$CatraMMS_PATH/curlpp/lib:$CatraMMS_PATH/ffmpeg/lib:$CatraMMS_PATH/ffmpeg/lib64:$CatraMMS_PATH/jsoncpp/lib:$CatraMMS_PATH/opencv/lib64:$CatraMMS_PATH/opencv/lib:$CatraMMS_PATH/aws-sdk-cpp/lib

export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:$CatraMMS_PATH/ffmpeg/lib:$CatraMMS_PATH/ffmpeg/lib64
export PATH=$PATH:$CatraMMS_PATH/ffmpeg/bin
