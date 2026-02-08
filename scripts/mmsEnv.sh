#!/bin/bash

MMS_PATH=/opt/mms

export LD_LIBRARY_PATH=$MMS_PATH/MMS/lib:$MMS_PATH/libpqxx/lib:$MMS_PATH/ImageMagick/lib:$MMS_PATH/curlpp/lib64:$MMS_PATH/curlpp/lib:$MMS_PATH/ffmpeg/lib:$MMS_PATH/ffmpeg/lib64:$MMS_PATH/jsoncpp/lib:$MMS_PATH/opencv/lib64:$MMS_PATH/opencv/lib:$MMS_PATH/aws-sdk-cpp/lib

export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:$MMS_PATH/ffmpeg/lib:$MMS_PATH/ffmpeg/lib64
export PATH=$PATH:$MMS_PATH/ffmpeg/bin
