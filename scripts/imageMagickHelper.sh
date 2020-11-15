#!/bin/bash

CatraMMS_PATH=/opt/catramms

#used by ImageMagick to look for the configuration files
export MAGICK_CONFIGURE_PATH=$CatraMMS_PATH/ImageMagick/etc/ImageMagick-7

export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:$CatraMMS_PATH/ImageMagick/lib
export PATH=$PATH:$CatraMMS_PATH/ImageMagick/bin


#$CatraMMS_PATH/ImageMagick/bin/convert LogoRSI.png LogoRSI.jpg


