
#ifndef CMSEngineProcessor_h
#define CMSEngineProcessor_h

#include <string>
// #define SPDLOG_DEBUG_ON
// #define SPDLOG_TRACE_ON
#include "spdlog/spdlog.h"
#include "catralibraries/MultiEventsSet.h"
#include "CMSEngineDBFacade.h"
#include "Customers.h"
#include "CMSRepository.h"

#define CMSENGINEPROCESSORNAME    "CMSEngineProcessor"

class CMSEngineProcessor
{
private:
    shared_ptr<spdlog::logger>          _logger;
    shared_ptr<CMSEngineDBFacade>       _cmsEngineDBFacade;
    shared_ptr<Customers>               _customers;
    shared_ptr<CMSRepository>           _cmsRepository;
    
    unsigned long                       _ulIngestionLastCustomerIndex;
    unsigned long                       _ulMaxIngestionsNumberPerCustomerEachIngestionPeriod;

    void handleCheckIngestionEvent();

public:
    CMSEngineProcessor(
            shared_ptr<spdlog::logger> logger, 
            shared_ptr<CMSEngineDBFacade> cmsEngineDBFacade,
            shared_ptr<Customers> customers,
            shared_ptr<CMSRepository> cmsRepository);
    
    ~CMSEngineProcessor();
    
    void operator()(shared_ptr<MultiEventsSet> multiEventsSet);
} ;

#endif

