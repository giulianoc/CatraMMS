#!/bin/bash

export MMS_PATH=/opt/mms

export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:$MMS_PATH/ffmpeg/lib:$MMS_PATH/ffmpeg/lib64
export PATH=$PATH:$MMS_PATH/ffmpeg/bin


#$MMS_PATH/ffmpeg/bin/ffmpeg -formats

#Display options specific to, and information about, a particular muxer:
#$MMS_PATH/ffmpeg/bin/ffmpeg -h muxer=matroska

#Display options specific to, and information about, a particular demuxer:
#$MMS_PATH/ffmpeg/bin/ffmpeg -h demuxer=gif

#$MMS_PATH/ffmpeg/bin/ffmpeg -codecs

#$MMS_PATH/ffmpeg/bin/ffmpeg -encoders

#Display options specific to, and information about, a particular encoder:
#$MMS_PATH/ffmpeg/bin/ffmpeg -h encoder=mpeg4

#$MMS_PATH/ffmpeg/bin/ffmpeg -decoders

#Display options specific to, and information about, a particular decoder:
#$MMS_PATH/ffmpeg/bin/ffmpeg -h decoder=aac

