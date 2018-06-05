#!/bin/bash

CatraMMS_PATH=/home/mms/catramms

export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:$CatraMMS_PATH/ffmpeg-3.4.2/lib
export PATH=$PATH:$CatraMMS_PATH/ffmpeg-3.4.2/bin


#/home/mms/catramms/ffmpeg-3.4.2/bin/ffmpeg -formats

#Display options specific to, and information about, a particular muxer:
#/home/mms/catramms/ffmpeg-3.4.2/bin/ffmpeg -h muxer=matroska

#Display options specific to, and information about, a particular demuxer:
#/home/mms/catramms/ffmpeg-3.4.2/bin/ffmpeg -h demuxer=gif

#/home/mms/catramms/ffmpeg-3.4.2/bin/ffmpeg -codecs

#/home/mms/catramms/ffmpeg-3.4.2/bin/ffmpeg -encoders

#Display options specific to, and information about, a particular encoder:
#/home/mms/catramms/ffmpeg-3.4.2/bin/ffmpeg -h encoder=mpeg4

#/home/mms/catramms/ffmpeg-3.4.2/bin/ffmpeg -decoders

#Display options specific to, and information about, a particular decoder:
#/home/mms/catramms/ffmpeg-3.4.2/bin/ffmpeg -h decoder=aac

