#!/bin/bash

MMSVAR="/var/mms"
MMSOPT="/opt/mms"

ssh-port()
{
	read -n 1 -s -r -p "ssh port 9255..."
	echo ""

	echo "Port 9255" >> /etc/ssh/sshd_config
	/etc/init.d/ssh restart
}

mms-account-creation()
{
	moduleType=$1

	read -n 1 -s -r -p "mms account creation..."
	echo ""

	echo "groupadd mms..."
	groupadd mms
	groupId=$(getent group mms | cut -d':' -f3)
	echo "adduser, groupId: $groupId..."
	adduser --gid $groupId mms

	#add temporary mms to sudoers in order to install and configure the server
	echo "usermod..."
	usermod -aG sudo mms

	#to change the password of root
	echo "Change the password of root..."
	passwd

	echo ".ssh initialization..."
	mkdir /home/mms/.ssh
	chmod 700 /home/mms/.ssh
	touch /home/mms/.ssh/authorized_keys
	chmod 600 /home/mms/.ssh/authorized_keys
	chown -R mms:mms /home/mms/.ssh

	read -n 1 -s -r -p "Add the authorized_keys..."
	vi /home/mms/.ssh/authorized_keys
	echo ""

	echo "A partire da ubuntu 24.04, l'utente mms non assume l'id 1000 ma 1001. Poichè è importante che l'id sia 1000,"
	echo "altrimenti ci saranno problemi di scrittura dei file in quanto i mount autorizzano l'id 1000,"
	echo "forziamo l'id ad essere 1000 con il comando usermod"
	usermod -u 1000 mms

	if [ "$moduleType" == "externalDelivery" ]; then
		#nel caso di externalDelivery, nginx deve partire come root
		#perchè ascolta sulla porta 443. Per cui aggiungiamo il comando sotto (/bin/bash)
		#che si giustifica guardando lo script nginx.sh
		#Inoltre bisogna aggiungere /bin/kill perchè lo script crontab.sh, durante la rotazione dei log file di nginx, esegue la kill
		#su nginx (che gira come root) per fargli ricreare i file di log
		#Inoltre bisogna aggiungere /usr/bin/certbot per il comando del rinnovo del certificato (da quando abbiamo il rinnovo automatico
		#probabilmente non serve piu)

		echo "mms ALL=(ALL) NOPASSWD: /bin/bash, /bin/kill, /usr/bin/certbot" > "/etc/sudoers.d/mms-nginx-commands"
		chmod 440 "/etc/sudoers.d/mms-nginx-commands"
	fi
}

time-zone()
{
	read -n 1 -s -r -p "set time zone..."
	echo ""

	timedatectl set-timezone Europe/Rome

	#Ubuntu uses by default using systemd's timesyncd service.Though timesyncd is fine for most purposes,
	#some applications that are very sensitive to even the slightest perturbations in time may be better served by ntpd,
	#as it uses more sophisticated techniques to constantly and gradually keep the system time on track
	#Before installing ntpd, we should turn off timesyncd:
	echo "turn off timesyncd..."
	timedatectl set-ntp no

	systemctl stop systemd-timesyncd
	systemctl disable systemd-timesyncd
	apt-get -y remove systemd-timesyncd

	echo "update..."
	apt-get update

	echo "install ntp..."
	sleep 5
	apt-get -y install ntp

	echo "to force the synchronization..."
	service ntp stop
	ntpd -gq
	service ntp start

	#tutti utilizzano /etc/localtime tranne java che utilizza /etc/timezone.
	#/etc/localtime viene inizializzato correttamente dai comandi sopra.
	#/etc/timezone lo inizializzo io ora.
	#Qui trovi il commento trovato su Internet:
	#GNU libc (and thus any non-embedded Linux), reads /etc/localtime to determine the system's time zone (the default timezone if not overridden
	#by the TZ environment variable or by an application-specific setting). *BSD does the same thing. Some embedded Linux systems do things differently.
	#/etc/localtime should be a symbolic link to a file under /usr/share/zoneinfo/. Normal applications don't mind, they only read the contents
	#of the file, but system management utilities such as timedatectl care more because they can also change the setting, and they would do that
	#by changing the target of the symbolic link.
	#Java does (or did?) things differently: it reads /etc/timezone, which contains a timezone name, which should be the path to a file relative
	#to /usr/share/zoneinfo. I'm not aware of any other program that uses /etc/timezone, and I don't know why Sun chose to do things differently
	#from the rest of the world.
	echo "Europe/Rome" > /etc/timezone
}

install-packages()
{
	moduleType=$1

	read -n 1 -s -r -p "install-packages..."
	echo ""

	echo ""
	read -n 1 -s -r -p "update..."
	echo ""
	apt update

	echo ""
	read -n 1 -s -r -p "upgrade..."
	echo ""
	apt -y upgrade

	echo ""
	read -n 1 -s -r -p "install parallel..."
	echo ""
	apt-get -y install parallel

	if [ "$moduleType" == "storage" ]; then

		#for storage just nfs is enougth
		echo ""
		read -n 1 -s -r -p "install nfs-kernel-server..."
		echo ""
		apt -y install nfs-kernel-server

		#aggiungiamo anche nfs-common anche se non obbligatorio
		echo ""
		read -n 1 -s -r -p "install nfs-common..."
		echo ""
		apt-get -y install nfs-common

		return
	fi

	echo ""
	read -n 1 -s -r -p "install build-essential git..."
	echo ""
	apt-get -y install build-essential git

	echo ""
	read -n 1 -s -r -p "install nfs-common..."
	echo ""
	apt-get -y install nfs-common

	echo ""
	read -n 1 -s -r -p "install cifs-utils..."
	echo ""
	apt-get -y install cifs-utils

	echo ""
	read -n 1 -s -r -p "install libfcgi-dev..."
	echo ""
	apt-get -y install libfcgi-dev

	echo ""
	read -n 1 -s -r -p "install spawn-fcgi..."
	echo ""
	apt -y install spawn-fcgi

	#in order to compile MMS (~/dev/CatraMMS) it is needed libcurl-dev:
	echo ""
	read -n 1 -s -r -p "install libcurl4-openssl-dev..."
	echo ""
	apt-get -y install libcurl4-openssl-dev

	echo ""
	read -n 1 -s -r -p "install curl..."
	echo ""
	apt-get install curl

	echo ""
	read -n 1 -s -r -p "install libjpeg-dev..."
	echo ""
	apt-get -y install libjpeg-dev

	echo ""
	read -n 1 -s -r -p "install librtmp-dev..."
	echo ""
	apt-get -y install librtmp-dev
	apt-get -y install libsndio7.0

	echo ""
	read -n 1 -s -r -p "install libpng-dev..."
	echo ""
	apt-get -y install libpng-dev

	echo ""
	read -n 1 -s -r -p "install libtiff-dev..."
	echo ""
	apt-get -y install libtiff-dev

	echo ""
	read -n 1 -s -r -p "install jq..."
	echo ""
	apt-get -y install jq

	#used by ffmpeg:
	echo ""
	read -n 1 -s -r -p "install libxv1..."
	echo ""
	apt-get -y install libxv1

	echo ""
	read -n 1 -s -r -p "install libxcb-xfixes0-dev..."
	echo ""
	apt-get -y install libxcb-xfixes0-dev
	#apt-get -y install libsndio6.1 (non funziona con ubuntu 20)

	#This is to be able to compile MMS (NOT install in case no compilation has to be done)
	#apt-get -y install --no-install-recommends libboost-all-dev

	#used by the opencv package
	echo ""
	read -n 1 -s -r -p "install libdc1394-dev..."
	echo ""
	apt-get -y install libdc1394-dev

	echo ""
	read -n 1 -s -r -p "install libmysqlcppconn-dev..."
	echo ""
	apt-get -y install libmysqlcppconn-dev

	echo ""
	read -n 1 -s -r -p "install libpq-dev..."
	echo ""
	apt-get -y install libpq-dev

	#istallato in /opt
	#echo ""
	#read -n 1 -s -r -p "install libpqxx-dev..."
	#echo ""
	#apt-get -y install libpqxx-dev

	echo ""
	read -n 1 -s -r -p "install libtiff5..."
	echo ""
	apt-get -y install libtiff5

	echo ""
	read -n 1 -s -r -p "install libfontconfig1..."
	echo ""
	apt-get -y install libfontconfig1

	echo ""
	read -n 1 -s -r -p "install libasound2-dev..."
	echo ""
	apt-get -y install libasound2-dev

	echo ""
	read -n 1 -s -r -p "install libpangocairo-1.0-0..."
	echo ""
	apt-get install -y libpangocairo-1.0-0

	#Per il transcoder sat
	echo ""
	read -n 1 -s -r -p "install dvb-tools/dvblast..."
	echo ""
	apt install -y dvb-tools
	apt install -y dvblast

	#Per monitorare il traffico di rete
	echo ""
	read -n 1 -s -r -p "install vnstat..."
	echo ""
	apt install -y vnstat
	#restart automatico al boot e partenza del servizio
	systemctl enable --now vnstat

	if [ "$moduleType" == "api" -o "$moduleType" == "delivery" -o "$moduleType" == "api-and-delivery" -o "$moduleType" == "integration" -o "$moduleType" == "integration-aws" ]; then

		#non possiamo far decidere a ubuntu la versione java da istallare, bisogna istallare la versione java
		#supportata dal nostro applicativo
		#echo ""
		#read -n 1 -s -r -p "install jre..."
		#echo ""
		#apt install -y default-jre

		echo ""
		read -n 1 -s -r -p "install openjdk..."
		echo ""
		#apt install -y openjdk-11-jdk
		apt install -y openjdk-21-jdk
	fi

	if [ "$moduleType" == "api" -o "$moduleType" == "delivery" -o "$moduleType" == "api-and-delivery" ]; then
		echo ""
		read -n 1 -s -r -p "install hetzner cloud..."
		echo ""
		apt install -y hcloud-cli

		mkdir -p ~/.config/hcloud
		echo "active_context = 'MMS'" > ~/.config/hcloud/cli.toml
		echo "" >> ~/.config/hcloud/cli.toml
		echo "[[contexts]]" >> ~/.config/hcloud/cli.toml
		echo "name = 'MMS'" >> ~/.config/hcloud/cli.toml
		echo "token = '6zXnVEIpq78LbftKopCJYaRq8QNmrA7gSMsHDuG0comi75B95Ch0GEjH9Z7Gkrnk'" >> ~/.config/hcloud/cli.toml
	fi

	if [ "$moduleType" == "engine" ]; then

		dbType="postgres"

		#MYSQL
		if [ "$dbType" == "mysql" ]; then
			echo ""
			read -n 1 -s -r -p "install mysql-client..."
			echo ""
			apt-get -y install mysql-client

			echo ""
			read -n 1 -s -r -p "install mysql-server..."
			echo ""
			apt-get -y install mysql-server

			echo ""
			echo -n "Type the DB name: "
			read dbName
			echo -n "Type the DB user: "
			read dbUser
			echo -n "Type the DB password: "
			read dbPassword
			echo "create database $dbName CHARACTER SET utf8mb4 COLLATE utf8mb4_0900_ai_ci" | mysql -u root -p$dbPassword


			echo "CREATE USER '$dbUser'@'%' IDENTIFIED BY '$dbPassword'" | mysql -u root -p$dbPassword
			echo "GRANT ALL PRIVILEGES ON *.* TO '$dbUser'@'%' WITH GRANT OPTION" | mysql -u root -p$dbPassword
			#grant process allows mysqldump
			echo "GRANT PROCESS ON *.* TO '$dbUser'@'%' WITH GRANT OPTION" | mysql -u root -p$dbPassword

			echo "CREATE USER '$dbUser'@'localhost' IDENTIFIED BY '$dbPassword'" | mysql -u root -p$dbPassword
			echo "GRANT ALL PRIVILEGES ON *.* TO '$dbUser'@'localhost' WITH GRANT OPTION" | mysql -u root -p$dbPassword
			#grant process allows mysqldump
			echo "GRANT PROCESS ON *.* TO '$dbUser'@'localhost' WITH GRANT OPTION" | mysql -u root -p$dbPassword


			readOnlyDBUser=${dbUser}_RO

			echo "CREATE USER '$readOnlyDBUser'@'%' IDENTIFIED BY '$dbPassword'" | mysql -u root -p$dbPassword
			echo "GRANT SELECT, CREATE TEMPORARY TABLES ON *.* TO '$readOnlyDBUser'@'%' WITH GRANT OPTION" | mysql -u root -p$dbPassword
			#grant process allows mysqldump
			echo "GRANT PROCESS ON *.* TO '$readOnlyDBUser'@'%' WITH GRANT OPTION" | mysql -u root -p$dbPassword

			echo "CREATE USER '$readOnlyDBUser'@'localhost' IDENTIFIED BY '$dbPassword'" | mysql -u root -p$dbPassword
			echo "GRANT SELECT, CREATE TEMPORARY TABLES ON *.* TO '$readOnlyDBUser'@'localhost' WITH GRANT OPTION" | mysql -u root -p$dbPassword
			#grant process allows mysqldump
			echo "GRANT PROCESS ON *.* TO '$readOnlyDBUser'@'localhost' WITH GRANT OPTION" | mysql -u root -p$dbPassword


			echo "Inside /etc/mysql/mysql.conf.d/mysqld.cnf change: bind-address, mysqlx-bind-address, max_connections, sort_buffer_size, server-id, skip-name-resolve, log_bin, binlog_expire_logs_seconds"

			echo "Follow the instructions to change the datadir (https://www.digitalocean.com/community/tutorials/how-to-move-a-mysql-data-directory-to-a-new-location-on-ubuntu-18-04)"

			echo "Then restart mysql and run the SQL command: create table if not exists MMS_TestConnection (testConnectionKey BIGINT UNSIGNED NOT NULL AUTO_INCREMENT, constraint MMS_TestConnection_PK PRIMARY KEY (testConnectionKey)) ENGINE=InnoDB"
		fi


		#Postgres
		if [ "$dbType" == "postgres" ]; then
			echo ""
			read -n 1 -s -r -p "install postgres-17..."
			echo ""

			#aggiungo i repository di postgresql
			sh -c 'echo "deb http://apt.postgresql.org/pub/repos/apt $(lsb_release -cs)-pgdg main" > /etc/apt/sources.list.d/pgdg.list'
			wget -qO- https://www.postgresql.org/media/keys/ACCC4CF8.asc | sudo tee /etc/apt/trusted.gpg.d/pgdg.asc &>/dev/null

			apt-get -y install postgresql-17 postgresql-contrib

			#dbName=mms
			#dbUser=mms
			#echo -n "Type the DB password: "
			#read dbPassword
			echo ""
			#echo "se serve una versione diversa di postgres bisogna:"
			#echo "- istallare la nuova versione"
			#echo "- rimuovere la vecchia versione"
			#echo "- assegnare la porta 5432 alla nuova versione"
			#read
			echo "seguire il paragrafo 'Config initialization' del mio doc di Postgres"
			read
			echo "change the data directory following my 'postgres' document"
			read
			echo "seguire il paragrafo 'Credentials e createDB' del mio doc di Postgres"
			read
			echo "seguire il paragrafo 'Nodo slave' o 'Nodo master' rispettivamente se sei uno slave o un master"
			read
			#echo "sudo vi /etc/hosts inizializzare postgres-master, postgres-slaves e postgres-localhost (usato da servicesStatusLibrary.sh)"
			#read
			echo "se serve eseguire il comando sotto"
			echo "create table if not exists MMS_TestConnection (testConnectionKey integer)"
			read
			echo "Per la CDN dell'MMS, per calcolare il deliveryServer piu vicino player, serve l'estenzione earthdistance e cube."
			echo "Le estenzioni sono 'per database', quindi sul master, la replicazione la crea automaticamente anche sugli slave,"
			echo "collegarsi come superuser sul DB mms: sudo -u postgres psql -d mms"
			echo "ed eseguire il comando: "
			echo "CREATE EXTENSION earthdistance CASCADE;"
			read
		fi
	fi

	if [ "$moduleType" == "encoder" -o "$moduleType" == "externalEncoder" -o "$moduleType" == "externalDelivery" ]; then
		echo ""
		read -n 1 -s -r -p "install incron..."
		echo ""
		apt-get -y install incron
		systemctl enable incron.service
		mkdir -p /etc/systemd/system/incron.service.d
		echo "[Service]" > /etc/systemd/system/incron.service.d/override.conf
		echo "Restart=always" >> /etc/systemd/system/incron.service.d/override.conf
		echo "#attende 3 secondi prima del riavvio" >> /etc/systemd/system/incron.service.d/override.conf
		echo "RestartSec=3" >> /etc/systemd/system/incron.service.d/override.conf
		echo "#se il servizio viene spesso riavviato da systemd (supera la soglia StartLimitBurst in quel periodo StartLimitIntervalSec)," >> /etc/systemd/system/incron.service.d/override.conf
		echo "#systemd blocca ulteriori riavvii e segna il servizio come “crashed permanently" >> /etc/systemd/system/incron.service.d/override.conf
		echo "#Questo parametro disattiva il controllo del numero di restart" >> /etc/systemd/system/incron.service.d/override.conf
		echo "StartLimitIntervalSec=0" >> /etc/systemd/system/incron.service.d/override.conf
		systemctl daemon-reload
		service incron start

		echo "mms" > /etc/incron.allow
	fi
}

