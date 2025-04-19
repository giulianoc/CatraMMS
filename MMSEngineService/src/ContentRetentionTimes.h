
#ifndef ContentRetentionTimes_h
#define ContentRetentionTimes_h

#include <memory>
#ifndef SPDLOG_ACTIVE_LEVEL
#define SPDLOG_ACTIVE_LEVEL SPDLOG_LEVEL_TRACE
#endif
#include "MultiEventsSet.h"
#include "Times2.h"
#include "spdlog/spdlog.h"

#define MMSENGINE_CONTENTRETENTIONTIMES_CLASSNAME "ContentRetentionTimes"
#define MMSENGINE_CONTENTRETENTIONTIMES_SOURCE "ContentRetentionTimes"

#define MMSENGINE_EVENTTYPEIDENTIFIER_CONTENTRETENTIONEVENT 4
#define MMSENGINEPROCESSORNAME "MMSEngineProcessor"

#ifndef __FILEREF__
#ifdef __APPLE__
#define __FILEREF__ string("[") + string(__FILE__).substr(string(__FILE__).find_last_of("/") + 1) + ":" + to_string(__LINE__) + "] "
#else
#define __FILEREF__ string("[") + basename(__FILE__) + ":" + to_string(__LINE__) + "] "
#endif
#endif

class ContentRetentionTimes : public Times2
{
  protected:
	shared_ptr<MultiEventsSet> _multiEventsSet;
	shared_ptr<spdlog::logger> _logger;

  public:
	ContentRetentionTimes(string contentRetentionTimesSchedule, shared_ptr<MultiEventsSet> multiEventsSet, shared_ptr<spdlog::logger> logger);

	virtual ~ContentRetentionTimes(void);

	virtual void handleTimeOut(void);
};

#endif
