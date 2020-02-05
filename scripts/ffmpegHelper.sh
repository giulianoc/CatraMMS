#!/bin/bash

export CatraMMS_PATH=/opt/catramms

export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:$CatraMMS_PATH/ffmpeg-4.2.2/lib:$CatraMMS_PATH/ffmpeg-4.2.2/lib64
export PATH=$PATH:$CatraMMS_PATH/ffmpeg-4.2.2/bin


#$CatraMMS_PATH/ffmpeg-3.4.2/bin/ffmpeg -formats

#Display options specific to, and information about, a particular muxer:
#$CatraMMS_PATH/ffmpeg-3.4.2/bin/ffmpeg -h muxer=matroska

#Display options specific to, and information about, a particular demuxer:
#$CatraMMS_PATH/ffmpeg-3.4.2/bin/ffmpeg -h demuxer=gif

#$CatraMMS_PATH/ffmpeg-3.4.2/bin/ffmpeg -codecs

#$CatraMMS_PATH/ffmpeg-3.4.2/bin/ffmpeg -encoders

#Display options specific to, and information about, a particular encoder:
#$CatraMMS_PATH/ffmpeg-3.4.2/bin/ffmpeg -h encoder=mpeg4

#$CatraMMS_PATH/ffmpeg-3.4.2/bin/ffmpeg -decoders

#Display options specific to, and information about, a particular decoder:
#$CatraMMS_PATH/ffmpeg-3.4.2/bin/ffmpeg -h decoder=aac