install-ftpserver()
{
	read -n 1 -s -r -p "install-ftpserver..."
	echo ""

	echo ""
	read -n 1 -s -r -p "update..."
	echo ""
	apt install vsftpd
	echo "in /etc/vsftpd.conf set"
	echo "anonymous_enable=NO"
	echo "local_enable=YES"
	echo "userlist_enable=YES"
	echo "userlist_deny=NO"
	echo "userlist_file=/etc/vsftpd_user_list"

	echo "write_enable=YES"
	echo "local_umask=022"
	#echo "chroot_local_user=YES"

	echo "#logging:"
	echo "dual_log_enable=YES"
	echo "xferlog_enable=YES"
	echo "xferlog_file=${MMSVAR}/logs/vsftpd/vsftpd_wuftp.log"
	echo "vsftpd_log_file=${MMSVAR}/logs/vsftpd/vsftpd_standard.log"
	echo "# If you want, you can have your log file in standard ftpd xferlog format"
	echo "xferlog_std_format=NO"
	echo "log_ftp_protocol=YES"

	echo "#timeouts in seconds"
	echo "idle_session_timeout=600"
	echo "data_connection_timeout=600"
		
	echo "#bytes al second"
	echo "local_max_rate=1024000"
	echo "#max_clients=1"
	echo "#max clients from the same IP"
	echo "max_per_ip=10"
	echo "#For security reason, it is allowed to STOR and RETR files but"
	echo "#it is not allowed to change the directory"
	echo "cmds_allowed=USER,PASS,SYST,TYPE,PWD,PORT,PASV,LIST,STOR,RETR,DELE,REST,MDTM,SIZE,QUIT"
	echo "abilitati all'FTP in /etc/vsftpd_user_list aggiungere solamente gli utenti abilitati all'FTP"

	echo "Add /sbin/nologin in /etc/shells"

	echo "listen_ipv6=NO"
	echo "listen=YES"
	echo "pasv_enable=Yes"
	echo "pasv_min_port=10090"
	echo "pasv_max_port=10100"
	echo "pasv_address=54.76.8.245"


	echo "To start the service at boot..."
	echo "systemctl enable vsftpd"
	echo "To restart the running service..."
	echo "systemctl restart vsftpd"


	echo "Per create un utente (i.e.: europa_tv)..."
	echo "recupero group id (of ftp group)..."
	echo "groupId=$(getent group ftp | cut -d':' -f3)"
	echo "adduser..."
	echo "adduser --gid $groupId --home /data/ftp-users/europa_tv europa_tv"
	echo "usermod europa_tv -s /sbin/nologin"
	echo "add user (europa_tv) to /etc/vsftpd_user_list"
}

create-directory()
{
	moduleType=$1

	read -n 1 -s -r -p "create-directory..."
	echo ""

	case "$moduleType" in
		"storage")
			create-directory-storage

			;;
		"api")
			create-directory-api $moduleType

			;;
		"api-and-delivery")
			create-directory-api $moduleType
			create-directory-delivery

			;;
		"delivery")
			create-directory-delivery

			;;
		"externalDelivery")
			create-directory-externalDelivery

			;;
		"engine")
			create-directory-engine

			;;
		"encoder")
			create-directory-encoder $moduleType

			;;
		"externalEncoder")
			create-directory-encoder $moduleType

			;;
		"integration")
			create-directory-integration

			;;
		"integration-aws")
			create-directory-integration

			;;
		*) echo "moduleType unknown: $moduleType"

			;;
	esac
}

create-directory-storage()
{
	mkdir -p ${MMSOPT}
	chown -R mms:mms ${MMSOPT}
}

create-directory-api()
{
	moduleType=$1

	mkdir -p ${MMSOPT}
	chown -R mms:mms ${MMSOPT}

	mkdir -p ${MMSVAR}
	mkdir -p ${MMSVAR}/pids
	chown -R mms:mms ${MMSVAR}

	if [ "$moduleType" == "api" ]; then
		read -n 1 -s -r -p "create the following directories (mkdir -p /mnt/storage-1/commonConfiguration; chown -R mms:mms /mnt/storage-1), press a key once done"
		echo ""
		read -n 1 -s -r -p "set /etc/fstab and mount the dirs just created (es: 10.0.1.16:/mnt/storage-1/commonConfiguration /mnt/storage-1/commonConfiguration nfs defaults 0 0)"
		echo ""
	else
		read -n 1 -s -r -p "create the following directories (mkdir -p /mnt/mmsStorage-1/mmsIngestionRepository /mnt/mmsStorage-1/mmsRepository0000 /mnt/mmsStorage-1/commonConfiguration /mnt/mmsStorage-1/MMSGUI /mnt/mmsStorage-1/MMSLive /mnt/mmsStorage-1/MMSRepositoryFree; chown -R mms:mms /mnt/mmsStorage-1), press a key once done"
		echo ""
		read -n 1 -s -r -p "set /etc/fstab and mount the dirs just created"
		echo ""

		mkdir -p /mnt/mmsStorage-1/mmsIngestionRepository/users
	fi

	mkdir -p ${MMSVAR}/storage
	if [ ! -e /home/mms/storage ]; then
		ln -s ${MMSVAR}/storage /home/mms
	fi

	mkdir -p /mnt/local-data/logs/mmsAPI
	mkdir -p /mnt/local-data/logs/catraMMSWEBServices
	mkdir -p /mnt/local-data/logs/nginx
	if [ ! -e ${MMSVAR}/logs ]; then
		ln -s /mnt/local-data/logs ${MMSVAR}
	fi
	chown -R mms:mms /mnt/local-data/logs

	mkdir -p /mnt/local-data/cache/nginx
	if [ ! -e ${MMSVAR}/cache ]; then
		ln -s /mnt/local-data/cache ${MMSVAR}/cache
	fi
	chown -R mms:mms /mnt/local-data/cache

	if [ ! -e ${MMSVAR}/storage/commonConfiguration ]; then
		ln -s /mnt/mmsStorage-1/commonConfiguration ${MMSVAR}/storage
	fi

	if [ ! -e /home/mms/logs ]; then
		ln -s ${MMSVAR}/logs /home/mms
	fi
}

create-directory-delivery()
{
	mkdir -p ${MMSOPT}
	chown -R mms:mms ${MMSOPT}

	mkdir -p ${MMSVAR}
	mkdir -p ${MMSVAR}/pids
	chown -R mms:mms ${MMSVAR}

	read -n 1 -s -r -p "create the following directories (mkdir -p /mnt/mmsStorage-1/mmsIngestionRepository /mnt/mmsStorage-1/mmsRepository0000 /mnt/mmsStorage-1/commonConfiguration /mnt/mmsStorage-1/MMSGUI /mnt/mmsStorage-1/MMSLive /mnt/mmsStorage-1/MMSRepositoryFree; chown -R mms:mms /mnt/mmsStorage-1), press a key once done"
	echo ""
	read -n 1 -s -r -p "set /etc/fstab and mount the dirs just created"
	echo ""

	#mkdir -p serve per evitare l'errore nel caso in cui la dir già esiste
	mkdir -p /mnt/mmsStorage-1/mmsIngestionRepository/users

	#aggiunta a seguito di externalDelivery
	mkdir -p ${MMSVAR}/storage/nginxWorkingAreaRepository

	mkdir -p ${MMSVAR}/storage/MMSRepository
	if [ ! -e /home/mms/storage ]; then
		ln -s ${MMSVAR}/storage /home/mms
	fi

	mkdir -p /mnt/local-data/logs/mmsAPI
	mkdir -p /mnt/local-data/logs/nginx
	mkdir -p /mnt/local-data/logs/rsyncd
	mkdir -p /mnt/local-data/logs/tomee-gui
	mkdir -p /mnt/local-data/logs/tomeeWorkDir/work
	mkdir -p /mnt/local-data/logs/tomeeWorkDir/temp
	mkdir -p /mnt/local-data/cache/nginx
	if [ ! -e ${MMSVAR}/logs ]; then
		ln -s /mnt/local-data/logs ${MMSVAR}
	fi
	if [ ! -e ${MMSVAR}/cache ]; then
		ln -s /mnt/local-data/cache ${MMSVAR}/cache
	fi
	chown -R mms:mms /mnt/local-data/logs
	chown -R mms:mms /mnt/local-data/cache

	if [ ! -e ${MMSVAR}/storage/IngestionRepository ]; then
		ln -s /mnt/mmsStorage-1/mmsIngestionRepository ${MMSVAR}/storage/IngestionRepository
	fi

	if [ ! -e ${MMSVAR}/storage/MMSGUI ]; then
		ln -s /mnt/mmsStorage-1/MMSGUI ${MMSVAR}/storage
	fi

	if [ ! -e ${MMSVAR}/storage/MMSRepository/MMS_0000 ]; then
		ln -s /mnt/mmsStorage-1/mmsRepository0000 ${MMSVAR}/storage/MMSRepository/MMS_0000
	fi
	if [ ! -e ${MMSVAR}/storage/MMSRepository/MMSLive ]; then
		ln -s /mnt/mmsStorage-1/MMSLive ${MMSVAR}/storage/MMSRepository
	fi

	if [ ! -e ${MMSVAR}/storage/MMSRepository-free ]; then
		ln -s /mnt/mmsStorage-1/MMSRepositoryFree ${MMSVAR}/storage/MMSRepository-free
	fi

	if [ ! -e ${MMSVAR}/storage/commonConfiguration ]; then
		ln -s /mnt/mmsStorage-1/commonConfiguration ${MMSVAR}/storage
	fi

	if [ ! -e /home/mms/logs ]; then
		ln -s ${MMSVAR}/logs /home/mms
	fi
}

