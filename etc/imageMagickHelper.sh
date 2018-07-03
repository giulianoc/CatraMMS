#!/bin/bash

CatraMMS_PATH=/home/mms/catramms

#used by ImageMagick to look for the configuration files
export MAGICK_CONFIGURE_PATH=$CatraMMS_PATH/ImageMagick-7.0.7-22/etc/ImageMagick-7

export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:$CatraMMS_PATH/ImageMagick-7.0.7-22/lib
export PATH=$PATH:$CatraMMS_PATH/ImageMagick-7.0.7-22/bin


#/home/mms/catramms/ImageMagick-7.0.7-22/bin/convert LogoRSI.png LogoRSI.jpg


