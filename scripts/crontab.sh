#!/bin/bash

#update certificate
sudo /usr/bin/certbot --quiet renew --pre-hook "/opt/catramms/CatraMMS/scripts/nginx.sh stop" --post-hook "/opt/catramms/CatraMMS/scripts/nginx.sh start"

#Retention (3 days: 4320 mins, 1 day: 1440 mins)
find /var/catramms/logs -mmin +4320 -type f -delete
find /var/catramms/storage/MMSGUI/temporaryPushUploads -mmin +4320 -type f -delete
find /var/catramms/storage/IngestionRepository/ -mmin +4320 -type f -delete
find /var/catramms/storage/MMSWorkingAreaRepository/ -mmin +1440 -type f -delete

find /var/catramms/storage/DownloadRepository/* -empty -mmin +4320 -type d -delete
find /var/catramms/storage/StreamingRepository/* -empty -mmin +4320 -type d -delete
find /var/catramms/storage/MMSRepository/MMS_????/*/* -empty -mmin +4320 -type d -delete
find /var/catramms/storage/MMSWorkingAreaRepository/Staging/* -empty -mmin +1440 -type d -delete

