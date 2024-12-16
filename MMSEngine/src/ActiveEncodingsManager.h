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

#ifndef ActiveEncodingsManager_h
#define ActiveEncodingsManager_h

#include "EncoderProxy.h"
#include "MMSEngineDBFacade.h"
#include "MMSStorage.h"
#include <chrono>
#include <condition_variable>
#include <vector>
#ifndef SPDLOG_ACTIVE_LEVEL
#define SPDLOG_ACTIVE_LEVEL SPDLOG_LEVEL_TRACE
#endif
#include "Magick++.h"
#include "spdlog/spdlog.h"

#define MAXHIGHENCODINGSTOBEMANAGED 200
#define MAXMEDIUMENCODINGSTOBEMANAGED 200
#define MAXLOWENCODINGSTOBEMANAGED 200

struct MaxEncodingsManagerCapacityReached : public exception
{
	char const *what() const throw() { return "Max Encoding Manager capacity reached"; };
};

class ActiveEncodingsManager
{
  public:
	ActiveEncodingsManager(
		json configuration, string processorMMS, shared_ptr<MultiEventsSet> multiEventsSet, shared_ptr<MMSEngineDBFacade> mmsEngineDBFacade,
		shared_ptr<MMSStorage> mmsStorage, shared_ptr<spdlog::logger> logger
	);

	virtual ~ActiveEncodingsManager();

	void operator()();

	unsigned long addEncodingItems(vector<shared_ptr<MMSEngineDBFacade::EncodingItem>> &vEncodingItems);

	// static void encodingImageFormatValidation(string newFormat);
	// static Magick::InterlaceType encodingImageInterlaceTypeValidation(string sNewInterlaceType);

  private:
	struct EncodingJob
	{
		EncoderProxy::EncodingJobStatus _status;
		chrono::system_clock::time_point _encodingJobStart;

		shared_ptr<MMSEngineDBFacade::EncodingItem> _encodingItem;
		EncoderProxy _encoderProxy;

		EncodingJob() { _status = EncoderProxy::EncodingJobStatus::Free; }
	};

	shared_ptr<spdlog::logger> _logger;
	json _configuration;
	shared_ptr<MMSEngineDBFacade> _mmsEngineDBFacade;
	shared_ptr<MMSStorage> _mmsStorage;
	shared_ptr<EncodersLoadBalancer> _encodersLoadBalancer;
	string _hostName;

	int _maxSecondsToWaitUpdateEncodingJobLock;

	condition_variable _cvAddedEncodingJob;
	mutex _mtEncodingJobs;

	EncodingJob _highPriorityEncodingJobs[MAXHIGHENCODINGSTOBEMANAGED];
	EncodingJob _mediumPriorityEncodingJobs[MAXMEDIUMENCODINGSTOBEMANAGED];
	EncodingJob _lowPriorityEncodingJobs[MAXLOWENCODINGSTOBEMANAGED];

#ifdef __LOCALENCODER__
	int _runningEncodingsNumber;
#endif

	// void getEncodingsProgressThread();

	bool isProcessorShutdown();

	void processEncodingJob(EncodingJob *encodingJob);
	void addEncodingItem(shared_ptr<MMSEngineDBFacade::EncodingItem> encodingItem);
	/*
	string encodeContentImage(
		shared_ptr<MMSEngineDBFacade::EncodingItem> encodingItem);
	int64_t processEncodedImage(
		shared_ptr<MMSEngineDBFacade::EncodingItem> encodingItem,
		string stagingEncodedAssetPathName);

	void readingImageProfile(
		string jsonProfile,
		string& newFormat,
		int& newWidth,
		int& newHeight,
		bool& newAspect,
		string& sNewInterlaceType,
		Magick::InterlaceType& newInterlaceType
	);
	*/
};

#endif
