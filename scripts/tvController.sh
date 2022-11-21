#!/bin/bash

#RUN AS ROOT

#each frequency corrispond to a transponder providing several channels

#tv directory contains dvblast configuration file managed (created and updated) by the mmsEncoder.
#	the name of these files are: <frequency>-<symbol rate>-<bandwidthInMHz>-<modulation>
#	the extension of the file could be:
#		- .txt: process is already up and running
#		- .changed: there was a change into the file to be managed
#When the mmsEncoder stops the channel will updates the content of the dvblast configuration file removing the configuration and leaving the file empty. This script, in this scenario, kills the process and remove the configuration file

tvChannelConfigurationDirectory=/var/catramms/tv
tvLogsChannelsDir=/var/catramms/logs/tv
#dvbChannelsPathName=/opt/catramms/CatraMMS/conf/3_UNIVERSAL.channel.dvbv5.conf
dvbChannelsPathName=/opt/catramms/CatraMMS/conf/3_terrestrial_2022_09_07.channel.dvbv5.conf
#frontendToBeUsed=1
frontendToBeUsed=0

debug=1
debugFilename=/tmp/tvController.log


mkdir -p $tvChannelConfigurationDirectory
mkdir -p $tvLogsChannelsDir

#retention log file
threeDaysInMinutes=4320
find $tvLogsChannelsDir -mmin +$threeDaysInMinutes -type f -delete

getFreeDeviceNumber()
{
    tvFrequency=$1

	selectedDeviceNumber=255

	for deviceNumber in 0 1 2 3 4 5 6 7 8 9
	do
		#-x parameter just tunes and exit

		if [ $debug -eq 1 ]; then
			echo "$(date): getFreeDeviceNumber. dvbv5-zap $tvFrequency -a $deviceNumber -f $frontendToBeUsed -ss -x --all-pids -c $dvbChannelsPathName" >> $debugFilename
		fi
		timeout 120 dvbv5-zap $tvFrequency -a $deviceNumber -f $frontendToBeUsed -ss -x --all-pids -c $dvbChannelsPathName > /dev/null 2>&1
		if [ $? -eq 0 ]
		then
			selectedDeviceNumber=$deviceNumber

			break
		elif [ $? -eq 124 ]
		then
			if [ $debug -eq 1 ]; then
				echo "$(date): dvbv5-zap TIMED OUT" >> $debugFilename
			fi
		elif [ $? -eq 126 ]
		then
			if [ $debug -eq 1 ]; then
				echo "$(date): dvbv5-zap FAILED (Argument list too long)" >> $debugFilename
			fi
		fi
	done

	if [ $debug -eq 1 ]; then
		echo "$(date): getFreeDeviceNumber. selectedDeviceNumber: $selectedDeviceNumber" >> $debugFilename
	fi

	return $selectedDeviceNumber
}

getActualDeviceNumber()
{
    tvFrequency=$1

	selectedDeviceNumber=255

	for deviceNumber in 0 1 2 3 4 5 6 7 8 9
	do
		pgrep -f "dvblast -f $tvFrequency -a $deviceNumber" > /dev/null
		pgrepStatus=$?
		if [ $pgrepStatus -eq 0 ]
		then
			#process was found
			selectedDeviceNumber=$deviceNumber

			break
		fi
	done

	if [ $debug -eq 1 ]; then
		echo "$(date): getActualDeviceNumber. selectedDeviceNumber: $selectedDeviceNumber" >> $debugFilename
	fi

	return $selectedDeviceNumber
}

startOfProcess()
{
    type=$1
    frequency=$2
	if [ "$type" == "satellite" ]; then
		symbolRate=$3
	else
		bandwidthInMhz=$3
	fi
    modulation=$4
    dvblastConfPathName=$5
	pidProcessPathName=$6

	getFreeDeviceNumber $frequency
	deviceNumber=$?
	if [ $deviceNumber -eq 255 ]
	then
		if [ $debug -eq 1 ]; then
			echo "$(date): No deviceNumber available"
		fi

		return 1
	fi

	modulationParameter=""
	if [ "$modulation" != "n.a." ]; then
		modulationParameter="-m $modulation"
	fi

	logPathName=$tvLogsChannelsDir/$frequency".log"
	if [ $debug -eq 1 ]; then
		if [ "$type" == "satellite" ]; then
			echo "$(date): Start of the process. nohup dvblast -f $frequency -a $deviceNumber -s $symbolRate $modulationParameter -n $frontendToBeUsed -c $dvblastConfPathName > $logPathName 2>&1 &" >> $debugFilename
		else
			echo "$(date): Start of the process. nohup dvblast -f $frequency -a $deviceNumber -b $bandwidthInMhz $modulationParameter -n $frontendToBeUsed -c $dvblastConfPathName > $logPathName 2>&1 &" >> $debugFilename
		fi
	fi

	if [ "$type" == "satellite" ]; then
		nohup dvblast -f $frequency -a $deviceNumber -s $symbolRate $modulationParameter -n $frontendToBeUsed -c $dvblastConfPathName > $logPathName 2>&1 &
	else
		nohup dvblast -f $frequency -a $deviceNumber -b $bandwidthInMhz $modulationParameter -n $frontendToBeUsed -c $dvblastConfPathName > $logPathName 2>&1 &
	fi
	echo $! > $pidProcessPathName
	#I saw it returned 0 even if the process failed. Better to check if the process is running
	#processReturn=$?
	sleep 5
	isProcessRunningFunc $frequency
	isProcessRunning=$?
	if [ $isProcessRunning -eq 0 ]
	then
		processReturn=1
	else
		processReturn=0
	fi

	return $processReturn
}

