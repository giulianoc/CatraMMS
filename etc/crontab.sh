#!/bin/bash

#update certificate
sudo /usr/bin/certbot --quiet renew --pre-hook "/home/mms/nginx.sh stop" --post-hook "/home/mms/nginx.sh start"

#Retention (3 days)
find /usr/local/CatraMMS/logs -mtime +3 -type f -delete
find /usr/local/CatraMMS/storage/MMSGUI/temporaryPushUploads -mtime +3 -type f -delete
find /usr/local/CatraMMS/storage/IngestionRepository -mtime +3 -type f -delete
find /usr/local/CatraMMS/storage/MMSWorkingAreaRepository -mtime +3 -type f -delete