create-directory-externalDelivery()
{
	mkdir -p ${MMSOPT}
	chown -R mms:mms ${MMSOPT}

	mkdir -p ${MMSVAR}
	mkdir -p ${MMSVAR}/pids
	chown -R mms:mms ${MMSVAR}

	read -n 1 -s -r -p "create the following directories (mkdir -p /mnt/mmsStorage-1/mmsRepository0000 /mnt/mmsStorage-1/MMSLive /mnt/mmsStorage-1/MMSRepositoryFree; chown -R mms:mms /mnt/mmsStorage-1), press a key once done"
	echo ""

	mkdir -p ${MMSVAR}/storage/nginxWorkingAreaRepository

	mkdir -p ${MMSVAR}/storage/MMSRepository
	if [ ! -e /home/mms/storage ]; then
		ln -s ${MMSVAR}/storage /home/mms
	fi

	mkdir -p /mnt/local-data/logs/mmsAPI
	mkdir -p /mnt/local-data/logs/nginx
	mkdir -p /mnt/local-data/logs/rsyncd
	mkdir -p /mnt/local-data/cache/nginx
	if [ ! -e ${MMSVAR}/logs ]; then
		ln -s /mnt/local-data/logs ${MMSVAR}
	fi
	if [ ! -e ${MMSVAR}/cache ]; then
		ln -s /mnt/local-data/cache ${MMSVAR}/cache
	fi
	chown -R mms:mms /mnt/local-data/logs
	chown -R mms:mms /mnt/local-data/cache

	if [ ! -e ${MMSVAR}/storage/MMSRepository/MMS_0000 ]; then
		ln -s /mnt/mmsStorage-1/mmsRepository0000 ${MMSVAR}/storage/MMSRepository/MMS_0000
	fi
	if [ ! -e ${MMSVAR}/storage/MMSRepository/MMSLive ]; then
		ln -s /mnt/mmsStorage-1/MMSLive ${MMSVAR}/storage/MMSRepository
	fi

	if [ ! -e ${MMSVAR}/storage/MMSRepository-free ]; then
		ln -s /mnt/mmsStorage-1/MMSRepositoryFree ${MMSVAR}/storage/MMSRepository-free
	fi

	if [ ! -e /home/mms/logs ]; then
		ln -s ${MMSVAR}/logs /home/mms
	fi
}

create-directory-engine()
{
	mkdir -p ${MMSOPT}
	chown -R mms:mms ${MMSOPT}

	mkdir -p ${MMSVAR}
	mkdir -p ${MMSVAR}/pids
	chown -R mms:mms ${MMSVAR}

	#Non usiamo RAID. Usiamo un disco separato di 1TB SSD dove mettere dati del DB. 500GB per il logs e 500GB per il sistema operativo
	read -n 1 -s -r -p "create the following directories (mkdir -p /mnt/local-data-logs /mnt/local-data-mmsDatabaseData /mnt/mmsStorage-1/mmsIngestionRepository /mnt/mmsStorage-1/mmsRepository0000 /mnt/mmsStorage-1/commonConfiguration /mnt/mmsStorage-1/dbDump /mnt/mmsStorage-1/MMSLive /mnt/mmsStorage-1/MMSWorkingAreaRepository; chown -R mms:mms /mnt/mmsStorage-1), press a key once done"
	echo ""
	read -n 1 -s -r -p "set /etc/fstab and mount the dirs just created"
	echo ""

	#mkdir -p serve per evitare l'errore nel caso in cui la dir già esiste
	mkdir -p /mnt/mmsStorage-1/mmsIngestionRepository/users

	mkdir -p ${MMSVAR}/storage/MMSRepository
	if [ ! -e /home/mms/storage ]; then
		ln -s ${MMSVAR}/storage /home/mms
	fi

	mkdir -p /mnt/local-data

	ln -s /mnt/local-data-logs /mnt/local-data/logs
	mkdir -p /mnt/local-data/logs/mmsEngineService

	ln -s /mnt/local-data-mmsDatabaseData /mnt/local-data/mmsDatabaseData

	#MMSTranscoderWorkingAreaRepository: confermato che serve all'engine per le sue attività con ffmpeg (ad esempio changeFileFormat)
	mkdir -p /mnt/local-data/MMSTranscoderWorkingAreaRepository/ffmpeg
	mkdir -p /mnt/local-data/MMSTranscoderWorkingAreaRepository/ffmpegEndlessRecursivePlaylist
	#questo link è importante perchè i path all'interno delle playlist in ffmpegEndlessRecursivePlaylist iniziano con storage/.../...
	ln -s ${MMSVAR}/storage /mnt/local-data/MMSTranscoderWorkingAreaRepository/ffmpegEndlessRecursivePlaylist/storage
	mkdir -p /mnt/local-data/MMSTranscoderWorkingAreaRepository/Staging
	if [ ! -e ${MMSVAR}/logs ]; then
		ln -s /mnt/local-data/logs ${MMSVAR}
	fi
	if [ ! -e ${MMSVAR}/storage/MMSTranscoderWorkingAreaRepository ]; then
		ln -s /mnt/local-data/MMSTranscoderWorkingAreaRepository ${MMSVAR}/storage
	fi
	chown -R mms:mms /mnt/local-data/logs/mmsEngineService
	chown -R mms:mms /mnt/local-data/MMSTranscoderWorkingAreaRepository

	if [ ! -e ${MMSVAR}/storage/commonConfiguration ]; then
		ln -s /mnt/mmsStorage-1/commonConfiguration ${MMSVAR}/storage
	fi
	if [ ! -e ${MMSVAR}/storage/dbDump ]; then
		ln -s /mnt/mmsStorage-1/dbDump ${MMSVAR}/storage
	fi
	if [ ! -e ${MMSVAR}/storage/IngestionRepository ]; then
		ln -s /mnt/mmsStorage-1/mmsIngestionRepository ${MMSVAR}/storage/IngestionRepository
	fi
	if [ ! -e ${MMSVAR}/storage/MMSRepository/MMS_0000 ]; then
		ln -s /mnt/mmsStorage-1/mmsRepository0000 ${MMSVAR}/storage/MMSRepository/MMS_0000
	fi
	if [ ! -e ${MMSVAR}/storage/MMSRepository/MMSLive ]; then
		ln -s /mnt/mmsStorage-1/MMSLive ${MMSVAR}/storage/MMSRepository
	fi

	mkdir -p /mnt/mmsStorage-1/MMSWorkingAreaRepository/nginx
	if [ ! -e ${MMSVAR}/storage/MMSWorkingAreaRepository ]; then
		ln -s /mnt/mmsStorage-1/MMSWorkingAreaRepository ${MMSVAR}/storage
	fi

	if [ ! -e /home/mms/logs ]; then
		ln -s ${MMSVAR}/logs /home/mms
	fi
}

create-directory-encoder()
{
	moduleType=$1

	mkdir -p ${MMSOPT}
	chown -R mms:mms ${MMSOPT}

	mkdir -p ${MMSVAR}
	mkdir -p ${MMSVAR}/pids
	chown -R mms:mms ${MMSVAR}

	if [ "$moduleType" == "encoder" ]; then
		read -n 1 -s -r -p "create the following directories (mkdir -p /mnt/mmsStorage-1/commonConfiguration /mnt/mmsStorage-1/MMSLive /mnt/mmsStorage-1/MMSWorkingAreaRepository /mnt/mmsStorage-1/mmsIngestionRepository /mnt/mmsStorage-1/mmsRepository0000; chown -R mms:mms /mnt/mmsStorage-1), press a key once done"
		echo ""
		read -n 1 -s -r -p "set /etc/fstab and mount the dirs just created above"
		echo ""
		#read -n 1 -s -r -p "create the following link (ln -s /mnt/storage-1 /mnt/mmsStorage)"
		#echo ""
		#read -n 1 -s -r -p "create the following links (ln -s /mnt/mmsRepository0000-1 /mnt/mmsRepository0000)"
		#echo ""
		#read -n 1 -s -r -p "create the following links (ln -s /mnt/mmsIngestionRepository-1 /mnt/mmsIngestionRepository)"
		#echo ""

		#mkdir -p serve per evitare l'errore nel caso in cui la dir già esiste
		mkdir -p /mnt/mmsStorage-1/mmsIngestionRepository/users
	fi

	mkdir -p ${MMSVAR}/storage/MMSRepository
	if [ ! -e /home/mms/storage ]; then
		ln -s ${MMSVAR}/storage /home/mms
	fi

	mkdir -p /mnt/local-data/logs/mmsEncoder
	mkdir -p /mnt/local-data/logs/nginx
	mkdir -p /mnt/local-data/logs/rsyncd
	if [ ! -e ${MMSVAR}/logs ]; then
		ln -s /mnt/local-data/logs ${MMSVAR}
	fi
	mkdir -p /mnt/local-data/MMSTranscoderWorkingAreaRepository/ffmpeg
	mkdir -p /mnt/local-data/MMSTranscoderWorkingAreaRepository/ffmpegEndlessRecursivePlaylist
	#questo link è importante perchè i path all'interno delle playlist in ffmpegEndlessRecursivePlaylist iniziano con storage/.../...
	ln -s ${MMSVAR}/storage /mnt/local-data/MMSTranscoderWorkingAreaRepository/ffmpegEndlessRecursivePlaylist/storage
	mkdir -p /mnt/local-data/MMSTranscoderWorkingAreaRepository/Staging
	if [ ! -e ${MMSVAR}/storage/MMSTranscoderWorkingAreaRepository ]; then
		ln -s /mnt/local-data/MMSTranscoderWorkingAreaRepository ${MMSVAR}/storage
	fi
	if [ ! -e ${MMSVAR}/storage/IngestionRepository ]; then
		ln -s /mnt/mmsStorage-1/mmsIngestionRepository ${MMSVAR}/storage/IngestionRepository
	fi
	#cache: anche se solo api, webapi e integration usano la cache, bisogna creare la dir anche per encoder, delivery perchè
	#path proxy_cache_path sono configurati in nginx.conf (globale a tutti gli nginx)
	mkdir -p /mnt/local-data/cache/nginx
	if [ ! -e ${MMSVAR}/cache ]; then
		ln -s /mnt/local-data/cache ${MMSVAR}
	fi
			chown -R mms:mms /mnt/local-data/logs
	chown -R mms:mms /mnt/local-data/MMSTranscoderWorkingAreaRepository
	chown -R mms:mms /mnt/local-data/cache

	mkdir -p ${MMSVAR}/tv
	chown -R mms:mms ${MMSVAR}/tv

	#link comodo per avere accesso ai file di log di ffmpeg
	ln -s ${MMSVAR}/storage/MMSTranscoderWorkingAreaRepository/ffmpeg /home/mms/

	if [ "$moduleType" == "encoder" ]; then
		if [ ! -e ${MMSVAR}/storage/commonConfiguration ]; then
			ln -s /mnt/mmsStorage-1/commonConfiguration ${MMSVAR}/storage
		fi
		mkdir -p /mnt/mmsStorage-1/MMSWorkingAreaRepository/nginx
		if [ ! -e ${MMSVAR}/storage/MMSWorkingAreaRepository ]; then
			ln -s /mnt/mmsStorage-1/MMSWorkingAreaRepository ${MMSVAR}/storage
		fi
		if [ ! -e ${MMSVAR}/storage/MMSRepository/MMS_0000 ]; then
			ln -s /mnt/mmsStorage-1/mmsRepository0000 ${MMSVAR}/storage/MMSRepository/MMS_0000
		fi
		if [ ! -e ${MMSVAR}/storage/MMSRepository/MMSLive ]; then
			ln -s /mnt/mmsStorage-1/MMSLive ${MMSVAR}/storage/MMSRepository
		fi
	else
		mkdir -p /mnt/local-data/MMSWorkingAreaRepository/nginx
		if [ ! -e ${MMSVAR}/storage/MMSWorkingAreaRepository ]; then
			ln -s /mnt/local-data/MMSWorkingAreaRepository ${MMSVAR}/storage
		fi
		mkdir -p /mnt/local-data/mmsRepository0000
		if [ ! -e ${MMSVAR}/storage/MMSRepository/MMS_0000 ]; then
			ln -s /mnt/local-data/mmsRepository0000 ${MMSVAR}/storage/MMSRepository/MMS_0000
		fi
		mkdir -p /mnt/local-data/MMSLive
		if [ ! -e ${MMSVAR}/storage/MMSRepository/MMSLive ]; then
			ln -s /mnt/local-data/MMSLive ${MMSVAR}/storage/MMSRepository
		fi

		chown -R mms:mms /mnt/local-data/MMSWorkingAreaRepository
		chown -R mms:mms /mnt/local-data/mmsRepository0000
		chown -R mms:mms /mnt/local-data/MMSLive
	fi

	if [ ! -e /home/mms/logs ]; then
		ln -s ${MMSVAR}/logs /home/mms
	fi
}

