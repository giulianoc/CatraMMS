
#include "IngestionDataRetentionTimes.h"
#include "catralibraries/Event2.h"


IngestionDataRetentionTimes:: IngestionDataRetentionTimes (string ingestionDataRetentionTimesSchedule,
	shared_ptr<MultiEventsSet> multiEventsSet, shared_ptr<spdlog::logger> logger): 
    Times2 (ingestionDataRetentionTimesSchedule, MMSENGINE_INGESTIONDATARETENTIONTIMES_CLASSNAME)

{
    _multiEventsSet     = multiEventsSet;
    _logger             = logger;
}

IngestionDataRetentionTimes::~IngestionDataRetentionTimes (void)
{
    
}

void IngestionDataRetentionTimes:: handleTimeOut (void)
{

    lock_guard<mutex>   locker(_mtTimesMutex);

    if (_schTimesStatus != SCHTIMES_STARTED)
    {
        return;
    }

    shared_ptr<Event2>    event = _multiEventsSet->getEventsFactory()->getFreeEvent<Event2>(
            MMSENGINE_EVENTTYPEIDENTIFIER_INGESTIONDATARETENTIONEVENT);

    event->setSource(MMSENGINE_INGESTIONDATARETENTIONTIMES_SOURCE);
    event->setDestination(MMSENGINEPROCESSORNAME);
    event->setExpirationTimePoint(chrono::system_clock::now());

    _multiEventsSet->addEvent(event);
    
    _logger->debug(__FILEREF__ + "addEvent: EVENT_TYPE" 
            + ", MMSENGINE_EVENTTYPEIDENTIFIER_RETENTION"
            + ", getEventKey().first: " + to_string(event->getEventKey().first)
            + ", getEventKey().second: " + to_string(event->getEventKey().second)
    );
}
