/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */

/*
 * File:   FFMPEGEncoder.cpp
 * Author: giuliano
 *
 * Created on February 18, 2018, 1:27 AM
 */
#include "FFMpeg.h"
#include "StringUtils.h"
#include "catralibraries/ProcessUtility.h"
#include "spdlog/spdlog.h"
#include <fstream>

void FFMpeg::addToIncrontab(int64_t ingestionJobKey, int64_t encodingJobKey, string directoryToBeMonitored)
{
	try
	{
		SPDLOG_INFO(
			"Received addToIncrontab"
			", ingestionJobKey: {}"
			", encodingJobKey: {}"
			", directoryToBeMonitored: {}",
			ingestionJobKey, encodingJobKey, directoryToBeMonitored
		);

		if (!fs::exists(_incrontabConfigurationDirectory))
		{
			SPDLOG_INFO(
				"addToIncrontab: create directory"
				", _ingestionJobKey: {}"
				", _encodingJobKey: {}"
				", _incrontabConfigurationDirectory: {}",
				ingestionJobKey, encodingJobKey, _incrontabConfigurationDirectory
			);

			fs::create_directories(_incrontabConfigurationDirectory);
			fs::permissions(
				_incrontabConfigurationDirectory,
				fs::perms::owner_read | fs::perms::owner_write | fs::perms::owner_exec | fs::perms::group_read | fs::perms::group_write |
					fs::perms::group_exec | fs::perms::others_read | fs::perms::others_write | fs::perms::others_exec,
				fs::perm_options::replace
			);
		}

		string incrontabConfigurationPathName = _incrontabConfigurationDirectory + "/" + _incrontabConfigurationFileName;

		bool directoryAlreadyMonitored = false;
		{
			ifstream ifConfigurationFile(incrontabConfigurationPathName);
			if (ifConfigurationFile)
			{
				string configuration;
				while (getline(ifConfigurationFile, configuration))
				{
					string trimmedConfiguration = StringUtils::trimNewLineAndTabToo(configuration);

					if (configuration.size() >= directoryToBeMonitored.size() &&
						0 == configuration.compare(0, directoryToBeMonitored.size(), directoryToBeMonitored))
					{
						directoryAlreadyMonitored = true;

						break;
					}
				}

				ifConfigurationFile.close();
			}
		}

		if (directoryAlreadyMonitored)
		{
			string errorMessage = std::format(
				"addToIncrontab: directory is already into the incontab configuration file"
				", ingestionJobKey: {}"
				", encodingJobKey: {}"
				", incrontabConfigurationPathName: {}",
				ingestionJobKey, encodingJobKey, incrontabConfigurationPathName
			);
			SPDLOG_WARN(errorMessage);

			// throw runtime_error(errorMessage);
		}
		else
		{
			ofstream ofConfigurationFile(incrontabConfigurationPathName, ofstream::app);
			if (!ofConfigurationFile)
			{
				string errorMessage = std::format(
					"addToIncrontab: open incontab configuration file failed"
					", ingestionJobKey: {}"
					", encodingJobKey: {}"
					", incrontabConfigurationPathName: {}",
					ingestionJobKey, encodingJobKey, incrontabConfigurationPathName
				);
				SPDLOG_ERROR(errorMessage);

				throw runtime_error(errorMessage);
			}

			string configuration = std::format(
				"{} IN_MODIFY,IN_CLOSE_WRITE,IN_CREATE,IN_DELETE,IN_MOVED_FROM,IN_MOVED_TO,IN_MOVE_SELF "
				"/opt/catramms/CatraMMS/scripts/incrontab.sh $% $@ $#",
				directoryToBeMonitored
			);

			SPDLOG_INFO(
				"addToIncrontab: adding incontab configuration"
				", ingestionJobKey: {}"
				", encodingJobKey: {}"
				", configuration: {}",
				ingestionJobKey, encodingJobKey, configuration
			);

			ofConfigurationFile << configuration << endl;
			ofConfigurationFile.close();
		}

		{
			string incrontabExecuteCommand = std::format("{} {}", _incrontabBinary, incrontabConfigurationPathName);

			SPDLOG_INFO(
				"addToIncrontab: Executing incontab command"
				", ingestionJobKey: {}"
				", encodingJobKey: {}"
				", incrontabExecuteCommand: {}",
				ingestionJobKey, encodingJobKey, incrontabExecuteCommand
			);

			int executeCommandStatus = ProcessUtility::execute(incrontabExecuteCommand);
			if (executeCommandStatus != 0)
			{
				string errorMessage = std::format(
					"addToIncrontab: incrontab command failed"
					", ingestionJobKey: {}"
					", encodingJobKey: {}"
					", executeCommandStatus: {}"
					", incrontabExecuteCommand: {}",
					ingestionJobKey, encodingJobKey, executeCommandStatus, incrontabExecuteCommand
				);
				SPDLOG_ERROR(errorMessage);

				throw runtime_error(errorMessage);
			}
		}
	}
	catch (...)
	{
		string errorMessage = std::format(
			"addToIncrontab failed"
			", ingestionJobKey: {}"
			", encodingJobKey: {}",
			ingestionJobKey, encodingJobKey
		);
		SPDLOG_ERROR(errorMessage);
	}
}

