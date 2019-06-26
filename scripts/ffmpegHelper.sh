#!/bin/bash

export CatraMMS_PATH=/opt/catramms

export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:$CatraMMS_PATH/ffmpeg-4.1.3/lib:$CatraMMS_PATH/ffmpeg-4.1.3/lib64
export PATH=$PATH:$CatraMMS_PATH/ffmpeg-4.1.3/bin


#/opt/catramms/ffmpeg-3.4.2/bin/ffmpeg -formats

#Display options specific to, and information about, a particular muxer:
#/opt/catramms/ffmpeg-3.4.2/bin/ffmpeg -h muxer=matroska

#Display options specific to, and information about, a particular demuxer:
#/opt/catramms/ffmpeg-3.4.2/bin/ffmpeg -h demuxer=gif

#/opt/catramms/ffmpeg-3.4.2/bin/ffmpeg -codecs

#/opt/catramms/ffmpeg-3.4.2/bin/ffmpeg -encoders

#Display options specific to, and information about, a particular encoder:
#/opt/catramms/ffmpeg-3.4.2/bin/ffmpeg -h encoder=mpeg4

#/opt/catramms/ffmpeg-3.4.2/bin/ffmpeg -decoders

#Display options specific to, and information about, a particular decoder:
#/opt/catramms/ffmpeg-3.4.2/bin/ffmpeg -h decoder=aac

