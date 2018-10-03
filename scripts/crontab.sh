#!/bin/bash

#update certificate
sudo /usr/bin/certbot --quiet renew --pre-hook "/opt/catramms/CatraMMS/scripts/nginx.sh stop" --post-hook "/opt/catramms/CatraMMS/scripts/nginx.sh start"

#Retention (3 days)
find /var/catramms/logs -mtime +3 -type f -delete
find /var/catramms/storage/MMSGUI/temporaryPushUploads -mtime +3 -type f -delete
find /var/catramms/storage/IngestionRepository -mtime +3 -type f -delete
find /var/catramms/storage/MMSWorkingAreaRepository -mtime +3 -type f -delete

find /var/catramms/storage/DownloadRepository/* -empty -mtime +3 -type d -delete
find /var/catramms/storage/StreamingRepository/* -empty -mtime +3 -type d -delete
find /var/catramms/storage/MMSRepository/MMS_????/*/* -empty -mtime +3 -type d -delete
find /var/catramms/storage/MMSWorkingAreaRepository/Staging/* -empty -mtime +3 -type d -delete

