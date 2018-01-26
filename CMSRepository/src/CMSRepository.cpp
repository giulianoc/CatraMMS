/*
#include "CMSRepository.h"
#include "catralibraries/HttpGetThread.h"
#include "catralibraries/HttpPostThread.h"
#include "catralibraries/WebUtility.h"
#include "catralibraries/FileIO.h"
#include "catralibraries/System.h"
#include "catralibraries/DateTime.h"
#include <stdlib.h>
#include <libxml/parser.h>



CMSRepository:: CMSRepository (void)
{
}

CMSRepository:: ~CMSRepository (void)
{
}

Error CMSRepository:: init (
	ConfigurationFile_p pcfConfiguration,
	LoadBalancer_p plbWebServerLoadBalancer,
	Tracer_p ptTracer)

{

    Error_t					errSystem;
    Error_t					errCreateDir;
    Error_t					errGetItemValue;
    char					pConfigurationBuffer [
            CMSREP_CMSREPOSITORY_MAXCONFIGURATIONITEMLENGTH];


    if ((errSystem = System:: getHostName (_pHostName,
            CMSREP_CMSREPOSITORY_MAXHOSTNAMELENGTH_FAILED)) != errNoError)
    {
            ptTracer -> trace (Tracer:: TRACER_LERRR,
                    (const char *) errSystem,
                    __FILE__, __LINE__);

            Error err = ToolsErrors (__FILE__, __LINE__,
                    TOOLS_SYSTEM_GETHOSTNAME_FAILED);
            ptTracer -> trace (Tracer:: TRACER_LERRR, (const char *) err,
                    __FILE__, __LINE__);

            return err;
    }

    if (pcfConfiguration == (ConfigurationFile_p) NULL ||
            plbWebServerLoadBalancer == (LoadBalancer_p) NULL ||
            ptTracer == (Tracer_p) NULL)
    {
            Error err = CMSRepositoryErrors (__FILE__, __LINE__,
                    CMSREP_ACTIVATION_WRONG);
            ptTracer -> trace (Tracer:: TRACER_LERRR,
                    (const char *) err, __FILE__, __LINE__);

            return err;
    }

    _plbWebServerLoadBalancer		= plbWebServerLoadBalancer;
    _pcfConfiguration				= pcfConfiguration;
    _ptSystemTracer					= ptTracer;

    _pbRepositories [CMSREP_REPOSITORYTYPE_CMSCUSTOMER]		=
            &_bCMSRootRepository;
    _pbRepositories [CMSREP_REPOSITORYTYPE_DOWNLOAD]		=
            &_bDownloadRootRepository;
    _pbRepositories [CMSREP_REPOSITORYTYPE_STREAMING]		=
            &_bStreamingRootRepository;
    _pbRepositories [CMSREP_REPOSITORYTYPE_STAGING]		=
            &_bStagingRootRepository;
    _pbRepositories [CMSREP_REPOSITORYTYPE_DONE]		=
            &_bDoneRootRepository;
    _pbRepositories [CMSREP_REPOSITORYTYPE_ERRORS]		=
            &_bErrorRootRepository;
    _pbRepositories [CMSREP_REPOSITORYTYPE_FTP]		=
            &_bFTPRootRepository;

    if ((errGetItemValue = _pcfConfiguration -> getItemValue ("cmsEngine",
            "IPhoneAliasForLive", _pIPhoneAliasForLive,
            CMSREP_CMSREPOSITORY_MAXRESERVEDDIRLENGTH)) != errNoError)
    {
            ptTracer -> trace (Tracer:: TRACER_LERRR,
                    (const char *) errGetItemValue,
                    __FILE__, __LINE__);

            Error err = ConfigurationErrors (__FILE__, __LINE__,
                    CFG_CONFIG_GETITEMVALUE_FAILED,
                    2, "CMSEngine", "IPhoneAliasForLive");
            ptTracer -> trace (Tracer:: TRACER_LERRR, (const char *) err,
                    __FILE__, __LINE__);

            return err;
    }

    if ((errGetItemValue = _pcfConfiguration -> getItemValue ("SanityCheck",
            "DownloadReservedDirectoryName", _pDownloadReservedDirectoryName,
            CMSREP_CMSREPOSITORY_MAXRESERVEDDIRLENGTH)) != errNoError)
    {
            ptTracer -> trace (Tracer:: TRACER_LERRR,
                    (const char *) errGetItemValue,
                    __FILE__, __LINE__);

            Error err = ConfigurationErrors (__FILE__, __LINE__,
                    CFG_CONFIG_GETITEMVALUE_FAILED,
                    2, "SanityCheck",
                    "DownloadReservedDirectoryName");
            ptTracer -> trace (Tracer:: TRACER_LERRR, (const char *) err,
                    __FILE__, __LINE__);

            return err;
    }

    if ((errGetItemValue = _pcfConfiguration -> getItemValue ("SanityCheck",
            "DownloadFreeDirectoryName", _pDownloadFreeDirectoryName,
            CMSREP_CMSREPOSITORY_MAXRESERVEDDIRLENGTH)) != errNoError)
    {
            ptTracer -> trace (Tracer:: TRACER_LERRR,
                    (const char *) errGetItemValue,
                    __FILE__, __LINE__);

            Error err = ConfigurationErrors (__FILE__, __LINE__,
                    CFG_CONFIG_GETITEMVALUE_FAILED,
                    2, "SanityCheck",
                    "DownloadFreeDirectoryName");
            ptTracer -> trace (Tracer:: TRACER_LERRR, (const char *) err,
                    __FILE__, __LINE__);

            return err;
    }

    if ((errGetItemValue = _pcfConfiguration -> getItemValue ("SanityCheck",
            "DownloadiPhoneLiveDirectoryName", _pDownloadiPhoneLiveDirectoryName,
            CMSREP_CMSREPOSITORY_MAXRESERVEDDIRLENGTH)) != errNoError)
    {
            ptTracer -> trace (Tracer:: TRACER_LERRR,
                    (const char *) errGetItemValue,
                    __FILE__, __LINE__);

            Error err = ConfigurationErrors (__FILE__, __LINE__,
                    CFG_CONFIG_GETITEMVALUE_FAILED,
                    2, "SanityCheck",
                    "DownloadiPhoneLiveDirectoryName");
            ptTracer -> trace (Tracer:: TRACER_LERRR, (const char *) err,
                    __FILE__, __LINE__);

            return err;
    }

    if ((errGetItemValue = _pcfConfiguration -> getItemValue ("SanityCheck",
            "DownloadSilverlightLiveDirectoryName",
            _pDownloadSilverlightLiveDirectoryName,
            CMSREP_CMSREPOSITORY_MAXRESERVEDDIRLENGTH)) != errNoError)
    {
            ptTracer -> trace (Tracer:: TRACER_LERRR,
                    (const char *) errGetItemValue,
                    __FILE__, __LINE__);

            Error err = ConfigurationErrors (__FILE__, __LINE__,
                    CFG_CONFIG_GETITEMVALUE_FAILED,
                    2, "SanityCheck",
                    "DownloadSilverlightLiveDirectoryName");
            ptTracer -> trace (Tracer:: TRACER_LERRR, (const char *) err,
                    __FILE__, __LINE__);

            return err;
    }

    if ((errGetItemValue = _pcfConfiguration -> getItemValue ("SanityCheck",
            "DownloadAdobeLiveDirectoryName",
            _pDownloadAdobeLiveDirectoryName,
            CMSREP_CMSREPOSITORY_MAXRESERVEDDIRLENGTH)) != errNoError)
    {
            ptTracer -> trace (Tracer:: TRACER_LERRR,
                    (const char *) errGetItemValue,
                    __FILE__, __LINE__);

            Error err = ConfigurationErrors (__FILE__, __LINE__,
                    CFG_CONFIG_GETITEMVALUE_FAILED,
                    2, "SanityCheck",
                    "DownloadAdobeLiveDirectoryName");
            ptTracer -> trace (Tracer:: TRACER_LERRR, (const char *) err,
                    __FILE__, __LINE__);

            return err;
    }

    if ((errGetItemValue = _pcfConfiguration -> getItemValue ("SanityCheck",
            "StreamingFreeDirectoryName", _pStreamingFreeDirectoryName,
            CMSREP_CMSREPOSITORY_MAXRESERVEDDIRLENGTH)) != errNoError)
    {
            ptTracer -> trace (Tracer:: TRACER_LERRR,
                    (const char *) errGetItemValue,
                    __FILE__, __LINE__);

            Error err = ConfigurationErrors (__FILE__, __LINE__,
                    CFG_CONFIG_GETITEMVALUE_FAILED,
                    2, "SanityCheck",
                    "StreamingFreeDirectoryName");
            ptTracer -> trace (Tracer:: TRACER_LERRR, (const char *) err,
                    __FILE__, __LINE__);

            return err;
    }

    if ((errGetItemValue = _pcfConfiguration -> getItemValue ("SanityCheck",
            "StreamingMetaDirectoryName", _pStreamingMetaDirectoryName,
            CMSREP_CMSREPOSITORY_MAXRESERVEDDIRLENGTH)) != errNoError)
    {
            ptTracer -> trace (Tracer:: TRACER_LERRR,
                    (const char *) errGetItemValue,
                    __FILE__, __LINE__);

            Error err = ConfigurationErrors (__FILE__, __LINE__,
                    CFG_CONFIG_GETITEMVALUE_FAILED,
                    2, "SanityCheck",
                    "StreamingMetaDirectoryName");
            ptTracer -> trace (Tracer:: TRACER_LERRR, (const char *) err,
                    __FILE__, __LINE__);

            return err;
    }

    if ((errGetItemValue = _pcfConfiguration -> getItemValue ("SanityCheck",
            "StreamingRecordingDirectoryName", _pStreamingRecordingDirectoryName,
            CMSREP_CMSREPOSITORY_MAXRESERVEDDIRLENGTH)) != errNoError)
    {
            ptTracer -> trace (Tracer:: TRACER_LERRR,
                    (const char *) errGetItemValue,
                    __FILE__, __LINE__);

            Error err = ConfigurationErrors (__FILE__, __LINE__,
                    CFG_CONFIG_GETITEMVALUE_FAILED,
                    2, "SanityCheck",
                    "StreamingRecordingDirectoryName");
            ptTracer -> trace (Tracer:: TRACER_LERRR, (const char *) err,
                    __FILE__, __LINE__);

            return err;
    }

    if ((errGetItemValue = _pcfConfiguration -> getItemValue ("HTTPProxy",
            "LocalIPAddress", _pWebServerLocalIPAddress,
            SCK_MAXIPADDRESSLENGTH)) != errNoError)
    {
            ptTracer -> trace (Tracer:: TRACER_LERRR,
                    (const char *) errGetItemValue,
                    __FILE__, __LINE__);

            Error err = ConfigurationErrors (__FILE__, __LINE__,
                    CFG_CONFIG_GETITEMVALUE_FAILED,
                    2, "HTTPProxy", "LocalIPAddress");
            ptTracer -> trace (Tracer:: TRACER_LERRR, (const char *) err,
                    __FILE__, __LINE__);

            return err;
    }

    if ((errGetItemValue = _pcfConfiguration -> getItemValue ("HTTPProxy",
            "TimeoutToWaitAnswerInSeconds", pConfigurationBuffer,
            CMSREP_CMSREPOSITORY_MAXCONFIGURATIONITEMLENGTH)) != errNoError)
    {
            ptTracer -> trace (Tracer:: TRACER_LERRR,
                    (const char *) errGetItemValue,
                    __FILE__, __LINE__);

            Error err = ConfigurationErrors (__FILE__, __LINE__,
                    CFG_CONFIG_GETITEMVALUE_FAILED,
                    2, "HTTPProxy", "TimeoutToWaitAnswerInSeconds");
            ptTracer -> trace (Tracer:: TRACER_LERRR, (const char *) err,
                    __FILE__, __LINE__);

            return err;
    }
    _ulWebServerTimeoutToWaitAnswerInSeconds		=
            strtoul (pConfigurationBuffer, (char **) NULL, 10);

    if ((errGetItemValue = _pcfConfiguration -> getItemValue ("SanityCheck",
            "UnexpectedFilesToBeRemoved", pConfigurationBuffer,
            CMSREP_CMSREPOSITORY_MAXCONFIGURATIONITEMLENGTH)) != errNoError)
    {
            ptTracer -> trace (Tracer:: TRACER_LERRR,
                    (const char *) errGetItemValue,
                    __FILE__, __LINE__);

            Error err = ConfigurationErrors (__FILE__, __LINE__,
                    CFG_CONFIG_GETITEMVALUE_FAILED,
                    2, "SanityCheck",
                    "UnexpectedFilesToBeRemoved");
            ptTracer -> trace (Tracer:: TRACER_LERRR, (const char *) err,
                    __FILE__, __LINE__);

            return err;
    }
    #ifdef WIN32
            if (!stricmp (pConfigurationBuffer, "true"))
    #else
            if (!strcasecmp (pConfigurationBuffer, "true"))
    #endif
            _bUnexpectedFilesToBeRemoved				= true;
    else
            _bUnexpectedFilesToBeRemoved				= false;

    if ((errGetItemValue = _pcfConfiguration -> getItemValue ("SanityCheck",
            "RetentionPeriodInDaysForTemporaryFiles", pConfigurationBuffer,
            CMSREP_CMSREPOSITORY_MAXCONFIGURATIONITEMLENGTH)) != errNoError)
    {
            ptTracer -> trace (Tracer:: TRACER_LERRR,
                    (const char *) errGetItemValue,
                    __FILE__, __LINE__);

            Error err = ConfigurationErrors (__FILE__, __LINE__,
                    CFG_CONFIG_GETITEMVALUE_FAILED,
                    2, "SanityCheck",
                    "RetentionPeriodInDaysForTemporaryFiles");
            ptTracer -> trace (Tracer:: TRACER_LERRR, (const char *) err,
                    __FILE__, __LINE__);

            return err;
    }
    _ulRetentionPeriodInSecondsForTemporaryFiles		= 
            strtoul (pConfigurationBuffer, (char **) NULL, 10) * 24 * 60 * 60;

    if ((errGetItemValue = _pcfConfiguration -> getItemValue ("SanityCheck",
            "MaxCMSCustomerFilesToBeProcessedPerSchedule", pConfigurationBuffer,
            CMSREP_CMSREPOSITORY_MAXCONFIGURATIONITEMLENGTH)) != errNoError)
    {
            ptTracer -> trace (Tracer:: TRACER_LERRR,
                    (const char *) errGetItemValue,
                    __FILE__, __LINE__);

            Error err = ConfigurationErrors (__FILE__, __LINE__,
                    CFG_CONFIG_GETITEMVALUE_FAILED,
                    2, "SanityCheck",
                    "MaxCMSCustomerFilesToBeProcessedPerSchedule");
            ptTracer -> trace (Tracer:: TRACER_LERRR, (const char *) err,
                    __FILE__, __LINE__);

            return err;
    }
    _ulMaxFilesToBeProcessedPerSchedule [CMSREP_REPOSITORYTYPE_CMSCUSTOMER]	=
            strtoul (pConfigurationBuffer, (char **) NULL, 10);

    if ((errGetItemValue = _pcfConfiguration -> getItemValue ("SanityCheck",
            "MaxDownloadFilesToBeProcessedPerSchedule", pConfigurationBuffer,
            CMSREP_CMSREPOSITORY_MAXCONFIGURATIONITEMLENGTH)) != errNoError)
    {
            ptTracer -> trace (Tracer:: TRACER_LERRR,
                    (const char *) errGetItemValue,
                    __FILE__, __LINE__);

            Error err = ConfigurationErrors (__FILE__, __LINE__,
                    CFG_CONFIG_GETITEMVALUE_FAILED,
                    2, "SanityCheck",
                    "MaxDownloadFilesToBeProcessedPerSchedule");
            ptTracer -> trace (Tracer:: TRACER_LERRR, (const char *) err,
                    __FILE__, __LINE__);

            return err;
    }
    _ulMaxFilesToBeProcessedPerSchedule [CMSREP_REPOSITORYTYPE_DOWNLOAD]	=
            strtoul (pConfigurationBuffer, (char **) NULL, 10);

    if ((errGetItemValue = _pcfConfiguration -> getItemValue ("SanityCheck",
            "MaxStreamingFilesToBeProcessedPerSchedule", pConfigurationBuffer,
            CMSREP_CMSREPOSITORY_MAXCONFIGURATIONITEMLENGTH)) != errNoError)
    {
            ptTracer -> trace (Tracer:: TRACER_LERRR,
                    (const char *) errGetItemValue,
                    __FILE__, __LINE__);

            Error err = ConfigurationErrors (__FILE__, __LINE__,
                    CFG_CONFIG_GETITEMVALUE_FAILED,
                    2, "SanityCheck",
                    "MaxStreamingFilesToBeProcessedPerSchedule");
            ptTracer -> trace (Tracer:: TRACER_LERRR, (const char *) err,
                    __FILE__, __LINE__);

            return err;
    }
    _ulMaxFilesToBeProcessedPerSchedule [CMSREP_REPOSITORYTYPE_STREAMING]	=
            strtoul (pConfigurationBuffer, (char **) NULL, 10);

    if ((errGetItemValue = _pcfConfiguration -> getItemValue ("SanityCheck",
            "MaxStagingFilesToBeProcessedPerSchedule", pConfigurationBuffer,
            CMSREP_CMSREPOSITORY_MAXCONFIGURATIONITEMLENGTH)) != errNoError)
    {
            ptTracer -> trace (Tracer:: TRACER_LERRR,
                    (const char *) errGetItemValue,
                    __FILE__, __LINE__);

            Error err = ConfigurationErrors (__FILE__, __LINE__,
                    CFG_CONFIG_GETITEMVALUE_FAILED,
                    2, "SanityCheck",
                    "MaxStagingFilesToBeProcessedPerSchedule");
            ptTracer -> trace (Tracer:: TRACER_LERRR, (const char *) err,
                    __FILE__, __LINE__);

            return err;
    }
    _ulMaxFilesToBeProcessedPerSchedule [CMSREP_REPOSITORYTYPE_STAGING]	=
            strtoul (pConfigurationBuffer, (char **) NULL, 10);

    if ((errGetItemValue = _pcfConfiguration -> getItemValue ("SanityCheck",
            "MaxDoneFilesToBeProcessedPerSchedule", pConfigurationBuffer,
            CMSREP_CMSREPOSITORY_MAXCONFIGURATIONITEMLENGTH)) != errNoError)
    {
            ptTracer -> trace (Tracer:: TRACER_LERRR,
                    (const char *) errGetItemValue,
                    __FILE__, __LINE__);

            Error err = ConfigurationErrors (__FILE__, __LINE__,
                    CFG_CONFIG_GETITEMVALUE_FAILED,
                    2, "SanityCheck",
                    "MaxDoneFilesToBeProcessedPerSchedule");
            ptTracer -> trace (Tracer:: TRACER_LERRR, (const char *) err,
                    __FILE__, __LINE__);

            return err;
    }
    _ulMaxFilesToBeProcessedPerSchedule [CMSREP_REPOSITORYTYPE_DONE]	=
            strtoul (pConfigurationBuffer, (char **) NULL, 10);

    if ((errGetItemValue = _pcfConfiguration -> getItemValue ("SanityCheck",
            "MaxErrorsFilesToBeProcessedPerSchedule", pConfigurationBuffer,
            CMSREP_CMSREPOSITORY_MAXCONFIGURATIONITEMLENGTH)) != errNoError)
    {
            ptTracer -> trace (Tracer:: TRACER_LERRR,
                    (const char *) errGetItemValue,
                    __FILE__, __LINE__);

            Error err = ConfigurationErrors (__FILE__, __LINE__,
                    CFG_CONFIG_GETITEMVALUE_FAILED,
                    2, "SanityCheck",
                    "MaxErrorsFilesToBeProcessedPerSchedule");
            ptTracer -> trace (Tracer:: TRACER_LERRR, (const char *) err,
                    __FILE__, __LINE__);

            return err;
    }
    _ulMaxFilesToBeProcessedPerSchedule [CMSREP_REPOSITORYTYPE_ERRORS]	=
            strtoul (pConfigurationBuffer, (char **) NULL, 10);

    if ((errGetItemValue = _pcfConfiguration -> getItemValue ("SanityCheck",
            "MaxFTPFilesToBeProcessedPerSchedule", pConfigurationBuffer,
            CMSREP_CMSREPOSITORY_MAXCONFIGURATIONITEMLENGTH)) != errNoError)
    {
            ptTracer -> trace (Tracer:: TRACER_LERRR,
                    (const char *) errGetItemValue,
                    __FILE__, __LINE__);

            Error err = ConfigurationErrors (__FILE__, __LINE__,
                    CFG_CONFIG_GETITEMVALUE_FAILED,
                    2, "SanityCheck",
                    "MaxFTPFilesToBeProcessedPerSchedule");
            ptTracer -> trace (Tracer:: TRACER_LERRR, (const char *) err,
                    __FILE__, __LINE__);

            return err;
    }
    _ulMaxFilesToBeProcessedPerSchedule [CMSREP_REPOSITORYTYPE_FTP]	=
            strtoul (pConfigurationBuffer, (char **) NULL, 10);

//    _pulFilesNumberAlreadyProcessed [CMSREP_REPOSITORYTYPE_CMSCUSTOMER]		= 0;
//    _pulFilesNumberAlreadyProcessed [CMSREP_REPOSITORYTYPE_DOWNLOAD]		= 0;
//    _pulFilesNumberAlreadyProcessed [CMSREP_REPOSITORYTYPE_STREAMING]		= 0;
//    _pulFilesNumberAlreadyProcessed [CMSREP_REPOSITORYTYPE_STAGING]			= 0;
//    _pulFilesNumberAlreadyProcessed [CMSREP_REPOSITORYTYPE_DONE]			= 0;
//    _pulFilesNumberAlreadyProcessed [CMSREP_REPOSITORYTYPE_ERRORS]			= 0;
//    _pulFilesNumberAlreadyProcessed [CMSREP_REPOSITORYTYPE_FTP]				= 0;

    if ((errGetItemValue = _pcfConfiguration -> getItemValue ("cmsEngine",
            "FreeSpaceToLeaveInEachPartition", pConfigurationBuffer,
            CMSREP_CMSREPOSITORY_MAXCONFIGURATIONITEMLENGTH)) != errNoError)
    {
            ptTracer -> trace (Tracer:: TRACER_LERRR,
                    (const char *) errGetItemValue,
                    __FILE__, __LINE__);

            Error err = ConfigurationErrors (__FILE__, __LINE__,
                    CFG_CONFIG_GETITEMVALUE_FAILED,
                    2, "cmsEngine", "FreeSpaceToLeaveinEachPartition");
            ptTracer -> trace (Tracer:: TRACER_LERRR, (const char *) err,
                    __FILE__, __LINE__);

            return err;
    }
    _ullFreeSpaceToLeaveInEachPartition		=
            strtoull (pConfigurationBuffer, (char **) NULL, 10);

    if ((errGetItemValue = _pcfConfiguration -> getItemValue ("cmsEngine",
            "FTPRootRepository", pConfigurationBuffer,
            CMSREP_CMSREPOSITORY_MAXCONFIGURATIONITEMLENGTH)) != errNoError)
    {
            _ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
                    (const char *) errGetItemValue,
                    __FILE__, __LINE__);

            Error err = ConfigurationErrors (__FILE__, __LINE__,
                    CFG_CONFIG_GETITEMVALUE_FAILED,
                    2, "cmsEngine", "FTPRootRepository");
            _ptSystemTracer -> trace (Tracer:: TRACER_LERRR, (const char *) err,
                    __FILE__, __LINE__);

            return err;
    }

    if (_bFTPRootRepository. init (pConfigurationBuffer) != errNoError)
    {
            Error err = ToolsErrors (__FILE__, __LINE__,
                    TOOLS_BUFFER_INIT_FAILED);
            _ptSystemTracer -> trace (Tracer:: TRACER_LERRR, (const char *) err,
                    __FILE__, __LINE__);

            return err;
    }

    #ifdef WIN32
            if (pConfigurationBuffer [strlen (pConfigurationBuffer) - 1] != '\\')
            {
                    if (_bFTPRootRepository. append ("\\") != errNoError)
                    {
                            Error err = ToolsErrors (__FILE__, __LINE__,
                                    TOOLS_BUFFER_APPEND_FAILED);
                            _ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
                                    (const char *) err, __FILE__, __LINE__);

                            return err;
                    }
            }
    #else
            if (pConfigurationBuffer [strlen (pConfigurationBuffer) - 1] != '/')
            {
                    if (_bFTPRootRepository. append ("/") != errNoError)
                    {
                            Error err = ToolsErrors (__FILE__, __LINE__,
                                    TOOLS_BUFFER_APPEND_FAILED);
                            _ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
                                    (const char *) err, __FILE__, __LINE__);

                            return err;
                    }
            }
    #endif

    #ifdef WIN32
            if ((errCreateDir = FileIO:: createDirectory (
                    (const char *) _bFTPRootRepository,
                    0, true, true)) != errNoError)
    #else
            if ((errCreateDir = FileIO:: createDirectory (
                    (const char *) _bFTPRootRepository,
                    S_IRUSR | S_IWUSR | S_IXUSR |
                    S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH, true, true)) !=
                    errNoError)
    #endif
    {
            Error err = ToolsErrors (__FILE__, __LINE__,
                    TOOLS_FILEIO_CREATEDIRECTORY_FAILED,
                    1, (const char *) _bFTPRootRepository);
            _ptSystemTracer -> trace (Tracer:: TRACER_LERRR, (const char *) err,
                    __FILE__, __LINE__);

            return err;
    }

    if ((errGetItemValue = _pcfConfiguration -> getItemValue ("cmsEngine",
            "CMSRootRepository", pConfigurationBuffer,
            CMSREP_CMSREPOSITORY_MAXCONFIGURATIONITEMLENGTH)) != errNoError)
    {
            _ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
                    (const char *) errGetItemValue,
                    __FILE__, __LINE__);

            Error err = ConfigurationErrors (__FILE__, __LINE__,
                    CFG_CONFIG_GETITEMVALUE_FAILED,
                    2, "cmsEngine", "CMSRootRepository");
            _ptSystemTracer -> trace (Tracer:: TRACER_LERRR, (const char *) err,
                    __FILE__, __LINE__);

            return err;
    }

    if (_bCMSRootRepository. init (pConfigurationBuffer) != errNoError)
    {
            Error err = ToolsErrors (__FILE__, __LINE__,
                    TOOLS_BUFFER_INIT_FAILED);
            _ptSystemTracer -> trace (Tracer:: TRACER_LERRR, (const char *) err,
                    __FILE__, __LINE__);

            return err;
    }

    #ifdef WIN32
            if (pConfigurationBuffer [strlen (pConfigurationBuffer) - 1] != '\\')
            {
                    if (_bCMSRootRepository. append ("\\") != errNoError)
                    {
                            Error err = ToolsErrors (__FILE__, __LINE__,
                                    TOOLS_BUFFER_APPEND_FAILED);
                            _ptSystemTracer -> trace (Tracer:: TRACER_LERRR, (const char *) err,
                                    __FILE__, __LINE__);

                            return err;
                    }
            }
    #else
            if (pConfigurationBuffer [strlen (pConfigurationBuffer) - 1] != '/')
            {
                    if (_bCMSRootRepository. append ("/") != errNoError)
                    {
                            Error err = ToolsErrors (__FILE__, __LINE__,
                                    TOOLS_BUFFER_APPEND_FAILED);
                            _ptSystemTracer -> trace (Tracer:: TRACER_LERRR, (const char *) err,
                                    __FILE__, __LINE__);

                            return err;
                    }
            }
    #endif

    #ifdef WIN32
            if ((errCreateDir = FileIO:: createDirectory (
                    (const char *) _bCMSRootRepository,
                    0, true, true)) != errNoError)
    #else
            if ((errCreateDir = FileIO:: createDirectory (
                    (const char *) _bCMSRootRepository,
                    S_IRUSR | S_IWUSR | S_IXUSR |
                    S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH, true, true)) !=
                    errNoError)
    #endif
    {
            Error err = ToolsErrors (__FILE__, __LINE__,
                    TOOLS_FILEIO_CREATEDIRECTORY_FAILED,
                    1, (const char *) _bCMSRootRepository);
            _ptSystemTracer -> trace (Tracer:: TRACER_LERRR, (const char *) err,
                    __FILE__, __LINE__);

            return err;
    }

    // create CMS_0000 in case it does not exist (first running of CMS)
    {
            Buffer_t			bCMS_0000Path;


            if (bCMS_0000Path. init ((const char *) _bCMSRootRepository) !=
                    errNoError ||
                    bCMS_0000Path. append ("CMS_0000") != errNoError)
            {
                    Error err = ToolsErrors (__FILE__, __LINE__,
                            TOOLS_BUFFER_INIT_FAILED);
                    _ptSystemTracer -> trace (Tracer:: TRACER_LERRR, (const char *) err,
                            __FILE__, __LINE__);

                    return err;
            }

            #ifdef WIN32
                    if ((errCreateDir = FileIO:: createDirectory (
                            (const char *) bCMS_0000Path,
                            0, true, true)) != errNoError)
            #else
                    if ((errCreateDir = FileIO:: createDirectory (
                            (const char *) bCMS_0000Path,
                            S_IRUSR | S_IWUSR | S_IXUSR |
                            S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH, true, true)) !=
                            errNoError)
            #endif
            {
                    Error err = ToolsErrors (__FILE__, __LINE__,
                            TOOLS_FILEIO_CREATEDIRECTORY_FAILED,
                            1, (const char *) bCMS_0000Path);
                    _ptSystemTracer -> trace (Tracer:: TRACER_LERRR, (const char *) err,
                            __FILE__, __LINE__);

                    return err;
            }
    }

    if ((errGetItemValue = _pcfConfiguration -> getItemValue ("cmsEngine",
            "DownloadRootRepository", pConfigurationBuffer,
            CMSREP_CMSREPOSITORY_MAXCONFIGURATIONITEMLENGTH)) != errNoError)
    {
            _ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
                    (const char *) errGetItemValue,
                    __FILE__, __LINE__);

            Error err = ConfigurationErrors (__FILE__, __LINE__,
                    CFG_CONFIG_GETITEMVALUE_FAILED,
                    2, "cmsEngine", "DownloadRootRepository");
            _ptSystemTracer -> trace (Tracer:: TRACER_LERRR, (const char *) err,
                    __FILE__, __LINE__);

            return err;
    }

    if (_bDownloadRootRepository. init (pConfigurationBuffer) != errNoError)
    {
            Error err = ToolsErrors (__FILE__, __LINE__,
                    TOOLS_BUFFER_INIT_FAILED);
            _ptSystemTracer -> trace (Tracer:: TRACER_LERRR, (const char *) err,
                    __FILE__, __LINE__);

            return err;
    }

    #ifdef WIN32
            if (pConfigurationBuffer [strlen (pConfigurationBuffer) - 1] != '\\')
            {
                    if (_bDownloadRootRepository. append ("\\") != errNoError)
                    {
                            Error err = ToolsErrors (__FILE__, __LINE__,
                                    TOOLS_BUFFER_APPEND_FAILED);
                            _ptSystemTracer -> trace (Tracer:: TRACER_LERRR, (const char *) err,
                                    __FILE__, __LINE__);

                            return err;
                    }
            }
    #else
            if (pConfigurationBuffer [strlen (pConfigurationBuffer) - 1] != '/')
            {
                    if (_bDownloadRootRepository. append ("/") != errNoError)
                    {
                            Error err = ToolsErrors (__FILE__, __LINE__,
                                    TOOLS_BUFFER_APPEND_FAILED);
                            _ptSystemTracer -> trace (Tracer:: TRACER_LERRR, (const char *) err,
                                    __FILE__, __LINE__);

                            return err;
                    }
            }
    #endif

    #ifdef WIN32
            if ((errCreateDir = FileIO:: createDirectory (
                    (const char *) _bDownloadRootRepository,
                    0, true, true)) != errNoError)
    #else
            if ((errCreateDir = FileIO:: createDirectory (
                    (const char *) _bDownloadRootRepository,
                    S_IRUSR | S_IWUSR | S_IXUSR |
                    S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH, true, true)) !=
                    errNoError)
    #endif
    {
            Error err = ToolsErrors (__FILE__, __LINE__,
                    TOOLS_FILEIO_CREATEDIRECTORY_FAILED,
                    1, (const char *) _bDownloadRootRepository);
            _ptSystemTracer -> trace (Tracer:: TRACER_LERRR, (const char *) err,
                    __FILE__, __LINE__);

            return err;
    }

    if ((errGetItemValue = _pcfConfiguration -> getItemValue ("cmsEngine",
            "StreamingRootRepository", pConfigurationBuffer,
            CMSREP_CMSREPOSITORY_MAXCONFIGURATIONITEMLENGTH)) != errNoError)
    {
            _ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
                    (const char *) errGetItemValue,
                    __FILE__, __LINE__);

            Error err = ConfigurationErrors (__FILE__, __LINE__,
                    CFG_CONFIG_GETITEMVALUE_FAILED,
                    2, "cmsEngine", "StreamingRootRepository");
            _ptSystemTracer -> trace (Tracer:: TRACER_LERRR, (const char *) err,
                    __FILE__, __LINE__);

            return err;
    }

    if (_bStreamingRootRepository. init (pConfigurationBuffer) != errNoError)
    {
            Error err = ToolsErrors (__FILE__, __LINE__,
                    TOOLS_BUFFER_INIT_FAILED);
            _ptSystemTracer -> trace (Tracer:: TRACER_LERRR, (const char *) err,
                    __FILE__, __LINE__);

            return err;
    }

    #ifdef WIN32
            if (pConfigurationBuffer [strlen (pConfigurationBuffer) - 1] != '\\')
            {
                    if (_bStreamingRootRepository. append ("\\") != errNoError)
                    {
                            Error err = ToolsErrors (__FILE__, __LINE__,
                                    TOOLS_BUFFER_APPEND_FAILED);
                            _ptSystemTracer -> trace (Tracer:: TRACER_LERRR, (const char *) err,
                                    __FILE__, __LINE__);

                            if (_bStreamingRootRepository. finish () != errNoError)
                            {
                                    Error err = ToolsErrors (__FILE__, __LINE__,
                                            TOOLS_BUFFER_FINISH_FAILED);
                                    _ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
                                            (const char *) err, __FILE__, __LINE__);
                            }

                            return err;
                    }
            }
    #else
            if (pConfigurationBuffer [strlen (pConfigurationBuffer) - 1] != '/')
            {
                    if (_bStreamingRootRepository. append ("/") != errNoError)
                    {
                            Error err = ToolsErrors (__FILE__, __LINE__,
                                    TOOLS_BUFFER_APPEND_FAILED);
                            _ptSystemTracer -> trace (Tracer:: TRACER_LERRR, (const char *) err,
                                    __FILE__, __LINE__);

                            if (_bStreamingRootRepository. finish () != errNoError)
                            {
                                    Error err = ToolsErrors (__FILE__, __LINE__,
                                            TOOLS_BUFFER_FINISH_FAILED);
                                    _ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
                                            (const char *) err, __FILE__, __LINE__);
                            }

                            return err;
                    }
            }
    #endif

    #ifdef WIN32
            if ((errCreateDir = FileIO:: createDirectory (
                    (const char *) _bStreamingRootRepository,
                    0, true, true)) != errNoError)
    #else
            if ((errCreateDir = FileIO:: createDirectory (
                    (const char *) _bStreamingRootRepository,
                    S_IRUSR | S_IWUSR | S_IXUSR |
                    S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH, true, true)) !=
                    errNoError)
    #endif
    {
            Error err = ToolsErrors (__FILE__, __LINE__,
                    TOOLS_FILEIO_CREATEDIRECTORY_FAILED,
                    1, (const char *) _bStreamingRootRepository);
            _ptSystemTracer -> trace (Tracer:: TRACER_LERRR, (const char *) err,
                    __FILE__, __LINE__);

            return err;
    }

    if ((errGetItemValue = _pcfConfiguration -> getItemValue ("cmsEngine",
            "ErrorRootRepository", pConfigurationBuffer,
            CMSREP_CMSREPOSITORY_MAXCONFIGURATIONITEMLENGTH)) != errNoError)
    {
            _ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
                    (const char *) errGetItemValue,
                    __FILE__, __LINE__);

            Error err = ConfigurationErrors (__FILE__, __LINE__,
                    CFG_CONFIG_GETITEMVALUE_FAILED,
                    2, "cmsEngine", "ErrorRootRepository");
            _ptSystemTracer -> trace (Tracer:: TRACER_LERRR, (const char *) err,
                    __FILE__, __LINE__);

            return err;
    }

    if (_bErrorRootRepository. init (pConfigurationBuffer) != errNoError)
    {
            Error err = ToolsErrors (__FILE__, __LINE__,
                    TOOLS_BUFFER_INIT_FAILED);
            _ptSystemTracer -> trace (Tracer:: TRACER_LERRR, (const char *) err,
                    __FILE__, __LINE__);

            return err;
    }

    #ifdef WIN32
            if (pConfigurationBuffer [strlen (pConfigurationBuffer) - 1] != '\\')
            {
                    if (_bErrorRootRepository. append ("\\") != errNoError)
                    {
                            Error err = ToolsErrors (__FILE__, __LINE__,
                                    TOOLS_BUFFER_APPEND_FAILED);
                            _ptSystemTracer -> trace (Tracer:: TRACER_LERRR, (const char *) err,
                                    __FILE__, __LINE__);

                            return err;
                    }
            }
    #else
            if (pConfigurationBuffer [strlen (pConfigurationBuffer) - 1] != '/')
            {
                    if (_bErrorRootRepository. append ("/") != errNoError)
                    {
                            Error err = ToolsErrors (__FILE__, __LINE__,
                                    TOOLS_BUFFER_APPEND_FAILED);
                            _ptSystemTracer -> trace (Tracer:: TRACER_LERRR, (const char *) err,
                                    __FILE__, __LINE__);

                            return err;
                    }
            }
    #endif

    #ifdef WIN32
            if ((errCreateDir = FileIO:: createDirectory (
                    (const char *) _bErrorRootRepository,
                    0, true, true)) != errNoError)
    #else
            if ((errCreateDir = FileIO:: createDirectory (
                    (const char *) _bErrorRootRepository,
                    S_IRUSR | S_IWUSR | S_IXUSR |
                    S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH, true, true)) !=
                    errNoError)
    #endif
    {
            Error err = ToolsErrors (__FILE__, __LINE__,
                    TOOLS_FILEIO_CREATEDIRECTORY_FAILED,
                    1, (const char *) _bErrorRootRepository);
            _ptSystemTracer -> trace (Tracer:: TRACER_LERRR, (const char *) err,
                    __FILE__, __LINE__);

            return err;
    }

    if ((errGetItemValue = _pcfConfiguration -> getItemValue ("cmsEngine",
            "DoneRootRepository", pConfigurationBuffer,
            CMSREP_CMSREPOSITORY_MAXCONFIGURATIONITEMLENGTH)) != errNoError)
    {
            _ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
                    (const char *) errGetItemValue,
                    __FILE__, __LINE__);

            Error err = ConfigurationErrors (__FILE__, __LINE__,
                    CFG_CONFIG_GETITEMVALUE_FAILED,
                    2, "cmsEngine", "DoneRootRepository");
            _ptSystemTracer -> trace (Tracer:: TRACER_LERRR, (const char *) err,
                    __FILE__, __LINE__);

            return err;
    }

    if (_bDoneRootRepository. init (pConfigurationBuffer) != errNoError)
    {
            Error err = ToolsErrors (__FILE__, __LINE__,
                    TOOLS_BUFFER_INIT_FAILED);
            _ptSystemTracer -> trace (Tracer:: TRACER_LERRR, (const char *) err,
                    __FILE__, __LINE__);

            return err;
    }

    #ifdef WIN32
            if (pConfigurationBuffer [strlen (pConfigurationBuffer) - 1] != '\\')
            {
                    if (_bDoneRootRepository. append ("\\") != errNoError)
                    {
                            Error err = ToolsErrors (__FILE__, __LINE__,
                                    TOOLS_BUFFER_APPEND_FAILED);
                            _ptSystemTracer -> trace (Tracer:: TRACER_LERRR, (const char *) err,
                                    __FILE__, __LINE__);

                            return err;
                    }
            }
    #else
            if (pConfigurationBuffer [strlen (pConfigurationBuffer) - 1] != '/')
            {
                    if (_bDoneRootRepository. append ("/") != errNoError)
                    {
                            Error err = ToolsErrors (__FILE__, __LINE__,
                                    TOOLS_BUFFER_APPEND_FAILED);
                            _ptSystemTracer -> trace (Tracer:: TRACER_LERRR, (const char *) err,
                                    __FILE__, __LINE__);

                            return err;
                    }
            }
    #endif

    #ifdef WIN32
            if ((errCreateDir = FileIO:: createDirectory (
                    (const char *) _bDoneRootRepository,
                    0, true, true)) != errNoError)
    #else
            if ((errCreateDir = FileIO:: createDirectory (
                    (const char *) _bDoneRootRepository,
                    S_IRUSR | S_IWUSR | S_IXUSR |
                    S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH, true, true)) !=
                    errNoError)
    #endif
    {
            Error err = ToolsErrors (__FILE__, __LINE__,
                    TOOLS_FILEIO_CREATEDIRECTORY_FAILED,
                    1, (const char *) _bDoneRootRepository);
            _ptSystemTracer -> trace (Tracer:: TRACER_LERRR, (const char *) err,
                    __FILE__, __LINE__);

            return err;
    }

    if ((errGetItemValue = _pcfConfiguration -> getItemValue ("cmsEngine",
            "ProfilesRootRepository", pConfigurationBuffer,
            CMSREP_CMSREPOSITORY_MAXCONFIGURATIONITEMLENGTH)) != errNoError)
    {
            _ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
                    (const char *) errGetItemValue,
                    __FILE__, __LINE__);

            Error err = ConfigurationErrors (__FILE__, __LINE__,
                    CFG_CONFIG_GETITEMVALUE_FAILED,
                    2, "cmsEngine", "ProfilesRootRepository");
            _ptSystemTracer -> trace (Tracer:: TRACER_LERRR, (const char *) err,
                    __FILE__, __LINE__);

            return err;
    }

    if (_bProfilesRootRepository. init (pConfigurationBuffer) !=
            errNoError)
    {
            Error err = ToolsErrors (__FILE__, __LINE__,
                    TOOLS_BUFFER_INIT_FAILED);
            _ptSystemTracer -> trace (Tracer:: TRACER_LERRR, (const char *) err,
                    __FILE__, __LINE__);

            return err;
    }

    #ifdef WIN32
            if (pConfigurationBuffer [strlen (pConfigurationBuffer) - 1] != '\\')
            {
                    if (_bProfilesRootRepository. append ("\\") != errNoError)
                    {
                            Error err = ToolsErrors (__FILE__, __LINE__,
                                    TOOLS_BUFFER_APPEND_FAILED);
                            _ptSystemTracer -> trace (Tracer:: TRACER_LERRR, (const char *) err,
                                    __FILE__, __LINE__);

                            return err;
                    }
            }
    #else
            if (pConfigurationBuffer [strlen (pConfigurationBuffer) - 1] != '/')
            {
                    if (_bProfilesRootRepository. append ("/") != errNoError)
                    {
                            Error err = ToolsErrors (__FILE__, __LINE__,
                                    TOOLS_BUFFER_APPEND_FAILED);
                            _ptSystemTracer -> trace (Tracer:: TRACER_LERRR, (const char *) err,
                                    __FILE__, __LINE__);

                            return err;
                    }
            }
    #endif

    #ifdef WIN32
            if ((errCreateDir = FileIO:: createDirectory (
                    (const char *) _bProfilesRootRepository,
                    0, true, true)) != errNoError)
    #else
            if ((errCreateDir = FileIO:: createDirectory (
                    (const char *) _bProfilesRootRepository,
                    S_IRUSR | S_IWUSR | S_IXUSR |
                    S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH, true, true)) !=
                    errNoError)
    #endif
    {
            Error err = ToolsErrors (__FILE__, __LINE__,
                    TOOLS_FILEIO_CREATEDIRECTORY_FAILED,
                    1, (const char *) _bProfilesRootRepository);
            _ptSystemTracer -> trace (Tracer:: TRACER_LERRR, (const char *) err,
                    __FILE__, __LINE__);

            return err;
    }

    if ((errGetItemValue = _pcfConfiguration -> getItemValue ("XOEAgent",
            "ProfilesRootDirectoryFromXOEMachine", pConfigurationBuffer,
            CMSREP_CMSREPOSITORY_MAXCONFIGURATIONITEMLENGTH)) != errNoError)
    {
            _ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
                    (const char *) errGetItemValue,
                    __FILE__, __LINE__);

            Error err = ConfigurationErrors (__FILE__, __LINE__,
                    CFG_CONFIG_GETITEMVALUE_FAILED,
                    2, "XOEAgent", "ProfilesRootDirectoryFromXOEMachine");
            _ptSystemTracer -> trace (Tracer:: TRACER_LERRR, (const char *) err,
                    __FILE__, __LINE__);

            return err;
    }

    if (_bProfilesRootDirectoryFromXOEMachine. init (pConfigurationBuffer) !=
            errNoError)
    {
            Error err = ToolsErrors (__FILE__, __LINE__,
                    TOOLS_BUFFER_INIT_FAILED);
            _ptSystemTracer -> trace (Tracer:: TRACER_LERRR, (const char *) err,
                    __FILE__, __LINE__);

            return err;
    }

    if (pConfigurationBuffer [strlen (pConfigurationBuffer) - 1] != '\\')
    {
            if (_bProfilesRootDirectoryFromXOEMachine. append ("\\") != errNoError)
            {
                    Error err = ToolsErrors (__FILE__, __LINE__,
                            TOOLS_BUFFER_APPEND_FAILED);
                    _ptSystemTracer -> trace (Tracer:: TRACER_LERRR, (const char *) err,
                            __FILE__, __LINE__);

                    return err;
            }
    }

    if ((errGetItemValue = _pcfConfiguration -> getItemValue ("cmsEngine",
            "StagingRootRepository", pConfigurationBuffer,
            CMSREP_CMSREPOSITORY_MAXCONFIGURATIONITEMLENGTH)) != errNoError)
    {
            _ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
                    (const char *) errGetItemValue,
                    __FILE__, __LINE__);

            Error err = ConfigurationErrors (__FILE__, __LINE__,
                    CFG_CONFIG_GETITEMVALUE_FAILED,
                    2, "cmsEngine", "StagingRootRepository");
            _ptSystemTracer -> trace (Tracer:: TRACER_LERRR, (const char *) err,
                    __FILE__, __LINE__);

            return err;
    }

    if (_bStagingRootRepository. init (pConfigurationBuffer) != errNoError)
    {
            Error err = ToolsErrors (__FILE__, __LINE__,
                    TOOLS_BUFFER_INIT_FAILED);
            _ptSystemTracer -> trace (Tracer:: TRACER_LERRR, (const char *) err,
                    __FILE__, __LINE__);

            return err;
    }

    #ifdef WIN32
            if (pConfigurationBuffer [strlen (pConfigurationBuffer) - 1] != '\\')
            {
                    if (_bStagingRootRepository. append ("\\") != errNoError)
                    {
                            Error err = ToolsErrors (__FILE__, __LINE__,
                                    TOOLS_BUFFER_APPEND_FAILED);
                            _ptSystemTracer -> trace (Tracer:: TRACER_LERRR, (const char *) err,
                                    __FILE__, __LINE__);

                            return err;
                    }
            }
    #else
            if (pConfigurationBuffer [strlen (pConfigurationBuffer) - 1] != '/')
            {
                    if (_bStagingRootRepository. append ("/") != errNoError)
                    {
                            Error err = ToolsErrors (__FILE__, __LINE__,
                                    TOOLS_BUFFER_APPEND_FAILED);
                            _ptSystemTracer -> trace (Tracer:: TRACER_LERRR, (const char *) err,
                                    __FILE__, __LINE__);

                            return err;
                    }
            }
    #endif

    #ifdef WIN32
            if ((errCreateDir = FileIO:: createDirectory (
                    (const char *) _bStagingRootRepository,
                    0, true, true)) != errNoError)
    #else
            if ((errCreateDir = FileIO:: createDirectory (
                    (const char *) _bStagingRootRepository,
                    S_IRUSR | S_IWUSR | S_IXUSR |
                    S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH, true, true)) !=
                    errNoError)
    #endif
    {
            Error err = ToolsErrors (__FILE__, __LINE__,
                    TOOLS_FILEIO_CREATEDIRECTORY_FAILED,
                    1, (const char *) _bStagingRootRepository);
            _ptSystemTracer -> trace (Tracer:: TRACER_LERRR, (const char *) err,
                    __FILE__, __LINE__);

            return err;
    }

    if ((errGetItemValue = _pcfConfiguration -> getItemValue ("XOEAgent",
            "StagingRootRepositoryFromXOEMachine", pConfigurationBuffer,
            CMSREP_CMSREPOSITORY_MAXCONFIGURATIONITEMLENGTH)) != errNoError)
    {
            _ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
                    (const char *) errGetItemValue,
                    __FILE__, __LINE__);

            Error err = ConfigurationErrors (__FILE__, __LINE__,
                    CFG_CONFIG_GETITEMVALUE_FAILED,
                    2, "XOEAgent", "StagingRootRepositoryFromXOEMachine");
            _ptSystemTracer -> trace (Tracer:: TRACER_LERRR, (const char *) err,
                    __FILE__, __LINE__);

            return err;
    }

    if (_bStagingRootRepositoryFromXOEMachine. init (pConfigurationBuffer) !=
            errNoError)
    {
            Error err = ToolsErrors (__FILE__, __LINE__,
                    TOOLS_BUFFER_INIT_FAILED);
            _ptSystemTracer -> trace (Tracer:: TRACER_LERRR, (const char *) err,
                    __FILE__, __LINE__);

            return err;
    }

    if (pConfigurationBuffer [strlen (pConfigurationBuffer) - 1] != '\\')
    {
            if (_bStagingRootRepositoryFromXOEMachine. append ("\\") != errNoError)
            {
                    Error err = ToolsErrors (__FILE__, __LINE__,
                            TOOLS_BUFFER_APPEND_FAILED);
                    _ptSystemTracer -> trace (Tracer:: TRACER_LERRR, (const char *) err,
                            __FILE__, __LINE__);

                    return err;
            }
    }

    // Partitions staff
    {
            Boolean_t				bCMSAvailablePartitions;
            Buffer_t				bPathNameToGetFileSystemInfo;
            char					pCMSPartitionName [
                    CMSREP_CMSREPOSITORY_MAXCMSPARTITIONNAMELENGTH];
            unsigned long long		ullUsedInKB;
            unsigned long long		ullAvailableInKB;
            long					lPercentUsed;
            Error_t					errFileIO;


            if (_mtCMSPartitions. init (
                    PMutex:: MUTEX_RECURSIVE) != errNoError)
            {
                    Error err = PThreadErrors (__FILE__, __LINE__,
                            THREADLIB_PMUTEX_INIT_FAILED);
                    ptTracer -> trace (Tracer:: TRACER_LERRR,
                            (const char *) err, __FILE__, __LINE__);

                    {
                            Error err = PThreadErrors (__FILE__, __LINE__,
                                    THREADLIB_PMUTEX_INIT_FAILED);
                            _ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
                                    (const char *) err, __FILE__, __LINE__);
                    }

                    return err;
            }

            if (bPathNameToGetFileSystemInfo. init () != errNoError)
            {
                    Error err = ToolsErrors (__FILE__, __LINE__,
                            TOOLS_BUFFER_INIT_FAILED);
                    ptTracer -> trace (Tracer:: TRACER_LERRR,
                            (const char *) err, __FILE__, __LINE__);

                    if (_mtCMSPartitions. finish () != errNoError)
                    {
                            Error err = PThreadErrors (__FILE__, __LINE__,
                                    THREADLIB_PMUTEX_FINISH_FAILED);
                            _ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
                                    (const char *) err, __FILE__, __LINE__);
                    }

                    return err;
            }

            _ulCMSPartitionsNumber			= 0;
            _ulCurrentCMSPartitionIndex		= 0;
            bCMSAvailablePartitions			= true;

            // inizializzare FreeSize
            while (bCMSAvailablePartitions)
            {
                    if (bPathNameToGetFileSystemInfo. setBuffer (
                            (const char *) _bCMSRootRepository) != errNoError)
                    {
                            Error err = ToolsErrors (__FILE__, __LINE__,
                                    TOOLS_BUFFER_SETBUFFER_FAILED);
                            ptTracer -> trace (Tracer:: TRACER_LERRR,
                                    (const char *) err, __FILE__, __LINE__);

                            if (_mtCMSPartitions. finish () != errNoError)
                            {
                                    Error err = PThreadErrors (__FILE__, __LINE__,
                                            THREADLIB_PMUTEX_FINISH_FAILED);
                                    _ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
                                            (const char *) err, __FILE__, __LINE__);
                            }

                            return err;
                    }

                    sprintf (pCMSPartitionName, "CMS_%04lu", _ulCMSPartitionsNumber);

                    if (bPathNameToGetFileSystemInfo. append (pCMSPartitionName) !=
                            errNoError)
                    {
                            Error err = ToolsErrors (__FILE__, __LINE__,
                                    TOOLS_BUFFER_APPEND_FAILED);
                            ptTracer -> trace (Tracer:: TRACER_LERRR,
                                    (const char *) err, __FILE__, __LINE__);

                            if (_mtCMSPartitions. finish () != errNoError)
                            {
                                    Error err = PThreadErrors (__FILE__, __LINE__,
                                            THREADLIB_PMUTEX_FINISH_FAILED);
                                    _ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
                                            (const char *) err, __FILE__, __LINE__);
                            }

                            return err;
                    }

                    if ((errFileIO = FileIO:: getFileSystemInfo (
                            (const char *) bPathNameToGetFileSystemInfo,
                            &ullUsedInKB, &ullAvailableInKB, &lPercentUsed)) != errNoError)
                    {

                            break;
                    }

                    _ulCMSPartitionsNumber++;
            }

            if (_ulCMSPartitionsNumber == 0)
            {
                    Error err = CMSRepositoryErrors (__FILE__, __LINE__,
                            CMSREP_CMSREPOSITORY_NOCMSPARTITIONFOUND);
                    ptTracer -> trace (Tracer:: TRACER_LERRR,
                            (const char *) err, __FILE__, __LINE__);

                    if (_mtCMSPartitions. finish () != errNoError)
                    {
                            Error err = PThreadErrors (__FILE__, __LINE__,
                                    THREADLIB_PMUTEX_FINISH_FAILED);
                            _ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
                                    (const char *) err, __FILE__, __LINE__);
                    }

                    return err;
            }

            if ((_pullCMSPartitionsFreeSizeInMB = new unsigned long long [
                    _ulCMSPartitionsNumber]) == (unsigned long long *) NULL)
            {
                    Error err = CMSRepositoryErrors (__FILE__, __LINE__,
                            CMSREP_NEW_FAILED);
                    ptTracer -> trace (Tracer:: TRACER_LERRR,
                            (const char *) err, __FILE__, __LINE__);

                    if (_mtCMSPartitions. finish () != errNoError)
                    {
                            Error err = PThreadErrors (__FILE__, __LINE__,
                                    THREADLIB_PMUTEX_FINISH_FAILED);
                            _ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
                                    (const char *) err, __FILE__, __LINE__);
                    }

                    return err;
            }

            if (refreshPartitionsFreeSizes () != errNoError)
            {
                    Error err = CMSRepositoryErrors (__FILE__, __LINE__,
                            CMSREP_CMSREPOSITORY_REFRESHPARTITIONSFREESIZES_FAILED);
                    ptTracer -> trace (Tracer:: TRACER_LERRR,
                            (const char *) err, __FILE__, __LINE__);

                    delete [] _pullCMSPartitionsFreeSizeInMB;

                    if (_mtCMSPartitions. finish () != errNoError)
                    {
                            Error err = PThreadErrors (__FILE__, __LINE__,
                                    THREADLIB_PMUTEX_FINISH_FAILED);
                            _ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
                                    (const char *) err, __FILE__, __LINE__);
                    }

                    return err;
            }
    }


    return errNoError;
}


Error CMSRepository:: finish ()

{

	delete [] _pullCMSPartitionsFreeSizeInMB;

	if (_mtCMSPartitions. finish () != errNoError)
	{
		Error err = PThreadErrors (__FILE__, __LINE__,
			THREADLIB_PMUTEX_FINISH_FAILED);
		_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
			(const char *) err, __FILE__, __LINE__);
	}


	return errNoError;
}


const char *CMSRepository:: getIPhoneAliasForLive (void)

{

	return _pIPhoneAliasForLive;
}


const char *CMSRepository:: getCMSRootRepository (void)

{

	return ((const char *) _bCMSRootRepository);
}


const char *CMSRepository:: getStreamingRootRepository (void)

{

	return ((const char *) _bStreamingRootRepository);
}


const char *CMSRepository:: getDownloadRootRepository (void)

{

	return ((const char *) _bDownloadRootRepository);
}


const char *CMSRepository:: getFTPRootRepository (void)

{

	return ((const char *) _bFTPRootRepository);
}


const char *CMSRepository:: getStagingRootRepository (void)

{

	return ((const char *) _bStagingRootRepository);
}


const char *CMSRepository:: getErrorRootRepository (void)

{

	return ((const char *) _bErrorRootRepository);
}


const char *CMSRepository:: getDoneRootRepository (void)

{

	return ((const char *) _bDoneRootRepository);
}


Error CMSRepository:: refreshPartitionsFreeSizes (void)

{
	unsigned long			ulCMSPartitionIndex;
	Buffer_t				bPathNameToGetFileSystemInfo;
	char					pCMSPartitionName [
		CMSREP_CMSREPOSITORY_MAXCMSPARTITIONNAMELENGTH];
	Error_t					errFileIO;
	unsigned long long		ullUsedInKB;
	unsigned long long		ullAvailableInKB;
	long					lPercentUsed;


	if (bPathNameToGetFileSystemInfo. init () != errNoError)
	{
		Error err = ToolsErrors (__FILE__, __LINE__,
			TOOLS_BUFFER_INIT_FAILED);
		_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
			(const char *) err, __FILE__, __LINE__);

		return err;
	}

	if (_mtCMSPartitions. lock () != errNoError)
	{
		Error err = PThreadErrors (__FILE__, __LINE__,
			THREADLIB_PMUTEX_LOCK_FAILED);
		_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
			(const char *) err, __FILE__, __LINE__);

		if (bPathNameToGetFileSystemInfo. finish () != errNoError)
		{
			Error err = ToolsErrors (__FILE__, __LINE__,
				TOOLS_BUFFER_FINISH_FAILED);
			_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
				(const char *) err, __FILE__, __LINE__);
		}

		return err;
	}

	for (ulCMSPartitionIndex = 0;
		ulCMSPartitionIndex < _ulCMSPartitionsNumber;
		ulCMSPartitionIndex++)
	{
		if (bPathNameToGetFileSystemInfo. setBuffer (
			(const char *) _bCMSRootRepository) != errNoError)
		{
			Error err = ToolsErrors (__FILE__, __LINE__,
				TOOLS_BUFFER_SETBUFFER_FAILED);
			_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
				(const char *) err, __FILE__, __LINE__);

			if (_mtCMSPartitions. unLock () != errNoError)
			{
				Error err = PThreadErrors (__FILE__, __LINE__,
					THREADLIB_PMUTEX_UNLOCK_FAILED);
				_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
					(const char *) err, __FILE__, __LINE__);
			}

			if (bPathNameToGetFileSystemInfo. finish () != errNoError)
			{
				Error err = ToolsErrors (__FILE__, __LINE__,
					TOOLS_BUFFER_FINISH_FAILED);
				_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
					(const char *) err, __FILE__, __LINE__);
			}

			return err;
		}

		sprintf (pCMSPartitionName, "CMS_%04lu", ulCMSPartitionIndex);

		if (bPathNameToGetFileSystemInfo. append (pCMSPartitionName) !=
			errNoError)
		{
			Error err = ToolsErrors (__FILE__, __LINE__,
				TOOLS_BUFFER_APPEND_FAILED);
			_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
				(const char *) err, __FILE__, __LINE__);

			if (_mtCMSPartitions. unLock () != errNoError)
			{
				Error err = PThreadErrors (__FILE__, __LINE__,
					THREADLIB_PMUTEX_UNLOCK_FAILED);
				_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
					(const char *) err, __FILE__, __LINE__);
			}

			if (bPathNameToGetFileSystemInfo. finish () != errNoError)
			{
				Error err = ToolsErrors (__FILE__, __LINE__,
					TOOLS_BUFFER_FINISH_FAILED);
				_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
					(const char *) err, __FILE__, __LINE__);
			}

			return err;
		}

		if ((errFileIO = FileIO:: getFileSystemInfo (
			(const char *) bPathNameToGetFileSystemInfo,
			&ullUsedInKB, &ullAvailableInKB, &lPercentUsed)) !=
			errNoError)
		{
			_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
				(const char *) errFileIO, __FILE__, __LINE__);

			Error err = ToolsErrors (__FILE__, __LINE__,
				TOOLS_FILEIO_GETFILESYSTEMINFO_FAILED);
			_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
				(const char *) err, __FILE__, __LINE__);

			if (_mtCMSPartitions. unLock () != errNoError)
			{
				Error err = PThreadErrors (__FILE__, __LINE__,
					THREADLIB_PMUTEX_UNLOCK_FAILED);
				_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
					(const char *) err, __FILE__, __LINE__);
			}

			if (bPathNameToGetFileSystemInfo. finish () != errNoError)
			{
				Error err = ToolsErrors (__FILE__, __LINE__,
					TOOLS_BUFFER_FINISH_FAILED);
				_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
					(const char *) err, __FILE__, __LINE__);
			}

			return err;
		}

		_pullCMSPartitionsFreeSizeInMB [ulCMSPartitionIndex]		=
			ullAvailableInKB / 1024;

		{
			Message msg = CMSRepositoryMessages (
				__FILE__, __LINE__, 
				CMSREP_CMSREPOSITORY_AVAILABLESPACE,
				2, 
				(const char *) bPathNameToGetFileSystemInfo,
				_pullCMSPartitionsFreeSizeInMB [ulCMSPartitionIndex]);
			_ptSystemTracer -> trace (Tracer:: TRACER_LINFO,
				(const char *) msg, __FILE__, __LINE__);
		}
	}

	if (_mtCMSPartitions. unLock () != errNoError)
	{
		Error err = PThreadErrors (__FILE__, __LINE__,
			THREADLIB_PMUTEX_UNLOCK_FAILED);
		_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
			(const char *) err, __FILE__, __LINE__);

		if (bPathNameToGetFileSystemInfo. finish () != errNoError)
		{
			Error err = ToolsErrors (__FILE__, __LINE__,
				TOOLS_BUFFER_FINISH_FAILED);
			_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
				(const char *) err, __FILE__, __LINE__);
		}

		return err;
	}

	if (bPathNameToGetFileSystemInfo. finish () != errNoError)
	{
		Error err = ToolsErrors (__FILE__, __LINE__,
			TOOLS_BUFFER_FINISH_FAILED);
		_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
			(const char *) err, __FILE__, __LINE__);

		return err;
	}


	return errNoError;
}


Error CMSRepository:: creatingDirsUsingTerritories (
	unsigned long ulCurrentCMSPartitionIndex,
	const char *pRelativePath,
	const char *pCustomerDirectoryName,
	Boolean_t bDeliveryRepositoriesToo,
	TerritoriesHashMap_p phmTerritories,
	Buffer_p pbCMSAssetPathName)

{

	Buffer_t					bDownloadAssetPathName;
	Buffer_t					bStreamingAssetPathName;
	Boolean_t					bIsDirectoryExisting;
	char						pCMSPartitionName [
		CMSREP_CMSREPOSITORY_MAXCMSPARTITIONNAMELENGTH];
	Error_t						errFileIO;


	#ifdef WIN32
		if (pRelativePath == (const char *) NULL ||
			pRelativePath [0] != '\\' ||
			pCustomerDirectoryName == (const char *) NULL ||
			pbCMSAssetPathName == (Buffer_p) NULL)
	#else
		if (pRelativePath == (const char *) NULL ||
			pRelativePath [0] != '/' ||
			pCustomerDirectoryName == (const char *) NULL ||
			pbCMSAssetPathName == (Buffer_p) NULL)
	#endif
	{
		Error err = CMSRepositoryErrors (__FILE__, __LINE__,
			CMSREP_ACTIVATION_WRONG);
		_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
			(const char *) err, __FILE__, __LINE__);

		return err;
	}

	if (bDownloadAssetPathName. init () != errNoError)
	{
		Error err = ToolsErrors (__FILE__, __LINE__,
			TOOLS_BUFFER_INIT_FAILED);
		_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
			(const char *) err, __FILE__, __LINE__);

		return err;
	}

	if (bStreamingAssetPathName. init () != errNoError)
	{
		Error err = ToolsErrors (__FILE__, __LINE__,
			TOOLS_BUFFER_INIT_FAILED);
		_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
			(const char *) err, __FILE__, __LINE__);

		if (bDownloadAssetPathName. finish () != errNoError)
		{
			Error err = ToolsErrors (__FILE__, __LINE__,
				TOOLS_BUFFER_FINISH_FAILED);
			_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
				(const char *) err, __FILE__, __LINE__);
		}

		return err;
	}

	#ifdef WIN32
		sprintf (pCMSPartitionName, "CMS_%04lu\\", ulCurrentCMSPartitionIndex);
	#else
		sprintf (pCMSPartitionName, "CMS_%04lu/", ulCurrentCMSPartitionIndex);
	#endif

	#ifdef WIN32
		if (pbCMSAssetPathName -> setBuffer (
				(const char *) _bCMSRootRepository) != errNoError ||
			pbCMSAssetPathName -> append (
				pCMSPartitionName) != errNoError ||
			pbCMSAssetPathName -> append (
				pCustomerDirectoryName) != errNoError ||
			pbCMSAssetPathName -> append (
				pRelativePath) != errNoError)
	#else
		if (pbCMSAssetPathName -> setBuffer (
				(const char *) _bCMSRootRepository) != errNoError ||
			pbCMSAssetPathName -> append (
				pCMSPartitionName) != errNoError ||
			pbCMSAssetPathName -> append (
				pCustomerDirectoryName) != errNoError ||
			pbCMSAssetPathName -> append (
				pRelativePath) != errNoError)
	#endif
	{
		Error err = ToolsErrors (__FILE__, __LINE__,
			TOOLS_BUFFER_SETBUFFER_FAILED);
		_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
			(const char *) err, __FILE__, __LINE__);

		if (bStreamingAssetPathName. finish () != errNoError)
		{
			Error err = ToolsErrors (__FILE__, __LINE__,
				TOOLS_BUFFER_FINISH_FAILED);
			_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
				(const char *) err, __FILE__, __LINE__);
		}

		if (bDownloadAssetPathName. finish () != errNoError)
		{
			Error err = ToolsErrors (__FILE__, __LINE__,
				TOOLS_BUFFER_FINISH_FAILED);
			_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
				(const char *) err, __FILE__, __LINE__);
		}

		return err;
	}

	if (FileIO:: isDirectoryExisting (
		(const char *) (*pbCMSAssetPathName),
		&bIsDirectoryExisting) != errNoError)
	{
		Error err = ToolsErrors (__FILE__, __LINE__,
			TOOLS_FILEIO_ISDIRECTORYEXISTING_FAILED);
		_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
			(const char *) err, __FILE__, __LINE__);

		if (bStreamingAssetPathName. finish () != errNoError)
		{
			Error err = ToolsErrors (__FILE__, __LINE__,
				TOOLS_BUFFER_FINISH_FAILED);
			_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
				(const char *) err, __FILE__, __LINE__);
		}

		if (bDownloadAssetPathName. finish () != errNoError)
		{
			Error err = ToolsErrors (__FILE__, __LINE__,
				TOOLS_BUFFER_FINISH_FAILED);
			_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
				(const char *) err, __FILE__, __LINE__);
		}

		return err;
	}

	if (!bIsDirectoryExisting)
	{
		{
			Message msg = CMSRepositoryMessages (
				__FILE__, __LINE__,
				CMSREP_CMSREPOSITORY_CREATEDIRECTORY,
				2,
				pCustomerDirectoryName,
				(const char *) (*pbCMSAssetPathName));
			_ptSystemTracer -> trace (Tracer:: TRACER_LINFO,
				(const char *) msg, __FILE__, __LINE__);
		}

		#ifdef WIN32
			if ((errFileIO = FileIO:: createDirectory (
				(const char *) (*pbCMSAssetPathName),
				0, true, true)) != errNoError)
		#else
			if ((errFileIO = FileIO:: createDirectory (
				(const char *) (*pbCMSAssetPathName),
				S_IRUSR | S_IWUSR | S_IXUSR |
				S_IRGRP | S_IXGRP |
				S_IROTH | S_IXOTH, true, true)) !=
				errNoError)
		#endif
		{
			_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
				(const char *) errFileIO, __FILE__, __LINE__);

			Error err = ToolsErrors (__FILE__, __LINE__,
				TOOLS_FILEIO_CREATEDIRECTORY_FAILED,
				1, (const char *) (*pbCMSAssetPathName));
			_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
				(const char *) err, __FILE__, __LINE__);

			if (bStreamingAssetPathName. finish () != errNoError)
			{
				Error err = ToolsErrors (__FILE__, __LINE__,
					TOOLS_BUFFER_FINISH_FAILED);
				_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
					(const char *) err, __FILE__, __LINE__);
			}

			if (bDownloadAssetPathName. finish () != errNoError)
			{
				Error err = ToolsErrors (__FILE__, __LINE__,
					TOOLS_BUFFER_FINISH_FAILED);
				_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
					(const char *) err, __FILE__, __LINE__);
			}

			return err;
		}
	}

	// the pbCMSAssetPathName must finish with /
	#ifdef WIN32
		if (((const char *) (*pbCMSAssetPathName)) [
			(unsigned long) (*pbCMSAssetPathName) - 1] != '\\')
	#else
		if (((const char *) (*pbCMSAssetPathName)) [
			(unsigned long) (*pbCMSAssetPathName) - 1] != '/')
	#endif
	{
		#ifdef WIN32
			if (pbCMSAssetPathName -> append ("\\") != errNoError)
		#else
			if (pbCMSAssetPathName -> append ("/") != errNoError)
		#endif
		{
			Error err = ToolsErrors (__FILE__, __LINE__,
				TOOLS_BUFFER_APPEND_FAILED);
			_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
				(const char *) err, __FILE__, __LINE__);

			if (bStreamingAssetPathName. finish () != errNoError)
			{
				Error err = ToolsErrors (__FILE__, __LINE__,
					TOOLS_BUFFER_FINISH_FAILED);
				_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
					(const char *) err, __FILE__, __LINE__);
			}

			if (bDownloadAssetPathName. finish () != errNoError)
			{
				Error err = ToolsErrors (__FILE__, __LINE__,
					TOOLS_BUFFER_FINISH_FAILED);
				_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
					(const char *) err, __FILE__, __LINE__);
			}

			return err;
		}
	}

	if (bDeliveryRepositoriesToo)
	{
		TerritoriesHashMap_t:: iterator				it;
		Buffer_p									pbTerritoryName;


		for (it = phmTerritories -> begin ();
			it != phmTerritories -> end ();
			++it)
		{
			pbTerritoryName			= it -> second;

			#ifdef WIN32
				if (bDownloadAssetPathName. setBuffer (
						(const char *) _bDownloadRootRepository) !=
						errNoError ||
					bDownloadAssetPathName. append (
						pCMSPartitionName) != errNoError ||
					bDownloadAssetPathName. append (
						pCustomerDirectoryName) != errNoError ||
					bDownloadAssetPathName. append ("\\") != errNoError ||
					bDownloadAssetPathName. append ((const char *)
						(*pbTerritoryName)) != errNoError ||
					bDownloadAssetPathName. append (
						pRelativePath) != errNoError ||
					bStreamingAssetPathName. setBuffer (
						(const char *) _bStreamingRootRepository) !=
						errNoError ||
					bStreamingAssetPathName. append (
						pCMSPartitionName) != errNoError ||
					bStreamingAssetPathName. append (
						pCustomerDirectoryName) != errNoError ||
					bStreamingAssetPathName. append ("\\") != errNoError ||
					bStreamingAssetPathName. append ((const char *)
						(*pbTerritoryName)) != errNoError ||
					bStreamingAssetPathName. append (
						pRelativePath) != errNoError)
			#else
				if (bDownloadAssetPathName. setBuffer (
						(const char *) _bDownloadRootRepository) !=
						errNoError ||
					bDownloadAssetPathName. append (
						pCMSPartitionName) != errNoError ||
					bDownloadAssetPathName. append (
						pCustomerDirectoryName) != errNoError ||
					bDownloadAssetPathName. append ("/") != errNoError ||
					bDownloadAssetPathName. append ((const char *)
						(*pbTerritoryName)) != errNoError ||
					bDownloadAssetPathName. append (
						pRelativePath) != errNoError ||
					bStreamingAssetPathName. setBuffer (
						(const char *) _bStreamingRootRepository) !=
						errNoError ||
					bStreamingAssetPathName. append (
						pCMSPartitionName) != errNoError ||
					bStreamingAssetPathName. append (
						pCustomerDirectoryName) != errNoError ||
					bStreamingAssetPathName. append ("/") != errNoError ||
					bStreamingAssetPathName. append ((const char *)
						(*pbTerritoryName)) != errNoError ||
					bStreamingAssetPathName. append (
						pRelativePath) != errNoError)
			#endif
			{
				Error err = ToolsErrors (__FILE__, __LINE__,
					TOOLS_BUFFER_SETBUFFER_FAILED);
				_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
					(const char *) err, __FILE__, __LINE__);

				if (bStreamingAssetPathName. finish () != errNoError)
				{
					Error err = ToolsErrors (__FILE__, __LINE__,
						TOOLS_BUFFER_FINISH_FAILED);
					_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
						(const char *) err, __FILE__, __LINE__);
				}

				if (bDownloadAssetPathName. finish () != errNoError)
				{
					Error err = ToolsErrors (__FILE__, __LINE__,
						TOOLS_BUFFER_FINISH_FAILED);
					_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
						(const char *) err, __FILE__, __LINE__);
				}

				return err;
			}

			if (FileIO:: isDirectoryExisting (
				(const char *) bDownloadAssetPathName,
				&bIsDirectoryExisting) != errNoError)
			{
				Error err = ToolsErrors (__FILE__, __LINE__,
					TOOLS_FILEIO_ISDIRECTORYEXISTING_FAILED);
				_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
					(const char *) err, __FILE__, __LINE__);

				if (bStreamingAssetPathName. finish () != errNoError)
				{
					Error err = ToolsErrors (__FILE__, __LINE__,
						TOOLS_BUFFER_FINISH_FAILED);
					_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
						(const char *) err, __FILE__, __LINE__);
				}

				if (bDownloadAssetPathName. finish () != errNoError)
				{
					Error err = ToolsErrors (__FILE__, __LINE__,
						TOOLS_BUFFER_FINISH_FAILED);
					_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
						(const char *) err, __FILE__, __LINE__);
				}

				return err;
			}
	
			if (!bIsDirectoryExisting)
			{
				{
					Message msg = CMSRepositoryMessages (
						__FILE__, __LINE__,
						CMSREP_CMSREPOSITORY_CREATEDIRECTORY,
						2,
						pCustomerDirectoryName,
						(const char *) bDownloadAssetPathName);
					_ptSystemTracer -> trace (Tracer:: TRACER_LINFO,
						(const char *) msg, __FILE__, __LINE__);
				}

				#ifdef WIN32
					if ((errFileIO = FileIO:: createDirectory (
						(const char *) bDownloadAssetPathName,
						0, true, true)) != errNoError)
				#else
					if ((errFileIO = FileIO:: createDirectory (
						(const char *) bDownloadAssetPathName,
						S_IRUSR | S_IWUSR | S_IXUSR |
						S_IRGRP | S_IXGRP |
						S_IROTH | S_IXOTH, true, true)) !=
						errNoError)
				#endif
				{
					_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
						(const char *) errFileIO, __FILE__, __LINE__);

					Error err = ToolsErrors (__FILE__, __LINE__,
						TOOLS_FILEIO_CREATEDIRECTORY_FAILED,
						1, (const char *) bDownloadAssetPathName);
					_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
						(const char *) err, __FILE__, __LINE__);

					if (bStreamingAssetPathName. finish () != errNoError)
					{
						Error err = ToolsErrors (__FILE__, __LINE__,
							TOOLS_BUFFER_FINISH_FAILED);
						_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
							(const char *) err, __FILE__, __LINE__);
					}

					if (bDownloadAssetPathName. finish () != errNoError)
					{
						Error err = ToolsErrors (__FILE__, __LINE__,
							TOOLS_BUFFER_FINISH_FAILED);
						_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
							(const char *) err, __FILE__, __LINE__);
					}

					return err;
				}
			}

			if (FileIO:: isDirectoryExisting (
				(const char *) bStreamingAssetPathName,
				&bIsDirectoryExisting) != errNoError)
			{
				Error err = ToolsErrors (__FILE__, __LINE__,
					TOOLS_FILEIO_ISDIRECTORYEXISTING_FAILED);
				_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
					(const char *) err, __FILE__, __LINE__);

				if (bStreamingAssetPathName. finish () != errNoError)
				{
					Error err = ToolsErrors (__FILE__, __LINE__,
						TOOLS_BUFFER_FINISH_FAILED);
					_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
						(const char *) err, __FILE__, __LINE__);
				}

				if (bDownloadAssetPathName. finish () != errNoError)
				{
					Error err = ToolsErrors (__FILE__, __LINE__,
						TOOLS_BUFFER_FINISH_FAILED);
					_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
						(const char *) err, __FILE__, __LINE__);
				}

				return err;
			}

			if (!bIsDirectoryExisting)
			{
				{
					Message msg = CMSRepositoryMessages (
						__FILE__, __LINE__,
						CMSREP_CMSREPOSITORY_CREATEDIRECTORY,
						2,
						pCustomerDirectoryName,
						(const char *) bStreamingAssetPathName);
					_ptSystemTracer -> trace (Tracer:: TRACER_LINFO,
						(const char *) msg, __FILE__, __LINE__);
				}

				#ifdef WIN32
					if ((errFileIO = FileIO:: createDirectory (
						(const char *) bStreamingAssetPathName,
						0, true, true)) != errNoError)
				#else
					if ((errFileIO = FileIO:: createDirectory (
						(const char *) bStreamingAssetPathName,
						S_IRUSR | S_IWUSR | S_IXUSR |
						S_IRGRP | S_IXGRP |
						S_IROTH | S_IXOTH, true, true)) !=
						errNoError)
				#endif
				{
					_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
						(const char *) errFileIO, __FILE__, __LINE__);

					Error err = ToolsErrors (__FILE__, __LINE__,
						TOOLS_FILEIO_CREATEDIRECTORY_FAILED,
						1, (const char *) bStreamingAssetPathName);
					_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
						(const char *) err, __FILE__, __LINE__);

					if (bStreamingAssetPathName. finish () != errNoError)
					{
						Error err = ToolsErrors (__FILE__, __LINE__,
							TOOLS_BUFFER_FINISH_FAILED);
						_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
							(const char *) err, __FILE__, __LINE__);
					}

					if (bDownloadAssetPathName. finish () != errNoError)
					{
						Error err = ToolsErrors (__FILE__, __LINE__,
							TOOLS_BUFFER_FINISH_FAILED);
						_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
							(const char *) err, __FILE__, __LINE__);
					}

					return err;
				}
			}
		}
	}

	if (bStreamingAssetPathName. finish () != errNoError)
	{
		Error err = ToolsErrors (__FILE__, __LINE__,
			TOOLS_BUFFER_FINISH_FAILED);
		_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
			(const char *) err, __FILE__, __LINE__);

		if (bDownloadAssetPathName. finish () != errNoError)
		{
			Error err = ToolsErrors (__FILE__, __LINE__,
				TOOLS_BUFFER_FINISH_FAILED);
			_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
				(const char *) err, __FILE__, __LINE__);
		}

		return err;
	}

	if (bDownloadAssetPathName. finish () != errNoError)
	{
		Error err = ToolsErrors (__FILE__, __LINE__,
			TOOLS_BUFFER_FINISH_FAILED);
		_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
			(const char *) err, __FILE__, __LINE__);

		return err;
	}


	return errNoError;
}


Error CMSRepository:: moveContentInRepository (
	const char *pFilePathName,
	RepositoryType_t rtRepositoryType,
	const char *pCustomerDirectoryName,
	Boolean_t bAddDateTimeToFileName)

{

	return contentInRepository (
		1,
		pFilePathName,
		rtRepositoryType,
		pCustomerDirectoryName,
		bAddDateTimeToFileName);
}


Error CMSRepository:: copyFileInRepository (
	const char *pFilePathName,
	RepositoryType_t rtRepositoryType,
	const char *pCustomerDirectoryName,
	Boolean_t bAddDateTimeToFileName)

{

	return contentInRepository (
		0,
		pFilePathName,
		rtRepositoryType,
		pCustomerDirectoryName,
		bAddDateTimeToFileName);
}


Error CMSRepository:: contentInRepository (
	unsigned long ulIsCopyOrMove,
	const char *pContentPathName,
	RepositoryType_t rtRepositoryType,
	const char *pCustomerDirectoryName,
	Boolean_t bAddDateTimeToFileName)

{

	Buffer_t			bMetaDataFileInDestRepository;
	const char			*pFileName;
	Error_t				errFileIO;
	tm					tmDateTime;
	unsigned long		ulMilliSecs;
	FileIO:: DirectoryEntryType_t	detSourceFileType;


	// pDestRepository includes the '/' at the end
	if (bMetaDataFileInDestRepository. init (
		(const char *) (*(_pbRepositories [rtRepositoryType]))) != errNoError)
	{
		Error err = ToolsErrors (__FILE__, __LINE__,
			TOOLS_BUFFER_INIT_FAILED);
		_ptSystemTracer -> trace (Tracer:: TRACER_LERRR, (const char *) err,
			__FILE__, __LINE__);

		return err;
	}

	if (bMetaDataFileInDestRepository. append (pCustomerDirectoryName) !=
		errNoError)
	{
		Error err = ToolsErrors (__FILE__, __LINE__,
			TOOLS_BUFFER_APPEND_FAILED);
		_ptSystemTracer -> trace (Tracer:: TRACER_LERRR, (const char *) err,
			__FILE__, __LINE__);

		if (bMetaDataFileInDestRepository. finish () != errNoError)
		{
			Error err = ToolsErrors (__FILE__, __LINE__,
				TOOLS_BUFFER_FINISH_FAILED);
			_ptSystemTracer -> trace (Tracer:: TRACER_LERRR, (const char *) err,
				__FILE__, __LINE__);
		}

		return err;
	}

	#ifdef WIN32
		if (bMetaDataFileInDestRepository. append ("\\") !=
			errNoError)
		{
			Error err = ToolsErrors (__FILE__, __LINE__,
				TOOLS_BUFFER_APPEND_FAILED);
			_ptSystemTracer -> trace (Tracer:: TRACER_LERRR, (const char *) err,
				__FILE__, __LINE__);

			if (bMetaDataFileInDestRepository. finish () != errNoError)
			{
				Error err = ToolsErrors (__FILE__, __LINE__,
					TOOLS_BUFFER_FINISH_FAILED);
				_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
					(const char *) err, __FILE__, __LINE__);
			}

			return err;
		}
	#else
		if (bMetaDataFileInDestRepository. append ("/") !=
			errNoError)
		{
			Error err = ToolsErrors (__FILE__, __LINE__,
				TOOLS_BUFFER_APPEND_FAILED);
			_ptSystemTracer -> trace (Tracer:: TRACER_LERRR, (const char *) err,
				__FILE__, __LINE__);

			if (bMetaDataFileInDestRepository. finish () != errNoError)
			{
				Error err = ToolsErrors (__FILE__, __LINE__,
					TOOLS_BUFFER_FINISH_FAILED);
				_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
					(const char *) err, __FILE__, __LINE__);
			}

			return err;
		}
	#endif

	if (DateTime:: get_tm_LocalTime (
		&tmDateTime, &ulMilliSecs) != errNoError)
	{
		Error err = ToolsErrors (__FILE__, __LINE__,
			TOOLS_DATETIME_GET_TM_LOCALTIME_FAILED);
		_ptSystemTracer -> trace (Tracer:: TRACER_LERRR, (const char *) err,
			__FILE__, __LINE__);

		if (bMetaDataFileInDestRepository. finish () != errNoError)
		{
			Error err = ToolsErrors (__FILE__, __LINE__,
				TOOLS_BUFFER_FINISH_FAILED);
			_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
				(const char *) err, __FILE__, __LINE__);
		}

		return err;
	}

	if (rtRepositoryType == CMSREP_REPOSITORYTYPE_DONE ||
		rtRepositoryType == CMSREP_REPOSITORYTYPE_STAGING ||
		rtRepositoryType == CMSREP_REPOSITORYTYPE_ERRORS)
	{
		char				pDateTime [CMSREP_CMSREPOSITORY_MAXDATETIMELENGTH];
		Boolean_t			bIsDirectoryExisting;


		sprintf (pDateTime,
			"%04lu_%02lu_%02lu",
			(unsigned long) (tmDateTime. tm_year + 1900),
			(unsigned long) (tmDateTime. tm_mon + 1),
			(unsigned long) (tmDateTime. tm_mday));

		if (bMetaDataFileInDestRepository. append (pDateTime) !=
			errNoError)
		{
			Error err = ToolsErrors (__FILE__, __LINE__,
				TOOLS_BUFFER_APPEND_FAILED);
			_ptSystemTracer -> trace (Tracer:: TRACER_LERRR, (const char *) err,
				__FILE__, __LINE__);

			if (bMetaDataFileInDestRepository. finish () != errNoError)
			{
				Error err = ToolsErrors (__FILE__, __LINE__,
					TOOLS_BUFFER_FINISH_FAILED);
				_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
					(const char *) err, __FILE__, __LINE__);
			}

			return err;
		}

		if (FileIO:: isDirectoryExisting (
			(const char *) bMetaDataFileInDestRepository,
			&bIsDirectoryExisting) != errNoError)
		{
			Error err = ToolsErrors (__FILE__, __LINE__,
				TOOLS_FILEIO_ISDIRECTORYEXISTING_FAILED);
			_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
				(const char *) err, __FILE__, __LINE__);

			if (bMetaDataFileInDestRepository. finish () != errNoError)
			{
				Error err = ToolsErrors (__FILE__, __LINE__,
					TOOLS_BUFFER_FINISH_FAILED);
				_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
					(const char *) err, __FILE__, __LINE__);
			}

			return err;
		}

		if (!bIsDirectoryExisting)
		{
			{
				Message msg = CMSRepositoryMessages (
					__FILE__, __LINE__,
					CMSREP_CMSREPOSITORY_CREATEDIRECTORY,
					2,
					pCustomerDirectoryName,
					(const char *) bMetaDataFileInDestRepository);
				_ptSystemTracer -> trace (Tracer:: TRACER_LINFO,
					(const char *) msg, __FILE__, __LINE__);
			}

			#ifdef WIN32
				if ((errFileIO = FileIO:: createDirectory (
					(const char *) bMetaDataFileInDestRepository,
					0, true, true)) != errNoError)
			#else
				if ((errFileIO = FileIO:: createDirectory (
					(const char *) bMetaDataFileInDestRepository,
					S_IRUSR | S_IWUSR | S_IXUSR |
					S_IRGRP | S_IXGRP |
					S_IROTH | S_IXOTH, true, true)) !=
					errNoError)
			#endif
			{
				_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
					(const char *) errFileIO, __FILE__, __LINE__);

				Error err = ToolsErrors (__FILE__, __LINE__,
					TOOLS_FILEIO_CREATEDIRECTORY_FAILED,
					1, (const char *) bMetaDataFileInDestRepository);
				_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
					(const char *) err, __FILE__, __LINE__);

				if (bMetaDataFileInDestRepository. finish () != errNoError)
				{
					Error err = ToolsErrors (__FILE__, __LINE__,
						TOOLS_BUFFER_FINISH_FAILED);
					_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
						(const char *) err, __FILE__, __LINE__);
				}

				return err;
			}
		}

		#ifdef WIN32
			if (bMetaDataFileInDestRepository. append ("\\") !=
				errNoError)
			{
				Error err = ToolsErrors (__FILE__, __LINE__,
					TOOLS_BUFFER_APPEND_FAILED);
				_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
					(const char *) err, __FILE__, __LINE__);

				if (bMetaDataFileInDestRepository. finish () != errNoError)
				{
					Error err = ToolsErrors (__FILE__, __LINE__,
						TOOLS_BUFFER_FINISH_FAILED);
					_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
						(const char *) err, __FILE__, __LINE__);
				}

				return err;
			}
		#else
			if (bMetaDataFileInDestRepository. append ("/") !=
				errNoError)
			{
				Error err = ToolsErrors (__FILE__, __LINE__,
					TOOLS_BUFFER_APPEND_FAILED);
				_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
					(const char *) err, __FILE__, __LINE__);

				if (bMetaDataFileInDestRepository. finish () != errNoError)
				{
					Error err = ToolsErrors (__FILE__, __LINE__,
						TOOLS_BUFFER_FINISH_FAILED);
					_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
						(const char *) err, __FILE__, __LINE__);
				}

				return err;
			}
		#endif

		if (rtRepositoryType == CMSREP_REPOSITORYTYPE_DONE)
		{
			sprintf (pDateTime, "%02lu",
				(unsigned long) (tmDateTime. tm_hour));

			if (bMetaDataFileInDestRepository. append (pDateTime) != errNoError)
			{
				Error err = ToolsErrors (__FILE__, __LINE__,
					TOOLS_BUFFER_APPEND_FAILED);
				_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
					(const char *) err, __FILE__, __LINE__);

				if (bMetaDataFileInDestRepository. finish () != errNoError)
				{
					Error err = ToolsErrors (__FILE__, __LINE__,
						TOOLS_BUFFER_FINISH_FAILED);
					_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
						(const char *) err, __FILE__, __LINE__);
				}

				return err;
			}

			if (FileIO:: isDirectoryExisting (
				(const char *) bMetaDataFileInDestRepository,
				&bIsDirectoryExisting) != errNoError)
			{
				Error err = ToolsErrors (__FILE__, __LINE__,
					TOOLS_FILEIO_ISDIRECTORYEXISTING_FAILED);
				_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
					(const char *) err, __FILE__, __LINE__);

				if (bMetaDataFileInDestRepository. finish () != errNoError)
				{
					Error err = ToolsErrors (__FILE__, __LINE__,
						TOOLS_BUFFER_FINISH_FAILED);
					_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
						(const char *) err, __FILE__, __LINE__);
				}

				return err;
			}

			if (!bIsDirectoryExisting)
			{
				{
					Message msg = CMSRepositoryMessages (
						__FILE__, __LINE__,
						CMSREP_CMSREPOSITORY_CREATEDIRECTORY,
						2,
						pCustomerDirectoryName,
						(const char *) bMetaDataFileInDestRepository);
					_ptSystemTracer -> trace (Tracer:: TRACER_LINFO,
						(const char *) msg, __FILE__, __LINE__);
				}

				#ifdef WIN32
					if ((errFileIO = FileIO:: createDirectory (
						(const char *) bMetaDataFileInDestRepository,
						0, true, true)) != errNoError)
				#else
					if ((errFileIO = FileIO:: createDirectory (
						(const char *) bMetaDataFileInDestRepository,
						S_IRUSR | S_IWUSR | S_IXUSR |
						S_IRGRP | S_IXGRP |
						S_IROTH | S_IXOTH, true, true)) !=
						errNoError)
				#endif
				{
					_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
						(const char *) errFileIO, __FILE__, __LINE__);

					Error err = ToolsErrors (__FILE__, __LINE__,
						TOOLS_FILEIO_CREATEDIRECTORY_FAILED,
						1, (const char *) bMetaDataFileInDestRepository);
					_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
						(const char *) err, __FILE__, __LINE__);

					if (bMetaDataFileInDestRepository. finish () != errNoError)
					{
						Error err = ToolsErrors (__FILE__, __LINE__,
							TOOLS_BUFFER_FINISH_FAILED);
						_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
							(const char *) err, __FILE__, __LINE__);
					}

					return err;
				}
			}

			#ifdef WIN32
				if (bMetaDataFileInDestRepository. append ("\\") !=
					errNoError)
				{
					Error err = ToolsErrors (__FILE__, __LINE__,
						TOOLS_BUFFER_APPEND_FAILED);
					_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
						(const char *) err, __FILE__, __LINE__);

					if (bMetaDataFileInDestRepository. finish () != errNoError)
					{
						Error err = ToolsErrors (__FILE__, __LINE__,
							TOOLS_BUFFER_FINISH_FAILED);
						_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
							(const char *) err, __FILE__, __LINE__);
					}

					return err;
				}
			#else
				if (bMetaDataFileInDestRepository. append ("/") !=
					errNoError)
				{
					Error err = ToolsErrors (__FILE__, __LINE__,
						TOOLS_BUFFER_APPEND_FAILED);
					_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
						(const char *) err, __FILE__, __LINE__);

					if (bMetaDataFileInDestRepository. finish () != errNoError)
					{
						Error err = ToolsErrors (__FILE__, __LINE__,
							TOOLS_BUFFER_FINISH_FAILED);
						_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
							(const char *) err, __FILE__, __LINE__);
					}
	
					return err;
				}
			#endif
		}
	}

	if (bAddDateTimeToFileName)
	{
		char				pDateTime [CMSREP_CMSREPOSITORY_MAXDATETIMELENGTH];


		sprintf (pDateTime,
			"%04lu_%02lu_%02lu_%02lu_%02lu_%02lu_%04lu_",
			(unsigned long) (tmDateTime. tm_year + 1900),
			(unsigned long) (tmDateTime. tm_mon + 1),
			(unsigned long) (tmDateTime. tm_mday),
			(unsigned long) (tmDateTime. tm_hour),
			(unsigned long) (tmDateTime. tm_min),
			(unsigned long) (tmDateTime. tm_sec),
			ulMilliSecs);

		if (bMetaDataFileInDestRepository. append (pDateTime) !=
			errNoError)
		{
			Error err = ToolsErrors (__FILE__, __LINE__,
				TOOLS_BUFFER_APPEND_FAILED);
			_ptSystemTracer -> trace (Tracer:: TRACER_LERRR, (const char *) err,
				__FILE__, __LINE__);

			if (bMetaDataFileInDestRepository. finish () != errNoError)
			{
				Error err = ToolsErrors (__FILE__, __LINE__,
					TOOLS_BUFFER_FINISH_FAILED);
				_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
					(const char *) err, __FILE__, __LINE__);
			}

			return err;
		}
	}

	#ifdef WIN32
		if ((pFileName = strrchr (pContentPathName, '\\')) == (char *) NULL)
			pFileName		= (char *) pContentPathName;
		else
			pFileName++;
	#else
		if ((pFileName = strrchr (pContentPathName, '/')) == (char *) NULL)
			pFileName		= (char *) pContentPathName;
		else
			pFileName++;
	#endif

	if (bMetaDataFileInDestRepository. append (pFileName) !=
		errNoError)
	{
		Error err = ToolsErrors (__FILE__, __LINE__,
			TOOLS_BUFFER_APPEND_FAILED);
		_ptSystemTracer -> trace (Tracer:: TRACER_LERRR, (const char *) err,
			__FILE__, __LINE__);

		if (bMetaDataFileInDestRepository. finish () != errNoError)
		{
			Error err = ToolsErrors (__FILE__, __LINE__,
				TOOLS_BUFFER_FINISH_FAILED);
			_ptSystemTracer -> trace (Tracer:: TRACER_LERRR, (const char *) err,
				__FILE__, __LINE__);
		}

		return err;
	}

	// file in case of .3gp content OR
	// directory in case of IPhone content
	if ((errFileIO = FileIO:: getDirectoryEntryType (
		pContentPathName, &detSourceFileType)) != errNoError)
	{
		_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
			(const char *) errFileIO, __FILE__, __LINE__);

		Error err = ToolsErrors (__FILE__, __LINE__,
			TOOLS_FILEIO_GETDIRECTORYENTRYTYPE_FAILED,
			1, pContentPathName);
		_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
			(const char *) err, __FILE__, __LINE__);

		if (bMetaDataFileInDestRepository. finish () != errNoError)
		{
			Error err = ToolsErrors (__FILE__, __LINE__,
				TOOLS_BUFFER_FINISH_FAILED);
			_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
				(const char *) err, __FILE__, __LINE__);
		}

		return err;
	}

	if (ulIsCopyOrMove == 1)
	{
		if (detSourceFileType == FileIO:: TOOLS_FILEIO_DIRECTORY)
		{
			{
				Message msg = CMSRepositoryMessages (__FILE__, __LINE__,
					CMSREP_CMSREPOSITORY_MOVEDIRECTORY,
					3,
					pCustomerDirectoryName,
					pContentPathName,
					(const char *) bMetaDataFileInDestRepository);
				_ptSystemTracer -> trace (Tracer:: TRACER_LINFO,
					(const char *) msg, __FILE__, __LINE__);
			}

			if ((errFileIO = FileIO:: moveDirectory (
				pContentPathName,
				(const char *) bMetaDataFileInDestRepository,
				S_IRUSR | S_IWUSR | S_IXUSR |
				S_IRGRP | S_IXGRP |
				S_IROTH | S_IXOTH)) != errNoError)
			{
				_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
					(const char *) errFileIO, __FILE__, __LINE__);

				Error err = ToolsErrors (__FILE__, __LINE__,
					TOOLS_FILEIO_MOVEDIRECTORY_FAILED,
					2, pContentPathName,
					(const char *) bMetaDataFileInDestRepository);
				_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
					(const char *) err, __FILE__, __LINE__);

				if (bMetaDataFileInDestRepository. finish () != errNoError)
				{
					Error err = ToolsErrors (__FILE__, __LINE__,
						TOOLS_BUFFER_FINISH_FAILED);
					_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
						(const char *) err, __FILE__, __LINE__);
				}

				return err;
			}
		}
		else // if (detSourceFileType == FileIO:: TOOLS_FILEIO_REGULARFILE
		{
			{
				Message msg = CMSRepositoryMessages (__FILE__, __LINE__,
					CMSREP_CMSREPOSITORY_MOVEFILE,
					3,
					pCustomerDirectoryName,
					pContentPathName,
					(const char *) bMetaDataFileInDestRepository);
				_ptSystemTracer -> trace (Tracer:: TRACER_LINFO,
					(const char *) msg, __FILE__, __LINE__);
			}

			if ((errFileIO = FileIO:: moveFile (
				pContentPathName,
				(const char *) bMetaDataFileInDestRepository)) != errNoError)
			{
				_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
					(const char *) errFileIO, __FILE__, __LINE__);

				Error err = ToolsErrors (__FILE__, __LINE__,
					TOOLS_FILEIO_MOVEFILE_FAILED,
					2, pContentPathName,
					(const char *) bMetaDataFileInDestRepository);
				_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
					(const char *) err, __FILE__, __LINE__);

				if (bMetaDataFileInDestRepository. finish () != errNoError)
				{
					Error err = ToolsErrors (__FILE__, __LINE__,
						TOOLS_BUFFER_FINISH_FAILED);
					_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
						(const char *) err, __FILE__, __LINE__);
				}

				return err;
			}
		}
	}
	else
	{
		if (detSourceFileType == FileIO:: TOOLS_FILEIO_DIRECTORY)
		{
			{
				Message msg = CMSRepositoryMessages (__FILE__, __LINE__,
					CMSREP_CMSREPOSITORY_COPYDIRECTORY,
					3,
					pCustomerDirectoryName,
					pContentPathName,
					(const char *) bMetaDataFileInDestRepository);
				_ptSystemTracer -> trace (Tracer:: TRACER_LINFO,
					(const char *) msg, __FILE__, __LINE__);
			}

			if ((errFileIO = FileIO:: copyDirectory (
				pContentPathName,
				(const char *) bMetaDataFileInDestRepository,
				S_IRUSR | S_IWUSR | S_IXUSR |
				S_IRGRP | S_IXGRP |
				S_IROTH | S_IXOTH)) != errNoError)
			{
				_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
					(const char *) errFileIO, __FILE__, __LINE__);

				Error err = ToolsErrors (__FILE__, __LINE__,
					TOOLS_FILEIO_COPYDIRECTORY_FAILED,
					2, pContentPathName,
					(const char *) bMetaDataFileInDestRepository);
				_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
					(const char *) err, __FILE__, __LINE__);

				if (bMetaDataFileInDestRepository. finish () != errNoError)
				{
					Error err = ToolsErrors (__FILE__, __LINE__,
						TOOLS_BUFFER_FINISH_FAILED);
					_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
						(const char *) err, __FILE__, __LINE__);
				}

				return err;
			}
		}
		else
		{
			{
				Message msg = CMSRepositoryMessages (__FILE__, __LINE__,
					CMSREP_CMSREPOSITORY_COPYFILE,
					3,
					pCustomerDirectoryName,
					pContentPathName,
					(const char *) bMetaDataFileInDestRepository);
				_ptSystemTracer -> trace (Tracer:: TRACER_LINFO,
					(const char *) msg, __FILE__, __LINE__);
			}

			if ((errFileIO = FileIO:: copyFile (
				pContentPathName,
				(const char *) bMetaDataFileInDestRepository)) != errNoError)
			{
				_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
					(const char *) errFileIO, __FILE__, __LINE__);

				Error err = ToolsErrors (__FILE__, __LINE__,
					TOOLS_FILEIO_COPYFILE_FAILED,
					2, pContentPathName,
					(const char *) bMetaDataFileInDestRepository);
				_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
					(const char *) err, __FILE__, __LINE__);

				if (bMetaDataFileInDestRepository. finish () != errNoError)
				{
					Error err = ToolsErrors (__FILE__, __LINE__,
						TOOLS_BUFFER_FINISH_FAILED);
					_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
						(const char *) err, __FILE__, __LINE__);
				}

				return err;
			}
		}
	}

	if (bMetaDataFileInDestRepository. finish () != errNoError)
	{
		Error err = ToolsErrors (__FILE__, __LINE__,
			TOOLS_BUFFER_FINISH_FAILED);
		_ptSystemTracer -> trace (Tracer:: TRACER_LERRR, (const char *) err,
			__FILE__, __LINE__);

		return err;
	}


	return errNoError;
}


Error CMSRepository:: moveAssetInCMSRepository (
	const char *pSourceAssetPathName,
	const char *pCustomerDirectoryName,
	const char *pDestinationFileName,
	const char *pRelativePath,

	Boolean_t bIsPartitionIndexToBeCalculated,
	unsigned long *pulCMSPartitionIndexUsed,	// OUT if bIsPartitionIndexToBeCalculated is true, IN is bIsPartitionIndexToBeCalculated is false

	Boolean_t bDeliveryRepositoriesToo,
	TerritoriesHashMap_p phmTerritories,

	Buffer_p pbCMSAssetPathName)	// OUT

{

	Error_t							errMove;
	Error							errEntryType;
	FileIO:: DirectoryEntryType_t	detSourceFileType;


	if (pSourceAssetPathName == (const char *) NULL ||
		pCustomerDirectoryName == (const char *) NULL ||
		pDestinationFileName == (const char *) NULL ||
		pRelativePath == (const char *) NULL ||
		pRelativePath [0] != '/' ||

		pulCMSPartitionIndexUsed == (unsigned long *) NULL ||

		pbCMSAssetPathName == (Buffer_p) NULL)
	{
		Error err = CMSRepositoryErrors (__FILE__, __LINE__,
			CMSREP_ACTIVATION_WRONG);
		_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
			(const char *) err, __FILE__, __LINE__);

		return err;
	}

	if (_mtCMSPartitions. lock () != errNoError)
	{
		Error err = PThreadErrors (__FILE__, __LINE__,
			THREADLIB_PMUTEX_LOCK_FAILED);
		_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
			(const char *) err, __FILE__, __LINE__);

		return err;
	}

	// file in case of .3gp content OR
	// directory in case of IPhone content
	if ((errEntryType = FileIO:: getDirectoryEntryType (
		pSourceAssetPathName, &detSourceFileType)) != errNoError)
	{
		_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
			(const char *) errEntryType, __FILE__, __LINE__);

		Error err = ToolsErrors (__FILE__, __LINE__,
			TOOLS_FILEIO_GETDIRECTORYENTRYTYPE_FAILED,
			1, pSourceAssetPathName);
		_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
			(const char *) err, __FILE__, __LINE__);

		if (_mtCMSPartitions. unLock () != errNoError)
		{
			Error err = PThreadErrors (__FILE__, __LINE__,
				THREADLIB_PMUTEX_UNLOCK_FAILED);
			_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
				(const char *) err, __FILE__, __LINE__);
		}

		return errEntryType;
	}

	if (detSourceFileType != FileIO:: TOOLS_FILEIO_DIRECTORY &&
		detSourceFileType != FileIO:: TOOLS_FILEIO_REGULARFILE)
	{
		Error err = CMSRepositoryErrors (__FILE__, __LINE__,
			CMSREP_CMSREPOSITORY_WRONGDIRECTORYENTRYTYPE,
			2, (long) detSourceFileType, pSourceAssetPathName);
		_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
			(const char *) err, __FILE__, __LINE__);

		if (_mtCMSPartitions. unLock () != errNoError)
		{
			Error err = PThreadErrors (__FILE__, __LINE__,
				THREADLIB_PMUTEX_UNLOCK_FAILED);
			_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
				(const char *) err, __FILE__, __LINE__);
		}

		return err;
	}

	if (bIsPartitionIndexToBeCalculated)
	{
		Error_t							errGetFileSize;
		unsigned long long				ullFSEntrySizeInBytes;
		unsigned long					ulCMSPartitionIndex;
		unsigned long long				ullCMSPartitionsFreeSizeInKB;


		if (detSourceFileType == FileIO:: TOOLS_FILEIO_DIRECTORY)
		{
			if ((errGetFileSize = FileIO:: getDirectorySizeInBytes (
				pSourceAssetPathName, &ullFSEntrySizeInBytes)) != errNoError)
			{
				_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
					(const char *) errGetFileSize, __FILE__, __LINE__);

				Error err = ToolsErrors (__FILE__, __LINE__,
					TOOLS_FILEIO_GETDIRECTORYSIZEINBYTES_FAILED,
					1, pSourceAssetPathName);
				_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
					(const char *) err, __FILE__, __LINE__);

				if (_mtCMSPartitions. unLock () != errNoError)
				{
					Error err = PThreadErrors (__FILE__, __LINE__,
						THREADLIB_PMUTEX_UNLOCK_FAILED);
					_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
						(const char *) err, __FILE__, __LINE__);
				}

				return errGetFileSize;
			}
		}
		else // if (detSourceFileType == FileIO:: TOOLS_FILEIO_REGULARFILE)
		{
			unsigned long					ulFileSizeInBytes;


			if ((errGetFileSize = FileIO:: getFileSizeInBytes (
				pSourceAssetPathName, &ulFileSizeInBytes, false)) != errNoError)
			{
				_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
					(const char *) errGetFileSize, __FILE__, __LINE__);

				Error err = ToolsErrors (__FILE__, __LINE__,
					TOOLS_FILEIO_GETFILESIZEINBYTES_FAILED,
					1, pSourceAssetPathName);
				_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
					(const char *) err, __FILE__, __LINE__);

				if (_mtCMSPartitions. unLock () != errNoError)
				{
					Error err = PThreadErrors (__FILE__, __LINE__,
						THREADLIB_PMUTEX_UNLOCK_FAILED);
					_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
						(const char *) err, __FILE__, __LINE__);
				}

				return errGetFileSize;
			}

			ullFSEntrySizeInBytes		= ulFileSizeInBytes;
		}

		// find the CMS partition index
		for (ulCMSPartitionIndex = 0;
			ulCMSPartitionIndex < _ulCMSPartitionsNumber;
			ulCMSPartitionIndex++)
		{
			ullCMSPartitionsFreeSizeInKB		= (unsigned long long)
				((_pullCMSPartitionsFreeSizeInMB [
				_ulCurrentCMSPartitionIndex]) *
				1024);

			if (ullCMSPartitionsFreeSizeInKB <=
				(_ullFreeSpaceToLeaveInEachPartition * 1024))
			{
				{
					Message msg = CMSRepositoryMessages (
						__FILE__, __LINE__,
						CMSREP_CMSREPOSITORY_PARTITIONFREESPACETOOLOW,
						3, _ulCurrentCMSPartitionIndex,
						ullCMSPartitionsFreeSizeInKB,
						_ullFreeSpaceToLeaveInEachPartition * 1024);
					_ptSystemTracer -> trace (Tracer:: TRACER_LINFO,
						(const char *) msg, __FILE__, __LINE__);
				}

				if (_ulCurrentCMSPartitionIndex + 1 >=
					_ulCMSPartitionsNumber)
					_ulCurrentCMSPartitionIndex		= 0;
				else
					_ulCurrentCMSPartitionIndex++;

				continue;
			}

			if ((unsigned long long)
				(ullCMSPartitionsFreeSizeInKB -
				(_ullFreeSpaceToLeaveInEachPartition * 1024)) >
				(ullFSEntrySizeInBytes / 1024))
			{

				break;
			}

			if (_ulCurrentCMSPartitionIndex + 1 >=
				_ulCMSPartitionsNumber)
				_ulCurrentCMSPartitionIndex		= 0;
			else
				_ulCurrentCMSPartitionIndex++;
		}

		if (ulCMSPartitionIndex == _ulCMSPartitionsNumber)
		{
			Error err = CMSRepositoryErrors (__FILE__, __LINE__,
				CMSREP_CMSREPOSITORY_NOMORESPACEINCMSPARTITIONS,
				1, ullFSEntrySizeInBytes);
			_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
				(const char *) err, __FILE__, __LINE__);

			if (_mtCMSPartitions. unLock () != errNoError)
			{
				Error err = PThreadErrors (__FILE__, __LINE__,
					THREADLIB_PMUTEX_UNLOCK_FAILED);
				_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
					(const char *) err, __FILE__, __LINE__);
			}

			return err;
		}

		*pulCMSPartitionIndexUsed			= _ulCurrentCMSPartitionIndex;
	}

	// creating directories and build the bCMSAssetPathName
	{
		const char					*pStartCurrentDirectoryName;


		{
			Message msg = CMSRepositoryMessages (
				__FILE__, __LINE__,
				CMSREP_CMSREPOSITORY_CREATINGDIRS,
				3, *pulCMSPartitionIndexUsed, pRelativePath,
				(long) bDeliveryRepositoriesToo);
			_ptSystemTracer -> trace (Tracer:: TRACER_LINFO,
				(const char *) msg, __FILE__, __LINE__);
		}

		// to create the content provider directory and the
		// territories directories (if not already existing)
		#ifdef WIN32
			if (creatingDirsUsingTerritories (*pulCMSPartitionIndexUsed,
				pRelativePath, pCustomerDirectoryName,
				bDeliveryRepositoriesToo,
				phmTerritories,
				pbCMSAssetPathName) != errNoError)
		#else
			if (creatingDirsUsingTerritories (*pulCMSPartitionIndexUsed,
				pRelativePath, pCustomerDirectoryName,
				bDeliveryRepositoriesToo,
				phmTerritories,
				pbCMSAssetPathName) != errNoError)
		#endif
		{
			Error err = CMSRepositoryErrors (__FILE__, __LINE__,
				CMSREP_CMSREPOSITORY_CREATINGDIRSUSINGTERRITORIES_FAILED);
			_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
				(const char *) err, __FILE__, __LINE__);

			if (_mtCMSPartitions. unLock () != errNoError)
			{
				Error err = PThreadErrors (__FILE__, __LINE__,
					THREADLIB_PMUTEX_UNLOCK_FAILED);
				_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
					(const char *) err, __FILE__, __LINE__);
			}

			return err;
		}

		if (pbCMSAssetPathName -> append (pDestinationFileName) != errNoError)
		{
			Error err = ToolsErrors (__FILE__, __LINE__,
				TOOLS_BUFFER_SETBUFFER_FAILED);
			_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
				(const char *) err, __FILE__, __LINE__);

			if (_mtCMSPartitions. unLock () != errNoError)
			{
				Error err = PThreadErrors (__FILE__, __LINE__,
					THREADLIB_PMUTEX_UNLOCK_FAILED);
				_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
					(const char *) err, __FILE__, __LINE__);
			}

			return err;
		}
	}

	{
		Message msg = CMSRepositoryMessages (
			__FILE__, __LINE__, 
			CMSREP_CMSREPOSITORY_PARTITIONSELECTED,
			5, 
			pCustomerDirectoryName,
			*pulCMSPartitionIndexUsed,
			(const char *) (*pbCMSAssetPathName),
			_ulCurrentCMSPartitionIndex,
			_pullCMSPartitionsFreeSizeInMB [_ulCurrentCMSPartitionIndex]);
		_ptSystemTracer -> trace (Tracer:: TRACER_LINFO,
			(const char *) msg, __FILE__, __LINE__);
	}

	// move the file in case of .3gp content OR
	// move the directory in case of IPhone content
	{
		if (detSourceFileType == FileIO:: TOOLS_FILEIO_DIRECTORY)
		{
			{
				Message msg = CMSRepositoryMessages (__FILE__, __LINE__,
					CMSREP_CMSREPOSITORY_MOVEDIRECTORY,
					3,
					pCustomerDirectoryName,
					pSourceAssetPathName,
					(const char *) (*pbCMSAssetPathName));
				_ptSystemTracer -> trace (Tracer:: TRACER_LINFO,
					(const char *) msg, __FILE__, __LINE__);
			}

			if ((errMove = FileIO:: moveDirectory (pSourceAssetPathName,
				(const char *) (*pbCMSAssetPathName),
				S_IRUSR | S_IWUSR | S_IXUSR |
				S_IRGRP | S_IXGRP |
				S_IROTH | S_IXOTH)) != errNoError)
			{
				_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
					(const char *) errMove, __FILE__, __LINE__);

				Error err = ToolsErrors (__FILE__, __LINE__,
					TOOLS_FILEIO_MOVEDIRECTORY_FAILED,
					2, pSourceAssetPathName,
					(const char *) (*pbCMSAssetPathName));
				_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
					(const char *) err, __FILE__, __LINE__);

				if (_mtCMSPartitions. unLock () != errNoError)
				{
					Error err = PThreadErrors (__FILE__, __LINE__,
						THREADLIB_PMUTEX_UNLOCK_FAILED);
					_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
						(const char *) err, __FILE__, __LINE__);
				}

				return errMove;
			}
		}
		else // if (detDirectoryEntryType == FileIO:: TOOLS_FILEIO_REGULARFILE)
		{
			{
				Message msg = CMSRepositoryMessages (__FILE__, __LINE__,
					CMSREP_CMSREPOSITORY_MOVEFILE,
					3,
					pCustomerDirectoryName,
					pSourceAssetPathName,
					(const char *) (*pbCMSAssetPathName));
				_ptSystemTracer -> trace (Tracer:: TRACER_LINFO,
					(const char *) msg, __FILE__, __LINE__);
			}

			if ((errMove = FileIO:: moveFile (pSourceAssetPathName,
				(const char *) (*pbCMSAssetPathName))) != errNoError)
			{
				_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
					(const char *) errMove, __FILE__, __LINE__);

				Error err = ToolsErrors (__FILE__, __LINE__,
					TOOLS_FILEIO_MOVEFILE_FAILED,
					2, pSourceAssetPathName,
					(const char *) (*pbCMSAssetPathName));
				_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
					(const char *) err, __FILE__, __LINE__);

				if (_mtCMSPartitions. unLock () != errNoError)
				{
					Error err = PThreadErrors (__FILE__, __LINE__,
						THREADLIB_PMUTEX_UNLOCK_FAILED);
					_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
						(const char *) err, __FILE__, __LINE__);
				}

				return errMove;
			}
		}
	}

	// update _pullCMSPartitionsFreeSizeInMB ONLY if bIsPartitionIndexToBeCalculated
	if (bIsPartitionIndexToBeCalculated)
	{
		unsigned long long		ullUsedInKB;
		unsigned long long		ullAvailableInKB;
		long					lPercentUsed;
		Error_t					errFileIO;


		if ((errFileIO = FileIO:: getFileSystemInfo (
			(const char *) (*pbCMSAssetPathName),
			&ullUsedInKB, &ullAvailableInKB, &lPercentUsed)) !=
			errNoError)
		{
			_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
				(const char *) errFileIO, __FILE__, __LINE__);

			Error err = ToolsErrors (__FILE__, __LINE__,
				TOOLS_FILEIO_GETFILESYSTEMINFO_FAILED);
			_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
				(const char *) err, __FILE__, __LINE__);

			if (_mtCMSPartitions. unLock () != errNoError)
			{
				Error err = PThreadErrors (__FILE__, __LINE__,
					THREADLIB_PMUTEX_UNLOCK_FAILED);
				_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
					(const char *) err, __FILE__, __LINE__);
			}

			return errFileIO;
		}

		_pullCMSPartitionsFreeSizeInMB [_ulCurrentCMSPartitionIndex]     =
			ullAvailableInKB / 1024;

		{
			Message msg = CMSRepositoryMessages (
				__FILE__, __LINE__, 
				CMSREP_CMSREPOSITORY_AVAILABLESPACE,
				2, 
				(const char *) (*pbCMSAssetPathName),
				_pullCMSPartitionsFreeSizeInMB [_ulCurrentCMSPartitionIndex]);
			_ptSystemTracer -> trace (Tracer:: TRACER_LINFO,
				(const char *) msg, __FILE__, __LINE__);
		}

		{
			Message msg = CMSRepositoryMessages (__FILE__, __LINE__,
				CMSREP_CMSREPOSITORY_CMSPARTITIONAVAILABLESPACE,
				2,
				_ulCurrentCMSPartitionIndex,
				(_pullCMSPartitionsFreeSizeInMB [_ulCurrentCMSPartitionIndex]));
			_ptSystemTracer -> trace (Tracer:: TRACER_LINFO,
				(const char *) msg, __FILE__, __LINE__);
		}
	}

	if (_mtCMSPartitions. unLock () != errNoError)
	{
		Error err = PThreadErrors (__FILE__, __LINE__,
			THREADLIB_PMUTEX_UNLOCK_FAILED);
		_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
			(const char *) err, __FILE__, __LINE__);

		return err;
	}


	return errNoError;
}


Error CMSRepository:: sanityCheck_ContentsOnFileSystem (
	RepositoryType_t rtRepositoryType)

{

	FileIO:: Directory_t			dDeliveryDirectoryL1;
	Error_t							errOpenDir;
	Error_t							errReadDirectory;
	FileIO:: DirectoryEntryType_t	detDirectoryEntryType;
	SanityCheckContentInfo_t		psciSanityCheckContentsInfo [
		CMSREP_CMSREPOSITORY_MAXSANITYCHECKCONTENTSINFONUMBER];
	long							lSanityCheckContentsInfoCurrentIndex;
	time_t							tSanityCheckStart;
	unsigned long					ulDirectoryLevelIndexInsideCustomer;
	unsigned long					ulFileIndex;
	unsigned long					ulCurrentFileNumberProcessedInThisSchedule;
	unsigned long				ulCurrentFilesRemovedNumberInThisSchedule;
	// OthersFiles means: unexpected, WorkingArea
	unsigned long				ulCurrentDirectoriesRemovedNumberInThisSchedule;
	Boolean_t					bHasCustomerToBeResumed;
	Error_t						errSanityCheck;


	{
		Message msg = CMSRepositoryMessages (__FILE__, __LINE__,
			CMSREP_CMSREPOSITORY_STARTSANITYCHECKONREPOSITORY,
			1, (const char *) (*(_pbRepositories [rtRepositoryType])));
		_ptSystemTracer -> trace (Tracer:: TRACER_LINFO,
			(const char *) msg, __FILE__, __LINE__);
	}

	tSanityCheckStart								= time (NULL);

	bHasCustomerToBeResumed							= true;

	lSanityCheckContentsInfoCurrentIndex			= -1;

	ulFileIndex										= 0;
	ulCurrentFileNumberProcessedInThisSchedule		= 0;
	ulCurrentFilesRemovedNumberInThisSchedule	= 0;
	ulCurrentDirectoriesRemovedNumberInThisSchedule	= 0;

	if ((errOpenDir = FileIO:: openDirectory (
		(const char *) (*(_pbRepositories [rtRepositoryType])),
		&dDeliveryDirectoryL1)) != errNoError)
	{
		_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
			(const char *) errOpenDir, __FILE__, __LINE__);

		Error err = ToolsErrors (__FILE__, __LINE__,
			TOOLS_FILEIO_OPENDIRECTORY_FAILED);
		_ptSystemTracer -> trace (Tracer:: TRACER_LERRR, (const char *) err,
			__FILE__, __LINE__);

		return err;
	}

	if (rtRepositoryType == CMSREP_REPOSITORYTYPE_CMSCUSTOMER ||
		rtRepositoryType == CMSREP_REPOSITORYTYPE_DOWNLOAD ||
		rtRepositoryType == CMSREP_REPOSITORYTYPE_STREAMING)
	{
		// the customers inside the CMS repository are inside one more
		// level because this level is for the CMSREP_XXXX directories

		FileIO:: Directory_t			dDeliveryDirectoryL2;
		Buffer_t						bPartitionDirectory;
		Buffer_t						bCustomersDirectory;
		Boolean_t						bIsCMS_Directory;
		Boolean_t						bHasPartitionToBeResumed;


		if (bCustomersDirectory. init () != errNoError)
		{
			Error err = ToolsErrors (__FILE__, __LINE__,
				TOOLS_BUFFER_INIT_FAILED);
			_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
				(const char *) err, __FILE__, __LINE__);

			if (FileIO:: closeDirectory (&dDeliveryDirectoryL1) !=
				errNoError)
			{
				Error err = ToolsErrors (__FILE__, __LINE__,
					TOOLS_FILEIO_CLOSEDIRECTORY_FAILED);
				_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
					(const char *) err, __FILE__, __LINE__);
			}

			return err;
		}

		if (bPartitionDirectory. init () != errNoError)
		{
			Error err = ToolsErrors (__FILE__, __LINE__,
				TOOLS_BUFFER_INIT_FAILED);
			_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
				(const char *) err, __FILE__, __LINE__);

			if (FileIO:: closeDirectory (&dDeliveryDirectoryL1) !=
				errNoError)
			{
				Error err = ToolsErrors (__FILE__, __LINE__,
					TOOLS_FILEIO_CLOSEDIRECTORY_FAILED);
				_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
					(const char *) err, __FILE__, __LINE__);
			}

			return err;
		}

		bHasPartitionToBeResumed				= true;

		while ((errReadDirectory = FileIO:: readDirectory (
			&dDeliveryDirectoryL1, &bPartitionDirectory,
			&detDirectoryEntryType)) == errNoError)
		{
			if (bHasPartitionToBeResumed &&
				strcmp (_svlpcLastProcessedContent [rtRepositoryType].
					_pPartition, "") &&
				strcmp (_svlpcLastProcessedContent [rtRepositoryType].
					_pPartition, (const char *) bPartitionDirectory))
				continue;

			bHasPartitionToBeResumed			= false;

			if ((unsigned long) bPartitionDirectory >=
				CMSREP_CMSREPOSITORY_MAXCMSPARTITIONNAMELENGTH)
			{
				Error err = CMSRepositoryErrors (__FILE__, __LINE__,
					CMSREP_CMSREPOSITORY_BUFFERNOTENOUGHBIG,
					2,
					(unsigned long)
						CMSREP_CMSREPOSITORY_MAXCMSPARTITIONNAMELENGTH,
					(unsigned long) bPartitionDirectory);
				_ptSystemTracer -> trace (
					Tracer:: TRACER_LERRR,
					(const char *) err, __FILE__, __LINE__);

				if (FileIO:: closeDirectory (&dDeliveryDirectoryL1) !=
					errNoError)
				{
					Error err = ToolsErrors (__FILE__, __LINE__,
						TOOLS_FILEIO_CLOSEDIRECTORY_FAILED);
					_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
						(const char *) err, __FILE__, __LINE__);
				}

				return err;
			}
			strcpy (_svlpcLastProcessedContent [rtRepositoryType].
				_pPartition, (const char *) bPartitionDirectory);

			if ((detDirectoryEntryType == FileIO:: TOOLS_FILEIO_DIRECTORY ||
				detDirectoryEntryType == FileIO:: TOOLS_FILEIO_LINKFILE) &&
				strncmp ((const char *) bPartitionDirectory, "CMS_", 4) == 0)
			{
				bIsCMS_Directory			= true;
			}
			else
			{
				bIsCMS_Directory			= false;
			}

			if ((rtRepositoryType == CMSREP_REPOSITORYTYPE_DOWNLOAD &&
					!strcmp ((const char *) bPartitionDirectory,
					_pDownloadReservedDirectoryName)) ||
				(rtRepositoryType == CMSREP_REPOSITORYTYPE_DOWNLOAD &&
					!strcmp ((const char *) bPartitionDirectory,
					_pDownloadFreeDirectoryName)) ||
				(rtRepositoryType == CMSREP_REPOSITORYTYPE_DOWNLOAD &&
					!strcmp ((const char *) bPartitionDirectory,
					_pDownloadiPhoneLiveDirectoryName)) ||
				(rtRepositoryType == CMSREP_REPOSITORYTYPE_DOWNLOAD &&
					!strcmp ((const char *) bPartitionDirectory,
					_pDownloadSilverlightLiveDirectoryName)) ||
				(rtRepositoryType == CMSREP_REPOSITORYTYPE_DOWNLOAD &&
					!strcmp ((const char *) bPartitionDirectory,
					_pDownloadAdobeLiveDirectoryName)) ||
				(rtRepositoryType == CMSREP_REPOSITORYTYPE_STREAMING &&
					!strcmp ((const char *) bPartitionDirectory,
					_pStreamingFreeDirectoryName)) ||
				(rtRepositoryType == CMSREP_REPOSITORYTYPE_STREAMING &&
					!strcmp ((const char *) bPartitionDirectory,
					_pStreamingMetaDirectoryName)) ||
				(rtRepositoryType == CMSREP_REPOSITORYTYPE_STREAMING &&
					!strcmp ((const char *) bPartitionDirectory,
					_pStreamingRecordingDirectoryName)))
				continue;

			if (bCustomersDirectory. setBuffer (
				(const char *) (*(_pbRepositories [rtRepositoryType]))) !=
				errNoError)
			{
				Error err = ToolsErrors (__FILE__, __LINE__,
					TOOLS_BUFFER_SETBUFFER_FAILED);
				_ptSystemTracer -> trace (
					Tracer:: TRACER_LERRR,
					(const char *) err, __FILE__, __LINE__);

				if (FileIO:: closeDirectory (&dDeliveryDirectoryL1) !=
					errNoError)
				{
					Error err = ToolsErrors (__FILE__, __LINE__,
						TOOLS_FILEIO_CLOSEDIRECTORY_FAILED);
					_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
						(const char *) err, __FILE__, __LINE__);
				}

				return err;
			}

			if (bCustomersDirectory. append (
				(const char *) bPartitionDirectory) != errNoError)
			{
				Error err = ToolsErrors (__FILE__, __LINE__,
					TOOLS_BUFFER_APPEND_FAILED);
				_ptSystemTracer -> trace (
					Tracer:: TRACER_LERRR,
					(const char *) err, __FILE__, __LINE__);

				if (FileIO:: closeDirectory (&dDeliveryDirectoryL1) !=
					errNoError)
				{
					Error err = ToolsErrors (__FILE__, __LINE__,
						TOOLS_FILEIO_CLOSEDIRECTORY_FAILED);
					_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
						(const char *) err, __FILE__, __LINE__);
				}

				return err;
			}

			if (bCustomersDirectory. append ("/") != errNoError)
			{
				Error err = ToolsErrors (__FILE__, __LINE__,
					TOOLS_BUFFER_APPEND_FAILED);
				_ptSystemTracer -> trace (
					Tracer:: TRACER_LERRR,
					(const char *) err, __FILE__, __LINE__);

				if (FileIO:: closeDirectory (&dDeliveryDirectoryL1) !=
					errNoError)
				{
					Error err = ToolsErrors (__FILE__, __LINE__,
						TOOLS_FILEIO_CLOSEDIRECTORY_FAILED);
					_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
						(const char *) err, __FILE__, __LINE__);
				}

				return err;
			}

			if (!strcmp ((const char *) bCustomersDirectory,
					(const char *) _bDoneRootRepository) ||
				!strcmp ((const char *) bCustomersDirectory,
					(const char *) _bErrorRootRepository) ||
				!strcmp ((const char *) bCustomersDirectory,
					(const char *) _bStagingRootRepository) ||
				!strcmp ((const char *) bCustomersDirectory,
					(const char *) _bProfilesRootRepository))
				continue;

			if (!bIsCMS_Directory)
			{
				Error err = CMSRepositoryErrors (__FILE__, __LINE__,
					CMSREP_CMSREPOSITORY_SANITYCHECKUNEXPECTEDFILE,
					2, (const char *) (*(_pbRepositories [rtRepositoryType])),
					(const char *) bCustomersDirectory);
				_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
					(const char *) err, __FILE__, __LINE__);

				// To Be Done: file to be removed
				// if (_bUnexpectedFilesToBeRemoved)

				continue;
			}

			if ((errOpenDir = FileIO:: openDirectory (
				(const char *) bCustomersDirectory,
				&dDeliveryDirectoryL2)) != errNoError)
			{
				_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
					(const char *) errOpenDir, __FILE__, __LINE__);

				Error err = ToolsErrors (__FILE__, __LINE__,
					TOOLS_FILEIO_OPENDIRECTORY_FAILED);
				_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
					(const char *) err, __FILE__, __LINE__);

				if (FileIO:: closeDirectory (&dDeliveryDirectoryL1) !=
					errNoError)
				{
					Error err = ToolsErrors (__FILE__, __LINE__,
						TOOLS_FILEIO_CLOSEDIRECTORY_FAILED);
					_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
						(const char *) err, __FILE__, __LINE__);
				}

				return err;
			}

			ulDirectoryLevelIndexInsideCustomer		= 0;

			if ((errSanityCheck = sanityCheck_CustomersDirectory (
				rtRepositoryType,
				(const char *) bCustomersDirectory,
				&dDeliveryDirectoryL2,
				psciSanityCheckContentsInfo,
				&lSanityCheckContentsInfoCurrentIndex,
				&ulFileIndex,
				&ulCurrentFileNumberProcessedInThisSchedule,
				&ulCurrentFilesRemovedNumberInThisSchedule,
				&ulCurrentDirectoriesRemovedNumberInThisSchedule,
				&ulDirectoryLevelIndexInsideCustomer,
				&bHasCustomerToBeResumed)) != errNoError)
			{
				Error err = CMSRepositoryErrors (__FILE__, __LINE__,
					CMSREP_CMSREPOSITORY_SANITYCHECK_CUSTOMERSDIRECTORY_FAILED);

				if ((unsigned long) errSanityCheck ==
					CMSREP_CMSREPOSITORY_REACHEDMAXNUMBERTOBEPROCESSED)
				{
					_ptSystemTracer -> trace (Tracer:: TRACER_LINFO,
						(const char *) err, __FILE__, __LINE__);
				}
				else
				{
					_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
						(const char *) err, __FILE__, __LINE__);
				}

//				if (FileIO:: closeDirectory (&dDeliveryDirectoryL2) !=
//					errNoError)
//				{
//					Error err = ToolsErrors (__FILE__, __LINE__,
//						TOOLS_FILEIO_CLOSEDIRECTORY_FAILED);
//					_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
//						(const char *) err, __FILE__, __LINE__);
//				}
//
//				if (FileIO:: closeDirectory (&dDeliveryDirectoryL1) !=
//					errNoError)
//				{
//					Error err = ToolsErrors (__FILE__, __LINE__,
//						TOOLS_FILEIO_CLOSEDIRECTORY_FAILED);
//					_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
//						(const char *) err, __FILE__, __LINE__);
//				}
//
//				return err;
			}

			if (FileIO:: closeDirectory (&dDeliveryDirectoryL2) !=
				errNoError)
			{
				Error err = ToolsErrors (__FILE__, __LINE__,
					TOOLS_FILEIO_CLOSEDIRECTORY_FAILED);
				_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
					(const char *) err, __FILE__, __LINE__);

				if (FileIO:: closeDirectory (&dDeliveryDirectoryL1) !=
					errNoError)
				{
					Error err = ToolsErrors (__FILE__, __LINE__,
						TOOLS_FILEIO_CLOSEDIRECTORY_FAILED);
					_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
						(const char *) err, __FILE__, __LINE__);
				}

				return err;
			}
		}

		if (bHasPartitionToBeResumed)
		{
			// if bHasPartitionToBeResumed is still true, it means the
			// Partition was not found and we will reset it

			strcpy (_svlpcLastProcessedContent [rtRepositoryType].
				_pPartition, "");
		}
	}
	else
	{
		ulDirectoryLevelIndexInsideCustomer		= 0;

		if ((errSanityCheck = sanityCheck_CustomersDirectory (rtRepositoryType,
			(const char *) (*(_pbRepositories [rtRepositoryType])),
			&dDeliveryDirectoryL1,
			psciSanityCheckContentsInfo,
			&lSanityCheckContentsInfoCurrentIndex,
			&ulFileIndex,
			&ulCurrentFileNumberProcessedInThisSchedule,
			&ulCurrentFilesRemovedNumberInThisSchedule,
			&ulCurrentDirectoriesRemovedNumberInThisSchedule,
			&ulDirectoryLevelIndexInsideCustomer,
			&bHasCustomerToBeResumed)) != errNoError)
		{
			Error err = CMSRepositoryErrors (__FILE__, __LINE__,
				CMSREP_CMSREPOSITORY_SANITYCHECK_CUSTOMERSDIRECTORY_FAILED);
			if ((unsigned long) errSanityCheck ==
				CMSREP_CMSREPOSITORY_REACHEDMAXNUMBERTOBEPROCESSED)
			{
				_ptSystemTracer -> trace (Tracer:: TRACER_LINFO,
					(const char *) err, __FILE__, __LINE__);
			}
			else
			{
				_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
					(const char *) err, __FILE__, __LINE__);
			}

//			if (FileIO:: closeDirectory (&dDeliveryDirectoryL1) != errNoError)
//			{
//				Error err = ToolsErrors (__FILE__, __LINE__,
//					TOOLS_FILEIO_CLOSEDIRECTORY_FAILED);
//				_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
//					(const char *) err, __FILE__, __LINE__);
//			}
//
//			return err;
		}
	}

	if (bHasCustomerToBeResumed)
	{
		// if bHasCustomerToBeResumed is still true, it means the
		// CustomerDirectoryName was not found and we will reset it

		strcpy (_svlpcLastProcessedContent [rtRepositoryType].
			_pCustomerDirectoryName, "");

		strcpy (_svlpcLastProcessedContent [rtRepositoryType]. _pPartition, "");

		_svlpcLastProcessedContent [rtRepositoryType].
			_ulFilesNumberAlreadyProcessed					= 0;
	}

	if (FileIO:: closeDirectory (&dDeliveryDirectoryL1) != errNoError)
	{
		Error err = ToolsErrors (__FILE__, __LINE__,
			TOOLS_FILEIO_CLOSEDIRECTORY_FAILED);
		_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
			(const char *) err, __FILE__, __LINE__);

		return err;
	}

	if (lSanityCheckContentsInfoCurrentIndex >= 0)
	{
		if (sanityCheck_runOnContentsInfo (
			psciSanityCheckContentsInfo,
			lSanityCheckContentsInfoCurrentIndex,
			rtRepositoryType,
			&ulCurrentFilesRemovedNumberInThisSchedule,
			&ulCurrentDirectoriesRemovedNumberInThisSchedule) != errNoError)
		{
			Error err = CMSRepositoryErrors (__FILE__, __LINE__,
				CMSREP_CMSREPOSITORY_SANITYCHECK_RUNONCONTENTSINFO_FAILED);
			_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
				(const char *) err, __FILE__, __LINE__);

			// return err;
		}

		lSanityCheckContentsInfoCurrentIndex		= -1;
	}

	{
		Message msg = CMSRepositoryMessages (__FILE__, __LINE__,
			CMSREP_CMSREPOSITORY_ENDSANITYCHECKONREPOSITORY,
			7, (const char *) (*(_pbRepositories [rtRepositoryType])),
			(unsigned long) (time (NULL) - tSanityCheckStart),
			_svlpcLastProcessedContent [rtRepositoryType]. _pPartition,
			_svlpcLastProcessedContent [rtRepositoryType]. _pCustomerDirectoryName,
			_svlpcLastProcessedContent [rtRepositoryType].
				_ulFilesNumberAlreadyProcessed,
			ulCurrentFilesRemovedNumberInThisSchedule,
			ulCurrentDirectoriesRemovedNumberInThisSchedule);
		_ptSystemTracer -> trace (Tracer:: TRACER_LINFO,
			(const char *) msg, __FILE__, __LINE__);
	}

//	_svlpcLastProcessedContent [rtRepositoryType].
//		_ulFilesNumberAlreadyProcessed					+= 
//		ulCurrentFileNumberProcessedInThisSchedule;
//	
//	if (ulCurrentFileNumberProcessedInThisSchedule <
//		_ulMaxFilesToBeProcessedPerSchedule [rtRepositoryType])
//	{
//		// all the files were processed
//
//		_svlpcLastProcessedContent [rtRepositoryType].
//			_ulFilesNumberAlreadyProcessed					= 0;
//	}


	return errNoError;
}


Error CMSRepository:: sanityCheck_CustomersDirectory (
	RepositoryType_t rtRepositoryType,
	const char *pCustomersDirectory,
	FileIO:: Directory_p pdDeliveryDirectory,
	SanityCheckContentInfo_p psciSanityCheckContentsInfo,
	long *plSanityCheckContentsInfoCurrentIndex,
	unsigned long *pulFileIndex,
	unsigned long *pulCurrentFileNumberProcessedInThisSchedule,
	unsigned long *pulCurrentFilesRemovedNumberInThisSchedule,
	unsigned long *pulCurrentDirectoriesRemovedNumberInThisSchedule,
	unsigned long *pulDirectoryLevelIndexInsideCustomer,
	Boolean_p pbHasCustomerToBeResumed)

{

	Error_t							errReadDirectory;
	FileIO:: DirectoryEntryType_t	detDirectoryEntryType;
	Buffer_t						bCustomerDirectoryName;
	Buffer_t						bCustomerDirectory;
	Error_t							errSanityCheck;


	{
		Message msg = CMSRepositoryMessages (__FILE__, __LINE__,
			CMSREP_CMSREPOSITORY_SANITYCHECKONCUSTOMERSDIRECTORY,
			1, pCustomersDirectory);
		_ptSystemTracer -> trace (Tracer:: TRACER_LINFO,
			(const char *) msg, __FILE__, __LINE__);
	}

	if (bCustomerDirectoryName. init () != errNoError)
	{
		Error err = ToolsErrors (__FILE__, __LINE__,
			TOOLS_BUFFER_INIT_FAILED);
		_ptSystemTracer -> trace (Tracer:: TRACER_LERRR, (const char *) err,
			__FILE__, __LINE__);

		return err;
	}

	if (bCustomerDirectory. init () != errNoError)
	{
		Error err = ToolsErrors (__FILE__, __LINE__,
			TOOLS_BUFFER_INIT_FAILED);
		_ptSystemTracer -> trace (Tracer:: TRACER_LERRR, (const char *) err,
			__FILE__, __LINE__);

		return err;
	}

	while ((errReadDirectory = FileIO:: readDirectory (pdDeliveryDirectory,
		&bCustomerDirectoryName, &detDirectoryEntryType)) == errNoError)
	{
		if (*pbHasCustomerToBeResumed &&
			strcmp (_svlpcLastProcessedContent [rtRepositoryType].
				_pCustomerDirectoryName, "") &&
			strcmp (_svlpcLastProcessedContent [rtRepositoryType].
				_pCustomerDirectoryName, (const char *) bCustomerDirectoryName))
		{
			continue;
		}

		if ((rtRepositoryType == CMSREP_REPOSITORYTYPE_DOWNLOAD &&
				!strcmp ((const char *) bCustomerDirectoryName,
				_pDownloadReservedDirectoryName)) ||
			(rtRepositoryType == CMSREP_REPOSITORYTYPE_DOWNLOAD &&
				!strcmp ((const char *) bCustomerDirectoryName,
				_pDownloadFreeDirectoryName)) ||
			(rtRepositoryType == CMSREP_REPOSITORYTYPE_DOWNLOAD &&
				!strcmp ((const char *) bCustomerDirectoryName,
				_pDownloadiPhoneLiveDirectoryName)) ||
			(rtRepositoryType == CMSREP_REPOSITORYTYPE_DOWNLOAD &&
				!strcmp ((const char *) bCustomerDirectoryName,
				_pDownloadSilverlightLiveDirectoryName)) ||
			(rtRepositoryType == CMSREP_REPOSITORYTYPE_DOWNLOAD &&
				!strcmp ((const char *) bCustomerDirectoryName,
				_pDownloadAdobeLiveDirectoryName)) ||
			(rtRepositoryType == CMSREP_REPOSITORYTYPE_STREAMING &&
				!strcmp ((const char *) bCustomerDirectoryName,
				_pStreamingFreeDirectoryName)) ||
			(rtRepositoryType == CMSREP_REPOSITORYTYPE_STREAMING &&
				!strcmp ((const char *) bCustomerDirectoryName,
				_pStreamingMetaDirectoryName)) ||
			(rtRepositoryType == CMSREP_REPOSITORYTYPE_STREAMING &&
				!strcmp ((const char *) bCustomerDirectoryName,
				_pStreamingRecordingDirectoryName)))
			continue;

		#ifdef WIN32
		#else
			if (!strcmp ((const char *) bCustomerDirectoryName, "lost+found"))
				continue;
		#endif

		if (!strcmp ((const char *) bCustomerDirectoryName, ".snapshot"))
			continue;

		if (*pbHasCustomerToBeResumed == false)
		{
			_svlpcLastProcessedContent [rtRepositoryType].
				_ulFilesNumberAlreadyProcessed					= 0;
		}

		*pbHasCustomerToBeResumed			= false;

		if ((unsigned long) bCustomerDirectoryName >=
			CMSREP_CMSREPOSITORY_MAXCUSTOMERNAMELENGTH)
		{
			Error err = CMSRepositoryErrors (__FILE__, __LINE__,
				CMSREP_CMSREPOSITORY_BUFFERNOTENOUGHBIG,
				2,
				(unsigned long)
					CMSREP_CMSREPOSITORY_MAXCUSTOMERNAMELENGTH,
				(unsigned long) bCustomerDirectoryName);
			_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
					(const char *) err, __FILE__, __LINE__);

			return err;
		}
		strcpy (_svlpcLastProcessedContent [rtRepositoryType].
			_pCustomerDirectoryName, (const char *) bCustomerDirectoryName);

		if (detDirectoryEntryType == FileIO:: TOOLS_FILEIO_REGULARFILE ||
			detDirectoryEntryType == FileIO:: TOOLS_FILEIO_LINKFILE ||
			detDirectoryEntryType == FileIO:: TOOLS_FILEIO_UNKNOWN)
		{
			Error err = CMSRepositoryErrors (__FILE__, __LINE__,
				CMSREP_CMSREPOSITORY_SANITYCHECKUNEXPECTEDFILE,
				2, pCustomersDirectory, (const char *) bCustomerDirectoryName);
			_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
				(const char *) err, __FILE__, __LINE__);

			// To Be Done: file to be removed if (_bUnexpectedFilesToBeRemoved)

			continue;
		}

		// we found a Customer directory
		#ifdef WIN32
			if (bCustomerDirectory. setBuffer (pCustomersDirectory) !=
					errNoError ||
				bCustomerDirectory. append (bCustomerDirectoryName. str()) !=
					errNoError ||
				bCustomerDirectory. append ("\\") != errNoError)
		#else
			if (bCustomerDirectory. setBuffer (pCustomersDirectory) !=
					errNoError ||
				bCustomerDirectory. append (bCustomerDirectoryName. str()) !=
					errNoError ||
				bCustomerDirectory. append ("/") != errNoError)
		#endif
		{
			Error err = ToolsErrors (__FILE__, __LINE__,
				TOOLS_BUFFER_INSERTAT_FAILED);
			_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
				(const char *) err, __FILE__, __LINE__);

			return err;
		}

		*pulDirectoryLevelIndexInsideCustomer			+= 1;

		if ((errSanityCheck = sanityCheck_ContentsDirectory (
			(const char *) bCustomerDirectoryName,
			(const char *) bCustomerDirectory,
			((unsigned long) bCustomerDirectory) - 1,
			rtRepositoryType,
			psciSanityCheckContentsInfo,
			plSanityCheckContentsInfoCurrentIndex,
			pulFileIndex,
			pulCurrentFileNumberProcessedInThisSchedule,
			pulCurrentFilesRemovedNumberInThisSchedule,
			pulCurrentDirectoriesRemovedNumberInThisSchedule,
			pulDirectoryLevelIndexInsideCustomer)) != errNoError)
		{
			if ((unsigned long) errSanityCheck ==
				CMSREP_CMSREPOSITORY_REACHEDMAXNUMBERTOBEPROCESSED)
			{
				Error err = CMSRepositoryErrors (__FILE__, __LINE__,
					CMSREP_CMSREPOSITORY_SANITYCHECK_CONTENTSDIRECTORY_FAILED);
				// _ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
				// 	(const char *) err, __FILE__, __LINE__);

				*pulDirectoryLevelIndexInsideCustomer			-= 1;

				return errSanityCheck;
			}
			else
			{
				Error err = CMSRepositoryErrors (__FILE__, __LINE__,
					CMSREP_CMSREPOSITORY_SANITYCHECK_CONTENTSDIRECTORY_FAILED);
				_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
					(const char *) err, __FILE__, __LINE__);

				*pulDirectoryLevelIndexInsideCustomer			-= 1;

				return err;
			}
		}

		*pulDirectoryLevelIndexInsideCustomer			-= 1;
	}

	// the CMSREP_CMSREPOSITORY_REACHEDMAXNUMBERTOBEPROCESSED was not received,
	// so the last customer saved is reset to start
	// from the beginning next time
	strcpy (_svlpcLastProcessedContent [rtRepositoryType]. _pCustomerDirectoryName, "");
	strcpy (_svlpcLastProcessedContent [rtRepositoryType]. _pPartition, "");
	_svlpcLastProcessedContent [rtRepositoryType].
		_ulFilesNumberAlreadyProcessed					= 0;


	return errNoError;
}


Error CMSRepository:: sanityCheck_ContentsDirectory (
	const char *pCustomerDirectoryName, const char *pContentsDirectory,
	unsigned long ulRelativePathIndex,
	RepositoryType_t rtRepositoryType,
	SanityCheckContentInfo_p psciSanityCheckContentsInfo,
	long *plSanityCheckContentsInfoCurrentIndex,
	unsigned long *pulFileIndex,
	unsigned long *pulCurrentFileNumberProcessedInThisSchedule,
	unsigned long *pulCurrentFilesRemovedNumberInThisSchedule,
	unsigned long *pulCurrentDirectoriesRemovedNumberInThisSchedule,
	unsigned long *pulDirectoryLevelIndexInsideCustomer)
	// pContentsDirectory finishes with / or \\

{

	FileIO:: Directory_t			dContentsDirectory;
	Error_t							errOpenDir;
	Error_t							errReadDirectory;
	FileIO:: DirectoryEntryType_t	detDirectoryEntryType;
	Buffer_t						bContentPathName;
	Error_t							errFileIO;
	Error_t							errSanityCheck;



	// pulDirectoryLevelIndexInsideCustomer:
	// In the CMSRepository:
	// 	1 means customer name
	// 	...
	// 	4 means the .3gp or a directory in case of IPhone

	{
		Message msg = CMSRepositoryMessages (__FILE__, __LINE__,
			CMSREP_CMSREPOSITORY_SANITYCHECKONDIRECTORY,
			6, pCustomerDirectoryName, pContentsDirectory,
			ulRelativePathIndex, *pulDirectoryLevelIndexInsideCustomer,
			*pulFileIndex, _svlpcLastProcessedContent [rtRepositoryType].
			_ulFilesNumberAlreadyProcessed);
		_ptSystemTracer -> trace (Tracer:: TRACER_LINFO,
			(const char *) msg, __FILE__, __LINE__);
	}

//	if (*pulCurrentFileNumberProcessedInThisSchedule >=
//		_ulMaxFilesToBeProcessedPerSchedule [rtRepositoryType])
//		return errNoError;

	if (bContentPathName. init () != errNoError)
	{
		Error err = ToolsErrors (__FILE__, __LINE__,
			TOOLS_BUFFER_INIT_FAILED);
		_ptSystemTracer -> trace (Tracer:: TRACER_LERRR, (const char *) err,
			__FILE__, __LINE__);

		return err;
	}

	if ((errOpenDir = FileIO:: openDirectory (
		pContentsDirectory, &dContentsDirectory)) != errNoError)
	{
		_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
			(const char *) errOpenDir, __FILE__, __LINE__);

		Error err = ToolsErrors (__FILE__, __LINE__,
			TOOLS_FILEIO_OPENDIRECTORY_FAILED);
		_ptSystemTracer -> trace (Tracer:: TRACER_LERRR, (const char *) err,
			__FILE__, __LINE__);

		return err;
	}

	while ((errReadDirectory = FileIO:: readDirectory (&dContentsDirectory,
		&bContentPathName, &detDirectoryEntryType)) == errNoError)
	{
		(*pulFileIndex)			+= 1;

		if (detDirectoryEntryType == FileIO:: TOOLS_FILEIO_UNKNOWN)
		{
			Error err = CMSRepositoryErrors (__FILE__, __LINE__,
				CMSREP_CMSREPOSITORY_SANITYCHECKUNEXPECTEDFILE,
				2, pContentsDirectory, (const char *) bContentPathName);
			_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
				(const char *) err, __FILE__, __LINE__);

			if (_bUnexpectedFilesToBeRemoved)
			{
				if (bContentPathName. insertAt (0,
					pContentsDirectory) != errNoError)
				{
					Error err = ToolsErrors (__FILE__, __LINE__,
						TOOLS_BUFFER_INSERTAT_FAILED);
					_ptSystemTracer -> trace (
						Tracer:: TRACER_LERRR,
						(const char *) err, __FILE__, __LINE__);

					if (FileIO:: closeDirectory (
						&dContentsDirectory) != errNoError)
					{
						Error err = ToolsErrors (
							__FILE__, __LINE__,
							TOOLS_FILEIO_CLOSEDIRECTORY_FAILED);
						_ptSystemTracer -> trace (
							Tracer:: TRACER_LERRR,
							(const char *) err,
							__FILE__, __LINE__);
					}

					return err;
				}

				(*pulCurrentFilesRemovedNumberInThisSchedule)		+= 1;

				{
					Message msg = CMSRepositoryMessages (
						__FILE__, __LINE__,
						CMSREP_CMSREPOSITORY_REMOVEFILE,
						2, pCustomerDirectoryName, bContentPathName. str());
					_ptSystemTracer -> trace (
						Tracer:: TRACER_LINFO,
						(const char *) msg, __FILE__, __LINE__);
				}

				if ((errFileIO = FileIO:: remove (
					(const char *) bContentPathName)) !=
					errNoError)
				{
					_ptSystemTracer -> trace (
						Tracer:: TRACER_LERRR,
						(const char *) errFileIO,
						__FILE__, __LINE__);

					Error err = ToolsErrors (__FILE__, __LINE__,
						TOOLS_FILEIO_REMOVE_FAILED,
						1,
						(const char *) bContentPathName);
					_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
						(const char *) err, __FILE__, __LINE__);

					if (FileIO:: closeDirectory (
						&dContentsDirectory) != errNoError)
					{
						Error err = ToolsErrors (
							__FILE__, __LINE__,
							TOOLS_FILEIO_CLOSEDIRECTORY_FAILED);
						_ptSystemTracer -> trace (
							Tracer:: TRACER_LERRR,
							(const char *) err,
							__FILE__, __LINE__);
					}

					return err;
				}
			}

			continue;

//			if (FileIO:: closeDirectory (&dContentsDirectory) != errNoError)
//			{
//				Error err = ToolsErrors (__FILE__, __LINE__,
//					TOOLS_FILEIO_CLOSEDIRECTORY_FAILED);
//				_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
//					(const char *) err, __FILE__, __LINE__);
//			}
//
//			return err;
		}
		else if (rtRepositoryType == CMSREP_REPOSITORYTYPE_STAGING ||
			rtRepositoryType == CMSREP_REPOSITORYTYPE_ERRORS ||
			rtRepositoryType == CMSREP_REPOSITORYTYPE_DONE)
		{
			// in this case, under <staging/error/done>/<customer name>
			// we should have directory like having the format <YYYY_MM_DD>
			// We will not go inside and we will remove the entire directory
			// if too old

			time_t							tLastModificationTime;
			FileIO:: DirectoryEntryType_t	detSourceFileType;



			if (bContentPathName. insertAt (0,
				pContentsDirectory) != errNoError)
			{
				Error err = ToolsErrors (__FILE__, __LINE__,
					TOOLS_BUFFER_INSERTAT_FAILED);
				_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
					(const char *) err, __FILE__, __LINE__);

				if (FileIO:: closeDirectory (&dContentsDirectory) !=
					errNoError)
				{
					Error err = ToolsErrors (__FILE__, __LINE__,
						TOOLS_FILEIO_CLOSEDIRECTORY_FAILED);
					_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
						(const char *) err, __FILE__, __LINE__);
				}

				return err;
			}

			if ((errFileIO = FileIO:: getDirectoryEntryType (
				bContentPathName. str(), &detSourceFileType)) !=
				errNoError)
			{
				_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
					(const char *) errFileIO, __FILE__, __LINE__);

				Error err = ToolsErrors (__FILE__, __LINE__,
					TOOLS_FILEIO_GETDIRECTORYENTRYTYPE_FAILED,
					1, (const char *) bContentPathName);
				_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
					(const char *) err, __FILE__, __LINE__);

				if (FileIO:: closeDirectory (&dContentsDirectory) !=
					errNoError)
				{
					Error err = ToolsErrors (__FILE__, __LINE__,
						TOOLS_FILEIO_CLOSEDIRECTORY_FAILED);
					_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
						(const char *) err, __FILE__, __LINE__);
				}

				return err;
			}

			if (detSourceFileType != FileIO:: TOOLS_FILEIO_DIRECTORY &&
				detSourceFileType != FileIO:: TOOLS_FILEIO_REGULARFILE)
			{
				Error err = CMSRepositoryErrors (__FILE__, __LINE__,
					CMSREP_CMSREPOSITORY_WRONGDIRECTORYENTRYTYPE,
					2, (long) detSourceFileType, bContentPathName. str());
				_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
					(const char *) err, __FILE__, __LINE__);

				if (FileIO:: closeDirectory (&dContentsDirectory) !=
					errNoError)
				{
					Error err = ToolsErrors (__FILE__, __LINE__,
						TOOLS_FILEIO_CLOSEDIRECTORY_FAILED);
					_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
						(const char *) err, __FILE__, __LINE__);
				}

				return err;
			}

			if (detSourceFileType == FileIO:: TOOLS_FILEIO_DIRECTORY)
			{
				char						pRetentionDate [
					CMSREP_CMSREPOSITORY_MAXDATETIMELENGTH];
				const char					*pDirectoryName;
				tm							tmDateTime;
				unsigned long				ulMilliSecs;
				unsigned long				ulRetentionYear;
				unsigned long				ulRetentionMonth;
				unsigned long				ulRetentionDay;
				unsigned long				ulRetentionHour;
				unsigned long				ulRetentionMinutes;
				unsigned long				ulRetentionSeconds;
				Boolean_t					bRetentionDaylightSavingTime;


				#ifdef WIN32
					if ((pDirectoryName = strrchr (bContentPathName. str(),
						'\\')) == (const char *) NULL)
				#else
					if ((pDirectoryName = strrchr (bContentPathName. str(),
						'/')) == (const char *) NULL)
				#endif
				{
					pDirectoryName		= bContentPathName. str();
				}
				else
				{
					pDirectoryName++;
				}

				if (!isdigit(pDirectoryName [0]) ||
					!isdigit(pDirectoryName [1]) ||
					!isdigit(pDirectoryName [2]) ||
					!isdigit(pDirectoryName [3]) ||
					pDirectoryName [4] != '_' ||
					!isdigit(pDirectoryName [5]) ||
					!isdigit(pDirectoryName [6]) ||
					pDirectoryName [7] != '_' ||
					!isdigit(pDirectoryName [8]) ||
					!isdigit(pDirectoryName [9]) ||
					pDirectoryName [10] != '\0')
				{
					// we expect a directory having a format YYYY_MM_DD

					Error err = CMSRepositoryErrors (__FILE__, __LINE__,
						CMSREP_CMSREPOSITORY_SANITYCHECKUNEXPECTEDFILE,
						2, pContentsDirectory, pDirectoryName);
					_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
						(const char *) err, __FILE__, __LINE__);

					if (_bUnexpectedFilesToBeRemoved)
					{
						(*pulCurrentDirectoriesRemovedNumberInThisSchedule)	+=
							1;

						{
							Message msg = CMSRepositoryMessages (
								__FILE__, __LINE__,
								CMSREP_CMSREPOSITORY_REMOVEDIRECTORY,
								2, pContentsDirectory, pDirectoryName);
							_ptSystemTracer -> trace (Tracer:: TRACER_LINFO,
								(const char *) msg, __FILE__, __LINE__);
						}

						if ((errFileIO = FileIO:: removeDirectory (
							(const char *) bContentPathName, true)) !=
							errNoError)
						{
							_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
								(const char *) errFileIO, __FILE__, __LINE__);

							Error err = ToolsErrors (__FILE__, __LINE__,
								TOOLS_FILEIO_REMOVEDIRECTORY_FAILED,
								1, bContentPathName. str ());
							_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
								(const char *) err, __FILE__, __LINE__);
						}
					}

					continue;
				}

				if (DateTime:: get_tm_LocalTime (&tmDateTime, &ulMilliSecs) !=
					errNoError)
				{
					Error err = ToolsErrors (__FILE__, __LINE__,
						TOOLS_DATETIME_GET_TM_LOCALTIME_FAILED);
					_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
						(const char *) err, __FILE__, __LINE__);

					if (FileIO:: closeDirectory (&dContentsDirectory) !=
						errNoError)
					{
						Error err = ToolsErrors (__FILE__, __LINE__,
							TOOLS_FILEIO_CLOSEDIRECTORY_FAILED);
						_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
							(const char *) err, __FILE__, __LINE__);
					}

					return err;
				}

				if (DateTime:: addSeconds (
					tmDateTime. tm_year + 1900,
					tmDateTime. tm_mon + 1,
					tmDateTime. tm_mday,
					tmDateTime. tm_hour,
					tmDateTime. tm_min,
					tmDateTime. tm_sec,
					tmDateTime. tm_isdst,
					_ulRetentionPeriodInSecondsForTemporaryFiles * -1,
					&ulRetentionYear,
					&ulRetentionMonth,
					&ulRetentionDay,
					&ulRetentionHour,
					&ulRetentionMinutes,
					&ulRetentionSeconds,
					&bRetentionDaylightSavingTime
					) != errNoError)
				{
					Error err = ToolsErrors (__FILE__, __LINE__,
						TOOLS_DATETIME_GET_TM_LOCALTIME_FAILED);
					_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
						(const char *) err, __FILE__, __LINE__);

					if (FileIO:: closeDirectory (&dContentsDirectory) !=
						errNoError)
					{
						Error err = ToolsErrors (__FILE__, __LINE__,
							TOOLS_FILEIO_CLOSEDIRECTORY_FAILED);
						_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
							(const char *) err, __FILE__, __LINE__);
					}

					return err;
				}

				sprintf (pRetentionDate, "%04lu_%02lu_%02lu",
					ulRetentionYear, ulRetentionMonth, ulRetentionDay);

				if (strcmp (pDirectoryName, pRetentionDate) < 0)
				{
					// remove directory
					if (_bUnexpectedFilesToBeRemoved)
					{
						(*pulCurrentDirectoriesRemovedNumberInThisSchedule)	+=
							1;

						{
							Message msg = CMSRepositoryMessages (
								__FILE__, __LINE__,
								CMSREP_CMSREPOSITORY_REMOVEDIRECTORY,
								2, pCustomerDirectoryName,
								bContentPathName. str());
							_ptSystemTracer -> trace (Tracer:: TRACER_LINFO,
								(const char *) msg, __FILE__, __LINE__);
						}

						if ((errFileIO = FileIO:: removeDirectory (
							bContentPathName. str(), true)) !=
							errNoError)
						{
							_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
								(const char *) errFileIO, __FILE__, __LINE__);

							Error err = ToolsErrors (__FILE__, __LINE__,
								TOOLS_FILEIO_REMOVEDIRECTORY_FAILED,
								1, bContentPathName. str ());
							_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
								(const char *) err, __FILE__, __LINE__);

							if (FileIO:: closeDirectory (
								&dContentsDirectory) != errNoError)
							{
								Error err = ToolsErrors (
									__FILE__, __LINE__,
									TOOLS_FILEIO_CLOSEDIRECTORY_FAILED);
								_ptSystemTracer -> trace (
									Tracer:: TRACER_LERRR,
									(const char *) err,
									__FILE__, __LINE__);
							}

							return err;
						}
					}
				}
			}
			else // if (detSourceFileType ==
				// FileIO:: TOOLS_FILEIO_REGULARFILE)
			{
				if ((errFileIO = FileIO:: getFileTime (
					(const char *) bContentPathName,
					&tLastModificationTime)) != errNoError)
				{
					_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
						(const char *) errFileIO, __FILE__, __LINE__);

					Error err = ToolsErrors (__FILE__, __LINE__,
						TOOLS_FILEIO_GETFILETIME_FAILED,
						1, (const char *) bContentPathName);
					_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
						(const char *) err, __FILE__, __LINE__);

					if (FileIO:: closeDirectory (&dContentsDirectory) !=
						errNoError)
					{
						Error err = ToolsErrors (__FILE__, __LINE__,
							TOOLS_FILEIO_CLOSEDIRECTORY_FAILED);
						_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
							(const char *) err, __FILE__, __LINE__);
					}

					return err;
				}

				if (((unsigned long) (time (NULL) -
					tLastModificationTime)) >=
					_ulRetentionPeriodInSecondsForTemporaryFiles)
				{
//					Error err = CMSRepositoryErrors (__FILE__, __LINE__,
//					CMSREP_CMSENGINEPROCESSOR_SANITYCHECKFILETOOOLDTOBEREMOVED,
//						1, (const char *) bContentPathName);
//					_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
//						(const char *) err, __FILE__, __LINE__);

					if (_bUnexpectedFilesToBeRemoved)
					{
						(*pulCurrentFilesRemovedNumberInThisSchedule)	+= 1;

						{
							Message msg = CMSRepositoryMessages (
								__FILE__, __LINE__,
								CMSREP_CMSREPOSITORY_REMOVEFILE,
								2, pCustomerDirectoryName,
								bContentPathName. str());
							_ptSystemTracer -> trace (Tracer:: TRACER_LINFO,
								(const char *) msg, __FILE__, __LINE__);
						}

						if ((errFileIO = FileIO:: remove (
							bContentPathName. str())) != errNoError)
						{
							_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
								(const char *) errFileIO, __FILE__, __LINE__);

							Error err = ToolsErrors (__FILE__, __LINE__,
								TOOLS_FILEIO_REMOVE_FAILED,
								1, (const char *) bContentPathName);
							_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
								(const char *) err, __FILE__, __LINE__);

							if (FileIO:: closeDirectory (&dContentsDirectory) !=
								errNoError)
							{
								Error err = ToolsErrors (__FILE__, __LINE__,
									TOOLS_FILEIO_CLOSEDIRECTORY_FAILED);
								_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
									(const char *) err, __FILE__, __LINE__);
							}

							return err;
						}
					}
				}
			}
		}
		else if (rtRepositoryType == CMSREP_REPOSITORYTYPE_FTP)
		{
			{
				Boolean_t				bIsIngestionLogFile;


				if (!strcmp ((const char *) bContentPathName, "Ingestion.log"))
				{
					bIsIngestionLogFile			= true;
				}
				else
				{
					bIsIngestionLogFile			= false;
				}

				if (bContentPathName. insertAt (0,
					pContentsDirectory) != errNoError)
				{
					Error err = ToolsErrors (__FILE__, __LINE__,
						TOOLS_BUFFER_INSERTAT_FAILED);
					_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
						(const char *) err, __FILE__, __LINE__);

					if (FileIO:: closeDirectory (&dContentsDirectory) !=
						errNoError)
					{
						Error err = ToolsErrors (__FILE__, __LINE__,
							TOOLS_FILEIO_CLOSEDIRECTORY_FAILED);
						_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
							(const char *) err, __FILE__, __LINE__);
					}

					return err;
				}

				if (!bIsIngestionLogFile)
				{
					time_t							tLastModificationTime;
					FileIO:: DirectoryEntryType_t	detSourceFileType;


					if ((errFileIO = FileIO:: getDirectoryEntryType (
						(const char *) bContentPathName, &detSourceFileType)) !=
						errNoError)
					{
						_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
							(const char *) errFileIO, __FILE__, __LINE__);

						Error err = ToolsErrors (__FILE__, __LINE__,
							TOOLS_FILEIO_GETDIRECTORYENTRYTYPE_FAILED,
							1, (const char *) bContentPathName);
						_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
							(const char *) err, __FILE__, __LINE__);

						if (FileIO:: closeDirectory (&dContentsDirectory) !=
							errNoError)
						{
							Error err = ToolsErrors (__FILE__, __LINE__,
								TOOLS_FILEIO_CLOSEDIRECTORY_FAILED);
							_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
								(const char *) err, __FILE__, __LINE__);
						}

						return err;
					}

					if ((errFileIO = FileIO:: getFileTime (
						(const char *) bContentPathName,
						&tLastModificationTime)) != errNoError)
					{
						_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
							(const char *) errFileIO,
							__FILE__, __LINE__);

						Error err = ToolsErrors (__FILE__, __LINE__,
							TOOLS_FILEIO_GETFILETIME_FAILED,
							1,
							(const char *) bContentPathName);
						_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
							(const char *) err, __FILE__, __LINE__);

						if (FileIO:: closeDirectory (&dContentsDirectory) !=
							errNoError)
						{
							Error err = ToolsErrors (__FILE__, __LINE__,
								TOOLS_FILEIO_CLOSEDIRECTORY_FAILED);
							_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
								(const char *) err, __FILE__, __LINE__);
						}

						return err;
					}

					if (((unsigned long) (time (NULL) -
						tLastModificationTime)) >=
						_ulRetentionPeriodInSecondsForTemporaryFiles)
					{
//						Error err = CMSRepositoryErrors (__FILE__, __LINE__,
//						CMSREP_CMSENGINEPROCESSOR_SANITYCHECKFILETOOOLDTOBEREMOVED,
//							1, (const char *) bContentPathName);
//						_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
//							(const char *) err, __FILE__, __LINE__);

						if (_bUnexpectedFilesToBeRemoved)
						{
							if (detSourceFileType ==
								FileIO:: TOOLS_FILEIO_DIRECTORY)
							{
							(*pulCurrentDirectoriesRemovedNumberInThisSchedule)
									+= 1;

								{
									Message msg = CMSRepositoryMessages (
										__FILE__, __LINE__,
										CMSREP_CMSREPOSITORY_REMOVEDIRECTORY,
										2,
										pCustomerDirectoryName,
										(const char *) bContentPathName);
									_ptSystemTracer -> trace (
										Tracer:: TRACER_LINFO,
										(const char *) msg, __FILE__, __LINE__);
								}

								if ((errFileIO = FileIO:: removeDirectory (
									(const char *) bContentPathName, true)) !=
									errNoError)
								{
									_ptSystemTracer -> trace (
										Tracer:: TRACER_LERRR,
										(const char *) errFileIO,
										__FILE__, __LINE__);

									Error err = ToolsErrors (__FILE__, __LINE__,
										TOOLS_FILEIO_REMOVEDIRECTORY_FAILED,
										1,
										(const char *) bContentPathName);
									_ptSystemTracer -> trace (
										Tracer:: TRACER_LERRR,
										(const char *) err, __FILE__, __LINE__);

									if (FileIO:: closeDirectory (
										&dContentsDirectory) != errNoError)
									{
										Error err = ToolsErrors (
											__FILE__, __LINE__,
											TOOLS_FILEIO_CLOSEDIRECTORY_FAILED);
										_ptSystemTracer -> trace (
											Tracer:: TRACER_LERRR,
											(const char *) err,
											__FILE__, __LINE__);
									}

									return err;
								}
							}
							else // if (detSourceFileType ==
								// FileIO:: TOOLS_FILEIO_REGULARFILE)
							{
							(*pulCurrentFilesRemovedNumberInThisSchedule)
									+= 1;

								{
									Message msg = CMSRepositoryMessages (
										__FILE__, __LINE__,
										CMSREP_CMSREPOSITORY_REMOVEFILE,
										2,
										pCustomerDirectoryName,
										(const char *) bContentPathName);
									_ptSystemTracer -> trace (
										Tracer:: TRACER_LINFO,
										(const char *) msg, __FILE__, __LINE__);
								}

								if ((errFileIO = FileIO:: remove (
									(const char *) bContentPathName)) !=
									errNoError)
								{
									_ptSystemTracer -> trace (
										Tracer:: TRACER_LERRR,
										(const char *) errFileIO,
										__FILE__, __LINE__);

									Error err = ToolsErrors (__FILE__, __LINE__,
										TOOLS_FILEIO_REMOVE_FAILED,
										1,
										(const char *) bContentPathName);
									_ptSystemTracer -> trace (
										Tracer:: TRACER_LERRR,
										(const char *) err, __FILE__, __LINE__);

									if (FileIO:: closeDirectory (
										&dContentsDirectory) != errNoError)
									{
										Error err = ToolsErrors (
											__FILE__, __LINE__,
											TOOLS_FILEIO_CLOSEDIRECTORY_FAILED);
										_ptSystemTracer -> trace (
											Tracer:: TRACER_LERRR,
											(const char *) err,
											__FILE__, __LINE__);
									}

									return err;
								}
							}
						}
					}
				}
				else
				{
					// It is the Ingestion.log file.
					// It will be removed only if too big
					unsigned long				ulFileSizeInBytes;


					if ((errFileIO = FileIO:: getFileSizeInBytes (
						(const char *) bContentPathName,
						&ulFileSizeInBytes, false)) != errNoError)
					{
						_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
							(const char *) errFileIO,
							__FILE__, __LINE__);

						Error err = ToolsErrors (__FILE__, __LINE__,
							TOOLS_FILEIO_GETFILESIZEINBYTES_FAILED,
							1,
							(const char *) bContentPathName);
						_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
							(const char *) err, __FILE__, __LINE__);

						if (FileIO:: closeDirectory (&dContentsDirectory) !=
							errNoError)
						{
							Error err = ToolsErrors (__FILE__, __LINE__,
								TOOLS_FILEIO_CLOSEDIRECTORY_FAILED);
							_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
								(const char *) err, __FILE__, __LINE__);
						}

						return err;
					}

					// the below check is in KB.
					// Remove if it is too big (10MB)
					if (ulFileSizeInBytes / 1024 >= 10 * 1024)
					{
						Error err = CMSRepositoryErrors (__FILE__, __LINE__,
						CMSREP_CMSREPOSITORY_SANITYCHECKFILETOOBIGTOBEREMOVED,
							2, (unsigned long) (ulFileSizeInBytes / 1024),
							(const char *) bContentPathName);
						_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
							(const char *) err, __FILE__, __LINE__);

						if (_bUnexpectedFilesToBeRemoved)
						{
							(*pulCurrentFilesRemovedNumberInThisSchedule)
								+= 1;

							{
								Message msg = CMSRepositoryMessages (
									__FILE__, __LINE__,
									CMSREP_CMSREPOSITORY_REMOVEFILE,
									2,
									pCustomerDirectoryName,
									(const char *) bContentPathName);
								_ptSystemTracer -> trace (Tracer:: TRACER_LINFO,
									(const char *) msg, __FILE__, __LINE__);
							}

							if ((errFileIO = FileIO:: remove (
								(const char *) bContentPathName)) !=
								errNoError)
							{
								_ptSystemTracer -> trace (
									Tracer:: TRACER_LERRR,
									(const char *) errFileIO,
									__FILE__, __LINE__);

								Error err = ToolsErrors (__FILE__, __LINE__,
									TOOLS_FILEIO_REMOVE_FAILED,
									1,
									(const char *) bContentPathName);
								_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
									(const char *) err, __FILE__, __LINE__);

								if (FileIO:: closeDirectory (
									&dContentsDirectory) != errNoError)
								{
									Error err = ToolsErrors (__FILE__, __LINE__,
										TOOLS_FILEIO_CLOSEDIRECTORY_FAILED);
									_ptSystemTracer -> trace (
										Tracer:: TRACER_LERRR,
										(const char *) err, __FILE__, __LINE__);
								}

								return err;
							}
						}
					}
				}
			}
		}
		else if (rtRepositoryType == CMSREP_REPOSITORYTYPE_DOWNLOAD ||
			rtRepositoryType == CMSREP_REPOSITORYTYPE_STREAMING ||
			rtRepositoryType == CMSREP_REPOSITORYTYPE_CMSCUSTOMER)
		{
			if (detDirectoryEntryType == FileIO:: TOOLS_FILEIO_DIRECTORY)
			{
				if (*pulDirectoryLevelIndexInsideCustomer == 4 &&
					rtRepositoryType == CMSREP_REPOSITORYTYPE_CMSCUSTOMER)
				{
					// In the scenario where
					// *pulDirectoryLevelIndexInsideCustomer == 4 &&
					//	rtRepositoryType == CMSREP_REPOSITORYTYPE_CMSCUSTOMER
					//	we should have the IPhone and that has not to be going
					//	through by the sanity check

					continue;
				}

				if (bContentPathName. insertAt (0,
					pContentsDirectory) != errNoError)
				{
					Error err = ToolsErrors (__FILE__, __LINE__,
						TOOLS_BUFFER_INSERTAT_FAILED);
					_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
						(const char *) err, __FILE__, __LINE__);

					if (FileIO:: closeDirectory (&dContentsDirectory) !=
						errNoError)
					{
						Error err = ToolsErrors (__FILE__, __LINE__,
							TOOLS_FILEIO_CLOSEDIRECTORY_FAILED);
						_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
							(const char *) err, __FILE__, __LINE__);
					}

					return err;
				}

				#ifdef WIN32
					if (bContentPathName. append ("\\") != errNoError)
				#else
					if (bContentPathName. append ("/") != errNoError)
				#endif
				{
					Error err = ToolsErrors (__FILE__, __LINE__,
						TOOLS_BUFFER_APPEND_FAILED);
					_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
						(const char *) err, __FILE__, __LINE__);

					if (FileIO:: closeDirectory (&dContentsDirectory) !=
						errNoError)
					{
						Error err = ToolsErrors (__FILE__, __LINE__,
							TOOLS_FILEIO_CLOSEDIRECTORY_FAILED);
						_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
							(const char *) err, __FILE__, __LINE__);
					}

					return err;
				}

				if (!strcmp ((const char *) bContentPathName,
					(const char *) _bProfilesRootRepository))
				{
					// profiles directory

					continue;
				}

				*pulDirectoryLevelIndexInsideCustomer			+= 1;

				if ((errSanityCheck = sanityCheck_ContentsDirectory (
					pCustomerDirectoryName,
					(const char *) bContentPathName,
					ulRelativePathIndex,
					rtRepositoryType,
					psciSanityCheckContentsInfo,
					plSanityCheckContentsInfoCurrentIndex,
					pulFileIndex,
					pulCurrentFileNumberProcessedInThisSchedule,
					pulCurrentFilesRemovedNumberInThisSchedule,
					pulCurrentDirectoriesRemovedNumberInThisSchedule,
					pulDirectoryLevelIndexInsideCustomer)) != errNoError)
				{
					if ((unsigned long) errSanityCheck ==
						CMSREP_CMSREPOSITORY_REACHEDMAXNUMBERTOBEPROCESSED)
					{
						*pulDirectoryLevelIndexInsideCustomer			-= 1;

						if (FileIO:: closeDirectory (&dContentsDirectory) !=
							errNoError)
						{
							Error err = ToolsErrors (__FILE__, __LINE__,
								TOOLS_FILEIO_CLOSEDIRECTORY_FAILED);
							_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
								(const char *) err, __FILE__, __LINE__);
						}

						return errSanityCheck;
					}
					else
					{
						Error err = CMSRepositoryErrors (__FILE__, __LINE__,
					CMSREP_CMSREPOSITORY_SANITYCHECK_CONTENTSDIRECTORY_FAILED);
						_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
							(const char *) err, __FILE__, __LINE__);

						// we will continue the sanity check in order that
						// the other directories can be verified and
						// we will stop it

//						*pulDirectoryLevelIndexInsideCustomer			-= 1;
//
//						if (FileIO:: closeDirectory (&dContentsDirectory) !=
//							errNoError)
//						{
//							Error err = ToolsErrors (__FILE__, __LINE__,
//								TOOLS_FILEIO_CLOSEDIRECTORY_FAILED);
//							_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
//								(const char *) err, __FILE__, __LINE__);
//						}
//
//						return err;
					}
				}

				*pulDirectoryLevelIndexInsideCustomer			-= 1;
			}
			else // TOOLS_FILEIO_REGULARFILE || TOOLS_FILEIO_LINKFILE)
			{
				if (*pulFileIndex <
					_svlpcLastProcessedContent [rtRepositoryType].
					_ulFilesNumberAlreadyProcessed)
				{
					// file already processed

					continue;
				}

				if (*pulCurrentFileNumberProcessedInThisSchedule >=
					_ulMaxFilesToBeProcessedPerSchedule [rtRepositoryType])
				{
					Error err = CMSRepositoryErrors (__FILE__, __LINE__,
						CMSREP_CMSREPOSITORY_REACHEDMAXNUMBERTOBEPROCESSED,
						2, 
						(const char *) (*(_pbRepositories [rtRepositoryType])),
						_ulMaxFilesToBeProcessedPerSchedule [rtRepositoryType]);
					_ptSystemTracer -> trace (Tracer:: TRACER_LINFO,
						(const char *) err, __FILE__, __LINE__);

					if (FileIO:: closeDirectory (&dContentsDirectory) !=
						errNoError)
					{
						Error err = ToolsErrors (__FILE__, __LINE__,
							TOOLS_FILEIO_CLOSEDIRECTORY_FAILED);
						_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
							(const char *) err, __FILE__, __LINE__);
					}

					return err;
				}

				(*pulCurrentFileNumberProcessedInThisSchedule)		+= 1;

				(_svlpcLastProcessedContent [rtRepositoryType].
					_ulFilesNumberAlreadyProcessed)					+= 1;

				{
					const char				*pRelativePath;
					char					pTerritoryName [
						CMSREP_CMSREPOSITORY_MAXTERRITORYNAME];


					// retrieve the TerritoryName and RelativePath
					if (rtRepositoryType == CMSREP_REPOSITORYTYPE_DOWNLOAD ||
						rtRepositoryType == CMSREP_REPOSITORYTYPE_STREAMING)
					{
						#ifdef WIN32
							pRelativePath	= strchr (
								pContentsDirectory + ulRelativePathIndex + 1,
								'\\');
						#else
							pRelativePath	= strchr (
								pContentsDirectory + ulRelativePathIndex + 1,
								'/');
						#endif

						if (pRelativePath == (const char *) NULL)
						{
							// we expect a directory (the territory) but
							// we found a file
							Error err = CMSRepositoryErrors (__FILE__, __LINE__,
							CMSREP_CMSREPOSITORY_SANITYCHECKUNEXPECTEDFILE,
								2, pContentsDirectory,
								(const char *) bContentPathName);
							_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
								(const char *) err, __FILE__, __LINE__);

							if (_bUnexpectedFilesToBeRemoved)
							{
								if (bContentPathName. insertAt (0,
									pContentsDirectory) != errNoError)
								{
									Error err = ToolsErrors (__FILE__, __LINE__,
										TOOLS_BUFFER_INSERTAT_FAILED);
									_ptSystemTracer -> trace (
										Tracer:: TRACER_LERRR,
										(const char *) err, __FILE__, __LINE__);

									if (FileIO:: closeDirectory (
										&dContentsDirectory) != errNoError)
									{
										Error err = ToolsErrors (
											__FILE__, __LINE__,
											TOOLS_FILEIO_CLOSEDIRECTORY_FAILED);
										_ptSystemTracer -> trace (
											Tracer:: TRACER_LERRR,
											(const char *) err,
											__FILE__, __LINE__);
									}

									return err;
								}

							(*pulCurrentFilesRemovedNumberInThisSchedule)
									+= 1;

								{
									Message msg = CMSRepositoryMessages (
										__FILE__, __LINE__,
										CMSREP_CMSREPOSITORY_REMOVEFILE,
										2,
										pCustomerDirectoryName,
										(const char *) bContentPathName);
									_ptSystemTracer -> trace (
										Tracer:: TRACER_LINFO,
										(const char *) msg, __FILE__, __LINE__);
								}

								// this is just a link because we are
								// in the Download or Streaming repository
								// or a file in case of playlist
								if ((errFileIO = FileIO:: remove (
									(const char *) bContentPathName)) !=
									errNoError)
								{
									_ptSystemTracer -> trace (
										Tracer:: TRACER_LERRR,
										(const char *) errFileIO,
										__FILE__, __LINE__);

									Error err = ToolsErrors (__FILE__, __LINE__,
										TOOLS_FILEIO_REMOVE_FAILED,
										1,
										(const char *) bContentPathName);
									_ptSystemTracer -> trace (
										Tracer:: TRACER_LERRR,
										(const char *) err, __FILE__, __LINE__);

//									if (FileIO:: closeDirectory (
//										&dContentsDirectory) != errNoError)
//									{
//										Error err = ToolsErrors (
//											__FILE__, __LINE__,
//											TOOLS_FILEIO_CLOSEDIRECTORY_FAILED);
//										_ptSystemTracer -> trace (
//											Tracer:: TRACER_LERRR,
//											(const char *) err,
//											__FILE__, __LINE__);
//									}
//
//									return err;
								}
							}

							continue;

//							if (FileIO:: closeDirectory (&dContentsDirectory) !=
//								errNoError)
//							{
//								Error err = ToolsErrors (__FILE__, __LINE__,
//									TOOLS_FILEIO_CLOSEDIRECTORY_FAILED);
//								_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
//									(const char *) err, __FILE__, __LINE__);
//							}
//
//							return err;
						}

						strncpy (pTerritoryName,
							pContentsDirectory + ulRelativePathIndex + 1,
							pRelativePath -
							(pContentsDirectory + ulRelativePathIndex + 1));
						pTerritoryName [pRelativePath -
							(pContentsDirectory + ulRelativePathIndex + 1)]	=
								'\0';
					}
					else
					{
						pRelativePath	=
							pContentsDirectory + ulRelativePathIndex;

						strcpy (pTerritoryName, "null");
					}

					if ((*plSanityCheckContentsInfoCurrentIndex) + 1 >=
						CMSREP_CMSREPOSITORY_MAXSANITYCHECKCONTENTSINFONUMBER)
					{
						{
							Message msg = CMSRepositoryMessages (
								__FILE__, __LINE__,
								CMSREP_CMSREPOSITORY_SANITYCHECKSTATUS,
								3,
								(const char *) (*(
									_pbRepositories [rtRepositoryType])),
								(*pulFileIndex),
								(*pulCurrentFileNumberProcessedInThisSchedule));
							_ptSystemTracer -> trace (Tracer:: TRACER_LINFO,
								(const char *) msg, __FILE__, __LINE__);
						}

						if (sanityCheck_runOnContentsInfo (
							psciSanityCheckContentsInfo,
							*plSanityCheckContentsInfoCurrentIndex,
							rtRepositoryType,
							pulCurrentFilesRemovedNumberInThisSchedule,
							pulCurrentDirectoriesRemovedNumberInThisSchedule) !=
							errNoError)
						{
							Error err = CMSRepositoryErrors (__FILE__, __LINE__,
					CMSREP_CMSREPOSITORY_SANITYCHECK_RUNONCONTENTSINFO_FAILED);
							_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
								(const char *) err, __FILE__, __LINE__);

//							if (FileIO:: closeDirectory (&dContentsDirectory) !=
//								errNoError)
//							{
//								Error err = ToolsErrors (__FILE__, __LINE__,
//									TOOLS_FILEIO_CLOSEDIRECTORY_FAILED);
//								_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
//									(const char *) err, __FILE__, __LINE__);
//							}
//
//							return err;
						}

						*plSanityCheckContentsInfoCurrentIndex		= 0;
					}
					else
					{
						(*plSanityCheckContentsInfoCurrentIndex)		+= 1;
					}
 
					(psciSanityCheckContentsInfo [
						*plSanityCheckContentsInfoCurrentIndex]).
							_ulContentFound		= 2;
					(psciSanityCheckContentsInfo [
						*plSanityCheckContentsInfoCurrentIndex]).
							_ulPublishingStatus	= 2;

					if ((psciSanityCheckContentsInfo [
						*plSanityCheckContentsInfoCurrentIndex]).
						_bContentsDirectory. setBuffer (pContentsDirectory) !=
						errNoError ||

						(psciSanityCheckContentsInfo [
						*plSanityCheckContentsInfoCurrentIndex]).
						_bCustomerDirectoryName. setBuffer (pCustomerDirectoryName) !=
						errNoError ||

						(psciSanityCheckContentsInfo [
						*plSanityCheckContentsInfoCurrentIndex]).
							_bTerritoryName. setBuffer (pTerritoryName) !=
							errNoError ||

						(psciSanityCheckContentsInfo [
						*plSanityCheckContentsInfoCurrentIndex]).
							_bRelativePath. setBuffer (pRelativePath) !=
							errNoError ||

						(psciSanityCheckContentsInfo [
						*plSanityCheckContentsInfoCurrentIndex]). _bFileName.
						setBuffer ((const char *) bContentPathName) !=
						errNoError)
					{
						Error err = ToolsErrors (__FILE__, __LINE__,
							TOOLS_BUFFER_SETBUFFER_FAILED);
						_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
							(const char *) err, __FILE__, __LINE__);

						if (FileIO:: closeDirectory (&dContentsDirectory) !=
							errNoError)
						{
							Error err = ToolsErrors (__FILE__, __LINE__,
								TOOLS_FILEIO_CLOSEDIRECTORY_FAILED);
							_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
								(const char *) err, __FILE__, __LINE__);
						}

						return err;
					}
				}
			}
		}
	}

	if (FileIO:: closeDirectory (&dContentsDirectory) != errNoError)
	{
		Error err = ToolsErrors (__FILE__, __LINE__,
			TOOLS_FILEIO_CLOSEDIRECTORY_FAILED);
		_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
			(const char *) err, __FILE__, __LINE__);

		return err;
	}


	return errNoError;
}


Error CMSRepository:: sanityCheck_runOnContentsInfo (
	SanityCheckContentInfo_p psciSanityCheckContentsInfo,
	unsigned long ulSanityCheckContentsInfoCurrentIndex,
	RepositoryType_t rtRepositoryType,
	unsigned long *pulCurrentFilesRemovedNumberInThisSchedule,
	unsigned long *pulCurrentDirectoriesRemovedNumberInThisSchedule)

{

	Buffer_t					bURIForHttpPost;
	Buffer_t					bURLParametersForHttpPost;
	Buffer_t					bHttpPostBodyRequest;
	Error_t						errGetAvailableModule;
	HttpPostThread_t			hgPostSanityCheckContentInfo;
	Error_t						errRun;
	Buffer_t					bHttpPostBodyResponse;
	char						pWebServerIPAddress [
		SCK_MAXIPADDRESSLENGTH];
	unsigned long				ulWebServerPort;
	unsigned long				ulSanityCheckContentInfoIndex;



	if (bHttpPostBodyResponse. init () != errNoError)
	{
		Error err = ToolsErrors (__FILE__, __LINE__,
			TOOLS_BUFFER_INIT_FAILED);
		_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
			(const char *) err, __FILE__, __LINE__);

		return err;
	}

	if (bURIForHttpPost. init (
		"/CMSEngine/getSanityCheckContentInfo") != errNoError)
	{
		Error err = ToolsErrors (__FILE__, __LINE__,
			TOOLS_BUFFER_INIT_FAILED);
		_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
			(const char *) err, __FILE__, __LINE__);

		return err;
	}

	if (bURLParametersForHttpPost. init ("") != errNoError)
	{
		Error err = ToolsErrors (__FILE__, __LINE__,
			TOOLS_BUFFER_INIT_FAILED);
		_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
			(const char *) err, __FILE__, __LINE__);

		return err;
	}

	if (bHttpPostBodyRequest. init (
		"<?xml version=\"1.0\" encoding=\"utf-8\"?> <GetSanityCheckContentInfo> ") !=
		errNoError)
	{
		Error err = ToolsErrors (__FILE__, __LINE__,
			TOOLS_BUFFER_INIT_FAILED);
		_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
			(const char *) err, __FILE__, __LINE__);

		return err;
	}

	for (ulSanityCheckContentInfoIndex = 0;
		ulSanityCheckContentInfoIndex <= ulSanityCheckContentsInfoCurrentIndex;
		ulSanityCheckContentInfoIndex++)
	{
		if (bHttpPostBodyRequest. append (
			"<ContentToBeChecked> ") != errNoError)
		{
			Error err = ToolsErrors (__FILE__, __LINE__,
				TOOLS_BUFFER_APPEND_FAILED);
			_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
				(const char *) err, __FILE__, __LINE__);

			return err;
		}

		if (bHttpPostBodyRequest. append (
			"<Identifier><![CDATA[") != errNoError)
		{
			Error err = ToolsErrors (__FILE__, __LINE__,
				TOOLS_BUFFER_APPEND_FAILED);
			_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
				(const char *) err, __FILE__, __LINE__);

			return err;
		}

		if (bHttpPostBodyRequest. append (
			ulSanityCheckContentInfoIndex) !=
			errNoError)
		{
			Error err = ToolsErrors (__FILE__, __LINE__,
				TOOLS_BUFFER_APPEND_FAILED);
			_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
				(const char *) err, __FILE__, __LINE__);

			return err;
		}

		if (bHttpPostBodyRequest. append (
			"]]></Identifier>") != errNoError)
		{
			Error err = ToolsErrors (__FILE__, __LINE__,
				TOOLS_BUFFER_APPEND_FAILED);
			_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
				(const char *) err, __FILE__, __LINE__);

			return err;
		}

		if (bHttpPostBodyRequest. append (
			"<CMSRepository><![CDATA[") != errNoError)
		{
			Error err = ToolsErrors (__FILE__, __LINE__,
				TOOLS_BUFFER_APPEND_FAILED);
			_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
				(const char *) err, __FILE__, __LINE__);

			return err;
		}

		if (bHttpPostBodyRequest. append ((long) rtRepositoryType) !=
			errNoError)
		{
			Error err = ToolsErrors (__FILE__, __LINE__,
				TOOLS_BUFFER_APPEND_FAILED);
			_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
				(const char *) err, __FILE__, __LINE__);

			return err;
		}

		if (bHttpPostBodyRequest. append (
			"]]></CMSRepository>") != errNoError)
		{
			Error err = ToolsErrors (__FILE__, __LINE__,
				TOOLS_BUFFER_APPEND_FAILED);
			_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
				(const char *) err, __FILE__, __LINE__);

			return err;
		}

		if (bHttpPostBodyRequest. append (
			"<CustomerDirectoryName><![CDATA[") != errNoError)
		{
			Error err = ToolsErrors (__FILE__, __LINE__,
				TOOLS_BUFFER_APPEND_FAILED);
			_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
				(const char *) err, __FILE__, __LINE__);

			return err;
		}

		if (bHttpPostBodyRequest. append (
			(const char *)
				((psciSanityCheckContentsInfo [ulSanityCheckContentInfoIndex]).
				_bCustomerDirectoryName)) !=
			errNoError)
		{
			Error err = ToolsErrors (__FILE__, __LINE__,
				TOOLS_BUFFER_APPEND_FAILED);
			_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
				(const char *) err, __FILE__, __LINE__);

			return err;
		}

		if (bHttpPostBodyRequest. append (
			"]]></CustomerDirectoryName>") != errNoError)
		{
			Error err = ToolsErrors (__FILE__, __LINE__,
				TOOLS_BUFFER_APPEND_FAILED);
			_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
				(const char *) err, __FILE__, __LINE__);

			return err;
		}

		if (bHttpPostBodyRequest. append (
			"<TerritoryName><![CDATA[") != errNoError)
		{
			Error err = ToolsErrors (__FILE__, __LINE__,
				TOOLS_BUFFER_APPEND_FAILED);
			_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
				(const char *) err, __FILE__, __LINE__);

			return err;
		}

		if (bHttpPostBodyRequest. append (
			(const char *)
				((psciSanityCheckContentsInfo [ulSanityCheckContentInfoIndex]).
				_bTerritoryName)) !=
			errNoError)
		{
			Error err = ToolsErrors (__FILE__, __LINE__,
				TOOLS_BUFFER_APPEND_FAILED);
			_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
				(const char *) err, __FILE__, __LINE__);

			return err;
		}

		if (bHttpPostBodyRequest. append (
			"]]></TerritoryName>") != errNoError)
		{
			Error err = ToolsErrors (__FILE__, __LINE__,
				TOOLS_BUFFER_APPEND_FAILED);
			_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
				(const char *) err, __FILE__, __LINE__);

			return err;
		}

		if (bHttpPostBodyRequest. append (
			"<RelativePath><![CDATA[") != errNoError)
		{
			Error err = ToolsErrors (__FILE__, __LINE__,
				TOOLS_BUFFER_APPEND_FAILED);
			_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
				(const char *) err, __FILE__, __LINE__);

			return err;
		}

		if (bHttpPostBodyRequest. append (
			(const char *)
				((psciSanityCheckContentsInfo [ulSanityCheckContentInfoIndex]).
				_bRelativePath)) !=
			errNoError)
		{
			Error err = ToolsErrors (__FILE__, __LINE__,
				TOOLS_BUFFER_APPEND_FAILED);
			_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
				(const char *) err, __FILE__, __LINE__);

			return err;
		}

		if (bHttpPostBodyRequest. append (
			"]]></RelativePath>") != errNoError)
		{
			Error err = ToolsErrors (__FILE__, __LINE__,
				TOOLS_BUFFER_APPEND_FAILED);
			_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
				(const char *) err, __FILE__, __LINE__);

			return err;
		}

		if (bHttpPostBodyRequest. append (
			"<FileName><![CDATA[") != errNoError)
		{
			Error err = ToolsErrors (__FILE__, __LINE__,
				TOOLS_BUFFER_APPEND_FAILED);
			_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
				(const char *) err, __FILE__, __LINE__);

			return err;
		}

		if (bHttpPostBodyRequest. append (
			(const char *)
				((psciSanityCheckContentsInfo [ulSanityCheckContentInfoIndex]).
				_bFileName)) !=
			errNoError)
		{
			Error err = ToolsErrors (__FILE__, __LINE__,
				TOOLS_BUFFER_APPEND_FAILED);
			_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
				(const char *) err, __FILE__, __LINE__);

			return err;
		}

		if (bHttpPostBodyRequest. append (
			"]]></FileName>") != errNoError)
		{
			Error err = ToolsErrors (__FILE__, __LINE__,
				TOOLS_BUFFER_APPEND_FAILED);
			_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
				(const char *) err, __FILE__, __LINE__);

			return err;
		}

		if (bHttpPostBodyRequest. append (
			"</ContentToBeChecked> ") != errNoError)
		{
			Error err = ToolsErrors (__FILE__, __LINE__,
				TOOLS_BUFFER_APPEND_FAILED);
			_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
				(const char *) err, __FILE__, __LINE__);

			return err;
		}
	}

	if (bHttpPostBodyRequest. append (
		"</GetSanityCheckContentInfo> ") !=
		errNoError)
	{
		Error err = ToolsErrors (__FILE__, __LINE__,
			TOOLS_BUFFER_APPEND_FAILED);
		_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
			(const char *) err, __FILE__, __LINE__);

		return err;
	}

	if ((errGetAvailableModule =
		_plbWebServerLoadBalancer -> getAvailableModule (
		"WebServers", pWebServerIPAddress,
		SCK_MAXIPADDRESSLENGTH,
		&ulWebServerPort)) != errNoError)
	{
		Error err = LoadBalancerErrors (
			__FILE__, __LINE__,
			LB_LOADBALANCER_GETAVAILABLEMODULE_FAILED,
			1, "WebServers");
		_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
			(const char *) err, __FILE__, __LINE__);

		return err;
	}

	{
		Message msg = CMSRepositoryMessages (__FILE__, __LINE__,
			CMSREP_HTTPPOSTTHREAD,
			8,
			"getSanityCheckContentInfo",
			pWebServerIPAddress,
			ulWebServerPort,
			_pWebServerLocalIPAddress,
			(const char *) bURIForHttpPost,
			(const char *) bURLParametersForHttpPost,
			(const char *) bHttpPostBodyRequest,
			_ulWebServerTimeoutToWaitAnswerInSeconds);
		_ptSystemTracer -> trace (Tracer:: TRACER_LINFO,
			(const char *) msg, __FILE__, __LINE__);
	}

	if (hgPostSanityCheckContentInfo. init (
		pWebServerIPAddress,
		ulWebServerPort,
		(const char *) bURIForHttpPost,
		(const char *) bHttpPostBodyRequest,
		(const char *) bURLParametersForHttpPost,
		(const char *) NULL,	// Cookie
		"CMS Engine",
		_ulWebServerTimeoutToWaitAnswerInSeconds,
		0,
		_ulWebServerTimeoutToWaitAnswerInSeconds,
		0,
		_pWebServerLocalIPAddress) != errNoError)
	{
		Error err = WebToolsErrors (__FILE__, __LINE__,
			WEBTOOLS_HTTPPOSTTHREAD_INIT_FAILED);
		_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
			(const char *) err, __FILE__, __LINE__);

		return err;
	}

	if ((errRun = hgPostSanityCheckContentInfo. run (
		(Buffer_p) NULL, &bHttpPostBodyResponse,
		(Buffer_p) NULL)) != errNoError)
	{
		_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
			(const char *) errRun, __FILE__, __LINE__);

		Error err = PThreadErrors (__FILE__, __LINE__,
			THREADLIB_PTHREAD_RUN_FAILED);
		_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
			(const char *) err, __FILE__, __LINE__);

		if (hgPostSanityCheckContentInfo. finish () !=
			errNoError)
		{
			Error err = WebToolsErrors (__FILE__, __LINE__,
				WEBTOOLS_HTTPPOSTTHREAD_FINISH_FAILED);
			_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
				(const char *) err, __FILE__, __LINE__);
		}

		return err;
	}

	if (strstr ((const char *) bHttpPostBodyResponse,
		"<Status><![CDATA[SUCCESS") == (char *) NULL)
	{
		Error err = CMSRepositoryErrors (__FILE__, __LINE__,
			CMSREP_SERVLETFAILED,
			4, "getSanityCheckContentInfo", pWebServerIPAddress,
			"",
			(const char *) bHttpPostBodyResponse);
		_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
			(const char *) err, __FILE__, __LINE__);

		if (hgPostSanityCheckContentInfo. finish () !=
			errNoError)
		{
			Error err = WebToolsErrors (__FILE__, __LINE__,
				WEBTOOLS_HTTPPOSTTHREAD_FINISH_FAILED);
			_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
				(const char *) err, __FILE__, __LINE__);
		}

		return err;
	}

	{
		bHttpPostBodyResponse. substitute (CMSREP_CMSREPOSITORY_NEWLINE, " ");

		Message msg = CMSRepositoryMessages (__FILE__, __LINE__,
			CMSREP_HTTPRESPONSE,
			5,
			"<not available>",
			"getSanityCheckContentInfo",
			"",
			(const char *) bHttpPostBodyResponse,
			(unsigned long) hgPostSanityCheckContentInfo);
		_ptSystemTracer -> trace (Tracer:: TRACER_LDBG6, (const char *) msg,
			__FILE__, __LINE__);
	}

	if (hgPostSanityCheckContentInfo. finish () !=
		errNoError)
	{
		Error err = WebToolsErrors (__FILE__, __LINE__,
			WEBTOOLS_HTTPPOSTTHREAD_FINISH_FAILED);
		_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
			(const char *) err, __FILE__, __LINE__);

		return err;
	}

	// parse the XML response and sanity check on the Content
	{
		unsigned long						ulResponseContentsInfoNumber;
		Boolean_t							bContentToBeRemoved;

		// parse the XML response
		{
			xmlDocPtr			pxdXMLDocument;
			xmlNodePtr			pxnXMLCMSNode;
			xmlNodePtr			pxnXMLContentInfoNode;
			xmlChar				*pxcValue;
			unsigned long		ulLocalContentInfoIdentifier;


			if ((pxdXMLDocument = xmlParseMemory (
				(const char *) bHttpPostBodyResponse,
				(unsigned long) bHttpPostBodyResponse)) ==
				(xmlDocPtr) NULL)
			{
				// parse error
				Error err = CMSRepositoryErrors (__FILE__, __LINE__,
					CMSREP_LIBXML2_XMLPARSEMEMORY_FAILED,
					2,
					"getSanityCheckContentInfo",
					(const char *) bHttpPostBodyResponse);
				_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
					(const char *) err, __FILE__, __LINE__);

				return err;
			}

			// CMS
			if ((pxnXMLCMSNode = xmlDocGetRootElement (pxdXMLDocument)) ==
				(xmlNodePtr) NULL)
			{
				// empty document
				Error err = CMSRepositoryErrors (__FILE__, __LINE__,
					CMSREP_LIBXML2_XMLDOCROOTELEMENT_FAILED);
				_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
					(const char *) err, __FILE__, __LINE__);

				xmlFreeDoc (pxdXMLDocument);

				return err;
			}

			while (pxnXMLCMSNode != (xmlNodePtr) NULL &&
				xmlStrcmp (pxnXMLCMSNode -> name,
				(const xmlChar *) "CMS"))
				pxnXMLCMSNode				= pxnXMLCMSNode -> next;

			if (pxnXMLCMSNode == (xmlNodePtr) NULL ||
				(pxnXMLCMSNode = pxnXMLCMSNode -> xmlChildrenNode) ==
				(xmlNodePtr) NULL)
			{
				Error err = CMSRepositoryErrors (__FILE__, __LINE__,
					CMSREP_XMLWRONG,
					2,
					"getSanityCheckContentInfo",
					(const char *) bHttpPostBodyResponse);
				_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
					(const char *) err, __FILE__, __LINE__);

				xmlFreeDoc (pxdXMLDocument);

				return err;
			}

			ulResponseContentsInfoNumber		= 0;

			while (pxnXMLCMSNode != (xmlNodePtr) NULL)
			{
				if (!xmlStrcmp (pxnXMLCMSNode -> name, (const xmlChar *) 
					"ContentInfo"))
				{
					SanityCheckContentInfo_t	sciLocalSanityCheckContentInfo;


					ulLocalContentInfoIdentifier			= 999999;

					if (ulResponseContentsInfoNumber >=
						ulSanityCheckContentsInfoCurrentIndex + 1)
					{
						Error err = CMSRepositoryErrors (__FILE__, __LINE__,
							CMSREP_XMLWRONG,
							2,
							"getSanityCheckContentInfo",
							(const char *) bHttpPostBodyResponse);
						_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
							(const char *) err, __FILE__, __LINE__);

						xmlFreeDoc (pxdXMLDocument);

						return err;
					}

					if ((pxnXMLContentInfoNode =
						pxnXMLCMSNode -> xmlChildrenNode) ==
						(xmlNodePtr) NULL)
					{
						Error err = CMSRepositoryErrors (__FILE__, __LINE__,
							CMSREP_XMLWRONG,
							2,
							"getSanityCheckContentInfo",
							(const char *) bHttpPostBodyResponse);
						_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
							(const char *) err, __FILE__, __LINE__);

						xmlFreeDoc (pxdXMLDocument);

						return err;
					}

					while (pxnXMLContentInfoNode != (xmlNodePtr) NULL)
					{
						if (!xmlStrcmp (pxnXMLContentInfoNode -> name,
							(const xmlChar *) "Identifier"))
						{
							if ((pxcValue = xmlNodeListGetString (
								pxdXMLDocument,
								pxnXMLContentInfoNode -> xmlChildrenNode,
								1)) != (xmlChar *) NULL)
							{
								// xmlNodeListGetString NULL means
								// an empty element

								// it will be 0 o 1
								ulLocalContentInfoIdentifier	= strtoul (
									(const char *) pxcValue, (char **) NULL,
									10);

								xmlFree (pxcValue);
							}
						}
						else if (!xmlStrcmp (pxnXMLContentInfoNode -> name,
							(const xmlChar *) "ContentFound"))
						{
							if ((pxcValue = xmlNodeListGetString (
								pxdXMLDocument,
								pxnXMLContentInfoNode -> xmlChildrenNode,
								1)) != (xmlChar *) NULL)
							{
								// xmlNodeListGetString NULL means
								// an empty element

								// it will be 0 o 1
								sciLocalSanityCheckContentInfo.
									_ulContentFound			= strtoul (
									(const char *) pxcValue, (char **) NULL,
									10);

								xmlFree (pxcValue);
							}
						}
						else if (!xmlStrcmp (pxnXMLContentInfoNode -> name,
							(const xmlChar *) "PublishingStatus"))
						{
							if ((pxcValue = xmlNodeListGetString (
								pxdXMLDocument,
								pxnXMLContentInfoNode -> xmlChildrenNode,
								1)) != (xmlChar *) NULL)
							{
								// xmlNodeListGetString NULL means
								// an empty element

								// it will be 0 o 1
								sciLocalSanityCheckContentInfo.
									_ulPublishingStatus		= strtoul (
									(const char *) pxcValue, (char **) NULL,
									10);

								xmlFree (pxcValue);
							}
						}
						else if (!xmlStrcmp (pxnXMLContentInfoNode -> name,
							(const xmlChar *) "text"))
						{
						}
						else if (!xmlStrcmp (pxnXMLContentInfoNode -> name,
							(const xmlChar *) "comment"))
						{
						}
						else
						{
							Error err = CMSRepositoryErrors (__FILE__, __LINE__,
								CMSREP_XMLPARAMETERUNKNOWN,
								2, "getSanityCheckContentInfo",
								(const char *) (pxnXMLContentInfoNode -> name));
							_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
								(const char *) err, __FILE__, __LINE__);

							xmlFreeDoc (pxdXMLDocument);

							return err;
						}

						if ((pxnXMLContentInfoNode =
							pxnXMLContentInfoNode -> next) == (xmlNodePtr) NULL)
						{
						}
					}

					if (ulLocalContentInfoIdentifier == 999999 ||
						ulLocalContentInfoIdentifier >=
						ulSanityCheckContentsInfoCurrentIndex + 1)
					{
						Error err = CMSRepositoryErrors (__FILE__, __LINE__,
							CMSREP_XMLWRONG,
							2,
							"getSanityCheckContentInfo",
							(const char *) bHttpPostBodyResponse);
						_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
							(const char *) err, __FILE__, __LINE__);

						xmlFreeDoc (pxdXMLDocument);

						return err;
					}

					(psciSanityCheckContentsInfo [
						ulLocalContentInfoIdentifier]). _ulContentFound		=
						sciLocalSanityCheckContentInfo. _ulContentFound;
					(psciSanityCheckContentsInfo [
						ulLocalContentInfoIdentifier]). _ulPublishingStatus	=
						sciLocalSanityCheckContentInfo. _ulPublishingStatus;

					ulResponseContentsInfoNumber++;
				}
				else if (!xmlStrcmp (pxnXMLCMSNode -> name,
					(const xmlChar *) "Status"))
				{
					// no check on the Status because it is already
					// verified before after the run method
				}
				else if (!xmlStrcmp (pxnXMLCMSNode -> name,
					(const xmlChar *) "text"))
				{
				}
				else if (!xmlStrcmp (pxnXMLCMSNode -> name,
					(const xmlChar *) "comment"))
				{
				}
				else
				{
					Error err = CMSRepositoryErrors (__FILE__, __LINE__,
						CMSREP_XMLPARAMETERUNKNOWN,
						2, "getSanityCheckContentInfo",
						(const char *) (pxnXMLCMSNode -> name));
					_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
						(const char *) err, __FILE__, __LINE__);

					xmlFreeDoc (pxdXMLDocument);

					return err;
				}

				if ((pxnXMLCMSNode = pxnXMLCMSNode -> next) ==
					(xmlNodePtr) NULL)
				{
				}
			}

			xmlFreeDoc (pxdXMLDocument);

			if (ulResponseContentsInfoNumber !=
				ulSanityCheckContentsInfoCurrentIndex + 1)
			{
				Error err = CMSRepositoryErrors (__FILE__, __LINE__,
					CMSREP_XMLWRONG,
					2,
					"getSanityCheckContentInfo",
					(const char *) bHttpPostBodyResponse);
				_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
					(const char *) err, __FILE__, __LINE__);

				return err;
			}
		}

		// sanity check on the Content
		{
			SanityCheckContentInfo_t			sciLocalSanityCheckContentInfo;
			Error_t								errFileIO;


			for (ulSanityCheckContentInfoIndex = 0;
				ulSanityCheckContentInfoIndex <=
				ulSanityCheckContentsInfoCurrentIndex;
				ulSanityCheckContentInfoIndex++)
			{
				sciLocalSanityCheckContentInfo		=
					psciSanityCheckContentsInfo [ulSanityCheckContentInfoIndex];

				if (
					(sciLocalSanityCheckContentInfo. _ulContentFound != 0 &&
					sciLocalSanityCheckContentInfo. _ulContentFound != 1) ||
					(sciLocalSanityCheckContentInfo. _ulPublishingStatus != 0 &&
					sciLocalSanityCheckContentInfo. _ulPublishingStatus != 1)
					)
				{
					Error err = CMSRepositoryErrors (__FILE__, __LINE__,
						CMSREP_SERVLETFAILED,
						4, "getSanityCheckContentInfo", pWebServerIPAddress,
						(const char *)
							(sciLocalSanityCheckContentInfo. _bFileName),
						(const char *) bHttpPostBodyResponse);
					_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
						(const char *) err, __FILE__, __LINE__);

					return err;
				}

				if (rtRepositoryType == CMSREP_REPOSITORYTYPE_CMSCUSTOMER)
				{
					if (sciLocalSanityCheckContentInfo. _ulContentFound == 0)
						bContentToBeRemoved			= true;
					else
						bContentToBeRemoved			= false;
				}
				else if (rtRepositoryType == CMSREP_REPOSITORYTYPE_DOWNLOAD ||
					rtRepositoryType == CMSREP_REPOSITORYTYPE_STREAMING)
				{
					if (sciLocalSanityCheckContentInfo. _ulContentFound == 0 ||
						sciLocalSanityCheckContentInfo. _ulPublishingStatus == 0)
					{
						bContentToBeRemoved			= true;
					}
					else
					{
						bContentToBeRemoved			= false;
					}
				}
				else
				{
					bContentToBeRemoved			= false;
				}

				if (bContentToBeRemoved)
				{
					Error err = CMSRepositoryErrors (__FILE__, __LINE__,
					CMSREP_CMSREPOSITORY_SANITYCHECKFILESYSTEMDBNOTCONSISTENT,
						8, 
						(const char *) (*(_pbRepositories [rtRepositoryType])),
						(const char *) (sciLocalSanityCheckContentInfo.
							_bCustomerDirectoryName),
						!strcmp ((const char *)
							(sciLocalSanityCheckContentInfo.
							_bTerritoryName),
							"null") ? "" : (const char *)
							(sciLocalSanityCheckContentInfo.
							_bTerritoryName),
						(const char *)
							(sciLocalSanityCheckContentInfo.
							_bRelativePath),
						(const char *) (sciLocalSanityCheckContentInfo.
							_bFileName),
						sciLocalSanityCheckContentInfo. _ulContentFound,
						sciLocalSanityCheckContentInfo. _ulPublishingStatus,
						_bUnexpectedFilesToBeRemoved);
					_ptSystemTracer -> trace (Tracer:: TRACER_LWRNG,
						(const char *) err, __FILE__, __LINE__);

					if (_bUnexpectedFilesToBeRemoved)
					{
						if ((sciLocalSanityCheckContentInfo. _bFileName).
							insertAt (0, (const char *)
							(sciLocalSanityCheckContentInfo.
							_bContentsDirectory)) != errNoError)
						{
							Error err = ToolsErrors (__FILE__, __LINE__,
								TOOLS_BUFFER_INSERTAT_FAILED);
							_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
								(const char *) err, __FILE__, __LINE__);

							return err;
						}

						if (rtRepositoryType ==
							CMSREP_REPOSITORYTYPE_DOWNLOAD ||
							rtRepositoryType == CMSREP_REPOSITORYTYPE_STREAMING)
						{
							(*pulCurrentFilesRemovedNumberInThisSchedule)
								+= 1;

							{
								Message msg = CMSRepositoryMessages (
									__FILE__, __LINE__,
									CMSREP_CMSREPOSITORY_REMOVEFILE,
									2,
									(const char *)
										(sciLocalSanityCheckContentInfo.
										_bCustomerDirectoryName),
									(const char *)
										(sciLocalSanityCheckContentInfo.
										_bFileName));
								_ptSystemTracer -> trace (
									Tracer:: TRACER_LINFO,
									(const char *) msg, __FILE__, __LINE__);
							}

							// this is just a link or a file in case of playlist
							if ((errFileIO = FileIO:: remove (
									(const char *)
										(sciLocalSanityCheckContentInfo.
										_bFileName))) !=
								errNoError)
							{
								_ptSystemTracer -> trace (
									Tracer:: TRACER_LERRR,
									(const char *) errFileIO,
									__FILE__, __LINE__);

								Error err = ToolsErrors (__FILE__, __LINE__,
									TOOLS_FILEIO_REMOVE_FAILED,
									1,
									(const char *)
										(sciLocalSanityCheckContentInfo.
										_bFileName));
								_ptSystemTracer -> trace (
									Tracer:: TRACER_LERRR,
									(const char *) err, __FILE__, __LINE__);

								return err;
							}
						}
						else if (rtRepositoryType ==
							CMSREP_REPOSITORYTYPE_CMSCUSTOMER)
						{
							// it could be a directory in case of IPhone
							// streaming content

							(*pulCurrentFilesRemovedNumberInThisSchedule)
								+= 1;

							if (moveContentInRepository (
								(const char *)
									(sciLocalSanityCheckContentInfo.
									_bFileName),
								CMSRepository:: CMSREP_REPOSITORYTYPE_STAGING,
								(const char *)
									(sciLocalSanityCheckContentInfo.
									_bCustomerDirectoryName), true) != errNoError)
							{
								Error err = CMSRepositoryErrors (
									__FILE__, __LINE__,
						CMSREP_CMSREPOSITORY_MOVECONTENTINREPOSITORY_FAILED);
								_ptSystemTracer -> trace (
									Tracer:: TRACER_LERRR,
									(const char *) err, __FILE__, __LINE__);

								return err;
							}
						}
					}
				}
			}
		}
	}


	return errNoError;
}


Error CMSRepository:: getCMSAssetPathName (
	Buffer_p pbAssetPathName,
	unsigned long ulPartitionNumber,
	const char *pCustomerDirectoryName,
	const char *pRelativePath,		// using '/'
	const char *pFileName,
	Boolean_t bIsFromXOEMachine)

{
	char					pCMSPartitionName [
		CMSREP_CMSREPOSITORY_MAXCMSPARTITIONNAMELENGTH];


	if (bIsFromXOEMachine)
	{
		Buffer_t					bRelativePathFromXOEMachine;
		char						pStorage [SCK_MAXHOSTNAMELENGTH];
		Error_t						errGetItemValue;


		if (bRelativePathFromXOEMachine. init (pRelativePath) != errNoError)
		{
			Error err = ToolsErrors (__FILE__, __LINE__,
				TOOLS_BUFFER_INIT_FAILED);
			_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
				(const char *) err, __FILE__, __LINE__);

			return err;
		}

		if (bRelativePathFromXOEMachine. substitute ("/", "\\") !=
			errNoError)
		{
			Error err = ToolsErrors (__FILE__, __LINE__,
				TOOLS_BUFFER_SUBSTITUTE_FAILED);
			_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
				(const char *) err, __FILE__, __LINE__);

			if (bRelativePathFromXOEMachine. finish () != errNoError)
			{
				Error err = ToolsErrors (__FILE__, __LINE__,
					TOOLS_BUFFER_FINISH_FAILED);
				_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
					(const char *) err, __FILE__, __LINE__);
			}

			return err;
		}

		if ((errGetItemValue = _pcfConfiguration -> getItemValue ("XOEAgent",
			"CMSDirectoryStorage", pStorage,
			SCK_MAXHOSTNAMELENGTH, ulPartitionNumber)) != errNoError)
		{
			_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
				(const char *) errGetItemValue,
				__FILE__, __LINE__);

			Error err = ConfigurationErrors (__FILE__, __LINE__,
				CFG_CONFIG_GETITEMVALUE_FAILED,
				2, "XOEAgent", "CMSDirectoryStorage");
			_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
				(const char *) err, __FILE__, __LINE__);

			if (bRelativePathFromXOEMachine. finish () != errNoError)
			{
				Error err = ToolsErrors (__FILE__, __LINE__,
					TOOLS_BUFFER_FINISH_FAILED);
				_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
					(const char *) err, __FILE__, __LINE__);
			}

			return err;
		}

		sprintf (pCMSPartitionName, "\\CMS_%04lu\\", ulPartitionNumber);

		if (
			pbAssetPathName -> setBuffer (pStorage) != errNoError ||
			pbAssetPathName -> append (
				pCMSPartitionName) != errNoError ||
			pbAssetPathName -> append (pCustomerDirectoryName) !=
				errNoError ||
			pbAssetPathName -> append (
				(const char *) bRelativePathFromXOEMachine) != errNoError ||
			pbAssetPathName -> append (pFileName) != errNoError)
		{
			Error err = ToolsErrors (__FILE__, __LINE__,
				TOOLS_BUFFER_SETBUFFER_FAILED);
			_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
				(const char *) err, __FILE__, __LINE__);

			if (bRelativePathFromXOEMachine. finish () != errNoError)
			{
				Error err = ToolsErrors (__FILE__, __LINE__,
					TOOLS_BUFFER_FINISH_FAILED);
				_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
					(const char *) err, __FILE__, __LINE__);
			}

			return err;
		}

		if (bRelativePathFromXOEMachine. finish () != errNoError)
		{
			Error err = ToolsErrors (__FILE__, __LINE__,
				TOOLS_BUFFER_FINISH_FAILED);
			_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
				(const char *) err, __FILE__, __LINE__);

			return err;
		}
	}
	else
	{
		#ifdef WIN32
			sprintf (pCMSPartitionName, "CMS_%04lu\\", ulPartitionNumber);
		#else
			sprintf (pCMSPartitionName, "CMS_%04lu/", ulPartitionNumber);
		#endif

		if (
			pbAssetPathName -> setBuffer (
				(const char *) _bCMSRootRepository) != errNoError ||
			pbAssetPathName -> append (
				pCMSPartitionName) != errNoError ||
			pbAssetPathName -> append (pCustomerDirectoryName) !=
				errNoError ||
			pbAssetPathName -> append (pRelativePath) != errNoError ||
			pbAssetPathName -> append (pFileName) != errNoError)
		{
			Error err = ToolsErrors (__FILE__, __LINE__,
				TOOLS_BUFFER_SETBUFFER_FAILED);
			_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
				(const char *) err, __FILE__, __LINE__);

			return err;
		}
	}


	return errNoError;
}


Error CMSRepository:: getDownloadLinkPathName (
	Buffer_p pbLinkPathName,
	unsigned long ulPartitionNumber,
	const char *pCustomerDirectoryName,
	const char *pTerritoryName,
	const char *pRelativePath,
	const char *pFileName,
	Boolean_t bDownloadRepositoryToo)

{

	char					pCMSPartitionName [
		CMSREP_CMSREPOSITORY_MAXCMSPARTITIONNAMELENGTH];


	if (bDownloadRepositoryToo)
	{
		#ifdef WIN32
			sprintf (pCMSPartitionName, "CMS_%04lu\\", ulPartitionNumber);
		#else
			sprintf (pCMSPartitionName, "CMS_%04lu/", ulPartitionNumber);
		#endif

		if (
			pbLinkPathName -> setBuffer (
				(const char *) _bDownloadRootRepository) != errNoError ||
			pbLinkPathName -> append (
				pCMSPartitionName) != errNoError ||
			pbLinkPathName -> append (pCustomerDirectoryName) !=
				errNoError ||
			pbLinkPathName -> append ("/") != errNoError ||
			pbLinkPathName -> append (pTerritoryName) != errNoError ||
			pbLinkPathName -> append (pRelativePath) != errNoError ||
			pbLinkPathName -> append (pFileName) != errNoError)
		{
			Error err = ToolsErrors (__FILE__, __LINE__,
				TOOLS_BUFFER_SETBUFFER_FAILED);
			_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
				(const char *) err, __FILE__, __LINE__);

			return err;
		}
	}
	else
	{
		#ifdef WIN32
			sprintf (pCMSPartitionName, "\\CMS_%04lu\\", ulPartitionNumber);
		#else
			sprintf (pCMSPartitionName, "/CMS_%04lu/", ulPartitionNumber);
		#endif

		if (
			pbLinkPathName -> setBuffer (
				pCMSPartitionName) != errNoError ||
			pbLinkPathName -> append (pCustomerDirectoryName) !=
				errNoError ||
			pbLinkPathName -> append ("/") != errNoError ||
			pbLinkPathName -> append (pTerritoryName) != errNoError ||
			pbLinkPathName -> append (pRelativePath) != errNoError ||
			pbLinkPathName -> append (pFileName) != errNoError)
		{
			Error err = ToolsErrors (__FILE__, __LINE__,
				TOOLS_BUFFER_SETBUFFER_FAILED);
			_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
				(const char *) err, __FILE__, __LINE__);

			return err;
		}
	}


	return errNoError;
}


Error CMSRepository:: getStreamingLinkPathName (
	Buffer_p pbLinkPathName,	// OUT
	unsigned long ulPartitionNumber,	// IN
	const char *pCustomerDirectoryName,	// IN
	const char *pTerritoryName,	// IN
	const char *pRelativePath,	// IN
	const char *pFileName)	// IN

{
	char					pCMSPartitionName [
		CMSREP_CMSREPOSITORY_MAXCMSPARTITIONNAMELENGTH];


	#ifdef WIN32
		sprintf (pCMSPartitionName, "CMS_%04lu\\", ulPartitionNumber);
	#else
		sprintf (pCMSPartitionName, "CMS_%04lu/", ulPartitionNumber);
	#endif

	if (
		pbLinkPathName -> setBuffer (
			(const char *) _bStreamingRootRepository) != errNoError ||
		pbLinkPathName -> append (
			pCMSPartitionName) != errNoError ||
		pbLinkPathName -> append (pCustomerDirectoryName) !=
			errNoError ||
		pbLinkPathName -> append ("/") != errNoError ||
		pbLinkPathName -> append (pTerritoryName) != errNoError ||
		pbLinkPathName -> append (pRelativePath) != errNoError ||
		pbLinkPathName -> append (pFileName) != errNoError)
	{
		Error err = ToolsErrors (__FILE__, __LINE__,
			TOOLS_BUFFER_SETBUFFER_FAILED);
		_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
			(const char *) err, __FILE__, __LINE__);

		return err;
	}


	return errNoError;
}


Error CMSRepository:: getStagingAssetPathName (
	Buffer_p pbAssetPathName,
	const char *pCustomerDirectoryName,
	const char *pRelativePath,
	const char *pFileName,
	long long llMediaItemKey,
	long long llPhysicalPathKey,
	Boolean_t bIsFromXOEMachine,
	Boolean_t bRemoveLinuxPathIfExist)

{
	char						pUniqueFileName [
		CMSREP_CMSREPOSITORY_MAXPATHNAMELENGTH];
	const char					*pLocalFileName;
	tm							tmDateTime;
	unsigned long				ulMilliSecs;
	char						pDateTime [
		CMSREP_CMSREPOSITORY_MAXDATETIMELENGTH];


	if (DateTime:: get_tm_LocalTime (
		&tmDateTime, &ulMilliSecs) != errNoError)
	{
		Error err = ToolsErrors (__FILE__, __LINE__,
			TOOLS_DATETIME_GET_TM_LOCALTIME_FAILED);
		_ptSystemTracer -> trace (Tracer:: TRACER_LERRR, (const char *) err,
			__FILE__, __LINE__);

		return err;
	}

	if (pFileName == (const char *) NULL)
	{
		sprintf (pUniqueFileName,
			"%04lu_%02lu_%02lu_%02lu_%02lu_%02lu_%04lu_%lld_%lld_%s",
			(unsigned long) (tmDateTime. tm_year + 1900),
			(unsigned long) (tmDateTime. tm_mon + 1),
			(unsigned long) (tmDateTime. tm_mday),
			(unsigned long) (tmDateTime. tm_hour),
			(unsigned long) (tmDateTime. tm_min),
			(unsigned long) (tmDateTime. tm_sec),
			ulMilliSecs,
			llMediaItemKey,
			llPhysicalPathKey,
			_pHostName);

		pLocalFileName			= pUniqueFileName;
	}
	else
	{
		pLocalFileName			= pFileName;
	}

	sprintf (pDateTime,
		"%04lu_%02lu_%02lu",
		(unsigned long) (tmDateTime. tm_year + 1900),
		(unsigned long) (tmDateTime. tm_mon + 1),
		(unsigned long) (tmDateTime. tm_mday));

	// create the 'date' directory in staging if not exist
	{
		Boolean_t			bIsDirectoryExisting;
		Error_t				errFileIO;


		if (
			pbAssetPathName -> setBuffer (
				(const char *) _bStagingRootRepository) != errNoError ||
			pbAssetPathName -> append (pCustomerDirectoryName) !=
				errNoError ||
			pbAssetPathName -> append ("/") != errNoError ||
			pbAssetPathName -> append (pDateTime) != errNoError ||
			pbAssetPathName -> append (pRelativePath) != errNoError)
		{
			Error err = ToolsErrors (__FILE__, __LINE__,
				TOOLS_BUFFER_SETBUFFER_FAILED);
			_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
				(const char *) err, __FILE__, __LINE__);

			return err;
		}

		if (FileIO:: isDirectoryExisting (
			(const char *) (*pbAssetPathName),
			&bIsDirectoryExisting) != errNoError)
		{
			Error err = ToolsErrors (__FILE__, __LINE__,
				TOOLS_FILEIO_ISDIRECTORYEXISTING_FAILED,
				1, pbAssetPathName -> str());
			_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
				(const char *) err, __FILE__, __LINE__);

			return err;
		}

		if (!bIsDirectoryExisting)
		{
			{
				Message msg = CMSRepositoryMessages (
					__FILE__, __LINE__,
					CMSREP_CMSREPOSITORY_CREATEDIRECTORY,
					2,
					pCustomerDirectoryName,
					(const char *) (*pbAssetPathName));
				_ptSystemTracer -> trace (Tracer:: TRACER_LINFO,
					(const char *) msg, __FILE__, __LINE__);
			}

			if ((errFileIO = FileIO:: createDirectory (
				(const char *) (*pbAssetPathName),
				S_IRUSR | S_IWUSR | S_IXUSR |
				S_IRGRP | S_IXGRP |
				S_IROTH | S_IXOTH, true, true)) !=
				errNoError)
			{
				_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
					(const char *) errFileIO, __FILE__, __LINE__);

				Error err = ToolsErrors (__FILE__, __LINE__,
					TOOLS_FILEIO_CREATEDIRECTORY_FAILED,
					1, (const char *) (*pbAssetPathName));
				_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
					(const char *) err, __FILE__, __LINE__);

				return err;
			}
		}
	}

	if (bIsFromXOEMachine)
	{
		Buffer_t					bRelativePathFromXOEMachine;


		if (bRelativePathFromXOEMachine. init (pRelativePath) != errNoError)
		{
			Error err = ToolsErrors (__FILE__, __LINE__,
				TOOLS_BUFFER_INIT_FAILED);
			_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
				(const char *) err, __FILE__, __LINE__);

			return err;
		}

		if (bRelativePathFromXOEMachine. substitute ("/", "\\") !=
			errNoError)
		{
			Error err = ToolsErrors (__FILE__, __LINE__,
				TOOLS_BUFFER_SUBSTITUTE_FAILED);
			_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
				(const char *) err, __FILE__, __LINE__);

			if (bRelativePathFromXOEMachine. finish () != errNoError)
			{
				Error err = ToolsErrors (__FILE__, __LINE__,
					TOOLS_BUFFER_FINISH_FAILED);
				_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
					(const char *) err, __FILE__, __LINE__);
			}

			return err;
		}

		if (
			pbAssetPathName -> setBuffer (
				(const char *) _bStagingRootRepositoryFromXOEMachine) !=
				errNoError ||
			pbAssetPathName -> append (pCustomerDirectoryName) !=
				errNoError ||
			pbAssetPathName -> append ("\\") != errNoError ||
			pbAssetPathName -> append (pDateTime) != errNoError ||
			pbAssetPathName -> append (
				(const char *) bRelativePathFromXOEMachine) != errNoError ||
			pbAssetPathName -> append (pLocalFileName) != errNoError)
		{
			Error err = ToolsErrors (__FILE__, __LINE__,
				TOOLS_BUFFER_SETBUFFER_FAILED);
			_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
				(const char *) err, __FILE__, __LINE__);

			if (bRelativePathFromXOEMachine. finish () != errNoError)
			{
				Error err = ToolsErrors (__FILE__, __LINE__,
					TOOLS_BUFFER_FINISH_FAILED);
				_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
					(const char *) err, __FILE__, __LINE__);
			}

			return err;
		}

		if (bRelativePathFromXOEMachine. finish () != errNoError)
		{
			Error err = ToolsErrors (__FILE__, __LINE__,
				TOOLS_BUFFER_FINISH_FAILED);
			_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
				(const char *) err, __FILE__, __LINE__);

			return err;
		}
	}
	else
	{
		if (
			pbAssetPathName -> setBuffer (
				(const char *) _bStagingRootRepository) != errNoError ||
			pbAssetPathName -> append (pCustomerDirectoryName) !=
				errNoError ||
			pbAssetPathName -> append ("/") != errNoError ||
			pbAssetPathName -> append (pDateTime) != errNoError ||
			pbAssetPathName -> append (pRelativePath) != errNoError ||
			pbAssetPathName -> append (pLocalFileName) != errNoError)
		{
			Error err = ToolsErrors (__FILE__, __LINE__,
				TOOLS_BUFFER_SETBUFFER_FAILED);
			_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
				(const char *) err, __FILE__, __LINE__);

			return err;
		}

		if (bRemoveLinuxPathIfExist)
		{
			FileIO:: DirectoryEntryType_t	detSourceFileType;
			Error_t					errFileIO;


			if ((errFileIO = FileIO:: getDirectoryEntryType (
				pbAssetPathName -> str(), &detSourceFileType)) !=
				errNoError)
			{
//				 * the entry does not exist
//				 *
//				_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
//					(const char *) errFileIO, __FILE__, __LINE__);
//
//				Error err = ToolsErrors (__FILE__, __LINE__,
//					TOOLS_FILEIO_GETDIRECTORYENTRYTYPE_FAILED,
//					1, (const char *) bContentPathName);
//				_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
//					(const char *) err, __FILE__, __LINE__);
//
//				return err;
			}
			else
			{
				if (detSourceFileType == FileIO:: TOOLS_FILEIO_DIRECTORY)
				{
					{
						Message msg = CMSRepositoryMessages (
							__FILE__, __LINE__,
							CMSREP_CMSREPOSITORY_REMOVEDIRECTORY,
							2,
							pCustomerDirectoryName,
							pbAssetPathName -> str());
						_ptSystemTracer -> trace (Tracer:: TRACER_LINFO,
							(const char *) msg, __FILE__, __LINE__);
					}

					if ((errFileIO = FileIO:: removeDirectory (
						pbAssetPathName -> str(), true)) != errNoError)
					{
						_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
							(const char *) errFileIO, __FILE__, __LINE__);

						Error err = ToolsErrors (__FILE__, __LINE__,
							TOOLS_FILEIO_REMOVEDIRECTORY_FAILED,
							1, pbAssetPathName -> str());
						_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
							(const char *) err, __FILE__, __LINE__);

						return err;
					}
				}
				else if (detSourceFileType == FileIO:: TOOLS_FILEIO_REGULARFILE)
				{
					{
						Message msg = CMSRepositoryMessages (__FILE__, __LINE__,
							CMSREP_CMSREPOSITORY_REMOVEFILE,
							2, pCustomerDirectoryName,
							pbAssetPathName -> str());
						_ptSystemTracer -> trace (Tracer:: TRACER_LINFO,
							(const char *) msg, __FILE__, __LINE__);
					}

					if ((errFileIO = FileIO:: remove (
						pbAssetPathName -> str())) != errNoError)
					{
						_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
							(const char *) errFileIO, __FILE__, __LINE__);

						Error err = ToolsErrors (__FILE__, __LINE__,
							TOOLS_FILEIO_REMOVE_FAILED,
							1, pbAssetPathName -> str());
						_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
							(const char *) err, __FILE__, __LINE__);

						return err;
					}
				}
				else
				{
					Error err = CMSRepositoryErrors (__FILE__, __LINE__,
						CMSREP_CMSREPOSITORY_UNEXPECTEDFILEINSTAGING,
						1, pbAssetPathName -> str());
					_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
						(const char *) err, __FILE__, __LINE__);

					return err;
				}
			}
		}
	}


	return errNoError;
}


Error CMSRepository:: getEncodingProfilePathName (
	Buffer_p pbEncodingProfilePathName,
	long long llEncodingProfileKey,
	const char *pProfileFileNameExtension,
	Boolean_t bIsFromXOEMachine)

{
	if (bIsFromXOEMachine)
	{
		if (
			pbEncodingProfilePathName -> setBuffer (
				(const char *) _bProfilesRootDirectoryFromXOEMachine) !=
				errNoError ||
			pbEncodingProfilePathName -> append (llEncodingProfileKey) !=
				errNoError ||
			pbEncodingProfilePathName -> append (pProfileFileNameExtension) !=
				errNoError)
		{
			Error err = ToolsErrors (__FILE__, __LINE__,
				TOOLS_BUFFER_SETBUFFER_FAILED);
			_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
				(const char *) err, __FILE__, __LINE__);

			return err;
		}
	}
	else
	{
		if (
			pbEncodingProfilePathName -> setBuffer (
				(const char *) _bProfilesRootRepository) !=
				errNoError ||
			pbEncodingProfilePathName -> append (llEncodingProfileKey) !=
				errNoError ||
			pbEncodingProfilePathName -> append (pProfileFileNameExtension) !=
				errNoError)
		{
			Error err = ToolsErrors (__FILE__, __LINE__,
				TOOLS_BUFFER_SETBUFFER_FAILED);
			_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
				(const char *) err, __FILE__, __LINE__);

			return err;
		}
	}


	return errNoError;
}


Error CMSRepository:: getFFMPEGEncodingProfilePathName (
	unsigned long ulContentType,
	Buffer_p pbEncodingProfilePathName,
	long long llEncodingProfileKey)

{

	if (ulContentType != 0 && ulContentType != 1 && ulContentType != 2 &&
		ulContentType != 4)		// video/audio/image/ringtone
	{
		Error err = CMSRepositoryErrors (__FILE__, __LINE__,
			CMSREP_ACTIVATION_WRONG);
		_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
			(const char *) err, __FILE__, __LINE__);

		return err;
	}

	if (
		pbEncodingProfilePathName -> setBuffer (
			(const char *) _bProfilesRootRepository) !=
			errNoError ||
		pbEncodingProfilePathName -> append (llEncodingProfileKey) !=
			errNoError)
	{
		Error err = ToolsErrors (__FILE__, __LINE__,
			TOOLS_BUFFER_SETBUFFER_FAILED);
		_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
			(const char *) err, __FILE__, __LINE__);

		return err;
	}

	if (ulContentType == 0)		// video
	{
		if (pbEncodingProfilePathName -> append (".vep") != errNoError)
		{
			Error err = ToolsErrors (__FILE__, __LINE__,
				TOOLS_BUFFER_SETBUFFER_FAILED);
			_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
				(const char *) err, __FILE__, __LINE__);

			return err;
		}
	}
	else if (ulContentType == 1 || ulContentType == 4)		// audio / ringtone
	{
		if (pbEncodingProfilePathName -> append (".aep") != errNoError)
		{
			Error err = ToolsErrors (__FILE__, __LINE__,
				TOOLS_BUFFER_SETBUFFER_FAILED);
			_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
				(const char *) err, __FILE__, __LINE__);

			return err;
		}
	}
	else if (ulContentType == 2)	// image
	{
		if (pbEncodingProfilePathName -> append (".iep") != errNoError)
		{
			Error err = ToolsErrors (__FILE__, __LINE__,
				TOOLS_BUFFER_SETBUFFER_FAILED);
			_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
				(const char *) err, __FILE__, __LINE__);

			return err;
		}
	}


	return errNoError;
}


Error CMSRepository:: getCustomerStorageUsage (
	const char *pCustomerDirectoryName,
	unsigned long *pulStorageUsageInMB)	// OUT

{

	unsigned long				ulCMSPartitionIndex;
	unsigned long long			ullDirectoryUsageInBytes;
	unsigned long long			ullCustomerStorageUsageInBytes;
	Error_t						errFileIO;
	Buffer_t					bContentProviderPathName;


	if (pCustomerDirectoryName == (const char *) NULL ||
		pulStorageUsageInMB == (unsigned long *) NULL)
	{
		Error err = CMSRepositoryErrors (__FILE__, __LINE__,
			CMSREP_ACTIVATION_WRONG);
		_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
			(const char *) err, __FILE__, __LINE__);

		return err;
	}

	if (bContentProviderPathName. init () != errNoError)
	{
		Error err = ToolsErrors (__FILE__, __LINE__,
			TOOLS_BUFFER_INIT_FAILED);
		_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
			(const char *) err, __FILE__, __LINE__);

		return err;
	}

	if (_mtCMSPartitions. lock () != errNoError)
	{
		Error err = PThreadErrors (__FILE__, __LINE__,
			THREADLIB_PMUTEX_LOCK_FAILED);
		_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
			(const char *) err, __FILE__, __LINE__);

		if (bContentProviderPathName. finish () != errNoError)
		{
			Error err = ToolsErrors (__FILE__, __LINE__,
				TOOLS_BUFFER_FINISH_FAILED);
			_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
				(const char *) err, __FILE__, __LINE__);
		}

		return err;
	}

	ullCustomerStorageUsageInBytes			= 0;

	for (ulCMSPartitionIndex = 0;
		ulCMSPartitionIndex < _ulCMSPartitionsNumber;
		ulCMSPartitionIndex++)
	{
		if (getCMSAssetPathName (&bContentProviderPathName,
			ulCMSPartitionIndex, pCustomerDirectoryName,
			"", "", false) != errNoError)
		{
			Error err = CMSRepositoryErrors (__FILE__, __LINE__,
				CMSREP_CMSREPOSITORY_GETCMSASSETPATHNAME_FAILED);
			_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
				(const char *) err, __FILE__, __LINE__);

			if (_mtCMSPartitions. unLock () != errNoError)
			{
				Error err = PThreadErrors (__FILE__, __LINE__,
					THREADLIB_PMUTEX_UNLOCK_FAILED);
				_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
					(const char *) err, __FILE__, __LINE__);
			}

			if (bContentProviderPathName. finish () != errNoError)
			{
				Error err = ToolsErrors (__FILE__, __LINE__,
					TOOLS_BUFFER_FINISH_FAILED);
				_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
					(const char *) err, __FILE__, __LINE__);
			}

			return err;
		}

		if ((errFileIO = FileIO:: getDirectoryUsage (
			(const char *) bContentProviderPathName,
			&ullDirectoryUsageInBytes)) != errNoError)
		{
			if ((unsigned long) errFileIO == TOOLS_FILEIO_DIRECTORYNOTEXISTING)
			{

				continue;
			}
			else
			{
				_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
					(const char *) errFileIO, __FILE__, __LINE__);

				Error err = ToolsErrors (__FILE__, __LINE__,
					TOOLS_FILEIO_GETDIRECTORYUSAGE_FAILED,
					1, (const char *) bContentProviderPathName);
				_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
					(const char *) err, __FILE__, __LINE__);

				if (_mtCMSPartitions. unLock () != errNoError)
				{
					Error err = PThreadErrors (__FILE__, __LINE__,
						THREADLIB_PMUTEX_UNLOCK_FAILED);
					_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
						(const char *) err, __FILE__, __LINE__);
				}

				if (bContentProviderPathName. finish () != errNoError)
				{
					Error err = ToolsErrors (__FILE__, __LINE__,
						TOOLS_BUFFER_FINISH_FAILED);
					_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
						(const char *) err, __FILE__, __LINE__);
				}

				return errFileIO;
			}
		}

		ullCustomerStorageUsageInBytes		+= ullDirectoryUsageInBytes;
	}

	if (_mtCMSPartitions. unLock () != errNoError)
	{
		Error err = PThreadErrors (__FILE__, __LINE__,
			THREADLIB_PMUTEX_UNLOCK_FAILED);
		_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
			(const char *) err, __FILE__, __LINE__);

		if (bContentProviderPathName. finish () != errNoError)
		{
			Error err = ToolsErrors (__FILE__, __LINE__,
				TOOLS_BUFFER_FINISH_FAILED);
			_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
				(const char *) err, __FILE__, __LINE__);
		}

		return err;
	}

	if (bContentProviderPathName. finish () != errNoError)
	{
		Error err = ToolsErrors (__FILE__, __LINE__,
			TOOLS_BUFFER_FINISH_FAILED);
		_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
			(const char *) err, __FILE__, __LINE__);

		return err;
	}

	*pulStorageUsageInMB				= (unsigned long)
		(ullCustomerStorageUsageInBytes / (1024 * 1024));


	return errNoError;
}


Error CMSRepository:: saveSanityCheckLastProcessedContent (
	const char *pFilePathName)

{

	int					iFileDescriptor;
	long long			llBytesWritten;




	if (pFilePathName == (const char *) NULL)
	{
		Error err = ToolsErrors (__FILE__, __LINE__,
			TOOLS_ACTIVATION_WRONG);
		_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
			(const char *) err, __FILE__, __LINE__);

		return err;
	}

	#ifdef WIN32
		if (FileIO:: open (pFilePathName,
			O_WRONLY | O_TRUNC | O_CREAT,
			_S_IREAD | _S_IWRITE, &iFileDescriptor) != errNoError)
	#else
		if (FileIO:: open (pFilePathName,
			O_WRONLY | O_TRUNC | O_CREAT,
			S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH,
			&iFileDescriptor) != errNoError)
	#endif
	{
		Error err = ToolsErrors (__FILE__, __LINE__,
			TOOLS_FILEIO_OPEN_FAILED, 1, pFilePathName);
		_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
			(const char *) err, __FILE__, __LINE__);

		return err;
	}

	// Repository: CMSREP_REPOSITORYTYPE_CMSCUSTOMER
	{
		{
			Message msg = CMSRepositoryMessages (
				__FILE__, __LINE__, 
				CMSREP_CMSREPOSITORY_SAVINGSANITYCHECKINFO,
				4,
				(long) CMSREP_REPOSITORYTYPE_CMSCUSTOMER,
				_svlpcLastProcessedContent [CMSREP_REPOSITORYTYPE_CMSCUSTOMER].
					_pPartition,
				_svlpcLastProcessedContent [CMSREP_REPOSITORYTYPE_CMSCUSTOMER].
					_pCustomerDirectoryName,
				_svlpcLastProcessedContent [CMSREP_REPOSITORYTYPE_CMSCUSTOMER].
					_ulFilesNumberAlreadyProcessed);
			_ptSystemTracer -> trace (Tracer:: TRACER_LINFO,
				(const char *) msg, __FILE__, __LINE__);
		}

		if (FileIO:: writeChars (iFileDescriptor,
			(char *)
				(_svlpcLastProcessedContent [CMSREP_REPOSITORYTYPE_CMSCUSTOMER].
				_pPartition),
			CMSREP_CMSREPOSITORY_MAXCMSPARTITIONNAMELENGTH,
			&llBytesWritten) != errNoError)
		{
			Error err = ToolsErrors (__FILE__, __LINE__,
				TOOLS_FILEIO_WRITECHARS_FAILED);
			_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
				(const char *) err, __FILE__, __LINE__);

			FileIO:: close (iFileDescriptor);

			return err;
		}

		if (FileIO:: writeChars (iFileDescriptor,
			(char *) "\n",
			1,
			&llBytesWritten) != errNoError)
		{
			Error err = ToolsErrors (__FILE__, __LINE__,
				TOOLS_FILEIO_WRITECHARS_FAILED);
			_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
				(const char *) err, __FILE__, __LINE__);

			FileIO:: close (iFileDescriptor);

			return err;
		}

		if (FileIO:: writeChars (iFileDescriptor,
			(char *)
				(_svlpcLastProcessedContent [CMSREP_REPOSITORYTYPE_CMSCUSTOMER].
				_pCustomerDirectoryName),
			CMSREP_CMSREPOSITORY_MAXCUSTOMERNAMELENGTH,
			&llBytesWritten) != errNoError)
		{
			Error err = ToolsErrors (__FILE__, __LINE__,
				TOOLS_FILEIO_WRITECHARS_FAILED);
			_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
				(const char *) err, __FILE__, __LINE__);

			FileIO:: close (iFileDescriptor);

			return err;
		}

		if (FileIO:: writeChars (iFileDescriptor,
			(char *) "\n",
			1,
			&llBytesWritten) != errNoError)
		{
			Error err = ToolsErrors (__FILE__, __LINE__,
				TOOLS_FILEIO_WRITECHARS_FAILED);
			_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
				(const char *) err, __FILE__, __LINE__);

			FileIO:: close (iFileDescriptor);

			return err;
		}

		{
			char				pUnsignedLongBuffer [128];


			memset (pUnsignedLongBuffer, '\0', 128);

			if (sprintf (pUnsignedLongBuffer, "%lu",
				(_svlpcLastProcessedContent [CMSREP_REPOSITORYTYPE_CMSCUSTOMER].
				_ulFilesNumberAlreadyProcessed)) < 0)
			{
				Error err = ToolsErrors (__FILE__, __LINE__,
					TOOLS_SPRINTF_FAILED);
				_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
					(const char *) err, __FILE__, __LINE__);

				FileIO:: close (iFileDescriptor);

				return err;
			}

			if (FileIO:: writeChars (iFileDescriptor,
				(char *) pUnsignedLongBuffer,
				128,
				&llBytesWritten) != errNoError)
			{
				Error err = ToolsErrors (__FILE__, __LINE__,
					TOOLS_FILEIO_WRITECHARS_FAILED);
				_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
					(const char *) err, __FILE__, __LINE__);

				FileIO:: close (iFileDescriptor);

				return err;
			}

			if (FileIO:: writeChars (iFileDescriptor,
				(char *) "\n",
				1,
				&llBytesWritten) != errNoError)
			{
				Error err = ToolsErrors (__FILE__, __LINE__,
					TOOLS_FILEIO_WRITECHARS_FAILED);
				_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
					(const char *) err, __FILE__, __LINE__);

				FileIO:: close (iFileDescriptor);

				return err;
			}
		}
	}

	// Repository: CMSREP_REPOSITORYTYPE_DOWNLOAD
	{
	{
		Message msg = CMSRepositoryMessages (
			__FILE__, __LINE__, 
			CMSREP_CMSREPOSITORY_SAVINGSANITYCHECKINFO,
			4,
			(long) CMSREP_REPOSITORYTYPE_DOWNLOAD,
			_svlpcLastProcessedContent [CMSREP_REPOSITORYTYPE_DOWNLOAD].
				_pPartition,
			_svlpcLastProcessedContent [CMSREP_REPOSITORYTYPE_DOWNLOAD].
				_pCustomerDirectoryName,
			_svlpcLastProcessedContent [CMSREP_REPOSITORYTYPE_DOWNLOAD].
				_ulFilesNumberAlreadyProcessed);
		_ptSystemTracer -> trace (Tracer:: TRACER_LINFO,
			(const char *) msg, __FILE__, __LINE__);
	}

	if (FileIO:: writeChars (iFileDescriptor,
		(char *)
			(_svlpcLastProcessedContent [CMSREP_REPOSITORYTYPE_DOWNLOAD].
			_pPartition),
		CMSREP_CMSREPOSITORY_MAXCMSPARTITIONNAMELENGTH,
		&llBytesWritten) != errNoError)
	{
		Error err = ToolsErrors (__FILE__, __LINE__,
			TOOLS_FILEIO_WRITECHARS_FAILED);
		_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
			(const char *) err, __FILE__, __LINE__);

		FileIO:: close (iFileDescriptor);

		return err;
	}

	if (FileIO:: writeChars (iFileDescriptor,
		(char *) "\n",
		1,
		&llBytesWritten) != errNoError)
	{
		Error err = ToolsErrors (__FILE__, __LINE__,
			TOOLS_FILEIO_WRITECHARS_FAILED);
		_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
			(const char *) err, __FILE__, __LINE__);

		FileIO:: close (iFileDescriptor);

		return err;
	}

	if (FileIO:: writeChars (iFileDescriptor,
		(char *)
			(_svlpcLastProcessedContent [CMSREP_REPOSITORYTYPE_DOWNLOAD].
			_pCustomerDirectoryName),
		CMSREP_CMSREPOSITORY_MAXCUSTOMERNAMELENGTH,
		&llBytesWritten) != errNoError)
	{
		Error err = ToolsErrors (__FILE__, __LINE__,
			TOOLS_FILEIO_WRITECHARS_FAILED);
		_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
			(const char *) err, __FILE__, __LINE__);

		FileIO:: close (iFileDescriptor);

		return err;
	}

	if (FileIO:: writeChars (iFileDescriptor,
		(char *) "\n",
		1,
		&llBytesWritten) != errNoError)
	{
		Error err = ToolsErrors (__FILE__, __LINE__,
			TOOLS_FILEIO_WRITECHARS_FAILED);
		_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
			(const char *) err, __FILE__, __LINE__);

		FileIO:: close (iFileDescriptor);

		return err;
	}

	{
		char				pUnsignedLongBuffer [128];


		memset (pUnsignedLongBuffer, '\0', 128);

		if (sprintf (pUnsignedLongBuffer, "%lu",
			(_svlpcLastProcessedContent [CMSREP_REPOSITORYTYPE_DOWNLOAD].
			_ulFilesNumberAlreadyProcessed)) < 0)
		{
			Error err = ToolsErrors (__FILE__, __LINE__,
				TOOLS_SPRINTF_FAILED);
			_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
				(const char *) err, __FILE__, __LINE__);

			FileIO:: close (iFileDescriptor);

			return err;
		}

		if (FileIO:: writeChars (iFileDescriptor,
			(char *) pUnsignedLongBuffer,
			128,
			&llBytesWritten) != errNoError)
		{
			Error err = ToolsErrors (__FILE__, __LINE__,
				TOOLS_FILEIO_WRITECHARS_FAILED);
			_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
				(const char *) err, __FILE__, __LINE__);

			FileIO:: close (iFileDescriptor);

			return err;
		}

		if (FileIO:: writeChars (iFileDescriptor,
			(char *) "\n",
			1,
			&llBytesWritten) != errNoError)
		{
			Error err = ToolsErrors (__FILE__, __LINE__,
				TOOLS_FILEIO_WRITECHARS_FAILED);
			_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
				(const char *) err, __FILE__, __LINE__);

			FileIO:: close (iFileDescriptor);

			return err;
		}
	}
	}

	// Repository: CMSREP_REPOSITORYTYPE_STREAMING
	{
	{
		Message msg = CMSRepositoryMessages (
			__FILE__, __LINE__, 
			CMSREP_CMSREPOSITORY_SAVINGSANITYCHECKINFO,
			4,
			(long) CMSREP_REPOSITORYTYPE_STREAMING,
			_svlpcLastProcessedContent [CMSREP_REPOSITORYTYPE_STREAMING].
				_pPartition,
			_svlpcLastProcessedContent [CMSREP_REPOSITORYTYPE_STREAMING].
				_pCustomerDirectoryName,
			_svlpcLastProcessedContent [CMSREP_REPOSITORYTYPE_STREAMING].
				_ulFilesNumberAlreadyProcessed);
		_ptSystemTracer -> trace (Tracer:: TRACER_LINFO,
			(const char *) msg, __FILE__, __LINE__);
	}

	if (FileIO:: writeChars (iFileDescriptor,
		(char *)
			(_svlpcLastProcessedContent [CMSREP_REPOSITORYTYPE_STREAMING].
			_pPartition),
		CMSREP_CMSREPOSITORY_MAXCMSPARTITIONNAMELENGTH,
		&llBytesWritten) != errNoError)
	{
		Error err = ToolsErrors (__FILE__, __LINE__,
			TOOLS_FILEIO_WRITECHARS_FAILED);
		_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
			(const char *) err, __FILE__, __LINE__);

		FileIO:: close (iFileDescriptor);

		return err;
	}

	if (FileIO:: writeChars (iFileDescriptor,
		(char *) "\n",
		1,
		&llBytesWritten) != errNoError)
	{
		Error err = ToolsErrors (__FILE__, __LINE__,
			TOOLS_FILEIO_WRITECHARS_FAILED);
		_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
			(const char *) err, __FILE__, __LINE__);

		FileIO:: close (iFileDescriptor);

		return err;
	}

	if (FileIO:: writeChars (iFileDescriptor,
		(char *)
			(_svlpcLastProcessedContent [CMSREP_REPOSITORYTYPE_STREAMING].
			_pCustomerDirectoryName),
		CMSREP_CMSREPOSITORY_MAXCUSTOMERNAMELENGTH,
		&llBytesWritten) != errNoError)
	{
		Error err = ToolsErrors (__FILE__, __LINE__,
			TOOLS_FILEIO_WRITECHARS_FAILED);
		_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
			(const char *) err, __FILE__, __LINE__);

		FileIO:: close (iFileDescriptor);

		return err;
	}

	if (FileIO:: writeChars (iFileDescriptor,
		(char *) "\n",
		1,
		&llBytesWritten) != errNoError)
	{
		Error err = ToolsErrors (__FILE__, __LINE__,
			TOOLS_FILEIO_WRITECHARS_FAILED);
		_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
			(const char *) err, __FILE__, __LINE__);

		FileIO:: close (iFileDescriptor);

		return err;
	}

	{
		char				pUnsignedLongBuffer [128];


		memset (pUnsignedLongBuffer, '\0', 128);

		if (sprintf (pUnsignedLongBuffer, "%lu",
			(_svlpcLastProcessedContent [CMSREP_REPOSITORYTYPE_STREAMING].
			_ulFilesNumberAlreadyProcessed)) < 0)
		{
			Error err = ToolsErrors (__FILE__, __LINE__,
				TOOLS_SPRINTF_FAILED);
			_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
				(const char *) err, __FILE__, __LINE__);

			FileIO:: close (iFileDescriptor);

			return err;
		}

		if (FileIO:: writeChars (iFileDescriptor,
			(char *) pUnsignedLongBuffer,
			128,
			&llBytesWritten) != errNoError)
		{
			Error err = ToolsErrors (__FILE__, __LINE__,
				TOOLS_FILEIO_WRITECHARS_FAILED);
			_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
				(const char *) err, __FILE__, __LINE__);

			FileIO:: close (iFileDescriptor);

			return err;
		}

		if (FileIO:: writeChars (iFileDescriptor,
			(char *) "\n",
			1,
			&llBytesWritten) != errNoError)
		{
			Error err = ToolsErrors (__FILE__, __LINE__,
				TOOLS_FILEIO_WRITECHARS_FAILED);
			_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
				(const char *) err, __FILE__, __LINE__);

			FileIO:: close (iFileDescriptor);

			return err;
		}
	}
	}

	// Repository: CMSREP_REPOSITORYTYPE_STAGING
	{
	{
		Message msg = CMSRepositoryMessages (
			__FILE__, __LINE__, 
			CMSREP_CMSREPOSITORY_SAVINGSANITYCHECKINFO,
			4,
			(long) CMSREP_REPOSITORYTYPE_STAGING,
			_svlpcLastProcessedContent [CMSREP_REPOSITORYTYPE_STAGING].
				_pPartition,
			_svlpcLastProcessedContent [CMSREP_REPOSITORYTYPE_STAGING].
				_pCustomerDirectoryName,
			_svlpcLastProcessedContent [CMSREP_REPOSITORYTYPE_STAGING].
				_ulFilesNumberAlreadyProcessed);
		_ptSystemTracer -> trace (Tracer:: TRACER_LINFO,
			(const char *) msg, __FILE__, __LINE__);
	}

	if (FileIO:: writeChars (iFileDescriptor,
		(char *)
			(_svlpcLastProcessedContent [CMSREP_REPOSITORYTYPE_STAGING].
			_pPartition),
		CMSREP_CMSREPOSITORY_MAXCMSPARTITIONNAMELENGTH,
		&llBytesWritten) != errNoError)
	{
		Error err = ToolsErrors (__FILE__, __LINE__,
			TOOLS_FILEIO_WRITECHARS_FAILED);
		_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
			(const char *) err, __FILE__, __LINE__);

		FileIO:: close (iFileDescriptor);

		return err;
	}

	if (FileIO:: writeChars (iFileDescriptor,
		(char *) "\n",
		1,
		&llBytesWritten) != errNoError)
	{
		Error err = ToolsErrors (__FILE__, __LINE__,
			TOOLS_FILEIO_WRITECHARS_FAILED);
		_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
			(const char *) err, __FILE__, __LINE__);

		FileIO:: close (iFileDescriptor);

		return err;
	}

	if (FileIO:: writeChars (iFileDescriptor,
		(char *)
			(_svlpcLastProcessedContent [CMSREP_REPOSITORYTYPE_STAGING].
			_pCustomerDirectoryName),
		CMSREP_CMSREPOSITORY_MAXCUSTOMERNAMELENGTH,
		&llBytesWritten) != errNoError)
	{
		Error err = ToolsErrors (__FILE__, __LINE__,
			TOOLS_FILEIO_WRITECHARS_FAILED);
		_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
			(const char *) err, __FILE__, __LINE__);

		FileIO:: close (iFileDescriptor);

		return err;
	}

	if (FileIO:: writeChars (iFileDescriptor,
		(char *) "\n",
		1,
		&llBytesWritten) != errNoError)
	{
		Error err = ToolsErrors (__FILE__, __LINE__,
			TOOLS_FILEIO_WRITECHARS_FAILED);
		_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
			(const char *) err, __FILE__, __LINE__);

		FileIO:: close (iFileDescriptor);

		return err;
	}

	{
		char				pUnsignedLongBuffer [128];


		memset (pUnsignedLongBuffer, '\0', 128);

		if (sprintf (pUnsignedLongBuffer, "%lu",
			(_svlpcLastProcessedContent [CMSREP_REPOSITORYTYPE_STAGING].
			_ulFilesNumberAlreadyProcessed)) < 0)
		{
			Error err = ToolsErrors (__FILE__, __LINE__,
				TOOLS_SPRINTF_FAILED);
			_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
				(const char *) err, __FILE__, __LINE__);

			FileIO:: close (iFileDescriptor);

			return err;
		}

		if (FileIO:: writeChars (iFileDescriptor,
			(char *) pUnsignedLongBuffer,
			128,
			&llBytesWritten) != errNoError)
		{
			Error err = ToolsErrors (__FILE__, __LINE__,
				TOOLS_FILEIO_WRITECHARS_FAILED);
			_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
				(const char *) err, __FILE__, __LINE__);

			FileIO:: close (iFileDescriptor);

			return err;
		}

		if (FileIO:: writeChars (iFileDescriptor,
			(char *) "\n",
			1,
			&llBytesWritten) != errNoError)
		{
			Error err = ToolsErrors (__FILE__, __LINE__,
				TOOLS_FILEIO_WRITECHARS_FAILED);
			_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
				(const char *) err, __FILE__, __LINE__);

			FileIO:: close (iFileDescriptor);

			return err;
		}
	}
	}

	// Repository: CMSREP_REPOSITORYTYPE_DONE
	{
	{
		Message msg = CMSRepositoryMessages (
			__FILE__, __LINE__, 
			CMSREP_CMSREPOSITORY_SAVINGSANITYCHECKINFO,
			4,
			(long) CMSREP_REPOSITORYTYPE_DONE,
			_svlpcLastProcessedContent [CMSREP_REPOSITORYTYPE_DONE].
				_pPartition,
			_svlpcLastProcessedContent [CMSREP_REPOSITORYTYPE_DONE].
				_pCustomerDirectoryName,
			_svlpcLastProcessedContent [CMSREP_REPOSITORYTYPE_DONE].
				_ulFilesNumberAlreadyProcessed);
		_ptSystemTracer -> trace (Tracer:: TRACER_LINFO,
			(const char *) msg, __FILE__, __LINE__);
	}

	if (FileIO:: writeChars (iFileDescriptor,
		(char *)
			(_svlpcLastProcessedContent [CMSREP_REPOSITORYTYPE_DONE].
			_pPartition),
		CMSREP_CMSREPOSITORY_MAXCMSPARTITIONNAMELENGTH,
		&llBytesWritten) != errNoError)
	{
		Error err = ToolsErrors (__FILE__, __LINE__,
			TOOLS_FILEIO_WRITECHARS_FAILED);
		_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
			(const char *) err, __FILE__, __LINE__);

		FileIO:: close (iFileDescriptor);

		return err;
	}

	if (FileIO:: writeChars (iFileDescriptor,
		(char *) "\n",
		1,
		&llBytesWritten) != errNoError)
	{
		Error err = ToolsErrors (__FILE__, __LINE__,
			TOOLS_FILEIO_WRITECHARS_FAILED);
		_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
			(const char *) err, __FILE__, __LINE__);

		FileIO:: close (iFileDescriptor);

		return err;
	}

	if (FileIO:: writeChars (iFileDescriptor,
		(char *)
			(_svlpcLastProcessedContent [CMSREP_REPOSITORYTYPE_DONE].
			_pCustomerDirectoryName),
		CMSREP_CMSREPOSITORY_MAXCUSTOMERNAMELENGTH,
		&llBytesWritten) != errNoError)
	{
		Error err = ToolsErrors (__FILE__, __LINE__,
			TOOLS_FILEIO_WRITECHARS_FAILED);
		_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
			(const char *) err, __FILE__, __LINE__);

		FileIO:: close (iFileDescriptor);

		return err;
	}

	if (FileIO:: writeChars (iFileDescriptor,
		(char *) "\n",
		1,
		&llBytesWritten) != errNoError)
	{
		Error err = ToolsErrors (__FILE__, __LINE__,
			TOOLS_FILEIO_WRITECHARS_FAILED);
		_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
			(const char *) err, __FILE__, __LINE__);

		FileIO:: close (iFileDescriptor);

		return err;
	}

	{
		char				pUnsignedLongBuffer [128];


		memset (pUnsignedLongBuffer, '\0', 128);

		if (sprintf (pUnsignedLongBuffer, "%lu",
			(_svlpcLastProcessedContent [CMSREP_REPOSITORYTYPE_DONE].
			_ulFilesNumberAlreadyProcessed)) < 0)
		{
			Error err = ToolsErrors (__FILE__, __LINE__,
				TOOLS_SPRINTF_FAILED);
			_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
				(const char *) err, __FILE__, __LINE__);

			FileIO:: close (iFileDescriptor);

			return err;
		}

		if (FileIO:: writeChars (iFileDescriptor,
			(char *) pUnsignedLongBuffer,
			128,
			&llBytesWritten) != errNoError)
		{
			Error err = ToolsErrors (__FILE__, __LINE__,
				TOOLS_FILEIO_WRITECHARS_FAILED);
			_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
				(const char *) err, __FILE__, __LINE__);

			FileIO:: close (iFileDescriptor);

			return err;
		}

		if (FileIO:: writeChars (iFileDescriptor,
			(char *) "\n",
			1,
			&llBytesWritten) != errNoError)
		{
			Error err = ToolsErrors (__FILE__, __LINE__,
				TOOLS_FILEIO_WRITECHARS_FAILED);
			_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
				(const char *) err, __FILE__, __LINE__);

			FileIO:: close (iFileDescriptor);

			return err;
		}
	}
	}

	// Repository: CMSREP_REPOSITORYTYPE_ERRORS
	{
	{
		Message msg = CMSRepositoryMessages (
			__FILE__, __LINE__, 
			CMSREP_CMSREPOSITORY_SAVINGSANITYCHECKINFO,
			4,
			(long) CMSREP_REPOSITORYTYPE_ERRORS,
			_svlpcLastProcessedContent [CMSREP_REPOSITORYTYPE_ERRORS].
				_pPartition,
			_svlpcLastProcessedContent [CMSREP_REPOSITORYTYPE_ERRORS].
				_pCustomerDirectoryName,
			_svlpcLastProcessedContent [CMSREP_REPOSITORYTYPE_ERRORS].
				_ulFilesNumberAlreadyProcessed);
		_ptSystemTracer -> trace (Tracer:: TRACER_LINFO,
			(const char *) msg, __FILE__, __LINE__);
	}

	if (FileIO:: writeChars (iFileDescriptor,
		(char *)
			(_svlpcLastProcessedContent [CMSREP_REPOSITORYTYPE_ERRORS].
			_pPartition),
		CMSREP_CMSREPOSITORY_MAXCMSPARTITIONNAMELENGTH,
		&llBytesWritten) != errNoError)
	{
		Error err = ToolsErrors (__FILE__, __LINE__,
			TOOLS_FILEIO_WRITECHARS_FAILED);
		_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
			(const char *) err, __FILE__, __LINE__);

		FileIO:: close (iFileDescriptor);

		return err;
	}

	if (FileIO:: writeChars (iFileDescriptor,
		(char *) "\n",
		1,
		&llBytesWritten) != errNoError)
	{
		Error err = ToolsErrors (__FILE__, __LINE__,
			TOOLS_FILEIO_WRITECHARS_FAILED);
		_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
			(const char *) err, __FILE__, __LINE__);

		FileIO:: close (iFileDescriptor);

		return err;
	}

	if (FileIO:: writeChars (iFileDescriptor,
		(char *)
			(_svlpcLastProcessedContent [CMSREP_REPOSITORYTYPE_ERRORS].
			_pCustomerDirectoryName),
		CMSREP_CMSREPOSITORY_MAXCUSTOMERNAMELENGTH,
		&llBytesWritten) != errNoError)
	{
		Error err = ToolsErrors (__FILE__, __LINE__,
			TOOLS_FILEIO_WRITECHARS_FAILED);
		_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
			(const char *) err, __FILE__, __LINE__);

		FileIO:: close (iFileDescriptor);

		return err;
	}

	if (FileIO:: writeChars (iFileDescriptor,
		(char *) "\n",
		1,
		&llBytesWritten) != errNoError)
	{
		Error err = ToolsErrors (__FILE__, __LINE__,
			TOOLS_FILEIO_WRITECHARS_FAILED);
		_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
			(const char *) err, __FILE__, __LINE__);

		FileIO:: close (iFileDescriptor);

		return err;
	}

	{
		char				pUnsignedLongBuffer [128];


		memset (pUnsignedLongBuffer, '\0', 128);

		if (sprintf (pUnsignedLongBuffer, "%lu",
			(_svlpcLastProcessedContent [CMSREP_REPOSITORYTYPE_ERRORS].
			_ulFilesNumberAlreadyProcessed)) < 0)
		{
			Error err = ToolsErrors (__FILE__, __LINE__,
				TOOLS_SPRINTF_FAILED);
			_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
				(const char *) err, __FILE__, __LINE__);

			FileIO:: close (iFileDescriptor);

			return err;
		}

		if (FileIO:: writeChars (iFileDescriptor,
			(char *) pUnsignedLongBuffer,
			128,
			&llBytesWritten) != errNoError)
		{
			Error err = ToolsErrors (__FILE__, __LINE__,
				TOOLS_FILEIO_WRITECHARS_FAILED);
			_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
				(const char *) err, __FILE__, __LINE__);

			FileIO:: close (iFileDescriptor);

			return err;
		}

		if (FileIO:: writeChars (iFileDescriptor,
			(char *) "\n",
			1,
			&llBytesWritten) != errNoError)
		{
			Error err = ToolsErrors (__FILE__, __LINE__,
				TOOLS_FILEIO_WRITECHARS_FAILED);
			_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
				(const char *) err, __FILE__, __LINE__);

			FileIO:: close (iFileDescriptor);

			return err;
		}
	}
	}

	// Repository: CMSREP_REPOSITORYTYPE_FTP
	{
	{
		Message msg = CMSRepositoryMessages (
			__FILE__, __LINE__, 
			CMSREP_CMSREPOSITORY_SAVINGSANITYCHECKINFO,
			4,
			(long) CMSREP_REPOSITORYTYPE_FTP,
			_svlpcLastProcessedContent [CMSREP_REPOSITORYTYPE_FTP].
				_pPartition,
			_svlpcLastProcessedContent [CMSREP_REPOSITORYTYPE_FTP].
				_pCustomerDirectoryName,
			_svlpcLastProcessedContent [CMSREP_REPOSITORYTYPE_FTP].
				_ulFilesNumberAlreadyProcessed);
		_ptSystemTracer -> trace (Tracer:: TRACER_LINFO,
			(const char *) msg, __FILE__, __LINE__);
	}

	if (FileIO:: writeChars (iFileDescriptor,
		(char *)
			(_svlpcLastProcessedContent [CMSREP_REPOSITORYTYPE_FTP].
			_pPartition),
		CMSREP_CMSREPOSITORY_MAXCMSPARTITIONNAMELENGTH,
		&llBytesWritten) != errNoError)
	{
		Error err = ToolsErrors (__FILE__, __LINE__,
			TOOLS_FILEIO_WRITECHARS_FAILED);
		_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
			(const char *) err, __FILE__, __LINE__);

		FileIO:: close (iFileDescriptor);

		return err;
	}

	if (FileIO:: writeChars (iFileDescriptor,
		(char *) "\n",
		1,
		&llBytesWritten) != errNoError)
	{
		Error err = ToolsErrors (__FILE__, __LINE__,
			TOOLS_FILEIO_WRITECHARS_FAILED);
		_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
			(const char *) err, __FILE__, __LINE__);

		FileIO:: close (iFileDescriptor);

		return err;
	}

	if (FileIO:: writeChars (iFileDescriptor,
		(char *)
			(_svlpcLastProcessedContent [CMSREP_REPOSITORYTYPE_FTP].
			_pCustomerDirectoryName),
		CMSREP_CMSREPOSITORY_MAXCUSTOMERNAMELENGTH,
		&llBytesWritten) != errNoError)
	{
		Error err = ToolsErrors (__FILE__, __LINE__,
			TOOLS_FILEIO_WRITECHARS_FAILED);
		_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
			(const char *) err, __FILE__, __LINE__);

		FileIO:: close (iFileDescriptor);

		return err;
	}

	if (FileIO:: writeChars (iFileDescriptor,
		(char *) "\n",
		1,
		&llBytesWritten) != errNoError)
	{
		Error err = ToolsErrors (__FILE__, __LINE__,
			TOOLS_FILEIO_WRITECHARS_FAILED);
		_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
			(const char *) err, __FILE__, __LINE__);

		FileIO:: close (iFileDescriptor);

		return err;
	}

	{
		char				pUnsignedLongBuffer [128];


		memset (pUnsignedLongBuffer, '\0', 128);

		if (sprintf (pUnsignedLongBuffer, "%lu",
			(_svlpcLastProcessedContent [CMSREP_REPOSITORYTYPE_FTP].
			_ulFilesNumberAlreadyProcessed)) < 0)
		{
			Error err = ToolsErrors (__FILE__, __LINE__,
				TOOLS_SPRINTF_FAILED);
			_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
				(const char *) err, __FILE__, __LINE__);

			FileIO:: close (iFileDescriptor);

			return err;
		}

		if (FileIO:: writeChars (iFileDescriptor,
			(char *) pUnsignedLongBuffer,
			128,
			&llBytesWritten) != errNoError)
		{
			Error err = ToolsErrors (__FILE__, __LINE__,
				TOOLS_FILEIO_WRITECHARS_FAILED);
			_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
				(const char *) err, __FILE__, __LINE__);

			FileIO:: close (iFileDescriptor);

			return err;
		}

		if (FileIO:: writeChars (iFileDescriptor,
			(char *) "\n",
			1,
			&llBytesWritten) != errNoError)
		{
			Error err = ToolsErrors (__FILE__, __LINE__,
				TOOLS_FILEIO_WRITECHARS_FAILED);
			_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
				(const char *) err, __FILE__, __LINE__);

			FileIO:: close (iFileDescriptor);

			return err;
		}
	}
	}

	if (FileIO:: close (iFileDescriptor) != errNoError)
	{
		Error err = ToolsErrors (__FILE__, __LINE__,
			TOOLS_FILEIO_CLOSE_FAILED);
		_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
			(const char *) err, __FILE__, __LINE__);

		return err;
	}

//	_pulFilesNumberAlreadyProcessed [CMSREP_REPOSITORYTYPE_CMSCUSTOMER]	=
//		pulFilesNumberAlreadyProcessed [CMSREP_REPOSITORYTYPE_CMSCUSTOMER];
//	_pulFilesNumberAlreadyProcessed [CMSREP_REPOSITORYTYPE_DOWNLOAD]		=
//		pulFilesNumberAlreadyProcessed [CMSREP_REPOSITORYTYPE_DOWNLOAD];
//	_pulFilesNumberAlreadyProcessed [CMSREP_REPOSITORYTYPE_STREAMING]	=
//		pulFilesNumberAlreadyProcessed [CMSREP_REPOSITORYTYPE_STREAMING];
//	_pulFilesNumberAlreadyProcessed [CMSREP_REPOSITORYTYPE_STAGING]		=
//		pulFilesNumberAlreadyProcessed [CMSREP_REPOSITORYTYPE_STAGING];
//	_pulFilesNumberAlreadyProcessed [CMSREP_REPOSITORYTYPE_DONE]			=
//		pulFilesNumberAlreadyProcessed [CMSREP_REPOSITORYTYPE_DONE];
//	_pulFilesNumberAlreadyProcessed [CMSREP_REPOSITORYTYPE_ERRORS]		=
//		pulFilesNumberAlreadyProcessed [CMSREP_REPOSITORYTYPE_ERRORS];
//	_pulFilesNumberAlreadyProcessed [CMSREP_REPOSITORYTYPE_FTP]			=
//		pulFilesNumberAlreadyProcessed [CMSREP_REPOSITORYTYPE_FTP];


	return errNoError;
}


Error CMSRepository:: readSanityCheckLastProcessedContent (
	const char *pFilePathName)

{

	int					iFileDescriptor;
	long long			llByteRead;
	char				pEndLine [2];


	if (pFilePathName == (const char *) NULL)
	{
		Error err = ToolsErrors (__FILE__, __LINE__,
			TOOLS_ACTIVATION_WRONG);
		_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
			(const char *) err, __FILE__, __LINE__);

		return err;
	}

	if (FileIO:: open (pFilePathName, O_RDONLY, &iFileDescriptor) != errNoError)
	{
		Error err = ToolsErrors (__FILE__, __LINE__,
			TOOLS_FILEIO_OPEN_FAILED, 1, pFilePathName);
		_ptSystemTracer -> trace (Tracer:: TRACER_LWRNG,
			(const char *) err, __FILE__, __LINE__);

		// Repository: CMSREP_REPOSITORYTYPE_CMSCUSTOMER
		strcpy (_svlpcLastProcessedContent [CMSREP_REPOSITORYTYPE_CMSCUSTOMER].
			_pPartition, "");
		strcpy (_svlpcLastProcessedContent [CMSREP_REPOSITORYTYPE_CMSCUSTOMER].
			_pCustomerDirectoryName, "");
		_svlpcLastProcessedContent [CMSREP_REPOSITORYTYPE_CMSCUSTOMER].
			_ulFilesNumberAlreadyProcessed		= 0;

		// Repository: CMSREP_REPOSITORYTYPE_DOWNLOAD
		strcpy (_svlpcLastProcessedContent [CMSREP_REPOSITORYTYPE_DOWNLOAD].
			_pPartition, "");
		strcpy (_svlpcLastProcessedContent [CMSREP_REPOSITORYTYPE_DOWNLOAD].
			_pCustomerDirectoryName, "");
		_svlpcLastProcessedContent [CMSREP_REPOSITORYTYPE_DOWNLOAD].
			_ulFilesNumberAlreadyProcessed		= 0;

		// Repository: CMSREP_REPOSITORYTYPE_STREAMING
		strcpy (_svlpcLastProcessedContent [CMSREP_REPOSITORYTYPE_STREAMING].
			_pPartition, "");
		strcpy (_svlpcLastProcessedContent [CMSREP_REPOSITORYTYPE_STREAMING].
			_pCustomerDirectoryName, "");
		_svlpcLastProcessedContent [CMSREP_REPOSITORYTYPE_STREAMING].
			_ulFilesNumberAlreadyProcessed		= 0;

		// Repository: CMSREP_REPOSITORYTYPE_STAGING
		strcpy (_svlpcLastProcessedContent [CMSREP_REPOSITORYTYPE_STAGING].
			_pPartition, "");
		strcpy (_svlpcLastProcessedContent [CMSREP_REPOSITORYTYPE_STAGING].
			_pCustomerDirectoryName, "");
		_svlpcLastProcessedContent [CMSREP_REPOSITORYTYPE_STAGING].
			_ulFilesNumberAlreadyProcessed		= 0;

		// Repository: CMSREP_REPOSITORYTYPE_DONE
		strcpy (_svlpcLastProcessedContent [CMSREP_REPOSITORYTYPE_DONE].
			_pPartition, "");
		strcpy (_svlpcLastProcessedContent [CMSREP_REPOSITORYTYPE_DONE].
			_pCustomerDirectoryName, "");
		_svlpcLastProcessedContent [CMSREP_REPOSITORYTYPE_DONE].
			_ulFilesNumberAlreadyProcessed		= 0;

		// Repository: CMSREP_REPOSITORYTYPE_ERRORS
		strcpy (_svlpcLastProcessedContent [CMSREP_REPOSITORYTYPE_ERRORS].
			_pPartition, "");
		strcpy (_svlpcLastProcessedContent [CMSREP_REPOSITORYTYPE_ERRORS].
			_pCustomerDirectoryName, "");
		_svlpcLastProcessedContent [CMSREP_REPOSITORYTYPE_ERRORS].
			_ulFilesNumberAlreadyProcessed		= 0;

		// Repository: CMSREP_REPOSITORYTYPE_FTP
		strcpy (_svlpcLastProcessedContent [CMSREP_REPOSITORYTYPE_FTP].
			_pPartition, "");
		strcpy (_svlpcLastProcessedContent [CMSREP_REPOSITORYTYPE_FTP].
			_pCustomerDirectoryName, "");
		_svlpcLastProcessedContent [CMSREP_REPOSITORYTYPE_FTP].
			_ulFilesNumberAlreadyProcessed		= 0;


		return errNoError;
	}

	// Repository: CMSREP_REPOSITORYTYPE_CMSCUSTOMER
	if (FileIO:: readChars (iFileDescriptor,
		_svlpcLastProcessedContent [CMSREP_REPOSITORYTYPE_CMSCUSTOMER].
			_pPartition,
		CMSREP_CMSREPOSITORY_MAXCMSPARTITIONNAMELENGTH,
		&llByteRead) != errNoError)
	{
		Error err = ToolsErrors (__FILE__, __LINE__,
			TOOLS_FILEIO_READCHARS_FAILED);
		_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
			(const char *) err, __FILE__, __LINE__);

		FileIO:: close (iFileDescriptor);

		return err;
	}

	if (FileIO:: readChars (iFileDescriptor,
		pEndLine,
		1,
		&llByteRead) != errNoError)
	{
		Error err = ToolsErrors (__FILE__, __LINE__,
			TOOLS_FILEIO_READCHARS_FAILED);
		_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
			(const char *) err, __FILE__, __LINE__);

		FileIO:: close (iFileDescriptor);

		return err;
	}

	if (FileIO:: readChars (iFileDescriptor,
		_svlpcLastProcessedContent [CMSREP_REPOSITORYTYPE_CMSCUSTOMER].
			_pCustomerDirectoryName,
		CMSREP_CMSREPOSITORY_MAXCUSTOMERNAMELENGTH,
		&llByteRead) != errNoError)
	{
		Error err = ToolsErrors (__FILE__, __LINE__,
			TOOLS_FILEIO_READCHARS_FAILED);
		_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
			(const char *) err, __FILE__, __LINE__);

		FileIO:: close (iFileDescriptor);

		return err;
	}

	if (FileIO:: readChars (iFileDescriptor,
		pEndLine,
		1,
		&llByteRead) != errNoError)
	{
		Error err = ToolsErrors (__FILE__, __LINE__,
			TOOLS_FILEIO_READCHARS_FAILED);
		_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
			(const char *) err, __FILE__, __LINE__);

		FileIO:: close (iFileDescriptor);

		return err;
	}

	{
		char				pUnsignedLongBuffer [128];


		memset (pUnsignedLongBuffer, '\0', 128);

		if (FileIO:: readChars (iFileDescriptor,
			pUnsignedLongBuffer,
			128,
			&llByteRead) != errNoError)
		{
			Error err = ToolsErrors (__FILE__, __LINE__,
				TOOLS_FILEIO_READCHARS_FAILED);
			_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
				(const char *) err, __FILE__, __LINE__);

			FileIO:: close (iFileDescriptor);

			return err;
		}

		_svlpcLastProcessedContent [CMSREP_REPOSITORYTYPE_CMSCUSTOMER].
			_ulFilesNumberAlreadyProcessed		=
			strtoul (pUnsignedLongBuffer, (char **) NULL, 10);

		if (FileIO:: readChars (iFileDescriptor,
			pEndLine,
			1,
			&llByteRead) != errNoError)
		{
			Error err = ToolsErrors (__FILE__, __LINE__,
				TOOLS_FILEIO_READCHARS_FAILED);
			_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
				(const char *) err, __FILE__, __LINE__);

			FileIO:: close (iFileDescriptor);

			return err;
		}
	}

	{
		Message msg = CMSRepositoryMessages (
			__FILE__, __LINE__, 
			CMSREP_CMSREPOSITORY_READSANITYCHECKINFO,
			4,
			(long) CMSREP_REPOSITORYTYPE_CMSCUSTOMER,
			_svlpcLastProcessedContent [CMSREP_REPOSITORYTYPE_CMSCUSTOMER].
				_pPartition,
			_svlpcLastProcessedContent [CMSREP_REPOSITORYTYPE_CMSCUSTOMER].
				_pCustomerDirectoryName,
			_svlpcLastProcessedContent [CMSREP_REPOSITORYTYPE_CMSCUSTOMER].
				_ulFilesNumberAlreadyProcessed);
		_ptSystemTracer -> trace (Tracer:: TRACER_LINFO,
			(const char *) msg, __FILE__, __LINE__);
	}

	// Repository: CMSREP_REPOSITORYTYPE_DOWNLOAD
	if (FileIO:: readChars (iFileDescriptor,
		_svlpcLastProcessedContent [CMSREP_REPOSITORYTYPE_DOWNLOAD].
			_pPartition,
		CMSREP_CMSREPOSITORY_MAXCMSPARTITIONNAMELENGTH,
		&llByteRead) != errNoError)
	{
		Error err = ToolsErrors (__FILE__, __LINE__,
			TOOLS_FILEIO_READCHARS_FAILED);
		_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
			(const char *) err, __FILE__, __LINE__);

		FileIO:: close (iFileDescriptor);

		return err;
	}

	if (FileIO:: readChars (iFileDescriptor,
		pEndLine,
		1,
		&llByteRead) != errNoError)
	{
		Error err = ToolsErrors (__FILE__, __LINE__,
			TOOLS_FILEIO_READCHARS_FAILED);
		_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
			(const char *) err, __FILE__, __LINE__);

		FileIO:: close (iFileDescriptor);

		return err;
	}

	if (FileIO:: readChars (iFileDescriptor,
		_svlpcLastProcessedContent [CMSREP_REPOSITORYTYPE_DOWNLOAD].
			_pCustomerDirectoryName,
		CMSREP_CMSREPOSITORY_MAXCUSTOMERNAMELENGTH,
		&llByteRead) != errNoError)
	{
		Error err = ToolsErrors (__FILE__, __LINE__,
			TOOLS_FILEIO_READCHARS_FAILED);
		_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
			(const char *) err, __FILE__, __LINE__);

		FileIO:: close (iFileDescriptor);

		return err;
	}

	if (FileIO:: readChars (iFileDescriptor,
		pEndLine,
		1,
		&llByteRead) != errNoError)
	{
		Error err = ToolsErrors (__FILE__, __LINE__,
			TOOLS_FILEIO_READCHARS_FAILED);
		_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
			(const char *) err, __FILE__, __LINE__);

		FileIO:: close (iFileDescriptor);

		return err;
	}

	{
		char				pUnsignedLongBuffer [128];


		memset (pUnsignedLongBuffer, '\0', 128);

		if (FileIO:: readChars (iFileDescriptor,
			pUnsignedLongBuffer,
			128,
			&llByteRead) != errNoError)
		{
			Error err = ToolsErrors (__FILE__, __LINE__,
				TOOLS_FILEIO_READCHARS_FAILED);
			_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
				(const char *) err, __FILE__, __LINE__);

			FileIO:: close (iFileDescriptor);

			return err;
		}

		_svlpcLastProcessedContent [CMSREP_REPOSITORYTYPE_DOWNLOAD].
			_ulFilesNumberAlreadyProcessed		=
			strtoul (pUnsignedLongBuffer, (char **) NULL, 10);

		if (FileIO:: readChars (iFileDescriptor,
			pEndLine,
			1,
			&llByteRead) != errNoError)
		{
			Error err = ToolsErrors (__FILE__, __LINE__,
				TOOLS_FILEIO_READCHARS_FAILED);
			_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
				(const char *) err, __FILE__, __LINE__);

			FileIO:: close (iFileDescriptor);

			return err;
		}
	}

	{
		Message msg = CMSRepositoryMessages (
			__FILE__, __LINE__, 
			CMSREP_CMSREPOSITORY_READSANITYCHECKINFO,
			4,
			(long) CMSREP_REPOSITORYTYPE_DOWNLOAD,
			_svlpcLastProcessedContent [CMSREP_REPOSITORYTYPE_DOWNLOAD].
				_pPartition,
			_svlpcLastProcessedContent [CMSREP_REPOSITORYTYPE_DOWNLOAD].
				_pCustomerDirectoryName,
			_svlpcLastProcessedContent [CMSREP_REPOSITORYTYPE_DOWNLOAD].
				_ulFilesNumberAlreadyProcessed);
		_ptSystemTracer -> trace (Tracer:: TRACER_LINFO,
			(const char *) msg, __FILE__, __LINE__);
	}


	// Repository: CMSREP_REPOSITORYTYPE_STREAMING
	if (FileIO:: readChars (iFileDescriptor,
		_svlpcLastProcessedContent [CMSREP_REPOSITORYTYPE_STREAMING].
			_pPartition,
		CMSREP_CMSREPOSITORY_MAXCMSPARTITIONNAMELENGTH,
		&llByteRead) != errNoError)
	{
		Error err = ToolsErrors (__FILE__, __LINE__,
			TOOLS_FILEIO_READCHARS_FAILED);
		_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
			(const char *) err, __FILE__, __LINE__);

		FileIO:: close (iFileDescriptor);

		return err;
	}

	if (FileIO:: readChars (iFileDescriptor,
		pEndLine,
		1,
		&llByteRead) != errNoError)
	{
		Error err = ToolsErrors (__FILE__, __LINE__,
			TOOLS_FILEIO_READCHARS_FAILED);
		_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
			(const char *) err, __FILE__, __LINE__);

		FileIO:: close (iFileDescriptor);

		return err;
	}

	if (FileIO:: readChars (iFileDescriptor,
		_svlpcLastProcessedContent [CMSREP_REPOSITORYTYPE_STREAMING].
			_pCustomerDirectoryName,
		CMSREP_CMSREPOSITORY_MAXCUSTOMERNAMELENGTH,
		&llByteRead) != errNoError)
	{
		Error err = ToolsErrors (__FILE__, __LINE__,
			TOOLS_FILEIO_READCHARS_FAILED);
		_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
			(const char *) err, __FILE__, __LINE__);

		FileIO:: close (iFileDescriptor);

		return err;
	}

	if (FileIO:: readChars (iFileDescriptor,
		pEndLine,
		1,
		&llByteRead) != errNoError)
	{
		Error err = ToolsErrors (__FILE__, __LINE__,
			TOOLS_FILEIO_READCHARS_FAILED);
		_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
			(const char *) err, __FILE__, __LINE__);

		FileIO:: close (iFileDescriptor);

		return err;
	}

	{
		char				pUnsignedLongBuffer [128];


		memset (pUnsignedLongBuffer, '\0', 128);

		if (FileIO:: readChars (iFileDescriptor,
			pUnsignedLongBuffer,
			128,
			&llByteRead) != errNoError)
		{
			Error err = ToolsErrors (__FILE__, __LINE__,
				TOOLS_FILEIO_READCHARS_FAILED);
			_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
				(const char *) err, __FILE__, __LINE__);

			FileIO:: close (iFileDescriptor);

			return err;
		}

		_svlpcLastProcessedContent [CMSREP_REPOSITORYTYPE_STREAMING].
			_ulFilesNumberAlreadyProcessed		=
			strtoul (pUnsignedLongBuffer, (char **) NULL, 10);

		if (FileIO:: readChars (iFileDescriptor,
			pEndLine,
			1,
			&llByteRead) != errNoError)
		{
			Error err = ToolsErrors (__FILE__, __LINE__,
				TOOLS_FILEIO_READCHARS_FAILED);
			_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
				(const char *) err, __FILE__, __LINE__);

			FileIO:: close (iFileDescriptor);

			return err;
		}
	}

	{
		Message msg = CMSRepositoryMessages (
			__FILE__, __LINE__, 
			CMSREP_CMSREPOSITORY_READSANITYCHECKINFO,
			4,
			(long) CMSREP_REPOSITORYTYPE_STREAMING,
			_svlpcLastProcessedContent [CMSREP_REPOSITORYTYPE_STREAMING].
				_pPartition,
			_svlpcLastProcessedContent [CMSREP_REPOSITORYTYPE_STREAMING].
				_pCustomerDirectoryName,
			_svlpcLastProcessedContent [CMSREP_REPOSITORYTYPE_STREAMING].
				_ulFilesNumberAlreadyProcessed);
		_ptSystemTracer -> trace (Tracer:: TRACER_LINFO,
			(const char *) msg, __FILE__, __LINE__);
	}

	// Repository: CMSREP_REPOSITORYTYPE_STAGING
	if (FileIO:: readChars (iFileDescriptor,
		_svlpcLastProcessedContent [CMSREP_REPOSITORYTYPE_STAGING].
			_pPartition,
		CMSREP_CMSREPOSITORY_MAXCMSPARTITIONNAMELENGTH,
		&llByteRead) != errNoError)
	{
		Error err = ToolsErrors (__FILE__, __LINE__,
			TOOLS_FILEIO_READCHARS_FAILED);
		_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
			(const char *) err, __FILE__, __LINE__);

		FileIO:: close (iFileDescriptor);

		return err;
	}

	if (FileIO:: readChars (iFileDescriptor,
		pEndLine,
		1,
		&llByteRead) != errNoError)
	{
		Error err = ToolsErrors (__FILE__, __LINE__,
			TOOLS_FILEIO_READCHARS_FAILED);
		_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
			(const char *) err, __FILE__, __LINE__);

		FileIO:: close (iFileDescriptor);

		return err;
	}

	if (FileIO:: readChars (iFileDescriptor,
		_svlpcLastProcessedContent [CMSREP_REPOSITORYTYPE_STAGING].
			_pCustomerDirectoryName,
		CMSREP_CMSREPOSITORY_MAXCUSTOMERNAMELENGTH,
		&llByteRead) != errNoError)
	{
		Error err = ToolsErrors (__FILE__, __LINE__,
			TOOLS_FILEIO_READCHARS_FAILED);
		_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
			(const char *) err, __FILE__, __LINE__);

		FileIO:: close (iFileDescriptor);

		return err;
	}

	if (FileIO:: readChars (iFileDescriptor,
		pEndLine,
		1,
		&llByteRead) != errNoError)
	{
		Error err = ToolsErrors (__FILE__, __LINE__,
			TOOLS_FILEIO_READCHARS_FAILED);
		_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
			(const char *) err, __FILE__, __LINE__);

		FileIO:: close (iFileDescriptor);

		return err;
	}

	{
		char				pUnsignedLongBuffer [128];


		memset (pUnsignedLongBuffer, '\0', 128);

		if (FileIO:: readChars (iFileDescriptor,
			pUnsignedLongBuffer,
			128,
			&llByteRead) != errNoError)
		{
			Error err = ToolsErrors (__FILE__, __LINE__,
				TOOLS_FILEIO_READCHARS_FAILED);
			_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
				(const char *) err, __FILE__, __LINE__);

			FileIO:: close (iFileDescriptor);

			return err;
		}

		_svlpcLastProcessedContent [CMSREP_REPOSITORYTYPE_STAGING].
			_ulFilesNumberAlreadyProcessed		=
			strtoul (pUnsignedLongBuffer, (char **) NULL, 10);

		if (FileIO:: readChars (iFileDescriptor,
			pEndLine,
			1,
			&llByteRead) != errNoError)
		{
			Error err = ToolsErrors (__FILE__, __LINE__,
				TOOLS_FILEIO_READCHARS_FAILED);
			_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
				(const char *) err, __FILE__, __LINE__);

			FileIO:: close (iFileDescriptor);

			return err;
		}
	}

	{
		Message msg = CMSRepositoryMessages (
			__FILE__, __LINE__, 
			CMSREP_CMSREPOSITORY_READSANITYCHECKINFO,
			4,
			(long) CMSREP_REPOSITORYTYPE_STAGING,
			_svlpcLastProcessedContent [CMSREP_REPOSITORYTYPE_STAGING].
				_pPartition,
			_svlpcLastProcessedContent [CMSREP_REPOSITORYTYPE_STAGING].
				_pCustomerDirectoryName,
			_svlpcLastProcessedContent [CMSREP_REPOSITORYTYPE_STAGING].
				_ulFilesNumberAlreadyProcessed);
		_ptSystemTracer -> trace (Tracer:: TRACER_LINFO,
			(const char *) msg, __FILE__, __LINE__);
	}

	// Repository: CMSREP_REPOSITORYTYPE_DONE
	if (FileIO:: readChars (iFileDescriptor,
		_svlpcLastProcessedContent [CMSREP_REPOSITORYTYPE_DONE].
			_pPartition,
		CMSREP_CMSREPOSITORY_MAXCMSPARTITIONNAMELENGTH,
		&llByteRead) != errNoError)
	{
		Error err = ToolsErrors (__FILE__, __LINE__,
			TOOLS_FILEIO_READCHARS_FAILED);
		_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
			(const char *) err, __FILE__, __LINE__);

		FileIO:: close (iFileDescriptor);

		return err;
	}

	if (FileIO:: readChars (iFileDescriptor,
		pEndLine,
		1,
		&llByteRead) != errNoError)
	{
		Error err = ToolsErrors (__FILE__, __LINE__,
			TOOLS_FILEIO_READCHARS_FAILED);
		_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
			(const char *) err, __FILE__, __LINE__);

		FileIO:: close (iFileDescriptor);

		return err;
	}

	if (FileIO:: readChars (iFileDescriptor,
		_svlpcLastProcessedContent [CMSREP_REPOSITORYTYPE_DONE].
			_pCustomerDirectoryName,
		CMSREP_CMSREPOSITORY_MAXCUSTOMERNAMELENGTH,
		&llByteRead) != errNoError)
	{
		Error err = ToolsErrors (__FILE__, __LINE__,
			TOOLS_FILEIO_READCHARS_FAILED);
		_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
			(const char *) err, __FILE__, __LINE__);

		FileIO:: close (iFileDescriptor);

		return err;
	}

	if (FileIO:: readChars (iFileDescriptor,
		pEndLine,
		1,
		&llByteRead) != errNoError)
	{
		Error err = ToolsErrors (__FILE__, __LINE__,
			TOOLS_FILEIO_READCHARS_FAILED);
		_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
			(const char *) err, __FILE__, __LINE__);

		FileIO:: close (iFileDescriptor);

		return err;
	}

	{
		char				pUnsignedLongBuffer [128];


		memset (pUnsignedLongBuffer, '\0', 128);

		if (FileIO:: readChars (iFileDescriptor,
			pUnsignedLongBuffer,
			128,
			&llByteRead) != errNoError)
		{
			Error err = ToolsErrors (__FILE__, __LINE__,
				TOOLS_FILEIO_READCHARS_FAILED);
			_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
				(const char *) err, __FILE__, __LINE__);

			FileIO:: close (iFileDescriptor);

			return err;
		}

		_svlpcLastProcessedContent [CMSREP_REPOSITORYTYPE_DONE].
			_ulFilesNumberAlreadyProcessed		=
			strtoul (pUnsignedLongBuffer, (char **) NULL, 10);

		if (FileIO:: readChars (iFileDescriptor,
			pEndLine,
			1,
			&llByteRead) != errNoError)
		{
			Error err = ToolsErrors (__FILE__, __LINE__,
				TOOLS_FILEIO_READCHARS_FAILED);
			_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
				(const char *) err, __FILE__, __LINE__);

			FileIO:: close (iFileDescriptor);

			return err;
		}
	}

	{
		Message msg = CMSRepositoryMessages (
			__FILE__, __LINE__, 
			CMSREP_CMSREPOSITORY_READSANITYCHECKINFO,
			4,
			(long) CMSREP_REPOSITORYTYPE_DONE,
			_svlpcLastProcessedContent [CMSREP_REPOSITORYTYPE_DONE].
				_pPartition,
			_svlpcLastProcessedContent [CMSREP_REPOSITORYTYPE_DONE].
				_pCustomerDirectoryName,
			_svlpcLastProcessedContent [CMSREP_REPOSITORYTYPE_DONE].
				_ulFilesNumberAlreadyProcessed);
		_ptSystemTracer -> trace (Tracer:: TRACER_LINFO,
			(const char *) msg, __FILE__, __LINE__);
	}

	// Repository: CMSREP_REPOSITORYTYPE_ERRORS
	if (FileIO:: readChars (iFileDescriptor,
		_svlpcLastProcessedContent [CMSREP_REPOSITORYTYPE_ERRORS].
			_pPartition,
		CMSREP_CMSREPOSITORY_MAXCMSPARTITIONNAMELENGTH,
		&llByteRead) != errNoError)
	{
		Error err = ToolsErrors (__FILE__, __LINE__,
			TOOLS_FILEIO_READCHARS_FAILED);
		_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
			(const char *) err, __FILE__, __LINE__);

		FileIO:: close (iFileDescriptor);

		return err;
	}

	if (FileIO:: readChars (iFileDescriptor,
		pEndLine,
		1,
		&llByteRead) != errNoError)
	{
		Error err = ToolsErrors (__FILE__, __LINE__,
			TOOLS_FILEIO_READCHARS_FAILED);
		_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
			(const char *) err, __FILE__, __LINE__);

		FileIO:: close (iFileDescriptor);

		return err;
	}

	if (FileIO:: readChars (iFileDescriptor,
		_svlpcLastProcessedContent [CMSREP_REPOSITORYTYPE_ERRORS].
			_pCustomerDirectoryName,
		CMSREP_CMSREPOSITORY_MAXCUSTOMERNAMELENGTH,
		&llByteRead) != errNoError)
	{
		Error err = ToolsErrors (__FILE__, __LINE__,
			TOOLS_FILEIO_READCHARS_FAILED);
		_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
			(const char *) err, __FILE__, __LINE__);

		FileIO:: close (iFileDescriptor);

		return err;
	}

	if (FileIO:: readChars (iFileDescriptor,
		pEndLine,
		1,
		&llByteRead) != errNoError)
	{
		Error err = ToolsErrors (__FILE__, __LINE__,
			TOOLS_FILEIO_READCHARS_FAILED);
		_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
			(const char *) err, __FILE__, __LINE__);

		FileIO:: close (iFileDescriptor);

		return err;
	}

	{
		char				pUnsignedLongBuffer [128];


		memset (pUnsignedLongBuffer, '\0', 128);

		if (FileIO:: readChars (iFileDescriptor,
			pUnsignedLongBuffer,
			128,
			&llByteRead) != errNoError)
		{
			Error err = ToolsErrors (__FILE__, __LINE__,
				TOOLS_FILEIO_READCHARS_FAILED);
			_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
				(const char *) err, __FILE__, __LINE__);

			FileIO:: close (iFileDescriptor);

			return err;
		}

		_svlpcLastProcessedContent [CMSREP_REPOSITORYTYPE_ERRORS].
			_ulFilesNumberAlreadyProcessed		=
			strtoul (pUnsignedLongBuffer, (char **) NULL, 10);

		if (FileIO:: readChars (iFileDescriptor,
			pEndLine,
			1,
			&llByteRead) != errNoError)
		{
			Error err = ToolsErrors (__FILE__, __LINE__,
				TOOLS_FILEIO_READCHARS_FAILED);
			_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
				(const char *) err, __FILE__, __LINE__);

			FileIO:: close (iFileDescriptor);

			return err;
		}
	}

	{
		Message msg = CMSRepositoryMessages (
			__FILE__, __LINE__, 
			CMSREP_CMSREPOSITORY_READSANITYCHECKINFO,
			4,
			(long) CMSREP_REPOSITORYTYPE_ERRORS,
			_svlpcLastProcessedContent [CMSREP_REPOSITORYTYPE_ERRORS].
				_pPartition,
			_svlpcLastProcessedContent [CMSREP_REPOSITORYTYPE_ERRORS].
				_pCustomerDirectoryName,
			_svlpcLastProcessedContent [CMSREP_REPOSITORYTYPE_ERRORS].
				_ulFilesNumberAlreadyProcessed);
		_ptSystemTracer -> trace (Tracer:: TRACER_LINFO,
			(const char *) msg, __FILE__, __LINE__);
	}

	// Repository: CMSREP_REPOSITORYTYPE_FTP
	if (FileIO:: readChars (iFileDescriptor,
		_svlpcLastProcessedContent [CMSREP_REPOSITORYTYPE_FTP].
			_pPartition,
		CMSREP_CMSREPOSITORY_MAXCMSPARTITIONNAMELENGTH,
		&llByteRead) != errNoError)
	{
		Error err = ToolsErrors (__FILE__, __LINE__,
			TOOLS_FILEIO_READCHARS_FAILED);
		_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
			(const char *) err, __FILE__, __LINE__);

		FileIO:: close (iFileDescriptor);

		return err;
	}

	if (FileIO:: readChars (iFileDescriptor,
		pEndLine,
		1,
		&llByteRead) != errNoError)
	{
		Error err = ToolsErrors (__FILE__, __LINE__,
			TOOLS_FILEIO_READCHARS_FAILED);
		_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
			(const char *) err, __FILE__, __LINE__);

		FileIO:: close (iFileDescriptor);

		return err;
	}

	if (FileIO:: readChars (iFileDescriptor,
		_svlpcLastProcessedContent [CMSREP_REPOSITORYTYPE_FTP].
			_pCustomerDirectoryName,
		CMSREP_CMSREPOSITORY_MAXCUSTOMERNAMELENGTH,
		&llByteRead) != errNoError)
	{
		Error err = ToolsErrors (__FILE__, __LINE__,
			TOOLS_FILEIO_READCHARS_FAILED);
		_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
			(const char *) err, __FILE__, __LINE__);

		FileIO:: close (iFileDescriptor);

		return err;
	}

	if (FileIO:: readChars (iFileDescriptor,
		pEndLine,
		1,
		&llByteRead) != errNoError)
	{
		Error err = ToolsErrors (__FILE__, __LINE__,
			TOOLS_FILEIO_READCHARS_FAILED);
		_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
			(const char *) err, __FILE__, __LINE__);

		FileIO:: close (iFileDescriptor);

		return err;
	}

	{
		char				pUnsignedLongBuffer [128];


		memset (pUnsignedLongBuffer, '\0', 128);

		if (FileIO:: readChars (iFileDescriptor,
			pUnsignedLongBuffer,
			128,
			&llByteRead) != errNoError)
		{
			Error err = ToolsErrors (__FILE__, __LINE__,
				TOOLS_FILEIO_READCHARS_FAILED);
			_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
				(const char *) err, __FILE__, __LINE__);

			FileIO:: close (iFileDescriptor);

			return err;
		}

		_svlpcLastProcessedContent [CMSREP_REPOSITORYTYPE_FTP].
			_ulFilesNumberAlreadyProcessed		=
			strtoul (pUnsignedLongBuffer, (char **) NULL, 10);

		if (FileIO:: readChars (iFileDescriptor,
			pEndLine,
			1,
			&llByteRead) != errNoError)
		{
			Error err = ToolsErrors (__FILE__, __LINE__,
				TOOLS_FILEIO_READCHARS_FAILED);
			_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
				(const char *) err, __FILE__, __LINE__);

			FileIO:: close (iFileDescriptor);

			return err;
		}
	}

	{
		Message msg = CMSRepositoryMessages (
			__FILE__, __LINE__, 
			CMSREP_CMSREPOSITORY_READSANITYCHECKINFO,
			4,
			(long) CMSREP_REPOSITORYTYPE_FTP,
			_svlpcLastProcessedContent [CMSREP_REPOSITORYTYPE_FTP].
				_pPartition,
			_svlpcLastProcessedContent [CMSREP_REPOSITORYTYPE_FTP].
				_pCustomerDirectoryName,
			_svlpcLastProcessedContent [CMSREP_REPOSITORYTYPE_FTP].
				_ulFilesNumberAlreadyProcessed);
		_ptSystemTracer -> trace (Tracer:: TRACER_LINFO,
			(const char *) msg, __FILE__, __LINE__);
	}

	if (FileIO:: close (iFileDescriptor) != errNoError)
	{
		Error err = ToolsErrors (__FILE__, __LINE__,
			TOOLS_FILEIO_CLOSE_FAILED);
		_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
			(const char *) err, __FILE__, __LINE__);

		return err;
	}


//	pulFilesNumberAlreadyProcessed [CMSREP_REPOSITORYTYPE_CMSCUSTOMER]		=
//		_pulFilesNumberAlreadyProcessed [CMSREP_REPOSITORYTYPE_CMSCUSTOMER];
//	pulFilesNumberAlreadyProcessed [CMSREP_REPOSITORYTYPE_DOWNLOAD]			=
//		_pulFilesNumberAlreadyProcessed [CMSREP_REPOSITORYTYPE_DOWNLOAD];
//	pulFilesNumberAlreadyProcessed [CMSREP_REPOSITORYTYPE_STREAMING]		=
//		_pulFilesNumberAlreadyProcessed [CMSREP_REPOSITORYTYPE_STREAMING];
//	pulFilesNumberAlreadyProcessed [CMSREP_REPOSITORYTYPE_STAGING]			=
//		_pulFilesNumberAlreadyProcessed [CMSREP_REPOSITORYTYPE_STAGING];
//	pulFilesNumberAlreadyProcessed [CMSREP_REPOSITORYTYPE_DONE]				=
//		_pulFilesNumberAlreadyProcessed [CMSREP_REPOSITORYTYPE_DONE];
//	pulFilesNumberAlreadyProcessed [CMSREP_REPOSITORYTYPE_ERRORS]			=
//		_pulFilesNumberAlreadyProcessed [CMSREP_REPOSITORYTYPE_ERRORS];
//	pulFilesNumberAlreadyProcessed [CMSREP_REPOSITORYTYPE_FTP]				=
//		_pulFilesNumberAlreadyProcessed [CMSREP_REPOSITORYTYPE_FTP];


	return errNoError;
}
 */