void FFMpeg::removeFromIncrontab(int64_t ingestionJobKey, int64_t encodingJobKey, string directoryToBeMonitored)
{
	try
	{
		SPDLOG_INFO(
			"Received removeFromIncrontab"
			", ingestionJobKey: {}"
			", encodingJobKey: {}"
			", directoryToBeMonitored: {}",
			ingestionJobKey, encodingJobKey, directoryToBeMonitored
		);

		string incrontabConfigurationPathName = _incrontabConfigurationDirectory + "/" + _incrontabConfigurationFileName;

		bool foundMonitoryDirectory = false;
		vector<string> vConfiguration;
		{
			ifstream ifConfigurationFile(incrontabConfigurationPathName);
			if (!ifConfigurationFile)
			{
				string errorMessage = std::format(
					"removeFromIncrontab: open incontab configuration file failed"
					", ingestionJobKey: {}"
					", encodingJobKey: {}"
					", incrontabConfigurationPathName: {}",
					ingestionJobKey, encodingJobKey, incrontabConfigurationPathName
				);
				SPDLOG_ERROR(errorMessage);

				throw runtime_error(errorMessage);
			}

			string configuration;
			while (getline(ifConfigurationFile, configuration))
			{
				string trimmedConfiguration = StringUtils::trimNewLineAndTabToo(configuration);

				if (configuration.size() >= directoryToBeMonitored.size() &&
					0 == configuration.compare(0, directoryToBeMonitored.size(), directoryToBeMonitored))
				{
					SPDLOG_INFO(
						"removeFromIncrontab: removing incontab configuration"
						", ingestionJobKey: {}"
						", encodingJobKey: {}"
						", configuration: {}",
						ingestionJobKey, encodingJobKey, configuration
					);

					foundMonitoryDirectory = true;
				}
				else
				{
					vConfiguration.push_back(trimmedConfiguration);
				}
			}
		}

		if (!foundMonitoryDirectory)
		{
			string errorMessage = std::format(
				"removeFromIncrontab: monitoring directory is not found into the incontab configuration file"
				", ingestionJobKey: {}"
				", encodingJobKey: {}"
				", incrontabConfigurationPathName: {}",
				ingestionJobKey, encodingJobKey, incrontabConfigurationPathName
			);
			SPDLOG_WARN(errorMessage);
		}
		else
		{
			ofstream ofConfigurationFile(incrontabConfigurationPathName, ofstream::trunc);
			if (!ofConfigurationFile)
			{
				string errorMessage = std::format(
					"removeFromIncrontab: open incontab configuration file failed"
					", ingestionJobKey: {}"
					", encodingJobKey: {}"
					", incrontabConfigurationPathName: {}",
					ingestionJobKey, encodingJobKey, incrontabConfigurationPathName
				);
				SPDLOG_ERROR(errorMessage);

				throw runtime_error(errorMessage);
			}

			for (string configuration : vConfiguration)
				ofConfigurationFile << configuration << endl;
			ofConfigurationFile.close();
		}

		{
			string incrontabExecuteCommand = std::format("{} {}", _incrontabBinary, incrontabConfigurationPathName);

			SPDLOG_INFO(
				"removeFromIncrontab: Executing incontab command"
				", ingestionJobKey: {}"
				", encodingJobKey: {}"
				", incrontabExecuteCommand: {}",
				ingestionJobKey, encodingJobKey, incrontabExecuteCommand
			);

			int executeCommandStatus = ProcessUtility::execute(incrontabExecuteCommand);
			if (executeCommandStatus != 0)
			{
				string errorMessage = std::format(
					"removeFromIncrontab: incrontab command failed"
					", ingestionJobKey: {}"
					", encodingJobKey: {}"
					", executeCommandStatus: {}"
					", incrontabExecuteCommand: {}",
					ingestionJobKey, encodingJobKey, executeCommandStatus, incrontabExecuteCommand
				);
				SPDLOG_ERROR(errorMessage);

				throw runtime_error(errorMessage);
			}
		}
	}
	catch (...)
	{
		string errorMessage = std::format(
			"removeFromIncrontab failed"
			", ingestionJobKey: {}"
			", encodingJobKey: {}",
			ingestionJobKey, encodingJobKey
		);
		SPDLOG_ERROR(errorMessage);
	}
}
