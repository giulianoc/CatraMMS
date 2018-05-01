#!/bin/bash

sudo /usr/bin/certbot --quiet renew --pre-hook "/home/mms/nginx.sh stop" --post-hook "/home/mms/nginx.sh start"