create-directory-integration()
{
	mkdir -p ${MMSOPT}
	chown -R mms:mms ${MMSOPT}

	mkdir -p ${MMSVAR}
	mkdir -p ${MMSVAR}/pids
	chown -R mms:mms ${MMSVAR}

	#DA VERIFICARE
	#mkdir -p /mnt/local-data/logs/tomcat-gui
	#mkdir -p /mnt/local-data/logs/tomcatWorkDir/work
	#mkdir -p /mnt/local-data/logs/tomcatWorkDir/temp
	mkdir -p /mnt/local-data/logs/nginx
	mkdir -p /mnt/local-data/cache/nginx
	if [ ! -e ${MMSVAR}/cache ]; then
		ln -s /mnt/local-data/cache ${MMSVAR}
	fi
	chown -R mms:mms /mnt/local-data/logs
	chown -R mms:mms /mnt/local-data/cache

	if [ ! -e ${MMSVAR}/logs ]; then
		ln -s /mnt/local-data/logs ${MMSVAR}
	fi

	if [ ! -e /home/mms/logs ]; then
		ln -s ${MMSVAR}/logs /home/mms
	fi
}

adds-to-bashrc()
{
	moduleType=$1

	read -n 1 -s -r -p "adds-to-bashrc..."
	echo ""

	read -n 1 -s -r -p ".bashrc..."
	echo ""
	echo -n "serverName for the 'bash prompt' (i.e. engine-db-1): "
	read serverName

	hostnamectl set-hostname $serverName

	if [ "$moduleType" != "storage" ]; then
		echo "export PATH=\$PATH:~mms" >> /home/mms/.bashrc
		echo "alias encoderLog='vi \$(printLogFileName.sh encoder)'" >> /home/mms/.bashrc
		echo "alias engineLog='vi \$(printLogFileName.sh engine)'" >> /home/mms/.bashrc
		echo "alias apiLog='vi \$(printLogFileName.sh api)'" >> /home/mms/.bashrc
	fi

	echo "alias h='history'" >> /home/mms/.bashrc
	echo "export EDITOR=/usr/bin/vi" >> /home/mms/.bashrc

	if [ "$moduleType" == "engine" ]; then
		echo "masterIP=\$(cat ~/mms/conf/mms-env.sh | grep MMS_DB_MASTER | cut -d'=' -f2)" >> /home/mms/.bashrc
		echo "if [ \"\$(ifconfig | grep \"inet \$masterIP\")\" != \"\" ]; then" >> /home/mms/.bashrc
		echo "	PS1=\${PS1//\\\\h/\\\\h-master-}" >> /home/mms/.bashrc
		echo "else" >> /home/mms/.bashrc
		echo "	PS1=\${PS1//\\\\h/\\\\h-slave-}" >> /home/mms/.bashrc
		echo "fi" >> /home/mms/.bashrc
		echo ""
		echo "alias tm='tail -f logs/mmsEngineService/mmsEngineService.log'" >> /home/mms/.bashrc
		echo "alias tme='tail -f logs/mmsEngineService/mmsEngineService-error.log'" >> /home/mms/.bashrc
		echo "alias tms='tail -f logs/mmsEngineService/mmsEngineService-slowquery.log'" >> /home/mms/.bashrc
	elif [[ "$moduleType" == *"ncoder"* ]]; then
		echo "alias tm='tail -f logs/mmsEncoder/mmsEncoder.log'" >> /home/mms/.bashrc
		echo "alias tme='tail -f logs/mmsEncoder/mmsEncoder-error.log'" >> /home/mms/.bashrc

		echo "PS1='$serverName-'\$PS1" >> /home/mms/.bashrc
	elif [[ "$moduleType" == *"api"* || "$moduleType" == *"elivery"* ]]; then
		echo "alias tm='tail -f logs/mmsAPI/mmsAPI.log'" >> /home/mms/.bashrc
		echo "alias tme='tail -f logs/mmsAPI/mmsAPI-error.log'" >> /home/mms/.bashrc
		echo "alias tw='tail -f logs/catraMMSWEBServices/catraMMSWEBServices.log'" >> /home/mms/.bashrc

		echo "PS1='$serverName-'\$PS1" >> /home/mms/.bashrc
	else
		echo "PS1='$serverName-'\$PS1" >> /home/mms/.bashrc
	fi

	echo "date" >> /home/mms/.bashrc
}

install-mms-packages()
{
	moduleType=$1

	#architecture=ubuntu-22.04
	architecture=ubuntu-24.04

	read -n 1 -s -r -p "install-mms-packages..."
	echo ""

	case "$moduleType" in
		"storage")
			install-mms-MMS-package $architecture
			install-mms-storage-conf $architecture
			configure-mms-sysctl $moduleType
			return
			;;
		"engine")
			install-mms-ImageMagick-package $architecture
			install-mms-FFMpeg-package $architecture
			install-mms-libpqxx-package $architecture
			install-mms-nginx-package $architecture $moduleType
			install-mms-opencv-package $architecture
			install-mms-youtube-dl-package $architecture
			install-mms-MMS-package $architecture
			install-mms-aws-sdk-cpp-package $architecture $moduleType
			install-mms-engine-conf $architecture

			echo "" > ~/.psqlrc
			echo "#evita che l’output sparisce" >> ~/.psqlrc
			echo "\setenv LESS -FX" >> ~/.psqlrc
			echo "" >> ~/.psqlrc
			echo "\timing on" >> ~/.psqlrc
			echo "" >> ~/.psqlrc
			echo "#senza \s e \t abbiamo un output simile a mysql" >> ~/.psqlrc
			echo "#\x off" >> ~/.psqlrc
			echo "#enable tuple, per avere solo tuple è necessario anche \x off" >> ~/.psqlrc
			echo "#\t on" >> ~/.psqlrc
			;;
		"api")
			install-mms-ImageMagick-package $architecture
			install-mms-FFMpeg-package $architecture
			install-mms-libpqxx-package $architecture
			install-mms-nginx-package $architecture $moduleType
			install-mms-opencv-package $architecture
			install-mms-youtube-dl-package $architecture
			install-mms-MMS-package $architecture
			install-mms-aws-sdk-cpp-package $architecture $moduleType
			install-mms-api-conf $architecture
			;;
		"delivery")
			install-mms-ImageMagick-package $architecture
			install-mms-FFMpeg-package $architecture
			install-mms-libpqxx-package $architecture
			install-mms-nginx-package $architecture $moduleType
			#install-mms-tomcat-package $architecture
			install-mms-tomee-package $architecture
			install-mms-opencv-package $architecture
			install-mms-youtube-dl-package $architecture
			install-mms-MMS-package $architecture
			install-mms-aws-sdk-cpp-package $architecture $moduleType
			install-mms-delivery-conf $architecture
			configure-mms-rsync-daemon-package
			configure-mms-sysctl $moduleType
			;;
		"externalDelivery")
			install-mms-ImageMagick-package $architecture
			install-mms-FFMpeg-package $architecture
			install-mms-libpqxx-package $architecture
			install-mms-nginx-package $architecture $moduleType
			install-mms-opencv-package $architecture
			install-mms-youtube-dl-package $architecture
			install-mms-MMS-package $architecture
			install-mms-aws-sdk-cpp-package $architecture $moduleType
			install-mms-externalDelivery-conf $architecture
			configure-mms-rsync-daemon-package
			configure-mms-sysctl $moduleType
			;;
		"api-and-delivery")
			install-mms-ImageMagick-package $architecture
			install-mms-FFMpeg-package $architecture
			install-mms-libpqxx-package $architecture
			install-mms-nginx-package $architecture $moduleType
			#install-mms-tomcat-package $architecture
			install-mms-tomee-package $architecture
			install-mms-opencv-package $architecture
			install-mms-youtube-dl-package $architecture
			install-mms-MMS-package $architecture
			install-mms-aws-sdk-cpp-package $architecture $moduleType
			install-mms-api-and-delivery-conf $architecture
			configure-mms-rsync-daemon-package
			configure-mms-sysctl $moduleType
			;;
		"encoder")
			install-mms-ImageMagick-package $architecture
			install-mms-FFMpeg-package $architecture
			install-mms-libpqxx-package $architecture
			install-mms-nginx-package $architecture $moduleType
			install-mms-opencv-package $architecture
			install-mms-youtube-dl-package $architecture
			install-mms-MMS-package $architecture
			install-mms-aws-sdk-cpp-package $architecture $moduleType
			install-mms-encoder-conf $architecture
			configure-mms-rsync-daemon-package
			configure-mms-sysctl $moduleType
			;;
		"externalEncoder")
			install-mms-ImageMagick-package $architecture
			install-mms-FFMpeg-package $architecture
			install-mms-libpqxx-package $architecture
			install-mms-nginx-package $architecture $moduleType
			install-mms-opencv-package $architecture
			install-mms-youtube-dl-package $architecture
			install-mms-MMS-package $architecture
			install-mms-aws-sdk-cpp-package $architecture $moduleType
			install-mms-externalEncoder-conf $architecture
			configure-mms-rsync-daemon-package
			configure-mms-sysctl $moduleType
			;;
		"integration")
			install-mms-FFMpeg-package $architecture
			install-mms-nginx-package $architecture $moduleType
			install-mms-MMS-package $architecture
			install-mms-aws-sdk-cpp-package $architecture $moduleType
			install-mms-integration-conf $architecture
			;;
		"integration-aws")
			install-mms-FFMpeg-package $architecture
			install-mms-nginx-package $architecture $moduleType
			install-mms-MMS-package $architecture
			install-mms-aws-sdk-cpp-package $architecture $moduleType
			install-mms-integration-conf $architecture
			;;
   	*) echo "$moduleType is not an option" >> $debugFilename
			;;
	esac

	if [ ! -e /home/mms/mmsStatusALL.sh ]; then
		ln -s /home/mms/mms/scripts/mmsStatusALL.sh /home/mms
	fi
	if [ ! -e /home/mms/mmsStartALL.sh ]; then
		ln -s /home/mms/mms/scripts/mmsStartALL.sh /home/mms
	fi
	if [ ! -e /home/mms/mmsStopALL.sh ]; then
		ln -s /home/mms/mms/scripts/mmsStopALL.sh /home/mms
	fi
	if [ ! -e /home/mms/nginx.sh ]; then
		ln -s ${MMSOPT}/MMS/scripts/nginx.sh /home/mms
	fi
	if [ ! -e /home/mms/mmsEncoder.sh ]; then
		ln -s ${MMSOPT}/MMS/scripts/mmsEncoder.sh /home/mms
	fi

	if [ ! -e /home/mms/micro-service.sh ]; then
		ln -s ${MMSOPT}/MMS/scripts/micro-service.sh /home/mms
	fi
	if [ ! -e /home/mms/mmsApi.sh ]; then
		ln -s ${MMSOPT}/MMS/scripts/mmsApi.sh /home/mms
	fi
	if [ ! -e /home/mms/mmsDelivery.sh ]; then
		ln -s ${MMSOPT}/MMS/scripts/mmsDelivery.sh /home/mms
	fi
	if [ ! -e /home/mms/mmsExternalDelivery.sh ]; then
		ln -s ${MMSOPT}/MMS/scripts/mmsExternalDelivery.sh /home/mms
	fi

	if [ ! -e /home/mms/mmsEngineService.sh ]; then
		ln -s ${MMSOPT}/MMS/scripts/mmsEngineService.sh /home/mms
	fi
	if [ ! -e /home/mms/mmsTail.sh ]; then
		ln -s ${MMSOPT}/MMS/scripts/mmsTail.sh /home/mms
	fi
	#if [ ! -e /home/mms/tomcat.sh ]; then
	#	ln -s ${MMSOPT}/MMS/scripts/tomcat.sh /home/mms
	#fi
	if [ ! -e /home/mms/tomee.sh ]; then
		ln -s ${MMSOPT}/MMS/scripts/tomee.sh /home/mms
	fi
	if [ ! -e /home/mms/printLogFileName.sh ]; then
		ln -s ${MMSOPT}/MMS/scripts/printLogFileName.sh /home/mms
	fi
}

install-mms-storage-conf()
{
	architecture=$1

	packageName=storageMmsConf
	echo ""
	package=$packageName
	echo "Downloading $package..."
	curl -o ~/$package.tar.gz "https://mms-delivery-f.catramms-cloud.com/packages/$architecture/$package.tar.gz"
	tar xvfz ~/$package.tar.gz -C ~mms

	chown -R mms:mms ~mms/mms
}

