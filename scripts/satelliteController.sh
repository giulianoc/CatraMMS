#!/bin/bash

#RUN AS ROOT

#each frequency corrispond to a transponder providing several channels

#satellite directory contains dvblast configuration file managed (created and updated) by the mmsEncoder.
#	the name of these files are: <frequency>-<symbol rate>-<modulation>
#	the extension of the file could be:
#		- .txt: process is already up and running
#		- .changed: there was a change into the file to be managed
#When the mmsEncoder stops the channel will updates the content of the dvblast configuration file removing
#	the configuration and leaving the file empty. This script, in this scenario, kills the process and remove the configuration file

satelliteChannelConfigurationDirectory=/var/catramms/satellite
satelliteLogsChannelsDir=/var/catramms/logs/satellite
dvbChannelsPathName=/opt/catramms/CatraMMS/conf/3_UNIVERSAL.channel.dvbv5.conf
frontendToBeUsed=1

debug=1


mkdir -p $satelliteChannelConfigurationDirectory
mkdir -p $satelliteLogsChannelsDir

#retention log file
threeDaysInMinutes=4320
find $satelliteLogsChannelsDir -mmin +$threeDaysInMinutes -type f -delete

getFreeDeviceNumber()
{
    satelliteFrequency=$1

	selectedDeviceNumber=255

	for deviceNumber in 0 1 2 3 4 5 6 7 8 9
	do
		#-x parameter just tunes and exit

		if [ $debug -eq 1 ]; then
			echo "getFreeDeviceNumber. dvbv5-zap $satelliteFrequency -a $deviceNumber -f $frontendToBeUsed -ss -x --all-pids -c $dvbChannelsPathName"
		fi
		dvbv5-zap $satelliteFrequency -a $deviceNumber -f $frontendToBeUsed -ss -x --all-pids -c $dvbChannelsPathName > /dev/null 2>&1
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
		pgrep -f "dvblast -f $satelliteFrequency -a $deviceNumber" > /dev/null
		pgrepStatus=$?
		if [ $pgrepStatus -eq 0 ]
		then
			#process was found
			selectedDeviceNumber=$deviceNumber

			break
		fi
	done

	if [ $debug -eq 1 ]; then
		echo "getActualDeviceNumber. selectedDeviceNumber: $selectedDeviceNumber"
	fi

	return $selectedDeviceNumber
}

startOfProcess()
{
    frequency=$1
    symbolRate=$2
    modulation=$3
    dvblastConfPathName=$4
	pidProcessPathName=$5

	getFreeDeviceNumber $frequency
	deviceNumber=$?
	if [ $deviceNumber -eq 255 ]
	then
		echo "No deviceNumber available"

		return 1
	fi

	modulationParameter=""
	if [ "$modulation" != "n.a." ]; then
		modulationParameter="-m $modulation"
	fi

	if [ $debug -eq 1 ]; then
		echo "Start of the process. frequency: $frequency, deviceNumber: $deviceNumber, symbolRate: $symbolRate, modulation: $modulation, dvblastConfPathName: $dvblastConfPathName, pidProcessPathName: $pidProcessPathName"
	fi

	logPathName=$satelliteLogsChannelsDir/$frequency".log"
	nohup dvblast -f $frequency -a $deviceNumber -s $symbolRate $modulationParameter -n $frontendToBeUsed -c $dvblastConfPathName > $logPathName 2>&1 &
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
		echo "isProcessRunning: $localIsProcessRunning"
	fi

	return $localIsProcessRunning
}

# MAIN MAIN MAIN

if [ $debug -eq 1 ]; then
	echo "script finished"
fi

configurationFiles=$(ls $satelliteChannelConfigurationDirectory)
for configurationFileName in $configurationFiles
do
	#for each <frequency>-<symbol rate>-<modulation>

	fileExtension=${configurationFileName##*.}
	if [ $debug -eq 1 ]; then
		echo "configurationFileName: $configurationFileName, fileExtension: $fileExtension"
	fi

	if [ "$fileExtension" == "txt" ]; then
		frequencySymbolRateModulation=$(basename $configurationFileName .txt)
	else
		frequencySymbolRateModulation=$(basename $configurationFileName .changed)
	fi
	frequency=$(echo $frequencySymbolRateModulation | cut -d'-' -f1)
	symbolRate=$(echo $frequencySymbolRateModulation | cut -d'-' -f2)
	modulation=$(echo $frequencySymbolRateModulation | cut -d'-' -f3)
	if [ "$modulation" == "" ]; then
		modulation="n.a."
	fi
	if [ $debug -eq 1 ]; then
		echo "frequency: $frequency, symbolRate: $symbolRate, modulation: $modulation"
	fi


	pidProcessPathName=/var/catramms/pids/satellite_$frequency".pid"

	isProcessRunningFunc $frequency
	isProcessRunning=$?

	if [ "$fileExtension" == "txt" ]; then
		echo "No changes to $configurationFileName"

		if [ $isProcessRunning -eq 0 ]; then
			echo "Process is not up and running, start it"

			startOfProcess $frequency $symbolRate $modulation $satelliteChannelConfigurationDirectory/$configurationFileName $pidProcessPathName
		fi

		continue
	fi

	#fileExtension is 'changed'

	if [ $isProcessRunning -eq 1 ]; then
		if [ -s $pidProcessPathName ]; then

			if [ $debug -eq 1 ]; then
				echo "kill. pidProcessPathName: $(cat $pidProcessPathName)"
			fi

			kill -9 $(cat $pidProcessPathName) > /dev/null 2>&1
			echo "" > $pidProcessPathName

			sleep 1
		else
			echo "ERROR: process is running but there is no PID to kill it"

			continue
		fi
	fi

	if [ ! -s $satelliteChannelConfigurationDirectory/$configurationFileName ]; then
		if [ $debug -eq 1 ]; then
			echo "dvblast configuration file empty, channel is removed, configurationFileName: $configurationFileName"
		fi

		#process is alredy killed (see above statements)

		if [ $debug -eq 1 ]; then
			echo "rm -f $satelliteChannelConfigurationDirectory/$configurationFileName"
		fi
		rm -f $satelliteChannelConfigurationDirectory/$configurationFileName
	else
		startOfProcess $frequency $symbolRate $modulation $satelliteChannelConfigurationDirectory/$configurationFileName $pidProcessPathName
		processReturn=$?
		if [ $processReturn -eq 0 ]; then
			if [ $debug -eq 1 ]; then
				echo "mv $satelliteChannelConfigurationDirectory/$frequencySymbolRateModulation.changed $satelliteChannelConfigurationDirectory/$frequencySymbolRateModulation.txt"
			fi
			mv $satelliteChannelConfigurationDirectory/$frequencySymbolRateModulation.changed $satelliteChannelConfigurationDirectory/$frequencySymbolRateModulation.txt
		else
			if [ $debug -eq 1 ]; then
				echo "Start of the process failed, processReturn: $processReturn"
			fi
		fi
	fi

done

if [ $debug -eq 1 ]; then
	echo "script finished"
fi

