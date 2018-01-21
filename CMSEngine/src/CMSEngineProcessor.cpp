
#include "CMSEngineProcessor.h"
#include "CheckIngestionTimes.h"

CMSEngineProcessor::CMSEngineProcessor(shared_ptr<spdlog::logger> logger)
{
    _logger     = logger;
}

CMSEngineProcessor::~CMSEngineProcessor()
{
    
}

void CMSEngineProcessor::operator ()(shared_ptr<MultiEventsSet> multiEventsSet) 
{
    bool blocking = true;
    chrono::milliseconds milliSecondsToBlock(100);

    // SPDLOG_DEBUG(_logger , "Enabled only #ifdef SPDLOG_TRACE_ON..{} ,{}", 1, 3.23);
    // SPDLOG_TRACE(_logger , "Enabled only #ifdef SPDLOG_TRACE_ON..{} ,{}", 1, 3.23);
    _logger->info("CMSEngineProcessor thread started");

    bool endEvent = false;
    while(!endEvent)
    {
        // cout << "Calling getAndRemoveFirstEvent" << endl;
        shared_ptr<Event> event = multiEventsSet->getAndRemoveFirstEvent(CMSENGINEPROCESSORNAME, blocking, milliSecondsToBlock);
        if (event == nullptr)
        {
            // cout << "No event found or event not yet expired" << endl;

            continue;
        }

        switch(event->getEventKey().first)
        {
            /*
            case GETDATAEVENT_TYPE:
            {                    
                shared_ptr<GetDataEvent>    getDataEvent = dynamic_pointer_cast<GetDataEvent>(event);
                cout << "getAndRemoveFirstEvent: GETDATAEVENT_TYPE (" << event->getEventKey().first << ", " << event->getEventKey().second << "): " << getDataEvent->getDataId() << endl << endl;
                multiEventsSet.getEventsFactory()->releaseEvent<GetDataEvent>(getDataEvent);
            }
            break;
            */
            case CMS_EVENT_CHECKINGESTION:	// 10
            {
                _logger->info("Received CMS_EVENT_CHECKINGESTION");

		handleCheckIngestionEvent ();

                multiEventsSet->getEventsFactory()->releaseEvent<Event>(event);

            }
            break;
            default:
                throw invalid_argument(string("Event type identifier not managed")
                        + to_string(event->getEventKey().first));
        }
    }

    _logger->info("CMSEngineProcessor thread terminated");
}

void CMSEngineProcessor::handleCheckIngestionEvent()
{
}