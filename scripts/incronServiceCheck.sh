#!/bin/bash

#Qualche volta il servizio incron, attivo per gli external encoder, crash 
#Questo controllo non funziona all'interno del crontab (di root) perchè
#trova sempre il processo running confondendosi con il comando che fa partire il crontab:
#	/bin/sh -c pgrep -f incrond > /dev/null .....
#Per cui ho creato questo script incronServiceCheck.sh facendo attenzione che il nome del file non contiene incrond
#altrimenti avremmo lo stesso problema.
#Questo script sarà chiamato comunque all'interno del crontab di root

pgrep -f incrond > /dev/null

if [ $? -eq 1 ]
then
	systemctl start incron.service
fi

