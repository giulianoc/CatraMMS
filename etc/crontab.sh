#!/bin/bash

#update certificate
sudo /usr/bin/certbot --quiet renew --pre-hook "/home/mms/nginx.sh stop" --post-hook "/home/mms/nginx.sh start"

#Retention (3 days)
find /var/CatraMMSData/logs -mtime +3 -type f -delete
find /var/CatraMMSData/storage/MMSGUI/temporaryPushUploads -mtime +3 -type f -delete
find /var/CatraMMSData/storage/IngestionRepository -mtime +3 -type f -delete
find /var/CatraMMSData/storage/MMSWorkingAreaRepository -mtime +3 -type f -delete

find /var/CatraMMSData/storage/DownloadRepository/* -empty -mtime +3 -type d -delete
find /var/CatraMMSData/storage/StreamingRepository/* -empty -mtime +3 -type d -delete
find /var/CatraMMSData/storage/MMSRepository/MMS_????/*/* -empty -mtime +3 -type d -delete
find /var/CatraMMSData/storage/MMSWorkingAreaRepository/Staging/* -empty -mtime +3 -type d -delete

