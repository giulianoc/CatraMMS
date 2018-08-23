#!/bin/bash

CatraMMS_PATH=/home/mms/catramms

#used by ImageMagick to look for the configuration files
export MAGICK_CONFIGURE_PATH=$CatraMMS_PATH/ImageMagick-7.0.8-10/etc/ImageMagick-7

export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:$CatraMMS_PATH/ImageMagick-7.0.8-10/lib
export PATH=$PATH:$CatraMMS_PATH/ImageMagick-7.0.8-10/bin


#/home/mms/catramms/ImageMagick-7.0.8-10/bin/convert LogoRSI.png LogoRSI.jpg


