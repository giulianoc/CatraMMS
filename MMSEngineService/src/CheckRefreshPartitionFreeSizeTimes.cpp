
#include "CheckRefreshPartitionFreeSizeTimes.h"
#include "catralibraries/Event2.h"


CheckRefreshPartitionFreeSizeTimes:: CheckRefreshPartitionFreeSizeTimes (
		string refreshPartitionFreeSizeTimesSchedule,
	shared_ptr<MultiEventsSet> multiEventsSet, shared_ptr<spdlog::logger> logger): 
    Times2 (refreshPartitionFreeSizeTimesSchedule,
			MMSENGINE_CHECKREFRESHPARTITIONFREESIZETIMES_CLASSNAME)

{
    _multiEventsSet     = multiEventsSet;
    _logger             = logger;
}

CheckRefreshPartitionFreeSizeTimes::~CheckRefreshPartitionFreeSizeTimes (void)
{
    
}

void CheckRefreshPartitionFreeSizeTimes:: handleTimeOut (void)
{

    lock_guard<mutex>   locker(_mtTimesMutex);

    if (_schTimesStatus != SCHTIMES_STARTED)
    {
        return;
    }

    shared_ptr<Event2>    event = _multiEventsSet->getEventsFactory()->getFreeEvent<Event2>(
			MMSENGINE_EVENTTYPEIDENTIFIER_CHECKREFRESHPARTITIONFREESIZEEVENT);

    event->setSource(MMSENGINE_CHECKREFRESHPARTITIONFREESIZETIMES_SOURCE);
    event->setDestination(MMSENGINEPROCESSORNAME);
    event->setExpirationTimePoint(chrono::system_clock::now());

    _multiEventsSet->addEvent(event);
    
    _logger->debug(__FILEREF__ + "addEvent: EVENT_TYPE" 
            + ", MMSENGINE_EVENTTYPEIDENTIFIER_CHECKREFRESHPARTITIONFREESIZE"
            + ", getEventKey().first: " + to_string(event->getEventKey().first)
            + ", getEventKey().second: " + to_string(event->getEventKey().second)
    );
}
