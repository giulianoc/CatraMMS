#!/bin/bash

export CatraMMS_PATH=/opt/catramms

export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:$CatraMMS_PATH/ffmpeg/lib:$CatraMMS_PATH/ffmpeg/lib64
export PATH=$PATH:$CatraMMS_PATH/ffmpeg/bin


#$CatraMMS_PATH/ffmpeg/bin/ffmpeg -formats

#Display options specific to, and information about, a particular muxer:
#$CatraMMS_PATH/ffmpeg/bin/ffmpeg -h muxer=matroska

#Display options specific to, and information about, a particular demuxer:
#$CatraMMS_PATH/ffmpeg/bin/ffmpeg -h demuxer=gif

#$CatraMMS_PATH/ffmpeg/bin/ffmpeg -codecs

#$CatraMMS_PATH/ffmpeg/bin/ffmpeg -encoders

#Display options specific to, and information about, a particular encoder:
#$CatraMMS_PATH/ffmpeg/bin/ffmpeg -h encoder=mpeg4

#$CatraMMS_PATH/ffmpeg/bin/ffmpeg -decoders

#Display options specific to, and information about, a particular decoder:
#$CatraMMS_PATH/ffmpeg/bin/ffmpeg -h decoder=aac

