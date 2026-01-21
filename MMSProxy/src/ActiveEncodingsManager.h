/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */

/*
 * File:   EnodingsManager.h
 * Author: giuliano
 *
 * Created on February 4, 2018, 7:18 PM
 */

#pragma once

#include "EncoderProxy.h"
#include "MMSEngineDBFacade.h"
#include "MMSStorage.h"
#include <chrono>
#include <condition_variable>
#include <vector>
#include "Magick++.h"
#include "spdlog/spdlog.h"

#define MAXHIGHENCODINGSTOBEMANAGED 200
#define MAXMEDIUMENCODINGSTOBEMANAGED 200
#define MAXLOWENCODINGSTOBEMANAGED 200

struct MaxEncodingsManagerCapacityReached : public std::exception
{
	char const *what() const throw() { return "Max Encoding Manager capacity reached"; };
};

class ActiveEncodingsManager
{
  public:
	ActiveEncodingsManager(
		const nlohmann::json& configuration, std::string processorMMS, const std::shared_ptr<MultiEventsSet>& multiEventsSet,
		const std::shared_ptr<MMSEngineDBFacade>& mmsEngineDBFacade, const std::shared_ptr<MMSStorage>& mmsStorage
	);

	virtual ~ActiveEncodingsManager();

	void operator()();

	unsigned long addEncodingItems(std::vector<std::shared_ptr<MMSEngineDBFacade::EncodingItem>> &vEncodingItems);

	// static void encodingImageFormatValidation(std::string newFormat);
	// static Magick::InterlaceType encodingImageInterlaceTypeValidation(std::string sNewInterlaceType);

  private:
	struct EncodingJob
	{
		EncoderProxy::EncodingJobStatus _status;
		std::chrono::system_clock::time_point _encodingJobStart;

		std::shared_ptr<MMSEngineDBFacade::EncodingItem> _encodingItem;
		EncoderProxy _encoderProxy;

		EncodingJob() { _status = EncoderProxy::EncodingJobStatus::Free; }
	};

	nlohmann::json _configuration;
	std::shared_ptr<MMSEngineDBFacade> _mmsEngineDBFacade;
	std::shared_ptr<MMSStorage> _mmsStorage;
	std::shared_ptr<EncodersLoadBalancer> _encodersLoadBalancer;
	std::string _hostName;

	int _maxSecondsToWaitUpdateEncodingJobLock;

	std::condition_variable _cvAddedEncodingJob;
	std::mutex _mtEncodingJobs;

	EncodingJob _highPriorityEncodingJobs[MAXHIGHENCODINGSTOBEMANAGED];
	EncodingJob _mediumPriorityEncodingJobs[MAXMEDIUMENCODINGSTOBEMANAGED];
	EncodingJob _lowPriorityEncodingJobs[MAXLOWENCODINGSTOBEMANAGED];

#ifdef __LOCALENCODER__
	int _runningEncodingsNumber;
#endif

	// void getEncodingsProgressThread();

	static bool isProcessorShutdown();

	static void processEncodingJob(EncodingJob *encodingJob);
	void addEncodingItem(std::shared_ptr<MMSEngineDBFacade::EncodingItem> encodingItem);
	/*
	std::string encodeContentImage(
		std::shared_ptr<MMSEngineDBFacade::EncodingItem> encodingItem);
	int64_t processEncodedImage(
		std::shared_ptr<MMSEngineDBFacade::EncodingItem> encodingItem,
		std::string stagingEncodedAssetPathName);

	void readingImageProfile(
		std::string jsonProfile,
		std::string& newFormat,
		int& newWidth,
		int& newHeight,
		bool& newAspect,
		std::string& sNewInterlaceType,
		Magick::InterlaceType& newInterlaceType
	);
	*/
};
