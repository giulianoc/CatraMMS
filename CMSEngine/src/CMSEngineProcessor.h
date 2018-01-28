
#ifndef CMSEngineProcessor_h
#define CMSEngineProcessor_h

#include <string>
// #define SPDLOG_DEBUG_ON
// #define SPDLOG_TRACE_ON
#include "spdlog/spdlog.h"
#include "catralibraries/MultiEventsSet.h"
#include "CMSEngineDBFacade.h"
#include "Customers.h"

#define CMSENGINEPROCESSORNAME    "CMSEngineProcessor"

class CMSEngineProcessor
{
private:
    shared_ptr<spdlog::logger>          _logger;
    shared_ptr<CMSEngineDBFacade>       _cmsEngineDBFacade;
    shared_ptr<Customers>               _customers;
    
    unsigned long                       _ulIngestionLastCustomerIndex;
    
    void handleCheckIngestionEvent();

public:
    CMSEngineProcessor(
            shared_ptr<spdlog::logger> logger, 
            shared_ptr<CMSEngineDBFacade> cmsEngineDBFacade,
            shared_ptr<Customers> customers);
    
    ~CMSEngineProcessor();
    
    void operator()(shared_ptr<MultiEventsSet> multiEventsSet);
} ;

#endif

