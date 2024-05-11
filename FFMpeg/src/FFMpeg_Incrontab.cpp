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
#include "catralibraries/ProcessUtility.h"
#include "catralibraries/StringUtils.h"
#include <fstream>
/*
#include "FFMpegEncodingParameters.h"
#include "FFMpegFilters.h"
#include "JSONUtils.h"
#include "MMSCURL.h"
#include <filesystem>
#include <regex>
#include <sstream>
#include <string>
*/

void FFMpeg::addToIncrontab(int64_t ingestionJobKey, int64_t encodingJobKey, string directoryToBeMonitored)
{
	try
	{
		_logger->info(
			__FILEREF__ + "Received addToIncrontab" + ", ingestionJobKey: " + to_string(ingestionJobKey) +
			", encodingJobKey: " + to_string(encodingJobKey) + ", directoryToBeMonitored: " + directoryToBeMonitored
		);

		if (!fs::exists(_incrontabConfigurationDirectory))
		{
			_logger->info(
				__FILEREF__ + "addToIncrontab: create directory" + ", _ingestionJobKey: " + to_string(ingestionJobKey) +
				", _encodingJobKey: " + to_string(encodingJobKey) + ", _incrontabConfigurationDirectory: " + _incrontabConfigurationDirectory
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
			string errorMessage = __FILEREF__ + "addToIncrontab: directory is already into the incontab configuration file" +
								  ", ingestionJobKey: " + to_string(ingestionJobKey) + ", encodingJobKey: " + to_string(encodingJobKey) +
								  ", incrontabConfigurationPathName: " + incrontabConfigurationPathName;
			_logger->warn(errorMessage);

			// throw runtime_error(errorMessage);
		}
		else
		{
			ofstream ofConfigurationFile(incrontabConfigurationPathName, ofstream::app);
			if (!ofConfigurationFile)
			{
				string errorMessage = __FILEREF__ + "addToIncrontab: open incontab configuration file failed" +
									  ", ingestionJobKey: " + to_string(ingestionJobKey) + ", encodingJobKey: " + to_string(encodingJobKey) +
									  ", incrontabConfigurationPathName: " + incrontabConfigurationPathName;
				_logger->error(errorMessage);

				throw runtime_error(errorMessage);
			}

			string configuration = directoryToBeMonitored + " IN_MODIFY,IN_CLOSE_WRITE,IN_CREATE,IN_DELETE,IN_MOVED_FROM,IN_MOVED_TO,IN_MOVE_SELF "
															"/opt/catramms/CatraMMS/scripts/incrontab.sh $% $@ $#";

			_logger->info(
				__FILEREF__ + "addToIncrontab: adding incontab configuration" + ", ingestionJobKey: " + to_string(ingestionJobKey) +
				", encodingJobKey: " + to_string(encodingJobKey) + ", configuration: " + configuration
			);

			ofConfigurationFile << configuration << endl;
			ofConfigurationFile.close();
		}

		{
			string incrontabExecuteCommand = _incrontabBinary + " " + incrontabConfigurationPathName;

			_logger->info(
				__FILEREF__ + "addToIncrontab: Executing incontab command" + ", ingestionJobKey: " + to_string(ingestionJobKey) +
				", encodingJobKey: " + to_string(encodingJobKey) + ", incrontabExecuteCommand: " + incrontabExecuteCommand
			);

			int executeCommandStatus = ProcessUtility::execute(incrontabExecuteCommand);
			if (executeCommandStatus != 0)
			{
				string errorMessage = __FILEREF__ + "addToIncrontab: incrontab command failed" + ", ingestionJobKey: " + to_string(ingestionJobKey) +
									  ", encodingJobKey: " + to_string(encodingJobKey) +
									  ", executeCommandStatus: " + to_string(executeCommandStatus) +
									  ", incrontabExecuteCommand: " + incrontabExecuteCommand;
				_logger->error(errorMessage);

				throw runtime_error(errorMessage);
			}
		}
	}
	catch (...)
	{
		string errorMessage = __FILEREF__ + "addToIncrontab failed" + ", ingestionJobKey: " + to_string(ingestionJobKey) +
							  ", encodingJobKey: " + to_string(encodingJobKey);
		_logger->error(errorMessage);
	}
}

void FFMpeg::removeFromIncrontab(int64_t ingestionJobKey, int64_t encodingJobKey, string directoryToBeMonitored)
{
	try
	{
		_logger->info(
			__FILEREF__ + "Received removeFromIncrontab" + ", ingestionJobKey: " + to_string(ingestionJobKey) +
			", encodingJobKey: " + to_string(encodingJobKey) + ", directoryToBeMonitored: " + directoryToBeMonitored
		);

		string incrontabConfigurationPathName = _incrontabConfigurationDirectory + "/" + _incrontabConfigurationFileName;

		bool foundMonitoryDirectory = false;
		vector<string> vConfiguration;
		{
			ifstream ifConfigurationFile(incrontabConfigurationPathName);
			if (!ifConfigurationFile)
			{
				string errorMessage = __FILEREF__ + "removeFromIncrontab: open incontab configuration file failed" +
									  ", ingestionJobKey: " + to_string(ingestionJobKey) + ", encodingJobKey: " + to_string(encodingJobKey) +
									  ", incrontabConfigurationPathName: " + incrontabConfigurationPathName;
				_logger->error(errorMessage);

				throw runtime_error(errorMessage);
			}

			string configuration;
			while (getline(ifConfigurationFile, configuration))
			{
				string trimmedConfiguration = StringUtils::trimNewLineAndTabToo(configuration);

				if (configuration.size() >= directoryToBeMonitored.size() &&
					0 == configuration.compare(0, directoryToBeMonitored.size(), directoryToBeMonitored))
				{
					_logger->info(
						__FILEREF__ + "removeFromIncrontab: removing incontab configuration" + ", ingestionJobKey: " + to_string(ingestionJobKey) +
						", encodingJobKey: " + to_string(encodingJobKey) + ", configuration: " + configuration
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
			string errorMessage = __FILEREF__ + "removeFromIncrontab: monitoring directory is not found into the incontab configuration file" +
								  ", ingestionJobKey: " + to_string(ingestionJobKey) + ", encodingJobKey: " + to_string(encodingJobKey) +
								  ", incrontabConfigurationPathName: " + incrontabConfigurationPathName;
			_logger->warn(errorMessage);
		}
		else
		{
			ofstream ofConfigurationFile(incrontabConfigurationPathName, ofstream::trunc);
			if (!ofConfigurationFile)
			{
				string errorMessage = __FILEREF__ + "removeFromIncrontab: open incontab configuration file failed" +
									  ", ingestionJobKey: " + to_string(ingestionJobKey) + ", encodingJobKey: " + to_string(encodingJobKey) +
									  ", incrontabConfigurationPathName: " + incrontabConfigurationPathName;
				_logger->error(errorMessage);

				throw runtime_error(errorMessage);
			}

			for (string configuration : vConfiguration)
				ofConfigurationFile << configuration << endl;
			ofConfigurationFile.close();
		}

		{
			string incrontabExecuteCommand = _incrontabBinary + " " + incrontabConfigurationPathName;

			_logger->info(
				__FILEREF__ + "removeFromIncrontab: Executing incontab command" + ", ingestionJobKey: " + to_string(ingestionJobKey) +
				", encodingJobKey: " + to_string(encodingJobKey) + ", incrontabExecuteCommand: " + incrontabExecuteCommand
			);

			int executeCommandStatus = ProcessUtility::execute(incrontabExecuteCommand);
			if (executeCommandStatus != 0)
			{
				string errorMessage = __FILEREF__ + "removeFromIncrontab: incrontab command failed" +
									  ", ingestionJobKey: " + to_string(ingestionJobKey) + ", encodingJobKey: " + to_string(encodingJobKey) +
									  ", executeCommandStatus: " + to_string(executeCommandStatus) +
									  ", incrontabExecuteCommand: " + incrontabExecuteCommand;
				_logger->error(errorMessage);

				throw runtime_error(errorMessage);
			}
		}
	}
	catch (...)
	{
		string errorMessage = __FILEREF__ + "removeFromIncrontab failed" + ", ingestionJobKey: " + to_string(ingestionJobKey) +
							  ", encodingJobKey: " + to_string(encodingJobKey);
		_logger->error(errorMessage);
	}
}
