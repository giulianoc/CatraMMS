#!/bin/bash

#RUN AS ROOR

export CatraMMS_PATH=/opt/catramms
export CatraMMS_PATH=/opt/catrasoftware/deploy
#aggiungere mkdir /var/catramms/satellite
#aggiungere mkdir /var/catramms/logs/satellite
#aggiungere retention per i log file del satellite
#i file di log potrebbero crescere troppo? questa é una omanda in generale

export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:$CatraMMS_PATH/ffmpeg/lib:$CatraMMS_PATH/ffmpeg/lib64
export PATH=$PATH:$CatraMMS_PATH/ffmpeg/bin


#mmsEncoder
#each frequency corrispond to a transponder providing several channels
#To minimize the number of devices/tuner/'usb TV device', the zapping command will get all the channels
#of the transponder (--all-pids parameter)

#1. when a new satellite frequency-channel starts, creates:
#	- a directory named with the frequency (transponder)
#	- a file inside the frequency directory named <ffmpeg listen port>.active
#		His content will be: <PID video>:<PID audio>
#2. when the frequency-channel stops, the extension of the file is renamed from active to removed

dvbChannelsPathName=/opt/catramms/CatraMMS/conf/dvb_channels.conf

debug=0


getFreeDeviceNumber()
{
    satelliteFrequency=$1

	selectedDeviceNumber=255

	for deviceNumber in 0 1 2 3 4 5 6 7 8 9
	do
		#-x parameter just tunes and exit

		dvbv5-zap $satelliteFrequency -a $deviceNumber -ss -x --all-pids -c $dvbChannelsPathName > /dev/null 2>&1
		if [ $? -eq 0 ]
		then
			selectedDeviceNumber=$deviceNumber

			break
		fi
	done

	if [ $debug -eq 1 ]; then
		echo "getFreeDeviceNumber. selectedDeviceNumber: $selectedDeviceNumber"
	fi

	return $selectedDeviceNumber
}

getActualDeviceNumber()
{
    satelliteFrequency=$1

	selectedDeviceNumber=255

	for deviceNumber in 0 1 2 3 4 5 6 7 8 9
	do
		pgrep -f "dvbv5-zap $satelliteFrequency -a $deviceNumber" > /dev/null
		pgrepStatus=$?
		if [ $pgrepStatus -eq 0 ]
		then
			selectedDeviceNumber=$deviceNumber

			break
		fi
	done

	if [ $debug -eq 1 ]; then
		echo "getActualDeviceNumber. selectedDeviceNumber: $selectedDeviceNumber"
	fi

	return $selectedDeviceNumber
}



satelliteChannelsDir=/var/catramms/satellite

