
#pragma once

#include <memory>
#include "MultiEventsSet.h"
#include "Times2.h"
#include "spdlog/spdlog.h"

#define MMSENGINE_DBDATARETENTIONTIMES_CLASSNAME "DBDataRetentionTimes"
#define MMSENGINE_DBDATARETENTIONTIMES_SOURCE "DBDataRetentionTimes"

#define MMSENGINE_EVENTTYPEIDENTIFIER_DBDATARETENTIONEVENT 7
#define MMSENGINEPROCESSORNAME "MMSEngineProcessor"

#ifndef __FILEREF__
#ifdef __APPLE__
#define __FILEREF__ string("[") + string(__FILE__).substr(string(__FILE__).find_last_of("/") + 1) + ":" + to_string(__LINE__) + "] "
#else
#define __FILEREF__ string("[") + basename(__FILE__) + ":" + to_string(__LINE__) + "] "
#endif
#endif

class DBDataRetentionTimes : public Times2
{
  protected:
	std::shared_ptr<MultiEventsSet> _multiEventsSet;
	std::shared_ptr<spdlog::logger> _logger;

  public:
	DBDataRetentionTimes(std::string dbDataRetentionTimesSchedule, std::shared_ptr<MultiEventsSet> multiEventsSet, std::shared_ptr<spdlog::logger> logger);

	virtual ~DBDataRetentionTimes(void);

	virtual void handleTimeOut(void);
};
