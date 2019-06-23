#!/bin/bash

CatraMMS_PATH=/opt/catramms

#used by ImageMagick to look for the configuration files
export MAGICK_CONFIGURE_PATH=$CatraMMS_PATH/ImageMagick-7.0.8-49/etc/ImageMagick-7

export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:$CatraMMS_PATH/ImageMagick-7.0.8-49/lib
export PATH=$PATH:$CatraMMS_PATH/ImageMagick-7.0.8-49/bin


#/opt/catramms/ImageMagick-7.0.8-49/bin/convert LogoRSI.png LogoRSI.jpg


