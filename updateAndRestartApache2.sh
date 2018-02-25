sudo service apache2 stop && echo "stopped" && make && cp API/src/API.fcgi /var/www/html/catracms/ && echo "copied" && sudo service apache2 start && tail -f /var/log/apache2/error.log