isProcessRunningFunc()
{
    frequency=$1

	pgrep -f "dvblast -f $frequency" > /dev/null
	if [ $? -eq 0 ]; then
		#process was found
		localIsProcessRunning=1
	else
		localIsProcessRunning=0
	fi
	if [ $debug -eq 1 ]; then
		echo "$(date): isProcessRunning: $localIsProcessRunning" >> $debugFilename
	fi

	return $localIsProcessRunning
}

# MAIN MAIN MAIN

if [ $debug -eq 1 ]; then
	echo "$(date): script started" >> $debugFilename
fi

configurationFiles=$(ls $tvChannelConfigurationDirectory)
for configurationFileName in $configurationFiles
do
	#for each <frequency>-<symbol rate>-<bandwidthInMhz>-<modulation>

	fileExtension=${configurationFileName##*.}
	if [ $debug -eq 1 ]; then
		echo "$(date): configurationFileName: $configurationFileName, fileExtension: $fileExtension" >> $debugFilename
	fi

	if [ "$fileExtension" == "txt" ]; then
		frequencySymbolRateBandwidthInMhzModulation=$(basename $configurationFileName .txt)
	else
		frequencySymbolRateBandwidthInMhzModulation=$(basename $configurationFileName .changed)
	fi
	frequency=$(echo $frequencySymbolRateBandwidthInMhzModulation | cut -d'-' -f1)
	symbolRate=$(echo $frequencySymbolRateBandwidthInMhzModulation | cut -d'-' -f2)
	bandwidthInMhz=$(echo $frequencySymbolRateBandwidthInMhzModulation | cut -d'-' -f3)
	modulation=$(echo $frequencySymbolRateBandwidthInMhzModulation | cut -d'-' -f4)
	if [ "$modulation" == "" ]; then
		modulation="n.a."
	fi
	#symbolRate is used by dvblast in case of satellite
	#bandwidthInMhz is used by dvblast in case of digital terrestrial
	if [ "$symbolRate" == "" ]; then
		type="digitalTerrestrial"
		localParam=$bandwidthInMhz
	else
		type="satellite"
		localParam=$symbolRate
	fi
	if [ $debug -eq 1 ]; then
		echo "$(date): frequency: $frequency, type: $type, symbolRate: $symbolRate, bandwidthInMhz: $bandwidthInMhz, modulation: $modulation" >> $debugFilename
	fi


	pidProcessPathName=/var/catramms/pids/tv_$frequency".pid"

	isProcessRunningFunc $frequency
	isProcessRunning=$?

	if [ "$fileExtension" == "txt" ]; then
		if [ $debug -eq 1 ]; then
			echo "$(date): No changes to $configurationFileName" >> $debugFilename
		fi

		if [ $isProcessRunning -eq 0 ]; then
			if [ $debug -eq 1 ]; then
				echo "$(date): Process is not up and running, start it" >> $debugFilename
			fi

			startOfProcess $type $frequency $localParam $modulation $tvChannelConfigurationDirectory/$configurationFileName $pidProcessPathName
		fi

		continue
	fi

	#fileExtension is 'changed'

	if [ $isProcessRunning -eq 1 ]; then
		if [ -s $pidProcessPathName ]; then

			if [ $debug -eq 1 ]; then
				echo "$(date): kill. pidProcessPathName: $(cat $pidProcessPathName)" >> $debugFilename
			fi

			kill -9 $(cat $pidProcessPathName) > /dev/null 2>&1
			echo "" > $pidProcessPathName

			sleep 1
		else
			if [ $debug -eq 1 ]; then
				echo "$(date): ERROR: process is running but there is no PID to kill it" >> $debugFilename
			fi

			continue
		fi
	fi

	fileSize=$(stat -c%s "$tvChannelConfigurationDirectory/$configurationFileName")
	#15 because we cannot have a conf less than 15 chars
	if [ $fileSize -lt 15 ]; then
		if [ $debug -eq 1 ]; then
			echo "$(date): dvblast configuration file is empty ($fileSize), channel is removed, configurationFileName: $configurationFileName" >> $debugFilename
		fi

		#process is alredy killed (see above statements)

		if [ $debug -eq 1 ]; then
			echo "$(date): rm -f $tvChannelConfigurationDirectory/$configurationFileName" >> $debugFilename
		fi
		rm -f $tvChannelConfigurationDirectory/$configurationFileName
	else
		startOfProcess $type $frequency $localParam $modulation $tvChannelConfigurationDirectory/$configurationFileName $pidProcessPathName
		processReturn=$?
		if [ $processReturn -eq 0 ]; then
			if [ $debug -eq 1 ]; then
				echo "$(date): mv $tvChannelConfigurationDirectory/$frequencySymbolRateBandwidthInMhzModulation.changed $tvChannelConfigurationDirectory/$frequencySymbolRateBandwidthInMhzModulation.txt" >> $debugFilename
			fi
			mv $tvChannelConfigurationDirectory/$frequencySymbolRateBandwidthInMhzModulation".changed" $tvChannelConfigurationDirectory/$frequencySymbolRateBandwidthInMhzModulation".txt"
		else
			if [ $debug -eq 1 ]; then
				echo "$(date): Start of the process failed, processReturn: $processReturn" >> $debugFilename
			fi
		fi
	fi

done

if [ $debug -eq 1 ]; then
	echo "$(date): script finished" >> $debugFilename
fi