install-mms-encoder-conf()
{
	architecture=$1

	packageName=encoderMmsConf
	echo ""
	package=$packageName
	echo "Downloading $package..."
	curl -o ~/$package.tar.gz "https://mms-delivery-f.catramms-cloud.com/packages/$architecture/$package.tar.gz"
	tar xvfz ~/$package.tar.gz -C ~mms

	chown -R mms:mms ~mms/mms
}

install-mms-externalEncoder-conf()
{
	architecture=$1

	packageName=externalEncoderMmsConf
	echo ""
	package=$packageName
	echo "Downloading $package..."
	curl -o ~/$package.tar.gz "https://mms-delivery-f.catramms-cloud.com/packages/$architecture/$package.tar.gz"
	tar xvfz ~/$package.tar.gz -C ~mms

	chown -R mms:mms ~mms/mms
}

install-mms-engine-conf()
{
	architecture=$1

	packageName=engineMmsConf
	echo ""
	package=$packageName
	echo "Downloading $package..."
	curl -o ~/$package.tar.gz "https://mms-delivery-f.catramms-cloud.com/packages/$architecture/$package.tar.gz"
	tar xvfz ~/$package.tar.gz -C ~mms

	chown -R mms:mms ~mms/mms
}

install-mms-api-conf()
{
	architecture=$1

	packageName=apiMmsConf
	echo ""
	package=$packageName
	echo "Downloading $package..."
	curl -o ~/$package.tar.gz "https://mms-delivery-f.catramms-cloud.com/packages/$architecture/$package.tar.gz"
	tar xvfz ~/$package.tar.gz -C ~mms

	chown -R mms:mms ~mms/mms
}

install-mms-delivery-conf()
{
	architecture=$1

	packageName=deliveryMmsConf
	echo ""
	package=$packageName
	echo "Downloading $package..."
	curl -o ~/$package.tar.gz "https://mms-delivery-f.catramms-cloud.com/packages/$architecture/$package.tar.gz"
	tar xvfz ~/$package.tar.gz -C ~mms

	chown -R mms:mms ~mms/mms
}

install-mms-externalDelivery-conf()
{
	architecture=$1

	packageName=externalDeliveryMmsConf
	echo ""
	package=$packageName
	echo "Downloading $package..."
	curl -o ~/$package.tar.gz "https://mms-delivery-f.catramms-cloud.com/packages/$architecture/$package.tar.gz"
	tar xvfz ~/$package.tar.gz -C ~mms

	chown -R mms:mms ~mms/mms
}

install-mms-api-and-delivery-conf()
{
	architecture=$1

	packageName=apiAndDeliveryMmsConf
	echo ""
	package=$packageName
	echo "Downloading $package..."
	curl -o ~/$package.tar.gz "https://mms-delivery-f.catramms-cloud.com/packages/$architecture/$package.tar.gz"
	tar xvfz ~/$package.tar.gz -C ~mms

	chown -R mms:mms ~mms/mms
}

install-mms-integration-conf()
{
	architecture=$1

	packageName=integrationMmsConf
	echo ""
	package=$packageName
	echo "Downloading $package..."
	curl -o ~/$package.tar.gz "https://mms-delivery-f.catramms-cloud.com/packages/$architecture/$package.tar.gz"
	tar xvfz ~/$package.tar.gz -C ~mms

	chown -R mms:mms ~mms/mms
}

install-mms-aws-sdk-cpp-package()
{
	architecture=$1
	moduleType=$2

	packageName=aws-sdk-cpp
	package=$packageName
	read -n 1 -s -r -p "Downloading $package..."
	echo ""
	echo "Downloading $package..."
	curl -o ${MMSOPT}/$package.tar.gz "https://mms-delivery-f.catramms-cloud.com/packages/$architecture/$package.tar.gz"
	tar xvfz ${MMSOPT}/$package.tar.gz -C ${MMSOPT}

	if [ "$moduleType" == "externalEncoder" ]; then
		echo ""
		echo -n "Type the AWS Access Key Id: "
		read awsAccessKeyId
		echo ""
		echo -n "Type the AWS Secret Access Key: "
		read awsSecretAccessKey
		mkdir -p /home/mms/.aws
		echo "[default]" > /home/mms/.aws/credentials
		echo "aws_access_key_id = $awsAccessKeyId" >> /home/mms/.aws/credentials
		echo "aws_secret_access_key = $awsSecretAccessKey" >> /home/mms/.aws/credentials
	else
		ln -s ${MMSVAR}/storage/commonConfiguration/.aws ~mms
	fi
}

install-mms-youtube-dl-package()
{
	architecture=$1

	#Only in case we have to download it again, AS mms user
	#	mkdir ${MMSOPT}/youtube-dl-$(date +'%Y-%m-%d')
	#	curl -k -L https://yt-dl.org/downloads/latest/youtube-dl -o ${MMSOPT}/youtube-dl-$(date +'%Y-%m-%d')/youtube-dl
	#	chmod a+rx ${MMSOPT}/youtube-dl-$(date +'%Y-%m-%d')/youtube-dl
	#	rm ${MMSOPT}/youtube-dl; ln -s ${MMSOPT}/youtube-dl-$(date +'%Y-%m-%d') ${MMSOPT}/youtube-dl
	packageName=youtube-dl
	echo ""
	youtubeDlVersion=2022-08-07
	echo -n "$packageName version (i.e.: $youtubeDlVersion)? "
	read version
	if [ "$version" == "" ]; then
		version=$youtubeDlVersion
	fi
	package=$packageName-$version
	echo "Downloading $package..."
	curl -o ${MMSOPT}/$package.tar.gz "https://mms-delivery-f.catramms-cloud.com/packages/$architecture/$package.tar.gz"
	tar xvfz ${MMSOPT}/$package.tar.gz -C ${MMSOPT}
	ln -rs ${MMSOPT}/$package ${MMSOPT}/$packageName
}

install-mms-opencv-package()
{
	architecture=$1

	package=opencv
	read -n 1 -s -r -p "Downloading $package..."
	echo ""
	curl -o ${MMSOPT}/$package.tar.gz "https://mms-delivery-f.catramms-cloud.com/packages/$architecture/$package.tar.gz"
	tar xvfz ${MMSOPT}/$package.tar.gz -C ${MMSOPT}
}