satelliteFrequencies=$(ls $satelliteChannelsDir)
for satelliteFrequency in $satelliteFrequencies
do
	if [ $debug -eq 1 ]; then
		echo "satelliteFrequency: $satelliteFrequency"
	fi

	pidZapPathName=/var/catramms/pids/satellite_$satelliteFrequency"_zap.pid"

	zapToBeStarted=0

	pgrep -f "dvbv5-zap $satelliteFrequency" > /dev/null
	toBeRestarted=$?

	if [ $debug -eq 1 ]; then
		echo "dvbv5-zap $satelliteFrequency. toBeRestarted: $toBeRestarted"
	fi

	if [ $toBeRestarted -eq 1 ]
	then
		getFreeDeviceNumber $satelliteFrequency
		deviceNumber=$?
		if [ $deviceNumber -eq 255 ]
		then
			echo "No deviceNumber available"

			continue
		fi

		zapToBeStarted=1
	else
		getActualDeviceNumber $satelliteFrequency
		deviceNumber=$?
		if [ $deviceNumber -eq 255 ]
		then
			echo "ERROR: no deviceNumber found!!"

			continue
		fi
	fi

	if [ $debug -eq 1 ]; then
		echo "zapToBeStarted: $zapToBeStarted"
	fi

	activesNumber=0
	ffmpegCommandChanged=0

	#for each channel
	#-map i:0x200 -map i:0x28a -c:v copy -c:a copy -f mpegts udp://127.0.0.1:12345
	ffmpegMapParameters=""

	satelliteChannels=$(ls $satelliteChannelsDir/$satelliteFrequency)
	for satelliteChannel in $satelliteChannels
	do
		ffmpegListenPort=$(echo $satelliteChannel | cut -d'.' -f1)

		command=$(echo $satelliteChannel | cut -d'.' -f2)

		ffmpegListenPortRunning=$(ps -ef | grep ffmpeg | grep -v grep | grep "127.0.0.1:$ffmpegListenPort")

		if [ $debug -eq 1 ]; then
			echo "satelliteChannel: $satelliteChannel, ffmpegListenPort: $ffmpegListenPort, command: $command"
		fi

		if [ "$command" == "active" ]
		then
			activesNumber=$((activesNumber+1))

			if [ $debug -eq 1 ]; then
				echo "channelInfo path name: $satelliteChannelsDir/$satelliteFrequency/$satelliteChannel"
			fi

			channelInfo=$(cat $satelliteChannelsDir/$satelliteFrequency/$satelliteChannel)

			videoPid=$(echo $channelInfo | cut -d':' -f1)
			audioPid=$(echo $channelInfo | cut -d':' -f2)

			ffmpegMapParameters=" -map i:$videoPid -map i:$audioPid -c:v copy -c:a copy -f mpegts udp://127.0.0.1:$ffmpegListenPort"

			if [ "$ffmpegListenPortRunning" == "" ]
			then
				ffmpegCommandChanged=1
			fi

			if [ $debug -eq 1 ]; then
				echo "channelInfo: $channelInfo, videoPid: $videoPid, audioPid: $audioPid, ffmpegCommandChanged: $ffmpegCommandChanged, ffmpegMapParameters: $ffmpegMapParameters"
			fi

		elif [ "$command" == "removed" ]
		then
			if [ "$ffmpegListenPortRunning" != "" ]
			then
				ffmpegCommandChanged=1
			fi

			if [ $debug -eq 1 ]; then
				echo "ffmpegCommandChanged: $ffmpegCommandChanged"
			fi
		fi
	done

	if [ $debug -eq 1 ]; then
		echo "ffmpegCommandChanged: $ffmpegCommandChanged, activesNumber: $activesNumber"
	fi

	pidFfmpegPathName=/var/catramms/pids/satellite_$satelliteFrequency"_ffmpeg.pid"
	if [ $ffmpegCommandChanged -eq 1 ]
	then
		if [ -s "$pidFfmpegPathName" ]
		then
			if [ $debug -eq 1 ]; then
				echo "kill. pidFfmpeg: $(cat $pidFfmpegPathName)"
			fi

			kill -9 $(cat $pidFfmpegPathName) > /dev/null 2>&1
			echo "" > $pidFfmpegPathName

			sleep 1
		fi

		if [ $activesNumber -gt 0 ]
		then
			if [ $zapToBeStarted -eq 1 ]
			then
				if [ $debug -eq 1 ]; then
					echo "dvbv5-zap. satelliteFrequency: $satelliteFrequency, deviceNumber: $deviceNumber, dvbChannelsPathName: $dvbChannelsPathName"
				fi

				logPathName=/var/catramms/logs/satellite/$satelliteFrequency"_zap.log"
				nohup dvbv5-zap $satelliteFrequency -a $deviceNumber -ss --all-pids -c $dvbChannelsPathName > $logPathName 2>&1 &
				echo $! > $pidZapPathName
			fi

			if [ $debug -eq 1 ]; then
				echo "ffmpeg. deviceNumber: $deviceNumber, ffmpegMapParameters: $ffmpegMapParameters"
			fi

			logPathName=/var/catramms/logs/satellite/$satelliteFrequency"_ffmpeg.log"
			nohup ffmpeg -i /dev/dvb/adapter$deviceNumber/dvr0 $ffmpegMapParameters > $logPathName 2>&1 &
			echo $! > $pidFfmpegPathName
		else
			if [ -s "$pidZapPathName" ]
			then
				if [ $debug -eq 1 ]; then
					echo "kill. pidZap: $(cat $pidZapPathName)"
				fi

				kill -9 $(cat $pidZapPathName) > /dev/null 2>&1
				echo "" > $pidZapPathName

				sleep 1
			fi
		fi
	else
		if [ $activesNumber -eq 0 ]
		then
			if [ -s "$pidFfmpegPathName" ]
			then
				if [ $debug -eq 1 ]; then
					echo "kill. pidFfmpeg: $(cat $pidFfmpegPathName)"
				fi

				kill -9 $(cat $pidFfmpegPathName) > /dev/null 2>&1
				echo "" > $pidFfmpegPathName

				sleep 1
			fi

			if [ -s "$pidZapPathName" ]
			then
				if [ $debug -eq 1 ]; then
					echo "kill. pidZap: $(cat $pidZapPathName)"
				fi

				kill -9 $(cat $pidZapPathName) > /dev/null 2>&1
				echo "" > $pidZapPathName

				sleep 1
			fi
		else
			if [ $zapToBeStarted -eq 1 ]
			then
				if [ $debug -eq 1 ]; then
					echo "dvbv5-zap. satelliteFrequency: $satelliteFrequency, deviceNumber: $deviceNumber, dvbChannelsPathName: $dvbChannelsPathName"
				fi

				logPathName=/var/catramms/logs/satellite/$satelliteFrequency"_zap.log"
				nohup dvbv5-zap $satelliteFrequency -a $deviceNumber -ss --all-pids -c $dvbChannelsPathName > $logPathName 2>&1 &
				echo $! > $pidZapPathName
			fi
		fi
	fi

done

if [ $debug -eq 1 ]; then
	echo "script finished"
fi

