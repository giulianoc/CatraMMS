#!/bin/bash

#'sudo su' before to run the command

ssh-port()
{
	read -n 1 -s -r -p "ssh port 9255..."
	echo ""

	echo "Port 9255" >> /etc/ssh/sshd_config
	/etc/init.d/ssh restart
}

mms-account-creation()
{
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

	echo "update..."
	apt-get update

	echo "install ntp..."
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

	if [ "$moduleType" == "storage" ]; then

		#for storage just nfs is enougth
		apt -y install nfs-kernel-server

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

	#in order to compile CatraMMS (~/dev/CatraMMS) it is needed libcurl-dev:
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

	#This is to be able to compile CatraMMS (NOT install in case no compilation has to be done)
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

	if [ "$moduleType" == "api" -o "$moduleType" == "delivery" -o "$moduleType" == "api-and-delivery" -o "$moduleType" == "integration" ]; then

		#non possiamo far decidere a ubuntu la versione java da istallare, bisogna istallare la versione java
		#supportata dal nostro applicativo
		#echo ""
		#read -n 1 -s -r -p "install jre..."
		#echo ""
		#apt install -y default-jre

		echo ""
		read -n 1 -s -r -p "install openjdk..."
		echo ""
		apt install -y openjdk-11-jdk
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
			echo "sudo vi /etc/hosts inizializzare postgres-master, postgres-slaves e postgres-localhost (usato da servicesStatusLibrary.sh)"
			read
			echo "se serve eseguire il comando sotto"
			echo "create table if not exists MMS_TestConnection (testConnectionKey integer)"
			read
		fi
	fi

	if [ "$moduleType" == "encoder" -o "$moduleType" == "externalEncoder" ]; then
		echo ""
		read -n 1 -s -r -p "install incron..."
		echo ""
		apt-get -y install incron
		systemctl enable incron.service
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
	echo "xferlog_file=/var/catramms/logs/vsftpd/vsftpd_wuftp.log"
	echo "vsftpd_log_file=/var/catramms/logs/vsftpd/vsftpd_standard.log"
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

	#preparazione mmsStorage (una tantum)
	if [ "$moduleType" != "integration" ]; then

		#mmsStorage è montato da tutti i moduli tranne integration

		if [ ! -e /mnt/mmsStorage ]; then
			ln -s /mnt/mmsStorage-1 /mnt/mmsStorage
		fi

		#mkdir -p serve per evitare l'errore nel caso in cui la dir già esiste
		mkdir -p /mnt/mmsStorage/commonConfiguration
		mkdir -p /mnt/mmsStorage/dbDump
		mkdir -p /mnt/mmsStorage/MMSGUI
		mkdir -p /mnt/mmsStorage/MMSLive
		mkdir -p /mnt/mmsStorage/MMSRepository-free
		mkdir -p /mnt/mmsStorage/MMSWorkingAreaRepository

		#links
		if [ "$moduleType" == "delivery" -o "$moduleType" == "api-and-delivery" -o "$moduleType" == "engine" ]; then
			if [ ! -e /mnt/mmsIngestionRepository ]; then
				ln -s /mnt/mmsIngestionRepository-1 /mnt/mmsIngestionRepository
			fi

			mkdir -p /mnt/mmsIngestionRepository/users
			if [ ! -e /mnt/mmsIngestionRepository ]; then
				ln -s /mnt/mmsIngestionRepository-1 /mnt/mmsIngestionRepository
			fi
		fi
		if [ "$moduleType" == "delivery" -o "$moduleType" == "api-and-delivery" -o "$moduleType" == "engine" -o "$moduleType" == "encoder" -o "$moduleType" == "externalEncoder" ]; then
			if [ ! -e /mnt/mmsRepository0000 ]; then
				ln -s /mnt/mmsRepository0000-1 /mnt/mmsRepository0000
			fi
		fi
	fi


	mkdir -p /opt/catramms

	mkdir -p /var/catramms
	mkdir -p /var/catramms/pids

	if [ "$moduleType" == "api" -o "$moduleType" == "api-and-delivery" ]; then
		mkdir -p /var/catramms/storage
		if [ ! -e /home/mms/storage ]; then
			ln -s /var/catramms/storage /home/mms
		fi

		mkdir -p /mnt/local-data/logs/mmsAPI
		mkdir -p /mnt/local-data/logs/catraMMSWEBServices
		mkdir -p /mnt/local-data/logs/nginx
		if [ ! -e /var/catramms/logs ]; then
			ln -s /mnt/local-data/logs /var/catramms
		fi
		chown -R mms:mms /mnt/local-data/logs

		mkdir -p /mnt/local-data/cache/nginx
		if [ ! -e /var/catramms/cache ]; then
			ln -s /mnt/local-data/cache /var/catramms/cache
		fi
		chown -R mms:mms /mnt/local-data/cache

		if [ ! -e /var/catramms/storage/commonConfiguration ]; then
			ln -s /mnt/mmsStorage/commonConfiguration /var/catramms/storage
		fi
	fi
	if [ "$moduleType" == "delivery" -o "$moduleType" == "api-and-delivery" ]; then	#insieme al delivery abbiamo anche la GUI
		mkdir -p /var/catramms/storage/MMSRepository
		if [ ! -e /home/mms/storage ]; then
			ln -s /var/catramms/storage /home/mms
		fi

		mkdir -p /mnt/local-data/logs/mmsAPI
		mkdir -p /mnt/local-data/logs/nginx
		mkdir -p /mnt/local-data/logs/tomcat-gui
		mkdir -p /mnt/local-data/logs/tomcatWorkDir/work
		mkdir -p /mnt/local-data/logs/tomcatWorkDir/temp
		mkdir -p /mnt/local-data/cache/nginx
		if [ ! -e /var/catramms/logs ]; then
			ln -s /mnt/local-data/logs /var/catramms
		fi
		if [ ! -e /var/catramms/cache ]; then
			ln -s /mnt/local-data/cache /var/catramms/cache
		fi
		chown -R mms:mms /mnt/local-data/logs
		chown -R mms:mms /mnt/local-data/cache

		if [ ! -e /var/catramms/storage/IngestionRepository ]; then
			ln -s /mnt/mmsIngestionRepository /var/catramms/storage/IngestionRepository
		fi

		if [ ! -e /var/catramms/storage/MMSGUI ]; then
			ln -s /mnt/mmsStorage/MMSGUI /var/catramms/storage
		fi

		if [ ! -e /var/catramms/storage/MMSRepository/MMS_0000 ]; then
			ln -s /mnt/mmsRepository0000 /var/catramms/storage/MMSRepository/MMS_0000
		fi
		if [ ! -e /var/catramms/storage/MMSRepository/MMSLive ]; then
			ln -s /mnt/mmsStorage/MMSLive /var/catramms/storage/MMSRepository
		fi

		if [ ! -e /var/catramms/storage/MMSRepository-free ]; then
			ln -s /mnt/mmsStorage/MMSRepository-free /var/catramms/storage
		fi

		if [ ! -e /var/catramms/storage/commonConfiguration ]; then
			ln -s /mnt/mmsStorage/commonConfiguration /var/catramms/storage
		fi
	fi
	if [ "$moduleType" == "engine" ]; then
		mkdir -p /var/catramms/storage/MMSRepository
		if [ ! -e /home/mms/storage ]; then
			ln -s /var/catramms/storage /home/mms
		fi

		mkdir -p /mnt/local-data/logs/mmsEngineService
		#MMSTranscoderWorkingAreaRepository: confermato che serve all'engine per le sue attività con ffmpeg (ad esempio changeFileFormat)
		mkdir -p /mnt/local-data/MMSTranscoderWorkingAreaRepository/ffmpeg
		mkdir -p /mnt/local-data/MMSTranscoderWorkingAreaRepository/ffmpegEndlessRecursivePlaylist
		#questo link è importante perchè i path all'interno delle playlist in ffmpegEndlessRecursivePlaylist iniziano con storage/.../...
		ln -s /var/catramms/storage /mnt/local-data/MMSTranscoderWorkingAreaRepository/ffmpegEndlessRecursivePlaylist/storage
		mkdir -p /mnt/local-data/MMSTranscoderWorkingAreaRepository/Staging
		if [ ! -e /var/catramms/logs ]; then
			ln -s /mnt/local-data/logs /var/catramms
		fi
		if [ ! -e /var/catramms/storage/MMSTranscoderWorkingAreaRepository ]; then
			ln -s /mnt/local-data/MMSTranscoderWorkingAreaRepository /var/catramms/storage
		fi
		chown -R mms:mms /mnt/local-data/logs/mmsEngineService
		chown -R mms:mms /mnt/local-data/MMSTranscoderWorkingAreaRepository

		if [ ! -e /var/catramms/storage/commonConfiguration ]; then
			ln -s /mnt/mmsStorage/commonConfiguration /var/catramms/storage
		fi
		if [ ! -e /var/catramms/storage/dbDump ]; then
			ln -s /mnt/mmsStorage/dbDump /var/catramms/storage
		fi
		if [ ! -e /var/catramms/storage/IngestionRepository ]; then
			ln -s /mnt/mmsIngestionRepository /var/catramms/storage/IngestionRepository
		fi
		if [ ! -e /var/catramms/storage/MMSRepository/MMS_0000 ]; then
			ln -s /mnt/mmsRepository0000 /var/catramms/storage/MMSRepository/MMS_0000
		fi
		if [ ! -e /var/catramms/storage/MMSRepository/MMSLive ]; then
			ln -s /mnt/mmsStorage/MMSLive /var/catramms/storage/MMSRepository
		fi

		mkdir -p /mnt/mmsStorage/MMSWorkingAreaRepository/nginx
		if [ ! -e /var/catramms/storage/MMSWorkingAreaRepository ]; then
			ln -s /mnt/mmsStorage/MMSWorkingAreaRepository /var/catramms/storage
		fi
	fi
	if [ "$moduleType" == "encoder" -o "$moduleType" == "externalEncoder" ]; then
		mkdir -p /var/catramms/storage/MMSRepository
		if [ ! -e /home/mms/storage ]; then
			ln -s /var/catramms/storage /home/mms
		fi

		mkdir -p /mnt/local-data/logs/mmsEncoder
		mkdir -p /mnt/local-data/logs/nginx
		if [ ! -e /var/catramms/logs ]; then
			ln -s /mnt/local-data/logs /var/catramms
		fi
		mkdir -p /var/catramms/storage/MMSRepository
		mkdir -p /mnt/local-data/MMSTranscoderWorkingAreaRepository/ffmpeg
		mkdir -p /mnt/local-data/MMSTranscoderWorkingAreaRepository/ffmpegEndlessRecursivePlaylist
		#questo link è importante perchè i path all'interno delle playlist in ffmpegEndlessRecursivePlaylist iniziano con storage/.../...
		ln -s /var/catramms/storage /mnt/local-data/MMSTranscoderWorkingAreaRepository/ffmpegEndlessRecursivePlaylist/storage
		mkdir -p /mnt/local-data/MMSTranscoderWorkingAreaRepository/Staging
		if [ ! -e /var/catramms/storage/MMSTranscoderWorkingAreaRepository ]; then
			ln -s /mnt/local-data/MMSTranscoderWorkingAreaRepository /var/catramms/storage
		fi
		#cache: anche se solo api, webapi e integration usano la cache, bisogna creare la dir anche per encoder, delivery perchè
		#path proxy_cache_path sono configurati in nginx.conf (globale a tutti gli nginx)
		mkdir -p /mnt/local-data/cache/nginx
		if [ ! -e /var/catramms/cache ]; then
			ln -s /mnt/local-data/cache /var/catramms
		fi
		chown -R mms:mms /mnt/local-data/logs
		chown -R mms:mms /mnt/local-data/MMSTranscoderWorkingAreaRepository
		chown -R mms:mms /mnt/local-data/cache

		mkdir -p /var/catramms/tv
		chown -R mms:mms /var/catramms/tv

		#link comodo per avere accesso ai file di log di ffmpeg
		ln -s /var/catramms/storage/MMSTranscoderWorkingAreaRepository/ffmpeg /home/mms/

		if [ "$moduleType" == "encoder" ]; then
			if [ ! -e /var/catramms/storage/commonConfiguration ]; then
				ln -s /mnt/mmsStorage/commonConfiguration /var/catramms/storage
			fi
			mkdir -p /mnt/mmsStorage/MMSWorkingAreaRepository/nginx
			if [ ! -e /var/catramms/storage/MMSWorkingAreaRepository ]; then
				ln -s /mnt/mmsStorage/MMSWorkingAreaRepository /var/catramms/storage
			fi
			if [ ! -e /var/catramms/storage/MMSRepository/MMS_0000 ]; then
				ln -s /mnt/mmsRepository0000 /var/catramms/storage/MMSRepository/MMS_0000
			fi
			if [ ! -e /var/catramms/storage/MMSRepository/MMSLive ]; then
				ln -s /mnt/mmsStorage/MMSLive /var/catramms/storage/MMSRepository
			fi
		else
			mkdir -p /mnt/local-data/MMSWorkingAreaRepository/nginx
			if [ ! -e /var/catramms/storage/MMSWorkingAreaRepository ]; then
				ln -s /mnt/local-data/MMSWorkingAreaRepository /var/catramms/storage
			fi
			mkdir -p /mnt/local-data/mmsRepository0000
			if [ ! -e /var/catramms/storage/MMSRepository/MMS_0000 ]; then
				ln -s /mnt/local-data/mmsRepository0000 /var/catramms/storage/MMSRepository/MMS_0000
			fi
			mkdir -p /mnt/local-data/MMSLive
			if [ ! -e /var/catramms/storage/MMSRepository/MMSLive ]; then
				ln -s /mnt/local-data/MMSLive /var/catramms/storage/MMSRepository
			fi

			chown -R mms:mms /mnt/local-data/MMSWorkingAreaRepository
			chown -R mms:mms /mnt/local-data/mmsRepository0000
			chown -R mms:mms /mnt/local-data/MMSLive
		fi
	fi
	if [ "$moduleType" == "integration" ]; then
		#DA VERIFICARE
		mkdir -p /mnt/local-data/logs/tomcat-gui
		mkdir -p /mnt/local-data/logs/tomcatWorkDir/work
		mkdir -p /mnt/local-data/logs/tomcatWorkDir/temp
		mkdir -p /mnt/local-data/logs/nginx
		mkdir -p /mnt/local-data/cache/nginx
		if [ ! -e /var/catramms/cache ]; then
			ln -s /mnt/local-data/cache /var/catramms
		fi
		chown -R mms:mms /mnt/local-data/logs
		chown -R mms:mms /mnt/local-data/cache
	fi

	if [ ! -e /home/mms/logs ]; then
		ln -s /var/catramms/logs /home/mms
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

	if [ "$moduleType" != "storage" ]; then
		echo "export PATH=\$PATH:~mms" >> /home/mms/.bashrc
		echo "alias encoderLog='vi \$(printLogFileName.sh encoder)'" >> /home/mms/.bashrc
		echo "alias engineLog='vi \$(printLogFileName.sh engine)'" >> /home/mms/.bashrc
		echo "alias apiLog='vi \$(printLogFileName.sh api)'" >> /home/mms/.bashrc
	fi

	echo "alias h='history'" >> /home/mms/.bashrc
	echo "export EDITOR=/usr/bin/vi" >> /home/mms/.bashrc

	if [[ "$serverName" == *"engine"* ]]; then
		echo "masterIP=\$(cat /etc/hosts | grep postgres-master | cut -d' ' -f1)" >> /home/mms/.bashrc
		echo "if [ \"\$(ifconfig | grep \"inet \$masterIP\")\" != \"\" ]; then" >> /home/mms/.bashrc
		echo "	PS1=\${PS1//\\\\h/\\\\h-master-}" >> /home/mms/.bashrc
		echo "else" >> /home/mms/.bashrc
		echo "	PS1=\${PS1//\\\\h/\\\\h-slave-}" >> /home/mms/.bashrc
		echo "fi" >> /home/mms/.bashrc
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

	#DA ELIMINARE, NON PENSO SERVE PIU
	#if [ "$moduleType" != "integration" ]; then
	#	package=jsoncpp
	#	read -n 1 -s -r -p "Downloading $package..."
	#	echo ""
	#	curl -o /opt/catramms/$package.tar.gz "https://mms-delivery-f.catramms-cloud.com/packages/$architecture/$package.tar.gz"
	#	tar xvfz /opt/catramms/$package.tar.gz -C /opt/catramms
	#fi


	if [ "$moduleType" != "integration" ]; then
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
		curl -o /opt/catramms/$package.tar.gz "https://mms-delivery-f.catramms-cloud.com/packages/$architecture/$package.tar.gz"
		tar xvfz /opt/catramms/$package.tar.gz -C /opt/catramms
		ln -rs /opt/catramms/$package /opt/catramms/$packageName
	fi


	if [ "$moduleType" != "integration" ]; then
		package=curlpp
		read -n 1 -s -r -p "Downloading $package..."
		echo ""
		curl -o /opt/catramms/$package.tar.gz "https://mms-delivery-f.catramms-cloud.com/packages/$architecture/$package.tar.gz"
		tar xvfz /opt/catramms/$package.tar.gz -C /opt/catramms
	fi


	packageName=ffmpeg
	echo ""
	ffmpegVersion=7.0.2
	echo -n "$packageName version (i.e.: $ffmpegVersion)? "
	read version
	if [ "$version" == "" ]; then
		version=$ffmpegVersion
	fi
	package=$packageName-$version
	echo "Downloading $package..."
	curl -o /opt/catramms/$package.tar.gz "https://mms-delivery-f.catramms-cloud.com/packages/$architecture/$package.tar.gz"
	tar xvfz /opt/catramms/$package.tar.gz -C /opt/catramms
	ln -rs /opt/catramms/$package /opt/catramms/$packageName


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
	curl -o /opt/catramms/$package.tar.gz "https://mms-delivery-f.catramms-cloud.com/packages/$architecture/$package.tar.gz"
	tar xvfz /opt/catramms/$package.tar.gz -C /opt/catramms
	ln -rs /opt/catramms/$package /opt/catramms/$packageName


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
	curl -o /opt/catramms/$package.tar.gz "https://mms-delivery-f.catramms-cloud.com/packages/$architecture/$package.tar.gz"
	tar xvfz /opt/catramms/$package.tar.gz -C /opt/catramms
	ln -rs /opt/catramms/$package /opt/catramms/$packageName

	#nginx configuration
	rm -rf /opt/catramms/nginx/logs
	ln -s /var/catramms/logs/nginx /opt/catramms/nginx/logs

	if [[ -f /opt/catramms/nginx/conf/nginx.conf ]]
	then
		mv /opt/catramms/nginx/conf/nginx.conf /opt/catramms/nginx/conf/nginx.conf.backup
	fi
	ln -s /opt/catramms/CatraMMS/conf/nginx.conf /opt/catramms/nginx/conf/

	mkdir /opt/catramms/nginx/conf/sites-enabled

	if [ "$moduleType" == "load-balancer" ]; then
		ln -s /home/mms/mms/conf/catrammsLoadBalancer.nginx /opt/catramms/nginx/conf/sites-enabled/
	else
		ln -s /home/mms/mms/conf/catramms.nginx /opt/catramms/nginx/conf/sites-enabled/
	fi

	#per evitare errori nginx: 24: Too many open files                                                        
	#In caso di un systemctl service servirebbe indicare nel file del servizio LimitNOFILE=500000
	#see https://access.redhat.com/solutions/1257953
	echo "fs.file-max = 70000" >> /etc/sysctl.conf                                                            
	echo "mms soft nofile 10000" >> /etc/security/limits.conf                                                 
	echo "mms hard nofile 30000" >> /etc/security/limits.conf                                                 

	if [ "$moduleType" == "delivery" -o "$moduleType" == "api-and-delivery" -o "$moduleType" == "integration" ]; then

		echo ""
		tomcatVersion=9.0.97
		echo -n "tomcat version (i.e.: $tomcatVersion)? Look the version at https://www-eu.apache.org/dist/tomcat: "
		read VERSION
		if [ "$VERSION" == "" ]; then
			VERSION=$tomcatVersion
		fi
		wget https://www-eu.apache.org/dist/tomcat/tomcat-9/v${VERSION}/bin/apache-tomcat-${VERSION}.tar.gz -P /tmp
		tar -xvf /tmp/apache-tomcat-${VERSION}.tar.gz -C /opt/catramms
		ln -rs /opt/catramms/apache-tomcat-${VERSION} /opt/catramms/tomcat

		rm -rf /opt/catramms/tomcat/logs
		ln -s /var/catramms/logs/tomcat-gui /opt/catramms/tomcat/logs

		#/opt/catramms/tomcat/work viene anche usato da tomcat per salvare i chunks
		#di p:fileUpload della GUI catramms. Per questo motivo viene rediretto, tramite questo link,
		#in /var/catramms/logs/tomcatWorkDir
		rm -rf /opt/catramms/tomcat/work
		ln -s /var/catramms/logs/tomcatWorkDir/work /opt/catramms/tomcat/work
		#/opt/catramms/tomcat/temp viene anche usato da tomcat per salvare i file temporanei (System.getProperty("java.io.tmpdir"))
		rm -rf /opt/catramms/tomcat/temp
		ln -s /var/catramms/logs/tomcatWorkDir/temp /opt/catramms/tomcat/temp

		echo "<meta http-equiv=\"Refresh\" content=\"0; URL=/catramms/login.xhtml\"/>" > /opt/catramms/tomcat/webapps/ROOT/index.html

		chown -R mms:mms /opt/catramms/apache-tomcat-${VERSION}

		chmod u+x /opt/catramms/tomcat/bin/*.sh

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
		echo "Environment=\"JAVA_HOME=/usr/lib/jvm/java-11-openjdk-amd64\"" >> /etc/systemd/system/tomcat.service
		echo "Environment=\"JAVA_OPTS=-Djava.security.egd=file:///dev/urandom -Djava.awt.headless=true\"" >> /etc/systemd/system/tomcat.service
		echo "" >> /etc/systemd/system/tomcat.service
		echo "Environment=\"CATALINA_BASE=/opt/catramms/tomcat\"" >> /etc/systemd/system/tomcat.service
		echo "Environment=\"CATALINA_HOME=/opt/catramms/tomcat\"" >> /etc/systemd/system/tomcat.service
		echo "Environment=\"CATALINA_PID=/var/catramms/pids/tomcat.pid\"" >> /etc/systemd/system/tomcat.service
		echo "Environment=\"CATALINA_OPTS=-Xms512M -Xmx4096M -server -XX:+UseParallelGC\"" >> /etc/systemd/system/tomcat.service
		echo "" >> /etc/systemd/system/tomcat.service
		echo "ExecStart=/opt/catramms/tomcat/bin/startup.sh" >> /etc/systemd/system/tomcat.service
		echo "ExecStop=/opt/catramms/tomcat/bin/shutdown.sh" >> /etc/systemd/system/tomcat.service
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
		echo "copiare catramms.war in /opt/catramms/tomcat/webapps"
		echo "far partire tomcat in modo che crea la directory catramms"
		echo "ln -s /opt/catramms/tomcat/webapps/catramms/WEB-INF/classes/catramms.cloud.properties /opt/catramms/tomcat/conf/catramms.properties"
		#favicon is selected by the <link ...> tag inside the xhtml of the project
		#echo "cp /opt/catramms/tomcat/webapps/catramms/favicon_2.ico /opt/catramms/tomcat/webapps/ROOT/"
	fi

	if [ "$moduleType" != "integration" ]; then
		package=opencv
		read -n 1 -s -r -p "Downloading $package..."
		echo ""
		curl -o /opt/catramms/$package.tar.gz "https://mms-delivery-f.catramms-cloud.com/packages/$architecture/$package.tar.gz"
		tar xvfz /opt/catramms/$package.tar.gz -C /opt/catramms
	fi


	if [ "$moduleType" != "integration" ]; then
		#Only in case we have to download it again, AS mms user
		#	mkdir /opt/catramms/youtube-dl-$(date +'%Y-%m-%d')
		#	curl -k -L https://yt-dl.org/downloads/latest/youtube-dl -o /opt/catramms/youtube-dl-$(date +'%Y-%m-%d')/youtube-dl
		#	chmod a+rx /opt/catramms/youtube-dl-$(date +'%Y-%m-%d')/youtube-dl
		#	rm /opt/catramms/youtube-dl; ln -s /opt/catramms/youtube-dl-$(date +'%Y-%m-%d') /opt/catramms/youtube-dl
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
		curl -o /opt/catramms/$package.tar.gz "https://mms-delivery-f.catramms-cloud.com/packages/$architecture/$package.tar.gz"
		tar xvfz /opt/catramms/$package.tar.gz -C /opt/catramms
		ln -rs /opt/catramms/$package /opt/catramms/$packageName
	fi


	if [ "$moduleType" != "integration" ]; then
		packageName=CatraLibraries
		echo ""
		catraLibrariesVersion=1.0.1990
		echo -n "$packageName version (i.e.: $catraLibrariesVersion)? "
		read version
		if [ "$version" == "" ]; then
			version=$catraLibrariesVersion
		fi
		package=$packageName-$version
		echo "Downloading $package..."
		curl -o /opt/catramms/$package.tar.gz "https://mms-delivery-f.catramms-cloud.com/packages/$architecture/$package.tar.gz"
		tar xvfz /opt/catramms/$package.tar.gz -C /opt/catramms
		ln -rs /opt/catramms/$packageName-$version /opt/catramms/$packageName
	fi


	packageName=CatraMMS
	echo ""
	catraMMSVersion=1.0.6181
	echo -n "$packageName version (i.e.: $catraMMSVersion)? "
	read version
	if [ "$version" == "" ]; then
		version=$catraMMSVersion
	fi
	package=$packageName-$version
	echo "Downloading $package..."
	curl -o /opt/catramms/$package.tar.gz "https://mms-delivery-f.catramms-cloud.com/packages/$architecture/$package.tar.gz"
	tar xvfz /opt/catramms/$package.tar.gz -C /opt/catramms
	ln -rs /opt/catramms/$packageName-$version /opt/catramms/$packageName


	packageName=aws-sdk-cpp
	package=$packageName
	read -n 1 -s -r -p "Downloading $package..."
	echo ""
	echo "Downloading $package..."
	curl -o /opt/catramms/$package.tar.gz "https://mms-delivery-f.catramms-cloud.com/packages/$architecture/$package.tar.gz"
	tar xvfz /opt/catramms/$package.tar.gz -C /opt/catramms

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
		ln -s /var/catramms/storage/commonConfiguration/.aws ~mms
	fi


	if [ "$moduleType" == "encoder" -o "$moduleType" == "externalEncoder" ]; then
		if [ "$moduleType" == "externalEncoder" ]; then
			packageName=externalEncoderMmsConf
		else
			packageName=encoderMmsConf
		fi
		echo ""
		package=$packageName
		echo "Downloading $package..."
		curl -o ~/$package.tar.gz "https://mms-delivery-f.catramms-cloud.com/packages/$architecture/$package.tar.gz"
		tar xvfz ~/$package.tar.gz -C ~mms

		chown -R mms:mms ~mms/mms
	fi
	if [ "$moduleType" == "engine" ]; then
		packageName=engineMmsConf
		echo ""
		package=$packageName
		echo "Downloading $package..."
		curl -o ~/$package.tar.gz "https://mms-delivery-f.catramms-cloud.com/packages/$architecture/$package.tar.gz"
		tar xvfz ~/$package.tar.gz -C ~mms

		chown -R mms:mms ~mms/mms
	fi
	if [ "$moduleType" == "api" ]; then
		packageName=apiMmsConf
		echo ""
		package=$packageName
		echo "Downloading $package..."
		curl -o ~/$package.tar.gz "https://mms-delivery-f.catramms-cloud.com/packages/$architecture/$package.tar.gz"
		tar xvfz ~/$package.tar.gz -C ~mms

		chown -R mms:mms ~mms/mms
	fi
	if [ "$moduleType" == "delivery" ]; then
		packageName=deliveryMmsConf
		echo ""
		package=$packageName
		echo "Downloading $package..."
		curl -o ~/$package.tar.gz "https://mms-delivery-f.catramms-cloud.com/packages/$architecture/$package.tar.gz"
		tar xvfz ~/$package.tar.gz -C ~mms

		chown -R mms:mms ~mms/mms
	fi
	if [ "$moduleType" == "api-and-delivery" ]; then
		packageName=apiAndDeliveryMmsConf
		echo ""
		package=$packageName
		echo "Downloading $package..."
		curl -o ~/$package.tar.gz "https://mms-delivery-f.catramms-cloud.com/packages/$architecture/$package.tar.gz"
		tar xvfz ~/$package.tar.gz -C ~mms

		chown -R mms:mms ~mms/mms
	fi

	chown -R mms:mms /opt/catramms
	chown -R mms:mms /var/catramms

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
		ln -s /opt/catramms/CatraMMS/scripts/nginx.sh /home/mms
	fi
	if [ ! -e /home/mms/mmsEncoder.sh ]; then
		ln -s /opt/catramms/CatraMMS/scripts/mmsEncoder.sh /home/mms
	fi

	if [ ! -e /home/mms/micro-service.sh ]; then
		ln -s /opt/catramms/CatraMMS/scripts/micro-service.sh /home/mms
	fi
	if [ ! -e /home/mms/mmsApi.sh ]; then
		ln -s /opt/catramms/CatraMMS/scripts/mmsApi.sh /home/mms
	fi
	if [ ! -e /home/mms/mmsDelivery.sh ]; then
		ln -s /opt/catramms/CatraMMS/scripts/mmsDelivery.sh /home/mms
	fi

	if [ ! -e /home/mms/mmsEngineService.sh ]; then
		ln -s /opt/catramms/CatraMMS/scripts/mmsEngineService.sh /home/mms
	fi
	if [ ! -e /home/mms/mmsTail.sh ]; then
		ln -s /opt/catramms/CatraMMS/scripts/mmsTail.sh /home/mms
	fi
	if [ ! -e /home/mms/tomcat.sh ]; then
		ln -s /opt/catramms/CatraMMS/scripts/tomcat.sh /home/mms
	fi
	if [ ! -e /home/mms/printLogFileName.sh ]; then
		ln -s /opt/catramms/CatraMMS/scripts/printLogFileName.sh /home/mms
	fi
}

firewall-rules()
{
	moduleType=$1

	read -n 1 -s -r -p "firewall-rules..."
	echo ""


	ufw default deny incoming
	ufw default allow outgoing

	#does not get the non-default port
	#ufw allow ssh
	ufw allow 9255

	if [ "$moduleType" == "encoder" ]; then
		#api and engine -> transcoder(nginx)
		ufw allow from 10.0.0.0/16 to any port 8088	#encoder internal

		#connection rtmp from public
		ufw allow 30000:31000/tcp
		#connection srt from public
		ufw allow 30000:31000/udp
	elif [ "$moduleType" == "externalEncoder" ]; then
		#external encoder (api ..., engine ...
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
	elif [ "$moduleType" == "api" ]; then
		# -> http(nginx) and https(nginx)
		#echo ""
		#echo -n "internalNetwork (i.e.: 10.0.0.0/16 (prod), the same for the test)? "
		#read internalNetwork
		internalNetwork=10.0.0.0/16
		ufw allow from $internalNetwork to any port 8086		#mms-webapi
		ufw allow from $internalNetwork to any port 8088		#mms-api

		echo "remember to add the API/ENGINE IP address to the firewall rules of any external transcoders (i.e.: aruba, serverplan, ...). THIS IS VERY IMPORTANT otherwise all those encoder, when called by API/ENGINE appear as 'not running' and the channels are not allocated to the encoder"
		echo "Per lo stesso motivo, modificare la funzione firewall-rules (sezione externalEncoder) di questo script per aggiungere the rule with API/ENGINE IP address"
		read
	elif [ "$moduleType" == "delivery" ]; then
		# -> http(nginx) and https(nginx)
		#echo ""
		#echo -n "internalNetwork (i.e.: 10.0.0.0/16 (prod), the same for the test)? "
		#read internalNetwork
		internalNetwork=10.0.0.0/16
		ufw allow from $internalNetwork to any port 8088		#mms-api
		ufw allow from $internalNetwork to any port 8089		#mms-gui
		ufw allow from $internalNetwork to any port 8090		#mms-binary
		ufw allow from $internalNetwork to any port 8091		#mms-delivery
		ufw allow from $internalNetwork to any port 8092		#mms-delivery-path
		ufw allow from $internalNetwork to any port 8093		#mms-delivery-f
	elif [ "$moduleType" == "api-and-delivery" ]; then
		# -> http(nginx) and https(nginx)
		#echo ""
		#echo -n "internalNetwork (i.e.: 10.0.0.0/16 (prod), the same for the test)? "
		#read internalNetwork
		internalNetwork=10.0.0.0/16
		ufw allow from $internalNetwork to any port 8086		#mms-webapi
		ufw allow from $internalNetwork to any port 8088		#mms-api
		ufw allow from $internalNetwork to any port 8089		#mms-gui
		ufw allow from $internalNetwork to any port 8090		#mms-binary
		ufw allow from $internalNetwork to any port 8091		#mms-delivery
		ufw allow from $internalNetwork to any port 8092		#mms-delivery-path
		ufw allow from $internalNetwork to any port 8093		#mms-delivery-f

		echo "remember to add the API/ENGINE IP address to the firewall rules of any external transcoders (i.e.: aruba, serverplan, ...). THIS IS VERY IMPORTANT otherwise all those encoder, when called by API/ENGINE appear as 'not running' and the channels are not allocated to the encoder"
		echo "Per lo stesso motivo, modificare la funzione firewall-rules (sezione externalEncoder) di questo script per aggiungere the rule with API/ENGINE IP address"
		read
	elif [ "$moduleType" == "engine" ]; then
		# -> mysql/postgres
		#ufw allow 3306
		#echo ""
		#echo -n "internalNetwork (i.e.: 10.0.0.0/16 (prod), the same for the test)? "
		#read internalNetwork
		internalNetwork=10.0.0.0/16
		#3306: commentato perchè non abbiamo piu mysql
		#ufw allow from $internalNetwork to any port 3306
		#anche se potrebbero esserci diverse versioni di postgres ognuna che ascolta su porte diverse,
		#è importante che la porta del postgres attivo sia la 5432 perchè questa porta è usata dappertutto:
		#dalla conf del load balancer per gli slaves, dagli script (monitoring agent, ....)
		ufw allow from $internalNetwork to any port 5432

		echo "remember to add the API/ENGINE IP address to the firewall rules of any external transcoders (i.e.: aruba, serverplan, ...). THIS IS VERY IMPORTANT otherwise all those encoder, when called by API/ENGINE appear as 'not running' and the channels are not allocated to the encoder"
		echo "Per lo stesso motivo, modificare la funzione firewall-rules (sezione externalEncoder) di questo script per aggiungere the rule with API/ENGINE IP address"
		read
	elif [ "$moduleType" == "load-balancer" ]; then
		# -> http(nginx) and https(nginx)
		ufw allow 80
		ufw allow 443
		ufw allow 8088
	elif [ "$moduleType" == "storage" ]; then
		ufw allow from 10.0.0.0/16 to any port nfs
		#ufw allow 111
		ufw allow from 10.0.0.0/16 to any port 111
		#ufw allow 13035
		ufw allow from 10.0.0.0/16 to any port 13035
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
	echo "usage $0 <moduleType (load-balancer or engine or api or delivery or api-and-delivery or encoder or externalEncoder or storage or integration)>"

	exit
fi

moduleType=$1

if [ "$moduleType" != "load-balancer" -a "$moduleType" != "engine" -a "$moduleType" != "api" -a "$moduleType" != "delivery" -a "$moduleType" != "api-and-delivery" -a "$moduleType" != "encoder" -a "$moduleType" != "externalEncoder" -a "$moduleType" != "storage" -a "$moduleType" != "integration" ]; then
	echo "usage $0 <moduleType (load-balancer or engine or api or delivery or api-and-delivery or encoder or externalEncoder or storage or integration)>"

	exit
fi

#LEGGERE LEGGERE LEGGERE LEGGERE LEGGERE LEGGERE LEGGERE LEGGERE LEGGERE LEGGERE

echo ""
echo ""

echo "se bisogna formattare e montare dischi"
echo "sudo fdisk /dev/nvme1n1 (p n p w)"
echo "sudo mkfs.ext4 /dev/nvme1n1p1"
echo "in fstab"
echo "UUID=XXXXXXXXXXXXX  /mnt/local-data-logs ext4 defaults 0 0"
echo "UUID=XXXXXXXXXXXXX  /mnt/local-data-mmsDatabaseData ext4 defaults 0 0"
echo "dove lo UUID puo essere recuperato con il comando lsblk -f"
read -n 1 -s -r -p "premi un tasto per continuare"
echo ""
echo ""

echo "In caso di server dedicato:"
echo "seguire le istruzioni nel doc Hetzner Info in google drive per far comunicare la rete interna del cloud con il server dedicato"
read -n 1 -s -r -p "reboot of the server to apply the new network configuration..."
echo ""
echo ""

echo "se c'è un mount per /mnt/local-data/logs, /mnt/mmsStorage, /mnt/mmsRepository000?, /mnt/MMSTranscoderWorkingAreaRepository (for encoder) aggiungere in /etc/fstab"
echo "e creare la directory"
echo "10.0.0.12:/mnt/mmsStorage /mnt/mmsStorage       nfs     nolock,hard     0       1"
echo "//u313562.your-storagebox.de/backup	/mnt/mmsRepository0000	cifs	username=u313562,password=vae3zh8wFVtooRiN,file_mode=0664,dir_mode=0775,uid=1000,gid=1000	0	0"
read -n 1 -s -r -p "andando avando con l'istallazione, dopo l'istallazione del pacchetto nfs e cifs eseguire il mount -a ..."
echo ""
echo ""


ssh-port
mms-account-creation
time-zone
install-packages $moduleType

adds-to-bashrc $moduleType
if [ "$moduleType" == "storage" ]; then

	echo "- to avoid nfs to listen on random ports (we would have problems open the firewall):"
	echo "- open /etc/default/nfs-kernel-server"
	echo "-	comment out the line RPCMOUNTDOPTS=--manage-gids"
	echo "- add the following line RPCMOUNTDOPTS=\"-p 13035\""
	echo "- Restart NFSd with sudo systemctl restart nfs-kernel-server"
else
	echo ""
	create-directory $moduleType
	install-mms-packages $moduleType
fi
firewall-rules $moduleType

read -n 1 -s -r -p "verificare ~/mms/conf/* e attivare il crontab -u mms ~/mms/conf/crontab.txt"
echo ""
echo ""

read -n 1 -s -r -p "verificare /etc/hosts"
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
	echo "- in case of api/engine/load-balancer, initialize /etc/hosts (add db-master e db-slaves)"
	echo ""
	echo "- run the commands as mms user <sudo mkdir /mnt/mmsRepository0001; sudo chown mms:mms /mnt/mmsRepository0001; ln -s /mnt/mmsRepository0001 /var/catramms/storage/MMSRepository/MMS_0001> for the others repositories"
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


