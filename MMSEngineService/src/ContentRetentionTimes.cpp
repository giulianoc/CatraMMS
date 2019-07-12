
#include "ContentRetentionTimes.h"
#include "catralibraries/Event2.h"


ContentRetentionTimes:: ContentRetentionTimes (string contentRetentionTimesSchedule,
	shared_ptr<MultiEventsSet> multiEventsSet, shared_ptr<spdlog::logger> logger): 
    Times2 (contentRetentionTimesSchedule, MMSENGINE_CONTENTRETENTIONTIMES_CLASSNAME)

{
    _multiEventsSet     = multiEventsSet;
    _logger             = logger;
}

ContentRetentionTimes::~ContentRetentionTimes (void)
{
    
}

void ContentRetentionTimes:: handleTimeOut (void)
{

    lock_guard<mutex>   locker(_mtTimesMutex);

    if (_schTimesStatus != SCHTIMES_STARTED)
    {
        return;
    }

    shared_ptr<Event2>    event = _multiEventsSet->getEventsFactory()->getFreeEvent<Event2>(
            MMSENGINE_EVENTTYPEIDENTIFIER_CONTENTRETENTIONEVENT);

    event->setSource(MMSENGINE_CONTENTRETENTIONTIMES_SOURCE);
    event->setDestination(MMSENGINEPROCESSORNAME);
    event->setExpirationTimePoint(chrono::system_clock::now());

    _multiEventsSet->addEvent(event);
    
    _logger->debug(__FILEREF__ + "addEvent: EVENT_TYPE" 
            + ", MMSENGINE_EVENTTYPEIDENTIFIER_RETENTION"
            + ", getEventKey().first: " + to_string(event->getEventKey().first)
            + ", getEventKey().second: " + to_string(event->getEventKey().second)
    );
}
