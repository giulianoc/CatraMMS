
#pragma once

#include <memory>
#ifndef SPDLOG_ACTIVE_LEVEL
#define SPDLOG_ACTIVE_LEVEL SPDLOG_LEVEL_TRACE
#endif
#include "MultiEventsSet.h"
#include "Times2.h"
#include "spdlog/spdlog.h"

#define MMSENGINE_GEOINFOTIMES_CLASSNAME "GEOInfoTimes"
#define MMSENGINE_GEOINFOTIMES_SOURCE "GEOInfoTimes"

#define MMSENGINE_EVENTTYPEIDENTIFIER_GEOINFOEVENT 10
#define MMSENGINEPROCESSORNAME "MMSEngineProcessor"

#ifndef __FILEREF__
#ifdef __APPLE__
#define __FILEREF__ string("[") + string(__FILE__).substr(string(__FILE__).find_last_of("/") + 1) + ":" + to_string(__LINE__) + "] "
#else
#define __FILEREF__ string("[") + basename(__FILE__) + ":" + to_string(__LINE__) + "] "
#endif
#endif

class GEOInfoTimes : public Times2
{
  protected:
	std::shared_ptr<MultiEventsSet> _multiEventsSet;
	std::shared_ptr<spdlog::logger> _logger;

  public:
	GEOInfoTimes(std::string geoInfoTimesSchedule, std::shared_ptr<MultiEventsSet> multiEventsSet, std::shared_ptr<spdlog::logger> logger);

	virtual ~GEOInfoTimes(void);

	virtual void handleTimeOut(void);
};