install-mms-tomcat-package()
{
	architecture=$1

	echo ""
	tomcatVersion=9.0.105
	echo -n "tomcat version (i.e.: $tomcatVersion)? Look the version at https://www-eu.apache.org/dist/tomcat: "
	read VERSION
	if [ "$VERSION" == "" ]; then
		VERSION=$tomcatVersion
	fi
	wget https://www-eu.apache.org/dist/tomcat/tomcat-9/v${VERSION}/bin/apache-tomcat-${VERSION}.tar.gz -P /tmp
	tar -xvf /tmp/apache-tomcat-${VERSION}.tar.gz -C ${MMSOPT}
	ln -rs ${MMSOPT}/apache-tomcat-${VERSION} ${MMSOPT}/tomcat

	rm -rf ${MMSOPT}/tomcat/logs
	ln -s ${MMSVAR}/logs/tomcat-gui ${MMSOPT}/tomcat/logs

	#${MMSOPT}/tomcat/work viene anche usato da tomcat per salvare i chunks
	#di p:fileUpload della GUI catramms. Per questo motivo viene rediretto, tramite questo link,
	#in ${MMSVAR}/logs/tomcatWorkDir
	rm -rf ${MMSOPT}/tomcat/work
	ln -s ${MMSVAR}/logs/tomcatWorkDir/work ${MMSOPT}/tomcat/work
	#${MMSOPT}/tomcat/temp viene anche usato da tomcat per salvare i file temporanei (System.getProperty("java.io.tmpdir"))
	rm -rf ${MMSOPT}/tomcat/temp
	ln -s ${MMSVAR}/logs/tomcatWorkDir/temp ${MMSOPT}/tomcat/temp

	echo "<meta http-equiv=\"Refresh\" content=\"0; URL=/catramms/login.xhtml\"/>" > ${MMSOPT}/tomcat/webapps/ROOT/index.html

	chown -R mms:mms ${MMSOPT}/apache-tomcat-${VERSION}

	chmod u+x ${MMSOPT}/tomcat/bin/*.sh

	echo "[Unit]" > /etc/systemd/system/tomcat.service
	echo "Description=Tomcat 9 servlet container" >> /etc/systemd/system/tomcat.service
	echo "After=network.target" >> /etc/systemd/system/tomcat.service
	echo "" >> /etc/systemd/system/tomcat.service
	echo "[Service]" >> /etc/systemd/system/tomcat.service
	echo "Type=forking" >> /etc/systemd/system/tomcat.service
	echo "" >> /etc/systemd/system/tomcat.service
	echo "User=mms" >> /etc/systemd/system/tomcat.service
	echo "Group=mms" >> /etc/systemd/system/tomcat.service
	echo "" >> /etc/systemd/system/tomcat.service
	echo "Environment=\"JAVA_HOME=/usr/lib/jvm/java-21-openjdk-amd64\"" >> /etc/systemd/system/tomcat.service
	echo "Environment=\"JAVA_OPTS=-Djava.security.egd=file:///dev/urandom -Djava.awt.headless=true\"" >> /etc/systemd/system/tomcat.service
	echo "" >> /etc/systemd/system/tomcat.service
	echo "Environment=\"CATALINA_BASE=${MMSOPT}/tomcat\"" >> /etc/systemd/system/tomcat.service
	echo "Environment=\"CATALINA_HOME=${MMSOPT}/tomcat\"" >> /etc/systemd/system/tomcat.service
	echo "Environment=\"CATALINA_PID=${MMSVAR}/pids/tomcat.pid\"" >> /etc/systemd/system/tomcat.service
	echo "Environment=\"CATALINA_OPTS=-Xms512M -Xmx4096M -server -XX:+UseParallelGC\"" >> /etc/systemd/system/tomcat.service
	echo "" >> /etc/systemd/system/tomcat.service
	echo "ExecStart=${MMSOPT}/tomcat/bin/startup.sh" >> /etc/systemd/system/tomcat.service
	echo "ExecStop=${MMSOPT}/tomcat/bin/shutdown.sh" >> /etc/systemd/system/tomcat.service
	echo "" >> /etc/systemd/system/tomcat.service
	echo "[Install]" >> /etc/systemd/system/tomcat.service
	echo "WantedBy=multi-user.target" >> /etc/systemd/system/tomcat.service
	echo "" >> /etc/systemd/system/tomcat.service

	#notify systemd that a new unit file exists
	systemctl daemon-reload

	systemctl enable --now tomcat

	echo "Make sure inside tomcat/conf/server.xml we have:"
	echo ""
	echo "<Connector port=\"8080\" protocol=\"HTTP/1.1\""
	echo "address=\"127.0.0.1\""
	echo "connectionTimeout=\"20000\""
	echo "URIEncoding=\"UTF-8\""
	echo "redirectPort=\"8443\" />"
	echo ""
	echo "Make sure inside the Host tag we have:"
	echo ""
	echo "<Context path=\"/catramms\" docBase=\"catramms\" reloadable=\"true\">"
	echo "<WatchedResource>WEB-INF/web.xml</WatchedResource>"
	echo "</Context>"
	echo ""
	echo "copiare catramms.war in ${MMSOPT}/tomcat/webapps"
	echo "far partire tomcat in modo che crea la directory catramms"
	echo "ln -s ${MMSOPT}/tomcat/webapps/catramms/WEB-INF/classes/catramms.cloud.properties ${MMSOPT}/tomcat/conf/catramms.properties"
	#favicon is selected by the <link ...> tag inside the xhtml of the project
	#echo "cp ${MMSOPT}/tomcat/webapps/catramms/favicon_2.ico ${MMSOPT}/tomcat/webapps/ROOT/"
}

install-mms-tomee-package()
{
	architecture=$1

	echo ""
	tomeeVersion=10.1.2
	echo -n "tomee version (i.e.: $tomeeVersion)? Look the version of webprofile at https://tomee.apache.org/download.html: "
	read VERSION
	if [ "$VERSION" == "" ]; then
		VERSION=$tomeeVersion
	fi
	wget https://dlcdn.apache.org/tomee/tomee-${VERSION}/apache-tomee-${VERSION}-webprofile.tar.gz -P /tmp
	tar -xvf /tmp/apache-tomee-${VERSION}-webprofile.tar.gz -C ${MMSOPT}
	ln -rs ${MMSOPT}/apache-tomee-webprofile-${VERSION} ${MMSOPT}/tomee

	rm -rf ${MMSOPT}/tomee/logs
	ln -s ${MMSVAR}/logs/tomee-gui ${MMSOPT}/tomee/logs

	#${MMSOPT}/tomee/work viene anche usato da tomee per salvare i chunks
	#di p:fileUpload della GUI catramms. Per questo motivo viene rediretto, tramite questo link,
	#in ${MMSVAR}/logs/tomeeWorkDir
	rm -rf ${MMSOPT}/tomee/work
	ln -s ${MMSVAR}/logs/tomeeWorkDir/work ${MMSOPT}/tomee/work
	#${MMSOPT}/tomee/temp viene anche usato da tomee per salvare i file temporanei (System.getProperty("java.io.tmpdir"))
	rm -rf ${MMSOPT}/tomee/temp
	ln -s ${MMSVAR}/logs/tomeeWorkDir/temp ${MMSOPT}/tomee/temp

	echo "<meta http-equiv=\"Refresh\" content=\"0; URL=/catramms/login.xhtml\"/>" > ${MMSOPT}/tomee/webapps/ROOT/index.html

	chown -R mms:mms ${MMSOPT}/apache-tomee-webprofile-${VERSION}

	chmod u+x ${MMSOPT}/tomee/bin/*.sh

	echo "[Unit]" > /etc/systemd/system/tomee.service
	echo "Description=Tomee 10 servlet container" >> /etc/systemd/system/tomee.service
	echo "After=network.target" >> /etc/systemd/system/tomee.service
	echo "" >> /etc/systemd/system/tomee.service
	echo "[Service]" >> /etc/systemd/system/tomee.service
	echo "Type=forking" >> /etc/systemd/system/tomee.service
	echo "" >> /etc/systemd/system/tomee.service
	echo "User=mms" >> /etc/systemd/system/tomee.service
	echo "Group=mms" >> /etc/systemd/system/tomee.service
	echo "" >> /etc/systemd/system/tomee.service
	echo "Environment=\"JAVA_HOME=/usr/lib/jvm/java-21-openjdk-amd64\"" >> /etc/systemd/system/tomee.service
	echo "Environment=\"JAVA_OPTS=-Djava.security.egd=file:///dev/urandom -Djava.awt.headless=true\"" >> /etc/systemd/system/tomee.service
	echo "" >> /etc/systemd/system/tomee.service
	echo "Environment=\"CATALINA_BASE=${MMSOPT}/tomee\"" >> /etc/systemd/system/tomee.service
	echo "Environment=\"CATALINA_HOME=${MMSOPT}/tomee\"" >> /etc/systemd/system/tomee.service
	echo "Environment=\"CATALINA_PID=${MMSVAR}/pids/tomee.pid\"" >> /etc/systemd/system/tomee.service
	echo "Environment=\"CATALINA_OPTS=-Xms512M -Xmx4096M -server -XX:+UseParallelGC\"" >> /etc/systemd/system/tomee.service
	echo "" >> /etc/systemd/system/tomee.service
	echo "ExecStart=${MMSOPT}/tomee/bin/startup.sh" >> /etc/systemd/system/tomee.service
	echo "ExecStop=${MMSOPT}/tomee/bin/shutdown.sh" >> /etc/systemd/system/tomee.service
	echo "" >> /etc/systemd/system/tomee.service
	echo "[Install]" >> /etc/systemd/system/tomee.service
	echo "WantedBy=multi-user.target" >> /etc/systemd/system/tomee.service
	echo "" >> /etc/systemd/system/tomee.service

	#notify systemd that a new unit file exists
	systemctl daemon-reload

	systemctl enable --now tomee

	echo "Make sure inside tomee/conf/server.xml we have:"
	echo ""
	echo "<Connector port=\"8080\" protocol=\"HTTP/1.1\""
	echo "address=\"127.0.0.1\""
	echo "connectionTimeout=\"20000\""
	echo "URIEncoding=\"UTF-8\""
	echo "redirectPort=\"8443\" />"
	echo ""
	echo "Make sure inside the Host tag we have:"
	echo ""
	echo "<Context path=\"/catramms\" docBase=\"catramms\" reloadable=\"true\">"
	echo "<WatchedResource>WEB-INF/web.xml</WatchedResource>"
	echo "</Context>"
	echo ""
	echo "copiare catramms.war in ${MMSOPT}/tomee/webapps"
	echo "far partire tomee in modo che crea la directory catramms"
	echo "ln -s ${MMSOPT}/tomee/webapps/catramms/WEB-INF/classes/catramms.cloud.properties ${MMSOPT}/tomee/conf/catramms.properties"
	#favicon is selected by the <link ...> tag inside the xhtml of the project
	#echo "cp ${MMSOPT}/tomee/webapps/catramms/favicon_2.ico ${MMSOPT}/tomee/webapps/ROOT/"
}

install-mms-nginx-package()
{
	architecture=$1
	moduleType=$2

	#assicurarsi che non ci sia il servizio nginx attivo al boot
	systemctl disable nginx

	packageName=nginx
	echo ""
	nginxVersion=1.27.2
	echo -n "$packageName version (i.e.: $nginxVersion)? "
	read version
	if [ "$version" == "" ]; then
		version=$nginxVersion
	fi
	package=$packageName-$version
	echo "Downloading $package..."
	curl -o ${MMSOPT}/$package.tar.gz "https://mms-delivery-f.catramms-cloud.com/packages/$architecture/$package.tar.gz"
	tar xvfz ${MMSOPT}/$package.tar.gz -C ${MMSOPT}
	ln -rs ${MMSOPT}/$package ${MMSOPT}/$packageName

	#nginx configuration
	rm -rf ${MMSOPT}/nginx/logs
	ln -s ${MMSVAR}/logs/nginx ${MMSOPT}/nginx/logs

	if [[ -f ${MMSOPT}/nginx/conf/nginx.conf ]]
	then
		mv ${MMSOPT}/nginx/conf/nginx.conf ${MMSOPT}/nginx/conf/nginx.conf.backup
	fi
	ln -s ${MMSOPT}/MMS/conf/nginx.conf ${MMSOPT}/nginx/conf/

	mkdir ${MMSOPT}/nginx/conf/sites-enabled

	if [ "$moduleType" == "load-balancer" ]; then
		ln -s /home/mms/mms/conf/mmsLoadBalancer.nginx ${MMSOPT}/nginx/conf/sites-enabled/
	else
		ln -s /home/mms/mms/conf/mms.nginx ${MMSOPT}/nginx/conf/sites-enabled/
	fi

	#per evitare errori nginx: 24: Too many open files                                                        
	#In caso di un systemctl service servirebbe indicare nel file del servizio LimitNOFILE=500000
	#see https://access.redhat.com/solutions/1257953
	echo "fs.file-max = 70000" >> /etc/sysctl.conf                                                            
	#echo "mms soft nofile 10000" >> /etc/security/limits.conf                                                 
	#echo "mms hard nofile 30000" >> /etc/security/limits.conf                                                 
	echo "mms soft nofile 65536" >> /etc/security/limits.conf                                                 
	echo "mms hard nofile 65536" >> /etc/security/limits.conf                                                 

	#è possibile verificare il Max Open Files, ad es. di nginx, con il seguente comando:
	#cat /proc/$(pidof nginx | awk '{print $1}')/limits | grep "Max open files"

	if [ "$moduleType" == "externalDelivery" ]; then
		#serve certbot per la configurazione del certificato)
		apt install -y certbot python3-certbot-nginx
	
		#METODO MANUALE, NON PIU USATO
		#echo "creeremo un certificato il cui dominio sarà validato tramite un record TXT che dovremo configurare nel DNS."
		#echo -n "Scrivi il nome del server (i.e.: srv-1.cibortvlive.com)? "
		#read servername
		#echo "Dopo aver fatto la configurazione DNS (https://www.cloudns.net/profile) aspettare qualche minuto che si sia propagato"
		#echo "Se vuoi verificare che il record sia visibile, il comando è:"
		#echo "dig TXT _acme-challenge.$servername +short oppure tramite sito https://dnschecker.org"
		#Questo è piu semplice che validare il dominio tramite nginx (plugin o site)
		#certbot certonly --manual --preferred-challenges dns -d $servername

		#METODO AUTOMATICO
		echo -n "Scrivi il nome del server (i.e.: srv-1.cibortvlive.com)? "
		read servername
		# Cartella dove certbot scriverà i token di validazione. Infatti certbot chiamerà l'url:
		# http://us1-blade53-5.cibortvlive.com/.well-known/acme-challenge/<token>
		chown -R mms:mms /var/www/html
		#nginx è già partito? Se non funziona scrivere che il comando sotto deve essere eseguito a fine istallazione
		#Questo comando serve anche ad attivare un scheduled task to automatically renew this certificate in the background
		#Per vedere il timer attivo:
		#     sudo systemctl list-timers | grep certbot
		#  dovresti vedere qualcosa tipo: Sun 2025-10-26 08:00:00 UTC  ...  certbot.timer  certbot.service
		#Puoi anche controllare lo stato con:
		# 		sudo systemctl status certbot.timer
		certbot certonly --webroot -w /var/www/html -d $servername
		#Il comando sotto server a dire a certbot di ricaricare la conf di nginx una volta che il certificato viene rinnovato
		#in modo che nginx usi il nuovo certificato
		echo "deploy-hook = sudo -u mms /home/mms/nginx.sh reload sudo" >> /etc/letsencrypt/cli.ini
		
		#per avere la lista dei certificati
		#certbot certificates
		#per rimuovere un certificato (verrà mostrata la lista dei certificati e devi selezionarne uno)
		#certbot delete
		#Per il rinnovo del certificato aggiungere nel crontab il comando (lo script deploy-hook viene eseguito solo se il certificato viene effettivamente rinnovato):
		#sudo certbot renew --deploy-hook 'sudo -u mms /home/mms/nginx.sh stop && sleep 2 && sudo -u mms /home/mms/nginx.sh start' --quiet
		#Se si vuole estendere il certificato con un nuovo hostname, devi inserire entrambi gli hostnames
		#1. sudo certbot certonly --expand --manual --preferred-challenges dns -d us2-blade7-1.cbrtvlv.com -d us2-blade7-1.cibortvlive.com
		#2. aggiungi il nuovo hostname in ~/mms/conf/mms.nginx (campo server_name)
		#3 restart di nginx

		#inoltre blocchiamo (ritorno 444 che chiude la connessione senza inviare nessuna risposta) tutte le richieste HTTPS con Host sbagliato (tipo xj5zr.usdsh.com)
		#Per questo motivo bisogna generare i files invalid.crt e invalid.key utilizzati in mms.nginx
		openssl req -x509 -nodes -days 365 -newkey rsa:2048 \
  		-keyout /etc/ssl/invalid.key -out /etc/ssl/invalid.crt \
  		-subj "/CN=invalid.local"
	fi
}

configure-mms-rsync-daemon-package()
{
	#rsync è già nel sistema operativo

	#/etc/rsyncd.conf
	echo "# Global" > /etc/rsyncd.conf
	echo "uid = mms" >> /etc/rsyncd.conf
	echo "gid = mms" >> /etc/rsyncd.conf
	echo "use chroot = false" >> /etc/rsyncd.conf
	echo "max connections = 50" >> /etc/rsyncd.conf
	echo "log file = ${MMSVAR}/logs/rsyncd/rsyncd.log" >> /etc/rsyncd.conf
	echo "pid file = /run/rsyncd.pid" >> /etc/rsyncd.conf
	echo "timeout = 600" >> /etc/rsyncd.conf
	echo "" >> /etc/rsyncd.conf
	echo "# Modulo" >> /etc/rsyncd.conf
	echo "[mmsdata]" >> /etc/rsyncd.conf
  	echo "path = /mnt/mmsStorage-1/MMSLive" >> /etc/rsyncd.conf
  	echo "read only = no" >> /etc/rsyncd.conf
  	echo "list = yes" >> /etc/rsyncd.conf
  	echo "hosts allow = 10.0.0.0/8 192.168.0.0/16" >> /etc/rsyncd.conf
  	echo "hosts deny = *" >> /etc/rsyncd.conf
  	echo "transfer logging = no" >> /etc/rsyncd.conf

	#logrotate funziona con il sistema operativo, non bisogna istallare nulla
	#/etc/logrotate.d/rsyncd
	echo "${MMSVAR}/logs/rsyncd/rsyncd.log {" > /etc/logrotate.d/rsyncd
    	echo "daily" >> /etc/logrotate.d/rsyncd
    	echo "rotate 7" >> /etc/logrotate.d/rsyncd
    	echo "compress" >> /etc/logrotate.d/rsyncd
    	echo "missingok" >> /etc/logrotate.d/rsyncd
    	echo "notifempty" >> /etc/logrotate.d/rsyncd
    	echo "su mms mms" >> /etc/logrotate.d/rsyncd
    	echo "create 640 mms mms" >> /etc/logrotate.d/rsyncd
    	echo "postrotate" >> /etc/logrotate.d/rsyncd
       	echo "	systemctl reload rsync >/dev/null 2>&1 || true" >> /etc/logrotate.d/rsyncd
    	echo "endscript" >> /etc/logrotate.d/rsyncd
	echo "}" >> /etc/logrotate.d/rsyncd

	echo ""
	echo "Assicurati che nel servizio /usr/lib/systemd/system/rsync.service ci siano (User=root to be added after RestartSec)"
	echo "Restart=always"
	echo "RestartSec=3"
	echo "User=root"
	echo "NoNewPrivileges=off"
	echo ""
	read -n 1 -s -r -p "premi un tasto per continuare"
	echo ""

	systemctl daemon-reload
	systemctl enable --now rsync
}

configure-mms-sysctl()
{
	moduleType=$1

	if [ "$moduleType" == "storage" ]; then
		echo "" >> /etc/sysctl.conf
		echo "#because of storage" >> /etc/sysctl.conf
		echo "net.core.rmem_max = 134217728" >> /etc/sysctl.conf
		echo "net.core.wmem_max = 134217728" >> /etc/sysctl.conf
		echo "net.ipv4.tcp_rmem = 4096 87380 134217728" >> /etc/sysctl.conf
		echo "net.ipv4.tcp_wmem = 4096 65536 134217728" >> /etc/sysctl.conf
		echo "net.core.netdev_max_backlog = 30000" >> /etc/sysctl.conf
		echo ""
		echo "sysctl configured, sysctl -p #per attivare i nuovi parametri, non serve alcun restart dei servizi/processi"
		echo ""
		echo ""
		read -n 1 -s -r -p "premi un tasto per continuare"
	elif [ "$moduleType" == "encoder" -o "$moduleType" == "externalEncoder" -o "$moduleType" == "delivery" -o "$moduleType" == "externalDelivery" ]; then
		echo "" >> /etc/sysctl.conf
		echo "#because of $moduleType (rsyncd)" >> /etc/sysctl.conf
		echo "net.core.default_qdisc = fq" >> /etc/sysctl.conf
		echo "net.ipv4.tcp_congestion_control = bbr" >> /etc/sysctl.conf
		echo "net.ipv4.tcp_mtu_probing = 1" >> /etc/sysctl.conf
		echo "net.ipv4.tcp_slow_start_after_idle = 0" >> /etc/sysctl.conf
		echo "net.ipv4.tcp_window_scaling = 1" >> /etc/sysctl.conf
		echo "net.ipv4.tcp_timestamps = 1" >> /etc/sysctl.conf
		echo "net.core.netdev_max_backlog = 16384" >> /etc/sysctl.conf
		echo "net.core.rmem_max = 268435456" >> /etc/sysctl.conf
		echo "net.core.wmem_max = 268435456" >> /etc/sysctl.conf
		echo "net.ipv4.tcp_rmem = 4096 87380 268435456" >> /etc/sysctl.conf
		echo "net.ipv4.tcp_wmem = 4096 65536 268435456" >> /etc/sysctl.conf
		echo "net.ipv4.tcp_max_syn_backlog = 8192" >> /etc/sysctl.conf
		echo "net.ipv4.tcp_fin_timeout = 15" >> /etc/sysctl.conf
		echo ""
		echo "sysctl configured, sysctl -p #per attivare i nuovi parametri, non serve alcun restart dei servizi/processi"
		echo ""
		echo ""
		read -n 1 -s -r -p "premi un tasto per continuare"
	fi
}

install-mms-libpqxx-package()
{
	architecture=$1

	packageName=libpqxx
	echo ""
	libpqxxVersion=7.9.2
	echo -n "$packageName version (i.e.: $libpqxxVersion)? "
	read version
	if [ "$version" == "" ]; then
		version=$libpqxxVersion
	fi
	package=$packageName-$version
	echo "Downloading $package..."
	curl -o ${MMSOPT}/$package.tar.gz "https://mms-delivery-f.catramms-cloud.com/packages/$architecture/$package.tar.gz"
	tar xvfz ${MMSOPT}/$package.tar.gz -C ${MMSOPT}
	ln -rs ${MMSOPT}/$package ${MMSOPT}/$packageName
}

install-mms-MMS-package()
{
	architecture=$1

	packageName=MMS
	echo ""
	mmsVersion=1.0.7042
	echo -n "$packageName version (i.e.: $mmsVersion)? "
	read version
	if [ "$version" == "" ]; then
		version=$mmsVersion
	fi
	package=$packageName-$version
	echo "Downloading $package..."
	curl -o ${MMSOPT}/$package.tar.gz "https://mms-delivery-f.catramms-cloud.com/packages/$architecture/$package.tar.gz"
	tar xvfz ${MMSOPT}/$package.tar.gz -C ${MMSOPT}
	ln -rs ${MMSOPT}/$packageName-$version ${MMSOPT}/MMS
}

install-mms-ImageMagick-package()
{
	architecture=$1

	packageName=ImageMagick
	echo ""
	imageMagickVersion=7.1.1
	echo -n "$packageName version (i.e.: $imageMagickVersion)? "
	read version
	if [ "$version" == "" ]; then
		version=$imageMagickVersion
	fi
	package=$packageName-$version
	echo "Downloading $package..."
	curl -o ${MMSOPT}/$package.tar.gz "https://mms-delivery-f.catramms-cloud.com/packages/$architecture/$package.tar.gz"
	tar xvfz ${MMSOPT}/$package.tar.gz -C ${MMSOPT}
	ln -rs ${MMSOPT}/$package ${MMSOPT}/$packageName
}

install-mms-FFMpeg-package()
{
	architecture=$1

	packageName=ffmpeg
	echo ""
	ffmpegVersion=8.1
	echo -n "$packageName version (i.e.: $ffmpegVersion)? "
	read version
	if [ "$version" == "" ]; then
		version=$ffmpegVersion
	fi
	package=$packageName-$version
	echo "Downloading $package..."
	curl -o ${MMSOPT}/$package.tar.gz "https://mms-delivery-f.catramms-cloud.com/packages/$architecture/$package.tar.gz"
	tar xvfz ${MMSOPT}/$package.tar.gz -C ${MMSOPT}
	ln -rs ${MMSOPT}/$package ${MMSOPT}/$packageName
}

firewall-rules()
{
	moduleType=$1

	if [ "$moduleType" == "integration-aws" ]; then
		return
	fi

	read -n 1 -s -r -p "firewall-rules..."
	echo ""


	ufw default deny incoming
	ufw default allow outgoing

	#does not get the non-default port
	#ufw allow ssh
	ufw allow 9255

	#10.0.0.0/8: range 10.0.0.0 – 10.255.255.255 tutti ip di rete interna
	internalNetwork_10=10.0.0.0/8
	#192.168.0.0/16: range 192.168.0.0 – 192.168.255.255 tutti ip di rete interna
	internalNetwork_192_168=192.168.0.0/16

	if [ "$moduleType" == "encoder" ]; then
		#api and engine -> transcoder(nginx)
		ufw allow from $internalNetwork_10 to any port 8088	#encoder internal
		ufw allow from $internalNetwork_192_168 to any port 8088	#encoder internal

		#connection rtmp from public
		ufw allow 30000:31000/tcp
		#connection srt from public
		ufw allow 30000:31000/udp

		#rsyncd
		ufw allow from $internalNetwork_10 to any port 873 proto tcp
		ufw allow from $internalNetwork_192_168 to any port 873 proto tcp

	elif [ "$moduleType" == "externalEncoder" ]; then
		#external encoder (api ..., engine ...
		ufw allow from 195.201.58.41 to any port 8088 #api-2
		ufw allow from 178.63.22.93 to any port 8088 #api-3
		ufw allow from 78.46.101.27 to any port 8088 #api-4
		ufw allow from 167.235.10.244 to any port 8088 #engine-db-1
		ufw allow from 116.202.81.159 to any port 8088 #engine-db-3
		ufw allow from 5.9.81.10 to any port 8088 #engine-db-5

		echo "In case of a terrestrial/satellite solution, remember to add the rule"
		echo "ufw allow from 192.168.1.249 to 239.255.1.1"
		read
		#this allows multicast (terrestrial/satellite solution)
		#ufw allow out proto udp to 224.0.0.0/3
		#ufw allow out proto udp to ff00::/8
		#ufw allow in proto udp to 224.0.0.0/3
		#ufw allow in proto udp to ff00::/8

		#connection rtmp from public
		ufw allow 30000:31000/tcp
		#connection srt from public
		ufw allow 30000:31000/udp

		#rsyncd
		ufw allow from $internalNetwork_10 to any port 873 proto tcp
		ufw allow from $internalNetwork_192_168 to any port 873 proto tcp

	elif [ "$moduleType" == "api" ]; then
		# -> http(nginx) and https(nginx)
		ufw allow from $internalNetwork_10 to any port 8086		#mms-webapi
		ufw allow from $internalNetwork_192_168 to any port 8086		#mms-webapi
		ufw allow from $internalNetwork_10 to any port 8088		#mms-api
		ufw allow from $internalNetwork_192_168 to any port 8088		#mms-api

		echo "bisogna aggiungere l'IP di API/ENGINE tra le regole del firewall di tutti gli external transcoder (i.e.: aws, aruba, serverplan, ...). THIS IS VERY IMPORTANT altrimenti questi encoder, quando chiamati da API/ENGINE appariranno come 'not running' e i canali non potranno essere configurati su questi encoder"
		echo "Per lo stesso motivo, modificare la funzione firewall-rules (sezione externalEncoder) di questo script per aggiungere the rule with API/ENGINE IP address"
		read
	elif [ "$moduleType" == "delivery" ]; then
		# -> http(nginx) and https(nginx)
		ufw allow from $internalNetwork_10 to any port 8088		#mms-api
		ufw allow from $internalNetwork_10 to any port 8089		#mms-gui
		ufw allow from $internalNetwork_10 to any port 8090		#mms-binary
		ufw allow from $internalNetwork_10 to any port 8091		#mms-delivery
		ufw allow from $internalNetwork_10 to any port 8092		#mms-delivery-path
		ufw allow from $internalNetwork_10 to any port 8093		#mms-delivery-f

		#rsyncd
		ufw allow from $internalNetwork_10 to any port 873 proto tcp
		ufw allow from $internalNetwork_192_168 to any port 873 proto tcp

	elif [ "$moduleType" == "externalDelivery" ]; then
		#HTTP Per ora commentato perchè le richieste saranno su https. Se si abilitasse HTTP
		#dovremmo aggiungere la relativa sezione su mms.nginx che dovrebbe redirigere o autorizzare la richiesta
		#ufw allow 80 		#HTTP Per ora commentato perchè le richieste saranno su https
		ufw allow 443 	#HTTPS/SSL
		ufw allow 80 	#HTTP per permettere a certbot di aggiornare il certificato

		#rsyncd
		ufw allow from $internalNetwork_10 to any port 873 proto tcp
		ufw allow from $internalNetwork_192_168 to any port 873 proto tcp

	elif [ "$moduleType" == "api-and-delivery" ]; then
		# -> http(nginx) and https(nginx)
		ufw allow from $internalNetwork_10 to any port 8086		#mms-webapi
		ufw allow from $internalNetwork_10 to any port 8088		#mms-api
		ufw allow from $internalNetwork_10 to any port 8089		#mms-gui
		ufw allow from $internalNetwork_10 to any port 8090		#mms-binary
		ufw allow from $internalNetwork_10 to any port 8091		#mms-delivery
		ufw allow from $internalNetwork_10 to any port 8092		#mms-delivery-path
		ufw allow from $internalNetwork_10 to any port 8093		#mms-delivery-f

		echo "remember to add the API/ENGINE IP address to the firewall rules of any external transcoders (i.e.: aruba, serverplan, ...). THIS IS VERY IMPORTANT otherwise all those encoder, when called by API/ENGINE appear as 'not running' and the channels are not allocated to the encoder"
		echo "Per lo stesso motivo, modificare la funzione firewall-rules (sezione externalEncoder) di questo script per aggiungere the rule with API/ENGINE IP address"
		read

		#rsyncd
		ufw allow from $internalNetwork_10 to any port 873 proto tcp
		ufw allow from $internalNetwork_192_168 to any port 873 proto tcp

	elif [ "$moduleType" == "engine" ]; then
		# -> mysql/postgres
		#ufw allow 3306
		#echo ""
		#3306: commentato perchè non abbiamo piu mysql
		#ufw allow from $internalNetwork to any port 3306
		#anche se potrebbero esserci diverse versioni di postgres ognuna che ascolta su porte diverse,
		#è importante che la porta del postgres attivo sia la 5432 perchè questa porta è usata dappertutto:
		#dalla conf del load balancer per gli slaves, dagli script (monitoring agent, ....)
		ufw allow from $internalNetwork_10 to any port 5432

		echo "remember to add the API/ENGINE IP address to the firewall rules of any external transcoders (i.e.: aruba, serverplan, ...). THIS IS VERY IMPORTANT otherwise all those encoder, when called by API/ENGINE appear as 'not running' and the channels are not allocated to the encoder"
		echo "Per lo stesso motivo, modificare la funzione firewall-rules (sezione externalEncoder) di questo script per aggiungere the rule with API/ENGINE IP address"
		read
	elif [ "$moduleType" == "load-balancer" ]; then
		# -> http(nginx) and https(nginx)
		ufw allow 80
		ufw allow 443
		ufw allow 8088
	elif [ "$moduleType" == "storage" ]; then
		echo ""
		ufw allow from $internalNetwork_10 to any port 2049 proto tcp
		ufw allow from $internalNetwork_10 to any port 2049 proto udp

		ufw allow from $internalNetwork_10 to any port 111 proto tcp
		ufw allow from $internalNetwork_10 to any port 111 proto udp

		ufw allow from $internalNetwork_10 to any port 20048

		ufw allow from $internalNetwork_10 to any port 32765:32767 proto tcp
		ufw allow from $internalNetwork_10 to any port 32765:32767 proto udp
	elif [ "$moduleType" == "integration" ]; then
		ufw allow from $internalNetwork_10 to any port 8088		#cibortv
		ufw allow from $internalNetwork_10 to any port 8884		#icml
		ufw allow from $internalNetwork_10 to any port 3306		#mysql
		ufw allow from $internalNetwork_10 to any port 8090		#epg
		ufw allow from $internalNetwork_10 to any port 8091		#apk
		ufw allow from $internalNetwork_10 to any port 8886		#groupitaliantelevision
		ufw allow from $internalNetwork_10 to any port 2049		#NFS

	fi

	ufw enable
	ufw status verbose

	#to delete a rule it's the same command to allow with 'delete' after ufw, i.e.: ufw delete allow ssh

	#to allow port ranges
	#ufw allow 6000:6007/tcp
	#ufw allow 6000:6007/udp

	#to allow traffic from a specific IP address (client)
	#ufw allow from 203.0.113.4

	#to allow traffic from a specific IP address (client) and a specific port
	#ufw allow from 203.0.113.4 to any port 22

	#to allow traffic from a subnet (client)
	#ufw allow from 203.0.113.0/24

	#to allow traffic from a subnet (client) and a specific port
	#ufw allow from 203.0.113.0/24 to any port 22

	#To block or deny all packets from 192.168.1.5
	#sudo ufw deny from 192.168.1.5 to any

	#Instead of deny rule we can reject connection from any IP
	#Reject sends a reject response to the source, while the deny
	#(DROP) target sends nothing at all.
	#sudo ufw reject from 192.168.1.5 to any

	#status of UFW
	#ufw status verbose

	#to disable the firewall
	#ufw disable

	#to enable again
	#ufw enable

	#This will disable UFW and delete any rules that were previously defined.
	#This should give you a fresh start with UFW.
	#ufw reset
}

if [ $# -ne 1 ]
then
	echo "usage $0 <moduleType (load-balancer or engine or api or delivery or externalDelivery or api-and-delivery or encoder or externalEncoder or storage or integration or integration-aws)>"

	exit
fi

moduleType=$1

if [ "$moduleType" != "load-balancer" -a "$moduleType" != "engine" -a "$moduleType" != "api" -a "$moduleType" != "delivery" -a "$moduleType" != "externalDelivery" -a "$moduleType" != "api-and-delivery" -a "$moduleType" != "encoder" -a "$moduleType" != "externalEncoder" -a "$moduleType" != "storage" -a "$moduleType" != "integration" -a "$moduleType" != "integration-aws" ]; then
	echo "usage $0 <moduleType (load-balancer or engine or api or delivery or api-and-delivery or encoder or externalEncoder or storage or integration or integration-aws)>"

	exit
fi

#LEGGERE LEGGERE LEGGERE LEGGERE LEGGERE LEGGERE LEGGERE LEGGERE LEGGERE LEGGERE

echo ""
echo ""

echo "se bisogna formattare e montare dischi"
echo ""
echo "sudo fdisk /dev/nvme1n1 (p n p w)"
echo "sudo mkfs.ext4 /dev/nvme1n1p1"
echo "in fstab"
echo "UUID=XXXXXXXXXXXXX  /mnt/local-data-logs ext4 defaults 0 0"
echo "UUID=XXXXXXXXXXXXX  /mnt/local-data-mmsDatabaseData ext4 defaults 0 0"
echo "dove lo UUID puo essere recuperato con il comando lsblk -f"
read -n 1 -s -r -p "premi un tasto per continuare"
echo ""
echo ""

echo "Per sapere dove il cavo di rete è connesso bisogna eseguire il comando"
echo "ethtool enp24s0f1"
echo "e controllare se viene scritto: Link detected: yes"
echo "Per applicare la conf in /etv/netplan"
echo "netplan apply oppure netplan try"
echo "Per mostrare tutte le interfaccie"
echo "ip addr show"
echo "Per verificare lo stato di un link"
echo "ip addr show enp24s0f0"
echo "Per attivare un link che è down"
echo "ip link set dev enp24s0f0 up"
echo ""
echo ""

echo "In caso di server dedicato:"
echo ""
echo "seguire le istruzioni nel doc Hetzner Info in google drive per far comunicare la rete interna del cloud con il server dedicato"
read -n 1 -s -r -p "reboot of the server to apply the new network configuration..."
echo ""
echo ""

read -n 1 -s -r -p "La creazione delle dirs in /mnt viene gestita piu avanti, per cui non bisogna fare nulla"
echo ""
echo ""


if [[ $EUID -ne 0 ]]; then
    echo "Questo script deve essere eseguito come root" >&2
    exit 1
fi

if [ "$moduleType" != "integration-aws" ]; then
	ssh-port
fi
mms-account-creation $moduleType
time-zone
install-packages $moduleType

adds-to-bashrc $moduleType
if [ "$moduleType" == "storage" ]; then

	echo "Se alta concorrenza, aumenta thread NFS con"
	echo "EDITOR=vi systemctl edit nfs-server"
	echo "Elimina tutto il contenuto del file altrimenti il setting non viene applicato"
	echo "Nel file ci deve essere solamente"
	echo "[Service]"
	# 128 threads per essere molto performante (altre opzioni possono essere 16, 64)
	echo "ExecStart="
	echo "ExecStart=/usr/sbin/rpc.nfsd 128"
	echo ""
	echo ""
	echo "to avoid nfs to listen on random ports (we would have problems to open the firewall):"
	echo "edita /etc/nfs.conf"
	echo "[nfsd]"
	echo "port = 2049"
	echo "[mountd]"
	echo "port = 20048"
	echo "[statd]"
	echo "port = 32765"
	echo "outgoing-port = 32766"
	echo "[lockd]"
	echo "port = 32767"
	echo ""
	echo ""
	echo "Configurare /etc/exports con le directory da esportare, ad es, per externalDelivery:"
	echo "/mnt/mmsStorage-1/MMSLive 10.50.50.0/24(rw,sync,no_subtree_check,no_root_squash)"
	echo "/mnt/mmsStorage-1/mmsRepository0000 10.50.50.0/24(rw,sync,no_subtree_check,no_root_squash)"
	echo "/mnt/mmsStorage-1/MMSRepositoryFree 10.50.50.0/24(rw,sync,no_subtree_check,no_root_squash)"
	echo ""
	echo "/mnt/mmsStorage-1/dbDump 10.0.0.0/16(rw,sync,no_subtree_check,no_root_squash)"
	echo "/mnt/mmsStorage-1/MMSGUI 10.0.0.0/16(rw,sync,no_subtree_check,no_root_squash)"
	echo "/mnt/mmsStorage-1/mmsIngestionRepository 10.0.0.0/16(rw,sync,no_subtree_check,no_root_squash)"
	echo "/mnt/mmsStorage-1/MMSLive 10.0.0.0/16(rw,sync,no_subtree_check,no_root_squash)"
	echo "/mnt/mmsStorage-1/mmsRepository0000 10.0.0.0/16(rw,sync,no_subtree_check,no_root_squash)"
	echo "/mnt/mmsStorage-1/MMSRepositoryFree 10.0.0.0/16(rw,sync,no_subtree_check,no_root_squash)"
	echo "/mnt/mmsStorage-1/MMSWorkingAreaRepository 10.0.0.0/16(rw,sync,no_subtree_check,no_root_squash)"
	echo "/mnt/mmsStorage-1/nginxWorkingAreaRepository 10.0.0.0/16(rw,sync,no_subtree_check,no_root_squash)"
	echo ""
	echo ""
	echo "Comandi per ricaricare le conf sono:"
	echo "sudo exportfs -a"
	echo "sudo systemctl restart nfs-kernel-server"
	echo ""
	echo ""
	echo "Ottimizzazione di uno storage lato client, in /etc/fstab usare the following flags: ro,noatime,nodiratime,vers=4.1,soft,timeo=600,retrans=5,_netdev,x-systemd.automount (queste opzioni consentono: Unmount sempre possibile, Niente kernel lock,Timeout gestibile,Ottima per HLS, playlist,segmenti)
"
	echo "esempio: 192.168.100.1:/mnt/mmsStorage-1/MMSLive /mnt/mmsStorage-1/MMSLive nfs ro,noatime,nodiratime,vers=4.1,soft,timeo=600,retrans=5,_netdev,x-systemd.automount 0 0"
	echo "ro: se è sufficiente la sola lettura"
	echo "noatime,nodiratime: non aggiorna il timestamp di accesso, meno scritture"
	echo "vers=3: NFSv3 è spesso più veloce di v4 per HLS (meno overhead di locking), ma puoi testare anche vers=4.1"
	echo "async: conferma scritture più velocemente (rischio di perdita dati in caso di crash, ma OK per streaming)"
	echo ""
	echo ""
	read -n 1 -s -r -p "premi un tasto per continuare"
fi

echo ""
create-directory $moduleType
install-mms-packages $moduleType
firewall-rules $moduleType

if [ "$moduleType" == "delivery" -o "$moduleType" == "api-and-delivery" -o "$moduleType" == "externalDelivery" ]; then
	read -n 1 -s -r -p "tramite la GUI aggiungere il server di delivery e marcare la sua Key"
	echo ""
	echo ""
elif [ "$moduleType" == "encoder" -o "$moduleType" == "externalEncoder" ]; then
	read -n 1 -s -r -p "tramite la GUI aggiungere l'encoder e marcare la sua Key"
	echo ""
	echo ""
fi

read -n 1 -s -r -p "verificare ~/mms/conf/* (in particolare mms-env.sh) e attivare il crontab -u mms ~/mms/conf/crontab.txt"
echo ""
echo ""

read -n 1 -s -r -p "crontab -e di root ed aggiungere"
echo ""
read -n 1 -s -r -p "0 * * * * > /var/log/auth.log"
echo ""
echo ""

#htpasswd: curl -u asdkljc0:iMCjYNGTTUYu8YQikBSC1UUA1wr2
read -n 1 -s -r -p "in caso di integration, copiare il file .htpasswd in /etc (serve per il download di EPG e APK)"
read -n 1 -s -r -p "in caso di integration, aggiungere il file cibortv.env in /etc (serve per la scelta del dominio da parte dell'app)"
echo ""
echo ""

read -n 1 -s -r -p "Aggiornare foglio su google Server List's"
echo ""
echo ""

read -n 1 -s -r -p "Inserire alias in myBashProfile.sh oppure configurare in .ssh/config"
echo ""
echo ""


read -n 1 -s -r -p "Aggiornare <dev server>/.../scripts/servers.sh"
echo ""
echo ""

read -n 1 -s -r -p "in case of engine ibrido, cambiare hostname (istruzioni nel doc Hetzner info)"
echo ""
echo ""

read -n 1 -s -r -p "remove installServer.sh"
echo ""
echo ""

read -n 1 -s -r -p "remove ssh key from /home/ubuntu/.ssh/authorized_keys"
echo ""
echo ""

if [ "$moduleType" == "storage" ]; then

	echo "- fdisk and mkfs to format the disks"
	echo "- mkdir /mnt/MMSRepository/MMS_XXXX"
	echo "- initialize /etc/fstab"
	echo "- mount -a"
	echo "- chown -R mms:mms /mnt/MMSRepository"
	echo "- initialize /etc/exports"
	echo "- exportfs -ra"
else
	echo ""
	#echo "- in case of api/engine/load-balancer, initialize /etc/hosts (add db-master e db-slaves)"
	#echo ""
	echo "- run the commands as mms user <sudo mkdir /mnt/mmsRepository0001; sudo chown mms:mms /mnt/mmsRepository0001; ln -s /mnt/mmsRepository0001 ${MMSVAR}/storage/MMSRepository/MMS_0001> for the others repositories"
	echo ""
	echo "- in case of the storage is just created and has to be initialized OR in case of an external transcoder, run the following commands (it is assumed the storage partition is /mnt/mmsStorage): mkdir /mnt/mmsIngestionRepository; mkdir /mnt/mmsStorage/MMSGUI; mkdir /mnt/mmsStorage/MMSWorkingAreaRepository; mkdir /mnt/mmsStorage/MMSRepository-free; mkdir /mnt/mmsStorage/MMSLive; mkdir /mnt/mmsStorage/dbDump; mkdir /mnt/mmsStorage/commonConfiguration; chown -R mms:mms /mnt/mmsStorage/*"
	echo ""
	echo "- in case it is NOT an external transcoder OR it is a nginx-load-balancer, in /etc/fstab add:"
	echo "10.24.71.41:zpool-127340/mnt/mmsStorage	/mmsStorage	nfs	rw,_netdev,mountproto=tcp	0	0"
	echo "for each MMSRepository:"
	echo "10.24.71.41:zpool-127340/mmsRepository0000	/mmsRepository0000	nfs	rw,_netdev,mountproto=tcp	0	0"
	echo "if the NAS Repository does not have the access to the IP of the new server, add it, go to the OVH Site, login to the CiborTV project, click on Server → NAS e CDN, Aggiungi un accesso per mmsStorage, Aggiungi un accesso per mmsRepository0000"
	echo ""
fi

echo "if a temporary user has to be removed <deluser test>"
echo ""
echo "Restart of the machine and connect as ssh -p 9255 mms@<server ip>"
echo ""


