package com.catramms.utility.catramms;

import com.catramms.backing.entity.*;
import com.catramms.backing.workflowEditor.utility.IngestionResult;
import com.catramms.utility.httpFetcher.HttpFeedFetcher;
import org.apache.log4j.Logger;
import org.json.JSONArray;
import org.json.JSONObject;

import java.io.File;
import java.io.FileInputStream;
import java.io.InputStream;
import java.text.SimpleDateFormat;
import java.util.*;

/**
 * Created by multi on 08.06.18.
 */
public class CatraMMS {

    private final Logger mLogger = Logger.getLogger(this.getClass());

    static public final String configFileName = "catramms.properties";

    private int timeoutInSeconds;
    private int maxRetriesNumber;
    private String mmsAPIProtocol;
    private String mmsAPIHostName;
    private int mmsAPIPort;
    private String mmsBinaryProtocol;
    private String mmsBinaryHostName;
    private int mmsBinaryPort;

    public CatraMMS()
    {
        try {
            mLogger.info("loadConfigurationParameters...");
            loadConfigurationParameters();
        } catch (Exception e) {
            String errorMessage = "Problems to load the configuration file. Exception: " + e + ", ConfigurationFileName: " + configFileName;
            mLogger.error(errorMessage);

            return;
        }
    }

    public Long shareWorkspace(String username, String password,
                               Boolean userAlreadyPresent,
                               String userNameToShare, String emailAddressToShare,
                               String passwordToShare, String countryToShare,

                               Boolean ingestWorkflow, Boolean createProfiles,
                               Boolean deliveryAuthorization, Boolean shareWorkspace,
                               Boolean editMedia)
            throws Exception
    {
        String mmsInfo;
        try
        {
            String mmsURL = mmsAPIProtocol + "://" + mmsAPIHostName + ":" + mmsAPIPort + "/catramms/v1/workspace/share"
                    + "?userAlreadyPresent=" + userAlreadyPresent.toString()
                    + "&ingestWorkflow=" + ingestWorkflow.toString()
                    + "&createProfiles=" + createProfiles.toString()
                    + "&deliveryAuthorization=" + deliveryAuthorization.toString()
                    + "&shareWorkspace=" + shareWorkspace.toString()
                    + "&editMedia=" + editMedia.toString()
            ;

            String postBodyRequest;
            if (userAlreadyPresent)
                postBodyRequest = "{ "
                    + "\"EMail\": \"" + emailAddressToShare + "\" "
                    + "} "
                    ;
            else
                postBodyRequest = "{ "
                        + "\"Name\": \"" + userNameToShare + "\", "
                        + "\"EMail\": \"" + emailAddressToShare + "\", "
                        + "\"Password\": \"" + passwordToShare + "\", "
                        + "\"Country\": \"" + countryToShare + "\" "
                        + "} "
                        ;

            mLogger.info("shareWorkspace"
                            + ", mmsURL: " + mmsURL
                            + ", postBodyRequest: " + postBodyRequest
            );

            Date now = new Date();
            mmsInfo = HttpFeedFetcher.fetchPutHttpsJson(mmsURL, timeoutInSeconds, maxRetriesNumber,
                    username, password, postBodyRequest);
            mLogger.info("Elapsed time shareWorkspace (@" + mmsURL + "@): @" + (new Date().getTime() - now.getTime()) + "@ millisecs.");
        }
        catch (Exception e)
        {
            String errorMessage = "shareWorkspace failed. Exception: " + e;
            mLogger.error(errorMessage);

            throw new Exception(errorMessage);
        }

        Long userKey;

        try
        {
            JSONObject joWMMSInfo = new JSONObject(mmsInfo);
            userKey = joWMMSInfo.getLong("userKey");
        }
        catch (Exception e)
        {
            String errorMessage = "Parsing workspaceDetails failed. Exception: " + e;
            mLogger.error(errorMessage);

            throw new Exception(errorMessage);
        }

        return userKey;
    }

    public Long register(String userNameToRegister, String emailAddressToRegister,
                         String passwordToRegister, String countryToRegister,
                         String workspaceNameToRegister)
            throws Exception
    {
        String mmsInfo;
        try
        {
            String mmsURL = mmsAPIProtocol + "://" + mmsAPIHostName + ":" + mmsAPIPort + "/catramms/v1/user";

            String postBodyRequest = "{ "
                    + "\"Name\": \"" + userNameToRegister + "\", "
                    + "\"EMail\": \"" + emailAddressToRegister + "\", "
                    + "\"Password\": \"" + passwordToRegister + "\", "
                    + "\"Country\": \"" + countryToRegister + "\", "
                    + "\"WorkspaceName\": \"" + workspaceNameToRegister + "\" "
                    + "} "
                    ;

            mLogger.info("register"
                            + ", mmsURL: " + mmsURL
                            + ", postBodyRequest: " + postBodyRequest
            );

            String username = null;
            String password = null;

            Date now = new Date();
            String contentType = null;
            mmsInfo = HttpFeedFetcher.fetchPostHttpsJson(mmsURL, contentType, timeoutInSeconds, maxRetriesNumber,
                    username, password, postBodyRequest);
            mLogger.info("Elapsed time register (@" + mmsURL + "@): @" + (new Date().getTime() - now.getTime()) + "@ millisecs.");
        }
        catch (Exception e)
        {
            String errorMessage = "shareWorkspace failed. Exception: " + e;
            mLogger.error(errorMessage);

            throw new Exception(errorMessage);
        }

        Long userKey;

        try
        {
            JSONObject joWMMSInfo = new JSONObject(mmsInfo);
            userKey = joWMMSInfo.getLong("userKey");
        }
        catch (Exception e)
        {
            String errorMessage = "Parsing workspaceDetails failed. Exception: " + e;
            mLogger.error(errorMessage);

            throw new Exception(errorMessage);
        }

        return userKey;
    }

    public String confirmRegistration(Long userKey, String confirmationCode)
            throws Exception
    {
        String mmsInfo;
        try
        {
            String mmsURL = mmsAPIProtocol + "://" + mmsAPIHostName + ":" + mmsAPIPort
                    + "/catramms/v1/user/" + userKey + "/" + confirmationCode;

            String username = null;
            String password = null;

            mLogger.info("confirmRegistration"
                            + ", mmsURL: " + mmsURL
            );

            Date now = new Date();
            mmsInfo = HttpFeedFetcher.fetchGetHttpsJson(mmsURL, timeoutInSeconds, maxRetriesNumber,
                    username, password);
            mLogger.info("Elapsed time confirmRegistration (@" + mmsURL + "@): @" + (new Date().getTime() - now.getTime()) + "@ millisecs.");
        }
        catch (Exception e)
        {
            String errorMessage = "confirmRegistration failed. Exception: " + e;
            mLogger.error(errorMessage);

            throw new Exception(errorMessage);
        }

        String apiKey;

        try
        {
            JSONObject joWMMSInfo = new JSONObject(mmsInfo);
            apiKey = joWMMSInfo.getString("apiKey");
        }
        catch (Exception e)
        {
            String errorMessage = "confirmRegistration failed. Exception: " + e;
            mLogger.error(errorMessage);

            throw new Exception(errorMessage);
        }

        return apiKey;
    }

    public UserProfile login(String username, String password, List<WorkspaceDetails> workspaceDetailsList)
            throws Exception
    {
        String mmsInfo;
        try
        {
            String mmsURL = mmsAPIProtocol + "://" + mmsAPIHostName + ":" + mmsAPIPort + "/catramms/v1/login";

            String postBodyRequest = "{ "
                    + "\"EMail\": \"" + username + "\", "
                    + "\"Password\": \"" + password + "\" "
                    + "} "
                    ;

            mLogger.info("login"
                    + ", mmsURL: " + mmsURL
                            + ", postBodyRequest: " + postBodyRequest
            );

            Date now = new Date();
            String contentType = null;
            mmsInfo = HttpFeedFetcher.fetchPostHttpsJson(mmsURL, contentType, timeoutInSeconds, maxRetriesNumber,
                    username, password, postBodyRequest);
            mLogger.info("Elapsed time login (@" + mmsURL + "@): @" + (new Date().getTime() - now.getTime()) + "@ millisecs.");
        }
        catch (Exception e)
        {
            String errorMessage = "Login MMS failed. Exception: " + e;
            mLogger.error(errorMessage);

            throw new Exception(errorMessage);
        }

        UserProfile userProfile = new UserProfile();

        try
        {
            JSONObject joWMMSInfo = new JSONObject(mmsInfo);

            fillUserProfile(userProfile, joWMMSInfo);

            userProfile.setPassword(password);

            JSONArray jaWorkspacesInfo = joWMMSInfo.getJSONArray("workspaces");

            fillWorkspaceDetailsList(workspaceDetailsList, jaWorkspacesInfo);
        }
        catch (Exception e)
        {
            String errorMessage = "Parsing workspaceDetails failed. Exception: " + e;
            mLogger.error(errorMessage);

            throw new Exception(errorMessage);
        }

        return userProfile;
    }

    public Long createWorkspace(String username, String password,
                                String workspaceNameToRegister)
            throws Exception
    {
        String mmsInfo;
        String mmsURL = null;
        try
        {
            mmsURL = mmsAPIProtocol + "://" + mmsAPIHostName + ":" + mmsAPIPort + "/catramms/v1/workspace";

            String postBodyRequest = "{ "
                    + "\"WorkspaceName\": \"" + workspaceNameToRegister + "\" "
                    + "} "
                    ;

            mLogger.info("createWorkspace"
                    + ", mmsURL: " + mmsURL
                    + ", postBodyRequest: " + postBodyRequest
            );

            Date now = new Date();
            String contentType = null;
            mmsInfo = HttpFeedFetcher.fetchPostHttpsJson(mmsURL, contentType, timeoutInSeconds, maxRetriesNumber,
                    username, password, postBodyRequest);
            mLogger.info("Elapsed time register (@" + mmsURL + "@): @" + (new Date().getTime() - now.getTime()) + "@ millisecs.");
        }
        catch (Exception e)
        {
            String errorMessage = "createWorkspace failed. Exception: " + e;
            mLogger.error(errorMessage);

            throw new Exception(errorMessage);
        }

        Long workspaceKey;

        try
        {
            JSONObject joWMMSInfo = new JSONObject(mmsInfo);
            workspaceKey = joWMMSInfo.getLong("workspaceKey");
        }
        catch (Exception e)
        {
            String errorMessage = "Parsing workspaceDetails failed. Exception: " + e;
            mLogger.error(errorMessage);

            throw new Exception(errorMessage);
        }

        return workspaceKey;
    }

    public WorkspaceDetails updateWorkspace(String username, String password,
                                       boolean newEnabled, String newMaxEncodingPriority,
                                       String newEncodingPeriod, Long newMaxIngestionsNumber,
                                       Long newMaxStorageInMB, String newLanguageCode,
                                       boolean newIngestWorkflow, boolean newCreateProfiles,
                                       boolean newDeliveryAuthorization, boolean newShareWorkspace,
                                       boolean newEditMedia)
            throws Exception
    {
        SimpleDateFormat simpleDateFormat = new SimpleDateFormat("yyyy-MM-dd'T'HH:mm:ss'Z'");
        simpleDateFormat.setTimeZone(TimeZone.getTimeZone("GMT"));

        String mmsInfo;
        try
        {
            String mmsURL = mmsAPIProtocol + "://" + mmsAPIHostName + ":" + mmsAPIPort + "/catramms/v1/workspace";

            String bodyRequest = "{ "
                    + "\"Enabled\": " + Boolean.toString(newEnabled) + ", "
                    + "\"MaxEncodingPriority\": \"" + newMaxEncodingPriority + "\", "
                    + "\"EncodingPeriod\": \"" + newEncodingPeriod + "\", "
                    + "\"MaxIngestionsNumber\": " + newMaxIngestionsNumber + ", "
                    + "\"MaxStorageInMB\": " + newMaxStorageInMB + ", "
                    + "\"LanguageCode\": \"" + newLanguageCode + "\", "
                    + "\"IngestWorkflow\": " + Boolean.toString(newIngestWorkflow) + ", "
                    + "\"CreateProfiles\": " + Boolean.toString(newCreateProfiles) + ", "
                    + "\"DeliveryAuthorization\": " + Boolean.toString(newDeliveryAuthorization) + ", "
                    + "\"ShareWorkspace\": " + Boolean.toString(newShareWorkspace) + ", "
                    + "\"EditMedia\": " + Boolean.toString(newEditMedia) + " "
                    + "} "
                    ;

            mLogger.info("updateUser"
                            + ", mmsURL: " + mmsURL
                            + ", bodyRequest: " + bodyRequest
            );

            Date now = new Date();
            mmsInfo = HttpFeedFetcher.fetchPutHttpsJson(mmsURL, timeoutInSeconds, maxRetriesNumber,
                    username, password, bodyRequest);
            mLogger.info("Elapsed time register (@" + mmsURL + "@): @" + (new Date().getTime() - now.getTime()) + "@ millisecs.");
        }
        catch (Exception e)
        {
            String errorMessage = "updateWorkspace failed. Exception: " + e;
            mLogger.error(errorMessage);

            throw new Exception(errorMessage);
        }

        WorkspaceDetails workspaceDetails = new WorkspaceDetails();

        try
        {
            JSONObject joWMMSInfo = new JSONObject(mmsInfo);

            // JSONObject jaWorkspaceInfo = joWMMSInfo.getJSONObject("workspace");

            fillWorkspaceDetails(workspaceDetails, joWMMSInfo);
        }
        catch (Exception e)
        {
            String errorMessage = "Parsing userProfile failed. Exception: " + e;
            mLogger.error(errorMessage);

            throw new Exception(errorMessage);
        }

        return workspaceDetails;
    }

    public UserProfile updateUserProfile(String username, String password,
                           String newName,
                           String newEmailAddress, String newPassword,
                           String newCountry)
            throws Exception
    {
        SimpleDateFormat simpleDateFormat = new SimpleDateFormat("yyyy-MM-dd'T'HH:mm:ss'Z'");
        simpleDateFormat.setTimeZone(TimeZone.getTimeZone("GMT"));

        String mmsInfo;
        try
        {
            String mmsURL = mmsAPIProtocol + "://" + mmsAPIHostName + ":" + mmsAPIPort + "/catramms/v1/user";

            String bodyRequest = "{ "
                    + "\"Name\": \"" + newName + "\", "
                    + "\"EMail\": \"" + newEmailAddress + "\", "
                    + "\"Password\": \"" + newPassword + "\", "
                    + "\"Country\": \"" + newCountry + "\" "
                    + "} "
                    ;

            mLogger.info("updateUser"
                            + ", mmsURL: " + mmsURL
                            + ", bodyRequest: " + bodyRequest
            );

            Date now = new Date();
            mmsInfo = HttpFeedFetcher.fetchPutHttpsJson(mmsURL, timeoutInSeconds, maxRetriesNumber,
                    username, password, bodyRequest);
            mLogger.info("Elapsed time register (@" + mmsURL + "@): @" + (new Date().getTime() - now.getTime()) + "@ millisecs.");
        }
        catch (Exception e)
        {
            String errorMessage = "updateUser failed. Exception: " + e;
            mLogger.error(errorMessage);

            throw new Exception(errorMessage);
        }

        UserProfile userProfile = new UserProfile();

        try
        {
            JSONObject joWMMSInfo = new JSONObject(mmsInfo);

            fillUserProfile(userProfile, joWMMSInfo);

            userProfile.setPassword(password);
        }
        catch (Exception e)
        {
            String errorMessage = "Parsing userProfile failed. Exception: " + e;
            mLogger.error(errorMessage);

            throw new Exception(errorMessage);
        }

        return userProfile;
    }

    public IngestionResult ingestWorkflow(String username, String password,
                                          String jsonWorkflow, List<IngestionResult> ingestionJobList)
            throws Exception
    {
        String mmsInfo;
        String mmsURL = null;
        try
        {
            mmsURL = mmsAPIProtocol + "://" + mmsAPIHostName + ":" + mmsAPIPort + "/catramms/v1/ingestion";

            mLogger.info("ingestWorkflow"
                    + ", mmsURL: " + mmsURL
                            + ", jsonWorkflow: " + jsonWorkflow
            );

            Date now = new Date();
            String contentType = null;
            mmsInfo = HttpFeedFetcher.fetchPostHttpsJson(mmsURL, contentType, timeoutInSeconds, maxRetriesNumber,
                    username, password, jsonWorkflow);
            mLogger.info("Elapsed time login (@" + mmsURL + "@): @" + (new Date().getTime() - now.getTime()) + "@ millisecs.");
        }
        catch (Exception e)
        {
            String errorMessage = "ingestWorkflow MMS failed"
                    + ", mmsURL: " + mmsURL
                    + ", Exception: " + e;
            mLogger.error(errorMessage);

            throw new Exception(errorMessage);
        }

        IngestionResult workflowRoot = new IngestionResult();

        try
        {
            JSONObject joWMMSInfo = new JSONObject(mmsInfo);
            JSONObject workflowInfo = joWMMSInfo.getJSONObject("workflow");
            JSONArray jaTasksInfo = joWMMSInfo.getJSONArray("tasks");

            workflowRoot.setKey(workflowInfo.getLong("ingestionRootKey"));
            workflowRoot.setLabel(workflowInfo.getString("label"));

            for (int taskIndex = 0; taskIndex < jaTasksInfo.length(); taskIndex++)
            {
                JSONObject taskInfo = jaTasksInfo.getJSONObject(taskIndex);

                IngestionResult ingestionJob = new IngestionResult();
                ingestionJob.setKey(taskInfo.getLong("ingestionJobKey"));
                ingestionJob.setLabel(taskInfo.getString("label"));

                ingestionJobList.add(ingestionJob);
            }
        }
        catch (Exception e)
        {
            String errorMessage = "ingestWorkflow failed. Exception: " + e;
            mLogger.error(errorMessage);

            throw new Exception(errorMessage);
        }

        return workflowRoot;
    }

    public String getMetaDataContent(String username, String password, Long ingestionRootKey)
            throws Exception
    {
        String metaDataContent;
        String mmsURL = null;
        try
        {
            mmsURL = mmsAPIProtocol + "://" + mmsAPIHostName + ":" + mmsAPIPort
                    + "/catramms/v1/ingestionRoot/metaDataContent/" + ingestionRootKey;

            mLogger.info("getMetaDataContent"
                    + ", mmsURL: " + mmsURL
                    + ", ingestionRootKey: " + ingestionRootKey
            );

            Date now = new Date();
            metaDataContent = HttpFeedFetcher.fetchGetHttpsJson(mmsURL, timeoutInSeconds, maxRetriesNumber,
                    username, password);
            mLogger.info("Elapsed time login (@" + mmsURL + "@): @" + (new Date().getTime() - now.getTime()) + "@ millisecs.");
        }
        catch (Exception e)
        {
            String errorMessage = "getMetaDataContent MMS failed"
                    + ", mmsURL: " + mmsURL
                    + ", Exception: " + e;
            mLogger.error(errorMessage);

            throw new Exception(errorMessage);
        }


        return metaDataContent;
    }

    public Long addEncodingProfile(String username, String password,
                                   String contentType, String jsonEncodingProfile)
            throws Exception
    {
        Long encodingProfileKey;

        String mmsInfo;
        try
        {
            String mmsURL = mmsAPIProtocol + "://" + mmsAPIHostName + ":" + mmsAPIPort + "/catramms/v1/profile/" + contentType;

            mLogger.info("addEncodingProfile"
                            + ", mmsURL: " + mmsURL
                            + ", contentType: " + contentType
                            + ", jsonEncodingProfile: " + jsonEncodingProfile
            );

            Date now = new Date();
            mmsInfo = HttpFeedFetcher.fetchPutHttpsJson(mmsURL, timeoutInSeconds, maxRetriesNumber,
                    username, password, jsonEncodingProfile);
            mLogger.info("Elapsed time login (@" + mmsURL + "@): @" + (new Date().getTime() - now.getTime()) + "@ millisecs.");
        }
        catch (Exception e)
        {
            String errorMessage = "addEncodingProfile MMS failed. Exception: " + e;
            mLogger.error(errorMessage);

            throw new Exception(errorMessage);
        }

        try
        {
            JSONObject joWMMSInfo = new JSONObject(mmsInfo);

            encodingProfileKey = joWMMSInfo.getLong("encodingProfileKey");
            String encodingProfileLabel = joWMMSInfo.getString("label");
        }
        catch (Exception e)
        {
            String errorMessage = "addEncodingProfile failed. Exception: " + e;
            mLogger.error(errorMessage);

            throw new Exception(errorMessage);
        }

        return encodingProfileKey;
    }

    public void removeEncodingProfile(String username, String password,
                                   Long encodingProfileKey)
            throws Exception
    {
        String mmsInfo;
        try
        {
            String mmsURL = mmsAPIProtocol + "://" + mmsAPIHostName + ":" + mmsAPIPort + "/catramms/v1/profile/" + encodingProfileKey;

            mLogger.info("removeEncodingProfile"
                            + ", mmsURL: " + mmsURL
                            + ", encodingProfileKey: " + encodingProfileKey
            );

            Date now = new Date();
            mmsInfo = HttpFeedFetcher.fetchDeleteHttpsJson(mmsURL, timeoutInSeconds, maxRetriesNumber,
                    username, password);
            mLogger.info("Elapsed time removeEncodingProfile (@" + mmsURL + "@): @" + (new Date().getTime() - now.getTime()) + "@ millisecs.");
        }
        catch (Exception e)
        {
            String errorMessage = "removeEncodingProfile MMS failed. Exception: " + e;
            mLogger.error(errorMessage);

            throw new Exception(errorMessage);
        }
    }

    public Long addEncodingProfilesSet(String username, String password,
                                   String contentType, String jsonEncodingProfilesSet)
            throws Exception
    {
        Long encodingProfilesSetKey;

        String mmsInfo;
        try
        {
            String mmsURL = mmsAPIProtocol + "://" + mmsAPIHostName + ":" + mmsAPIPort + "/catramms/v1/profilesSet/" + contentType;

            mLogger.info("addEncodingProfilesSet"
                            + ", mmsURL: " + mmsURL
                            + ", contentType: " + contentType
                            + ", jsonEncodingProfilesSet: " + jsonEncodingProfilesSet
            );

            Date now = new Date();
            mmsInfo = HttpFeedFetcher.fetchPutHttpsJson(mmsURL, timeoutInSeconds, maxRetriesNumber,
                    username, password, jsonEncodingProfilesSet);
            mLogger.info("Elapsed time addEncodingProfilesSet (@" + mmsURL + "@): @" + (new Date().getTime() - now.getTime()) + "@ millisecs.");
        }
        catch (Exception e)
        {
            String errorMessage = "addEncodingProfile MMS failed. Exception: " + e;
            mLogger.error(errorMessage);

            throw new Exception(errorMessage);
        }

        try
        {
            JSONObject joWMMSInfo = new JSONObject(mmsInfo);

            JSONObject joEncodingProfilesSet = joWMMSInfo.getJSONObject("encodingProfilesSet");

            encodingProfilesSetKey = joEncodingProfilesSet.getLong("encodingProfilesSetKey");
            String encodingProfilesSetLabel = joEncodingProfilesSet.getString("label");

            // ...
        }
        catch (Exception e)
        {
            String errorMessage = "addEncodingProfile failed. Exception: " + e;
            mLogger.error(errorMessage);

            throw new Exception(errorMessage);
        }

        return encodingProfilesSetKey;
    }

    public void removeEncodingProfilesSet(String username, String password,
                                      Long encodingProfilesSetKey)
            throws Exception
    {
        String mmsInfo;
        try
        {
            String mmsURL = mmsAPIProtocol + "://" + mmsAPIHostName + ":" + mmsAPIPort + "/catramms/v1/profilesSet/" + encodingProfilesSetKey;

            mLogger.info("removeEncodingProfilesSet"
                            + ", mmsURL: " + mmsURL
                            + ", encodingProfilesSetKey: " + encodingProfilesSetKey
            );

            Date now = new Date();
            mmsInfo = HttpFeedFetcher.fetchDeleteHttpsJson(mmsURL, timeoutInSeconds, maxRetriesNumber,
                    username, password);
            mLogger.info("Elapsed time removeEncodingProfilesSet (@" + mmsURL + "@): @" + (new Date().getTime() - now.getTime()) + "@ millisecs.");
        }
        catch (Exception e)
        {
            String errorMessage = "removeEncodingProfilesSet MMS failed. Exception: " + e;
            mLogger.error(errorMessage);

            throw new Exception(errorMessage);
        }
    }

    public void updateEncodingJobPriority(String username, String password,
                                          Long encodingJobKey, int newEncodingJobPriorityCode)
            throws Exception
    {
        try
        {
            String mmsURL = mmsAPIProtocol + "://" + mmsAPIHostName + ":" + mmsAPIPort
                    + "/catramms/v1/encodingJobs/" + encodingJobKey + "?newEncodingJobPriorityCode=" + newEncodingJobPriorityCode;

            mLogger.info("updateEncodingJobPriority"
                            + ", mmsURL: " + mmsURL
            );

            String putBodyRequest = "";
            Date now = new Date();
            HttpFeedFetcher.fetchPutHttpsJson(mmsURL, timeoutInSeconds, maxRetriesNumber,
                    username, password, putBodyRequest);
            mLogger.info("Elapsed time updateEncodingJobPriority (@" + mmsURL + "@): @" + (new Date().getTime() - now.getTime()) + "@ millisecs.");
        }
        catch (Exception e)
        {
            String errorMessage = "updateEncodingJobPriority MMS failed. Exception: " + e;
            mLogger.error(errorMessage);

            throw new Exception(errorMessage);
        }
    }

    public void updateEncodingJobTryAgain(String username, String password,
                                          Long encodingJobKey)
            throws Exception
    {
        try
        {
            String mmsURL = mmsAPIProtocol + "://" + mmsAPIHostName + ":" + mmsAPIPort
                    + "/catramms/v1/encodingJobs/" + encodingJobKey + "?tryEncodingAgain=true";

            mLogger.info("updateEncodingJobTryAgain"
                    + ", mmsURL: " + mmsURL
            );

            String putBodyRequest = "";
            Date now = new Date();
            HttpFeedFetcher.fetchPutHttpsJson(mmsURL, timeoutInSeconds, maxRetriesNumber,
                    username, password, putBodyRequest);
            mLogger.info("Elapsed time updateEncodingJobPriority (@" + mmsURL + "@): @" + (new Date().getTime() - now.getTime()) + "@ millisecs.");
        }
        catch (Exception e)
        {
            String errorMessage = "updateEncodingJobTryAgain MMS failed. Exception: " + e;
            mLogger.error(errorMessage);

            throw new Exception(errorMessage);
        }
    }

    public Vector<Long> getMediaItems(String username, String password,
                                      Long maxMediaItemsNumber, String contentType,
                                      Date begin, Date end, String title, String tags,
                                      String jsonCondition,
                                      String ingestionDateOrder, String jsonOrderBy,
                          List<MediaItem> mediaItemsList)
            throws Exception
    {
        Long numFound;
        Long workSpaceUsageInMB;
        Long maxStorageInMB;

        SimpleDateFormat simpleDateFormat = new SimpleDateFormat("yyyy-MM-dd'T'HH:mm:ss'Z'");
        simpleDateFormat.setTimeZone(TimeZone.getTimeZone("GMT"));

        String mmsInfo;
        try
        {
            String ingestionDatesParameters = "";
            if (begin != null && end != null)
                ingestionDatesParameters = "&startIngestionDate=" + simpleDateFormat.format(begin)
                        + "&endIngestionDate=" + simpleDateFormat.format(end);

            String mmsURL = mmsAPIProtocol + "://" + mmsAPIHostName + ":" + mmsAPIPort + "/catramms/v1/mediaItems"
                    + "?start=0"
                    + "&rows=" + maxMediaItemsNumber
                    + "&contentType=" + contentType
                    + "&title=" + (title == null ? "" : java.net.URLEncoder.encode(title, "UTF-8"))
                    + "&tags=" + (tags == null ? "" : java.net.URLEncoder.encode(tags, "UTF-8"))
                    + ingestionDatesParameters
                    + "&jsonCondition=" + (jsonCondition == null ? "" : java.net.URLEncoder.encode(jsonCondition, "UTF-8"))
                    + "&ingestionDateOrder=" + (ingestionDateOrder == null ? "" : ingestionDateOrder)
                    + "&jsonOrderBy=" + (jsonOrderBy == null ? "" : java.net.URLEncoder.encode(jsonOrderBy, "UTF-8"))
                    ;

            mLogger.info("mmsURL: " + mmsURL);

            Date now = new Date();
            mmsInfo = HttpFeedFetcher.fetchGetHttpsJson(mmsURL, timeoutInSeconds, maxRetriesNumber,
                    username, password);
            mLogger.info("Elapsed time getMediaItems (@" + mmsURL + "@): @" + (new Date().getTime() - now.getTime()) + "@ millisecs.");
        }
        catch (Exception e)
        {
            String errorMessage = "getMediaItems MMS failed. Exception: " + e;
            mLogger.error(errorMessage);

            throw new Exception(errorMessage);
        }

        try
        {
            mediaItemsList.clear();

            JSONObject joMMSInfo = new JSONObject(mmsInfo);
            JSONObject joResponse = joMMSInfo.getJSONObject("response");
            numFound = joResponse.getLong("numFound");
            workSpaceUsageInMB = joResponse.getLong("workSpaceUsageInMB");
            maxStorageInMB = joResponse.getLong("maxStorageInMB");
            JSONArray jaMediaItems = joResponse.getJSONArray("mediaItems");

            for (int mediaItemIndex = 0; mediaItemIndex < jaMediaItems.length(); mediaItemIndex++)
            {
                JSONObject mediaItemInfo = jaMediaItems.getJSONObject(mediaItemIndex);

                MediaItem mediaItem = new MediaItem();

                boolean deep = true;
                fillMediaItem(mediaItem, mediaItemInfo, deep);

                mediaItemsList.add(mediaItem);
            }
        }
        catch (Exception e)
        {
            String errorMessage = "getMediaItems failed. Exception: " + e;
            mLogger.error(errorMessage);

            throw new Exception(errorMessage);
        }

        Vector<Long> data = new Vector<>();
        data.add(numFound);
        data.add(workSpaceUsageInMB);
        data.add(maxStorageInMB);

        return data;
    }

    public MediaItem getMediaItem(String username, String password,
                                         Long mediaItemKey, Long physicalPathKey)
            throws Exception
    {
        SimpleDateFormat simpleDateFormat = new SimpleDateFormat("yyyy-MM-dd'T'HH:mm:ss'Z'");
        simpleDateFormat.setTimeZone(TimeZone.getTimeZone("GMT"));

        String mmsInfo;
        try
        {
            String mmsURL = mmsAPIProtocol + "://" + mmsAPIHostName + ":" + mmsAPIPort + "/catramms/v1/mediaItems/"
                    + (mediaItemKey == null ? 0 : mediaItemKey)
                    + "/"
                    + (physicalPathKey == null ? 0 : physicalPathKey)
                    ;

            mLogger.info("mmsURL: " + mmsURL);

            Date now = new Date();
            mmsInfo = HttpFeedFetcher.fetchGetHttpsJson(mmsURL, timeoutInSeconds, maxRetriesNumber,
                    username, password);
            mLogger.info("Elapsed time getMediaItems (@" + mmsURL + "@): @" + (new Date().getTime() - now.getTime()) + "@ millisecs.");
        }
        catch (Exception e)
        {
            String errorMessage = "MMS API failed. Exception: " + e;
            mLogger.error(errorMessage);

            throw new Exception(errorMessage);
        }

        MediaItem mediaItem = new MediaItem();

        try
        {
            JSONObject joMMSInfo = new JSONObject(mmsInfo);
            JSONObject joResponse = joMMSInfo.getJSONObject("response");
            JSONArray jaMediaItems = joResponse.getJSONArray("mediaItems");

            if (jaMediaItems.length() != 1)
            {
                String errorMessage = "Wrong MediaItems number returned, expected one. jaMediaItems.length: " + jaMediaItems.length();
                mLogger.error(errorMessage);

                throw new Exception(errorMessage);
            }

            {
                JSONObject mediaItemInfo = jaMediaItems.getJSONObject(0);

                boolean deep = true;
                fillMediaItem(mediaItem, mediaItemInfo, deep);
            }
        }
        catch (Exception e)
        {
            String errorMessage = "MMS API failed. Exception: " + e;
            mLogger.error(errorMessage);

            throw new Exception(errorMessage);
        }

        return mediaItem;
    }

    public Long getIngestionWorkflows(String username, String password,
                              Long maxIngestionWorkflowsNumber,
                              Date start, Date end, String status, boolean ascending,
                              List<IngestionWorkflow> ingestionWorkflowsList)
            throws Exception
    {
        Long numFound;

        SimpleDateFormat simpleDateFormat = new SimpleDateFormat("yyyy-MM-dd'T'HH:mm:ss'Z'");
        simpleDateFormat.setTimeZone(TimeZone.getTimeZone("GMT"));

        String mmsInfo;
        try
        {
            String mmsURL = mmsAPIProtocol + "://" + mmsAPIHostName + ":" + mmsAPIPort + "/catramms/v1/ingestionRoots"
                    + "?start=0"
                    + "&rows=" + maxIngestionWorkflowsNumber
                    + "&status=" + status
                    + "&asc=" + (ascending ? "true" : "false")
                    + "&startIngestionDate=" + simpleDateFormat.format(start)
                    + "&endIngestionDate=" + simpleDateFormat.format(end);

            mLogger.info("mmsURL: " + mmsURL);

            Date now = new Date();
            mmsInfo = HttpFeedFetcher.fetchGetHttpsJson(mmsURL, timeoutInSeconds, maxRetriesNumber,
                    username, password);
            mLogger.info("Elapsed time getIngestionWorkflows (@" + mmsURL + "@): @" + (new Date().getTime() - now.getTime()) + "@ millisecs.");
        }
        catch (Exception e)
        {
            String errorMessage = "getIngestionWorkflows MMS failed. Exception: " + e;
            mLogger.error(errorMessage);

            throw new Exception(errorMessage);
        }

        try
        {
            ingestionWorkflowsList.clear();

            JSONObject joMMSInfo = new JSONObject(mmsInfo);
            JSONObject joResponse = joMMSInfo.getJSONObject("response");
            numFound = joResponse.getLong("numFound");
            JSONArray jaWorkflows = joResponse.getJSONArray("workflows");

            for (int ingestionWorkflowIndex = 0; ingestionWorkflowIndex < jaWorkflows.length(); ingestionWorkflowIndex++)
            {
                JSONObject ingestionWorkflowInfo = jaWorkflows.getJSONObject(ingestionWorkflowIndex);

                IngestionWorkflow ingestionWorkflow = new IngestionWorkflow();

                boolean deep = true;
                fillIngestionWorkflow(ingestionWorkflow, ingestionWorkflowInfo, deep);

                ingestionWorkflowsList.add(ingestionWorkflow);
            }
        }
        catch (Exception e)
        {
            String errorMessage = "getIngestionWorkflows failed. Exception: " + e;
            mLogger.error(errorMessage);

            throw new Exception(errorMessage);
        }

        return numFound;
    }

    public IngestionWorkflow getIngestionWorkflow(String username, String password,
                                  Long ingestionRootKey)
            throws Exception
    {
        SimpleDateFormat simpleDateFormat = new SimpleDateFormat("yyyy-MM-dd'T'HH:mm:ss'Z'");
        simpleDateFormat.setTimeZone(TimeZone.getTimeZone("GMT"));

        String mmsInfo;
        try
        {
            if (ingestionRootKey == null)
            {
                String errorMessage = "getIngestionWorkflow. ingestionRootKey is null";
                mLogger.error(errorMessage);

                throw new Exception(errorMessage);
            }

            String mmsURL = mmsAPIProtocol + "://" + mmsAPIHostName + ":" + mmsAPIPort + "/catramms/v1/ingestionRoots/" + ingestionRootKey;

            mLogger.info("mmsURL: " + mmsURL);

            Date now = new Date();
            mmsInfo = HttpFeedFetcher.fetchGetHttpsJson(mmsURL, timeoutInSeconds, maxRetriesNumber,
                    username, password);
            mLogger.info("Elapsed time getIngestionWorkflow (@" + mmsURL + "@): @" + (new Date().getTime() - now.getTime()) + "@ millisecs.");
        }
        catch (Exception e)
        {
            String errorMessage = "MMS API failed. Exception: " + e;
            mLogger.error(errorMessage);

            throw new Exception(errorMessage);
        }

        IngestionWorkflow ingestionWorkflow = new IngestionWorkflow();

        try
        {
            JSONObject joMMSInfo = new JSONObject(mmsInfo);
            JSONObject joResponse = joMMSInfo.getJSONObject("response");
            JSONArray jaWorkflows = joResponse.getJSONArray("workflows");

            if (jaWorkflows.length() != 1)
            {
                String errorMessage = "Wrong Workflows number returned. jaWorkflows.length: " + jaWorkflows.length();
                mLogger.error(errorMessage);

                throw new Exception(errorMessage);
            }

            {
                JSONObject ingestionWorkflowInfo = jaWorkflows.getJSONObject(0);

                boolean deep = true;
                fillIngestionWorkflow(ingestionWorkflow, ingestionWorkflowInfo, deep);
            }
        }
        catch (Exception e)
        {
            String errorMessage = "getIngestionWorkflow failed. Exception: " + e;
            mLogger.error(errorMessage);

            throw new Exception(errorMessage);
        }

        return ingestionWorkflow;
    }

    public Long getIngestionJobs(String username, String password,
                                 Long maxIngestionJobsNumber,
                                 Date start, Date end,
                                 String status,
                                 boolean ascending,
                                 List<IngestionJob> ingestionJobsList)
            throws Exception
    {
        Long numFound;

        SimpleDateFormat simpleDateFormat = new SimpleDateFormat("yyyy-MM-dd'T'HH:mm:ss'Z'");
        simpleDateFormat.setTimeZone(TimeZone.getTimeZone("GMT"));

        String mmsInfo;
        try
        {
            String mmsURL = mmsAPIProtocol + "://" + mmsAPIHostName + ":" + mmsAPIPort + "/catramms/v1/ingestionJobs"
                    + "?start=0"
                    + "&rows=" + maxIngestionJobsNumber
                    + "&status=" + status
                    + "&asc=" + (ascending ? "true" : "false")
                    + "&startIngestionDate=" + simpleDateFormat.format(start)
                    + "&endIngestionDate=" + simpleDateFormat.format(end);

            mLogger.info("mmsURL: " + mmsURL);

            Date now = new Date();
            mmsInfo = HttpFeedFetcher.fetchGetHttpsJson(mmsURL, timeoutInSeconds, maxRetriesNumber,
                    username, password);
            mLogger.info("Elapsed time getIngestionJobs (@" + mmsURL + "@): @" + (new Date().getTime() - now.getTime()) + "@ millisecs.");
        }
        catch (Exception e)
        {
            String errorMessage = "getIngestionJobs MMS failed. Exception: " + e;
            mLogger.error(errorMessage);

            throw new Exception(errorMessage);
        }

        try
        {
            ingestionJobsList.clear();

            JSONObject joMMSInfo = new JSONObject(mmsInfo);
            JSONObject joResponse = joMMSInfo.getJSONObject("response");
            numFound = joResponse.getLong("numFound");
            JSONArray jaIngestionJobs = joResponse.getJSONArray("ingestionJobs");

            for (int ingestionJobIndex = 0; ingestionJobIndex < jaIngestionJobs.length(); ingestionJobIndex++)
            {
                JSONObject ingestionJobInfo = jaIngestionJobs.getJSONObject(ingestionJobIndex);

                IngestionJob ingestionJob = new IngestionJob();

                fillIngestionJob(ingestionJob, ingestionJobInfo);

                ingestionJobsList.add(ingestionJob);
            }
        }
        catch (Exception e)
        {
            String errorMessage = "getIngestionWorkflows failed. Exception: " + e;
            mLogger.error(errorMessage);

            throw new Exception(errorMessage);
        }

        return numFound;
    }

    public IngestionJob getIngestionJob(String username, String password,
                                 Long ingestionJobKey)
            throws Exception
    {
        SimpleDateFormat simpleDateFormat = new SimpleDateFormat("yyyy-MM-dd'T'HH:mm:ss'Z'");
        simpleDateFormat.setTimeZone(TimeZone.getTimeZone("GMT"));

        String mmsInfo;
        try
        {
            String mmsURL = mmsAPIProtocol + "://" + mmsAPIHostName + ":" + mmsAPIPort
                    + "/catramms/v1/ingestionJobs/" + ingestionJobKey;

            mLogger.info("mmsURL: " + mmsURL);

            Date now = new Date();
            mmsInfo = HttpFeedFetcher.fetchGetHttpsJson(mmsURL, timeoutInSeconds, maxRetriesNumber,
                    username, password);
            mLogger.info("Elapsed time getIngestionJob (@" + mmsURL + "@): @" + (new Date().getTime() - now.getTime()) + "@ millisecs.");
        }
        catch (Exception e)
        {
            String errorMessage = "getIngestionJob MMS failed. Exception: " + e;
            mLogger.error(errorMessage);

            throw new Exception(errorMessage);
        }

        IngestionJob ingestionJob = new IngestionJob();

        try
        {
            JSONObject joMMSInfo = new JSONObject(mmsInfo);
            JSONObject joResponse = joMMSInfo.getJSONObject("response");
            JSONArray jaIngestionJobs = joResponse.getJSONArray("ingestionJobs");

            if (jaIngestionJobs.length() != 1)
            {
                String errorMessage = "Wrong Jobs number returned. jaIngestionJobs.length: " + jaIngestionJobs.length();
                mLogger.error(errorMessage);

                throw new Exception(errorMessage);
            }

            {
                JSONObject ingestionJobInfo = jaIngestionJobs.getJSONObject(0);

                fillIngestionJob(ingestionJob, ingestionJobInfo);
            }
        }
        catch (Exception e)
        {
            String errorMessage = "getIngestionWorkflows failed. Exception: " + e;
            mLogger.error(errorMessage);

            throw new Exception(errorMessage);
        }

        return ingestionJob;
    }

    public Long getEncodingJobs(String username, String password,
                                 Long maxEncodingJobsNumber,
                                 Date start, Date end,
                                String status,
                                boolean ascending,
                                 List<EncodingJob> encodingJobsList)
            throws Exception
    {
        Long numFound;

        SimpleDateFormat simpleDateFormat = new SimpleDateFormat("yyyy-MM-dd'T'HH:mm:ss'Z'");
        simpleDateFormat.setTimeZone(TimeZone.getTimeZone("GMT"));

        String mmsInfo;
        try
        {
            String mmsURL = mmsAPIProtocol + "://" + mmsAPIHostName + ":" + mmsAPIPort + "/catramms/v1/encodingJobs"
                    + "?start=0"
                    + "&rows=" + maxEncodingJobsNumber
                    + "&status=" + status
                    + "&asc=" + (ascending ? "true" : "false")
                    + "&startIngestionDate=" + simpleDateFormat.format(start)
                    + "&endIngestionDate=" + simpleDateFormat.format(end);

            mLogger.info("mmsURL: " + mmsURL);

            Date now = new Date();
            mmsInfo = HttpFeedFetcher.fetchGetHttpsJson(mmsURL, timeoutInSeconds, maxRetriesNumber,
                    username, password);
            mLogger.info("Elapsed time getEncodingJobs (@" + mmsURL + "@): @" + (new Date().getTime() - now.getTime()) + "@ millisecs.");
        }
        catch (Exception e)
        {
            String errorMessage = "getEncodingJobs MMS failed. Exception: " + e;
            mLogger.error(errorMessage);

            throw new Exception(errorMessage);
        }

        try
        {
            encodingJobsList.clear();

            JSONObject joMMSInfo = new JSONObject(mmsInfo);
            JSONObject joResponse = joMMSInfo.getJSONObject("response");
            numFound = joResponse.getLong("numFound");
            JSONArray jaEncodingJobs = joResponse.getJSONArray("encodingJobs");

            for (int encodingJobIndex = 0; encodingJobIndex < jaEncodingJobs.length(); encodingJobIndex++)
            {
                JSONObject encodingJobInfo = jaEncodingJobs.getJSONObject(encodingJobIndex);

                EncodingJob encodingJob = new EncodingJob();

                fillEncodingJob(encodingJob, encodingJobInfo);

                encodingJobsList.add(encodingJob);
            }
        }
        catch (Exception e)
        {
            String errorMessage = "getEncodingJobs failed. Exception: " + e;
            mLogger.error(errorMessage);

            throw new Exception(errorMessage);
        }

        return numFound;
    }

    public EncodingJob getEncodingJob(String username, String password,
                                Long encodingJobKey)
            throws Exception
    {
        Long numFound;

        SimpleDateFormat simpleDateFormat = new SimpleDateFormat("yyyy-MM-dd'T'HH:mm:ss'Z'");
        simpleDateFormat.setTimeZone(TimeZone.getTimeZone("GMT"));

        String mmsInfo;
        try
        {
            String mmsURL = mmsAPIProtocol + "://" + mmsAPIHostName + ":" + mmsAPIPort + "/catramms/v1/encodingJobs/" + encodingJobKey;

            mLogger.info("mmsURL: " + mmsURL);

            Date now = new Date();
            mmsInfo = HttpFeedFetcher.fetchGetHttpsJson(mmsURL, timeoutInSeconds, maxRetriesNumber,
                    username, password);
            mLogger.info("Elapsed time getEncodingJob (@" + mmsURL + "@): @" + (new Date().getTime() - now.getTime()) + "@ millisecs.");
        }
        catch (Exception e)
        {
            String errorMessage = "getEncodingJob MMS failed. Exception: " + e;
            mLogger.error(errorMessage);

            throw new Exception(errorMessage);
        }

        EncodingJob encodingJob = new EncodingJob();

        try
        {
            JSONObject joMMSInfo = new JSONObject(mmsInfo);
            JSONObject joResponse = joMMSInfo.getJSONObject("response");
            JSONArray jaEncodingJobs = joResponse.getJSONArray("encodingJobs");

            if (jaEncodingJobs.length() != 1)
            {
                String errorMessage = "Wrong Jobs number returned. jaEncodingJobs.length: " + jaEncodingJobs.length();
                mLogger.error(errorMessage);

                throw new Exception(errorMessage);
            }

            {
                JSONObject encodingJobInfo = jaEncodingJobs.getJSONObject(0);

                fillEncodingJob(encodingJob, encodingJobInfo);
            }
        }
        catch (Exception e)
        {
            String errorMessage = "getEncodingJobs failed. Exception: " + e;
            mLogger.error(errorMessage);

            throw new Exception(errorMessage);
        }

        return encodingJob;
    }

    public EncodingProfile getEncodingProfile(String username, String password,
                                  Long encodingProfileKey)
            throws Exception
    {
        SimpleDateFormat simpleDateFormat = new SimpleDateFormat("yyyy-MM-dd'T'HH:mm:ss'Z'");
        simpleDateFormat.setTimeZone(TimeZone.getTimeZone("GMT"));

        String mmsInfo;
        try
        {
            String mmsURL = mmsAPIProtocol + "://" + mmsAPIHostName + ":" + mmsAPIPort + "/catramms/v1/profile/" + encodingProfileKey;

            mLogger.info("mmsURL: " + mmsURL);

            Date now = new Date();
            mmsInfo = HttpFeedFetcher.fetchGetHttpsJson(mmsURL, timeoutInSeconds, maxRetriesNumber,
                    username, password);
            mLogger.info("Elapsed time getMediaItems (@" + mmsURL + "@): @" + (new Date().getTime() - now.getTime()) + "@ millisecs.");
        }
        catch (Exception e)
        {
            String errorMessage = "MMS API failed. Exception: " + e;
            mLogger.error(errorMessage);

            throw new Exception(errorMessage);
        }

        EncodingProfile encodingProfile = new EncodingProfile();

        try
        {
            JSONObject joMMSInfo = new JSONObject(mmsInfo);
            JSONObject joResponse = joMMSInfo.getJSONObject("response");
            JSONArray jaEncodingProfiles = joResponse.getJSONArray("encodingProfiles");

            if (jaEncodingProfiles.length() != 1)
            {
                String errorMessage = "Wrong EncodingProfiles number returned. jaEncodingProfiles.length: " + jaEncodingProfiles.length();
                mLogger.error(errorMessage);

                throw new Exception(errorMessage);
            }

            {
                JSONObject encodingProfileInfo = jaEncodingProfiles.getJSONObject(0);

                boolean deep = true;
                fillEncodingProfile(encodingProfile, encodingProfileInfo, deep);
            }
        }
        catch (Exception e)
        {
            String errorMessage = "MMS API failed. Exception: " + e;
            mLogger.error(errorMessage);

            throw new Exception(errorMessage);
        }

        return encodingProfile;
    }

    public void getEncodingProfiles(String username, String password,
                                    String contentType,
                                    List<EncodingProfile> encodingProfileList)
            throws Exception
    {
        SimpleDateFormat simpleDateFormat = new SimpleDateFormat("yyyy-MM-dd'T'HH:mm:ss'Z'");
        simpleDateFormat.setTimeZone(TimeZone.getTimeZone("GMT"));

        String mmsInfo;
        try
        {
            String mmsURL = mmsAPIProtocol + "://" + mmsAPIHostName + ":" + mmsAPIPort + "/catramms/v1/profiles/" + contentType;

            mLogger.info("mmsURL: " + mmsURL);

            Date now = new Date();
            mmsInfo = HttpFeedFetcher.fetchGetHttpsJson(mmsURL, timeoutInSeconds, maxRetriesNumber,
                    username, password);
            mLogger.info("Elapsed time getEncodingProfiles (@" + mmsURL + "@): @" + (new Date().getTime() - now.getTime()) + "@ millisecs.");
        }
        catch (Exception e)
        {
            String errorMessage = "MMS API failed. Exception: " + e;
            mLogger.error(errorMessage);

            throw new Exception(errorMessage);
        }

        try
        {
            JSONObject joMMSInfo = new JSONObject(mmsInfo);
            JSONObject joResponse = joMMSInfo.getJSONObject("response");
            JSONArray jaEncodingProfiles = joResponse.getJSONArray("encodingProfiles");

            mLogger.info("jaEncodingProfiles.length(): " + jaEncodingProfiles.length());

            encodingProfileList.clear();

            for (int encodingProfileIndex = 0;
                 encodingProfileIndex < jaEncodingProfiles.length();
                 encodingProfileIndex++)
            {
                EncodingProfile encodingProfile = new EncodingProfile();

                JSONObject encodingProfileInfo = jaEncodingProfiles.getJSONObject(encodingProfileIndex);

                boolean deep = false;
                fillEncodingProfile(encodingProfile, encodingProfileInfo, deep);

                encodingProfileList.add(encodingProfile);
            }
        }
        catch (Exception e)
        {
            String errorMessage = "MMS API failed. Exception: " + e;
            mLogger.error(errorMessage);

            throw new Exception(errorMessage);
        }
    }

    public EncodingProfilesSet getEncodingProfilesSet(String username, String password,
                                              Long encodingProfilesSetKey)
            throws Exception
    {
        SimpleDateFormat simpleDateFormat = new SimpleDateFormat("yyyy-MM-dd'T'HH:mm:ss'Z'");
        simpleDateFormat.setTimeZone(TimeZone.getTimeZone("GMT"));

        String mmsInfo;
        try
        {
            String mmsURL = mmsAPIProtocol + "://" + mmsAPIHostName + ":" + mmsAPIPort + "/catramms/v1/profilesSet/" + encodingProfilesSetKey;

            mLogger.info("mmsURL: " + mmsURL);

            Date now = new Date();
            mmsInfo = HttpFeedFetcher.fetchGetHttpsJson(mmsURL, timeoutInSeconds, maxRetriesNumber,
                    username, password);
            mLogger.info("Elapsed time getMediaItems (@" + mmsURL + "@): @" + (new Date().getTime() - now.getTime()) + "@ millisecs.");
        }
        catch (Exception e)
        {
            String errorMessage = "MMS API failed. Exception: " + e;
            mLogger.error(errorMessage);

            throw new Exception(errorMessage);
        }

        EncodingProfilesSet encodingProfilesSet = new EncodingProfilesSet();

        try
        {
            JSONObject joMMSInfo = new JSONObject(mmsInfo);
            JSONObject joResponse = joMMSInfo.getJSONObject("response");
            JSONArray jaEncodingProfilesSets = joResponse.getJSONArray("encodingProfilesSets");

            if (jaEncodingProfilesSets.length() != 1)
            {
                String errorMessage = "Wrong EncodingProfilesSet number returned. jaEncodingProfilesSets.length: " + jaEncodingProfilesSets.length();
                mLogger.error(errorMessage);

                throw new Exception(errorMessage);
            }

            {
                JSONObject encodingProfilesSetInfo = jaEncodingProfilesSets.getJSONObject(0);

                boolean deep = true;
                fillEncodingProfilesSet(encodingProfilesSet, encodingProfilesSetInfo, deep);
            }
        }
        catch (Exception e)
        {
            String errorMessage = "MMS API failed. Exception: " + e;
            mLogger.error(errorMessage);

            throw new Exception(errorMessage);
        }

        return encodingProfilesSet;
    }

    public void getEncodingProfilesSets(String username, String password,
                                    String contentType,
                                    List<EncodingProfilesSet> encodingProfilesSetList)
            throws Exception
    {
        SimpleDateFormat simpleDateFormat = new SimpleDateFormat("yyyy-MM-dd'T'HH:mm:ss'Z'");
        simpleDateFormat.setTimeZone(TimeZone.getTimeZone("GMT"));

        String mmsInfo;
        try
        {
            String mmsURL = mmsAPIProtocol + "://" + mmsAPIHostName + ":" + mmsAPIPort + "/catramms/v1/profilesSets/" + contentType;

            mLogger.info("mmsURL: " + mmsURL);

            Date now = new Date();
            mmsInfo = HttpFeedFetcher.fetchGetHttpsJson(mmsURL, timeoutInSeconds, maxRetriesNumber,
                    username, password);
            mLogger.info("Elapsed time getEncodingProfilesSets (@" + mmsURL + "@): @" + (new Date().getTime() - now.getTime()) + "@ millisecs.");
        }
        catch (Exception e)
        {
            String errorMessage = "MMS API failed. Exception: " + e;
            mLogger.error(errorMessage);

            throw new Exception(errorMessage);
        }

        try
        {
            JSONObject joMMSInfo = new JSONObject(mmsInfo);
            JSONObject joResponse = joMMSInfo.getJSONObject("response");
            JSONArray jaEncodingProfilesSets = joResponse.getJSONArray("encodingProfilesSets");

            encodingProfilesSetList.clear();

            for (int encodingProfilesSetIndex = 0;
                 encodingProfilesSetIndex < jaEncodingProfilesSets.length();
                 encodingProfilesSetIndex++)
            {
                EncodingProfilesSet encodingProfilesSet = new EncodingProfilesSet();

                JSONObject encodingProfilesSetInfo = jaEncodingProfilesSets.getJSONObject(encodingProfilesSetIndex);

                boolean deep = false;
                fillEncodingProfilesSet(encodingProfilesSet, encodingProfilesSetInfo, deep);

                encodingProfilesSetList.add(encodingProfilesSet);
            }
        }
        catch (Exception e)
        {
            String errorMessage = "MMS API failed. Exception: " + e;
            mLogger.error(errorMessage);

            throw new Exception(errorMessage);
        }
    }

    public String getDeliveryURL(String username, String password,
                                 Long physicalPathKey,
                                 long ttlInSeconds, int maxRetries, Boolean save)
            throws Exception
    {
        SimpleDateFormat simpleDateFormat = new SimpleDateFormat("yyyy-MM-dd'T'HH:mm:ss'Z'");
        simpleDateFormat.setTimeZone(TimeZone.getTimeZone("GMT"));

        String mmsInfo;
        try
        {
            String mmsURL = mmsAPIProtocol + "://" + mmsAPIHostName + ":" + mmsAPIPort
                    + "/catramms/v1/delivery/" + physicalPathKey
                    + "?ttlInSeconds=" + ttlInSeconds
                    + "&maxRetries=" + maxRetries
                    + "&save=" + save.toString()
                    + "&redirect=false"
                    ;

            mLogger.info("mmsURL: " + mmsURL);

            Date now = new Date();
            mmsInfo = HttpFeedFetcher.fetchGetHttpsJson(mmsURL, timeoutInSeconds, maxRetriesNumber,
                    username, password);
            mLogger.info("Elapsed time getMediaItems (@" + mmsURL + "@): @" + (new Date().getTime() - now.getTime()) + "@ millisecs.");
        }
        catch (Exception e)
        {
            String errorMessage = "MMS API failed. Exception: " + e;
            mLogger.error(errorMessage);

            throw new Exception(errorMessage);
        }

        String deliveryURL;
        try
        {
            JSONObject joMMSInfo = new JSONObject(mmsInfo);

            deliveryURL = joMMSInfo.getString("deliveryURL");
        }
        catch (Exception e)
        {
            String errorMessage = "getDeliveryURL failed. Exception: " + e;
            mLogger.error(errorMessage);

            throw new Exception(errorMessage);
        }

        return deliveryURL;
    }

    /*
    public void ingestBinaryContent(String username, String password,
                                    File binaryPathName, Long ingestionJobKey)
            throws Exception
    {
        try
        {
            String mmsURL = mmsBinaryProtocol + "://" + mmsBinaryHostName + ":" + mmsBinaryPort
                    + "/catramms/v1/binary/" + ingestionJobKey;

            mLogger.info("ingestBinaryContentAndRemoveLocalFile"
                            + ", mmsURL: " + mmsURL
                            + ", binaryPathName: " + binaryPathName
            );

            Date now = new Date();
            HttpFeedFetcher.fetchPostHttpBinary(mmsURL, timeoutInSeconds, maxRetriesNumber,
                    username, password, binaryPathName);
            mLogger.info("Elapsed time ingestBinaryContent (@" + mmsURL + "@): @" + (new Date().getTime() - now.getTime()) + "@ millisecs.");
        }
        catch (Exception e)
        {
            String errorMessage = "ingestWorkflow MMS failed. Exception: " + e;
            mLogger.error(errorMessage);

            throw new Exception(errorMessage);
        }
    }
    */

    public void ingestBinaryContent(String username, String password,
                                    InputStream fileInputStream, long contentSize,
                                    Long ingestionJobKey)
            throws Exception
    {
        try
        {
            String mmsURL = mmsBinaryProtocol + "://" + mmsBinaryHostName + ":" + mmsBinaryPort
                    + "/catramms/v1/binary/" + ingestionJobKey;

            mLogger.info("ingestBinaryContent"
                            + ", mmsURL: " + mmsURL
                            + ", contentSize: " + contentSize
                            + ", ingestionJobKey: " + ingestionJobKey
            );

            Date now = new Date();
            HttpFeedFetcher.fetchPostHttpBinary(mmsURL, timeoutInSeconds, maxRetriesNumber,
                    username, password, fileInputStream, contentSize);
            mLogger.info("Elapsed time login (@" + mmsURL + "@): @" + (new Date().getTime() - now.getTime()) + "@ millisecs.");
        }
        catch (Exception e)
        {
            String errorMessage = "ingestWorkflow MMS failed. Exception: " + e;
            mLogger.error(errorMessage);

            throw new Exception(errorMessage);
        }
    }

    public void addYouTubeConf(String username, String password,
                               String label, String refreshToken)
            throws Exception
    {

        String mmsInfo;
        try
        {
            String jsonYouTubeConf;
            {
                JSONObject joYouTubeConf = new JSONObject();

                joYouTubeConf.put("Label", label);
                joYouTubeConf.put("RefreshToken", refreshToken);

                jsonYouTubeConf = joYouTubeConf.toString(4);
            }

            String mmsURL = mmsAPIProtocol + "://" + mmsAPIHostName + ":" + mmsAPIPort + "/catramms/v1/conf/youtube";

            mLogger.info("addYouTubeConf"
                            + ", mmsURL: " + mmsURL
                            + ", jsonYouTubeConf: " + jsonYouTubeConf
            );

            Date now = new Date();
            String contentType = null;
            mmsInfo = HttpFeedFetcher.fetchPostHttpsJson(mmsURL, contentType, timeoutInSeconds, maxRetriesNumber,
                    username, password, jsonYouTubeConf);
            mLogger.info("Elapsed time login (@" + mmsURL + "@): @" + (new Date().getTime() - now.getTime()) + "@ millisecs.");
        }
        catch (Exception e)
        {
            String errorMessage = "addYouTubeConf MMS failed. Exception: " + e;
            mLogger.error(errorMessage);

            throw new Exception(errorMessage);
        }
    }

    public void modifyYouTubeConf(String username, String password,
                               Long confKey, String label, String refreshToken)
            throws Exception
    {

        String mmsInfo;
        try
        {
            String jsonYouTubeConf;
            {
                JSONObject joYouTubeConf = new JSONObject();

                joYouTubeConf.put("Label", label);
                joYouTubeConf.put("RefreshToken", refreshToken);

                jsonYouTubeConf = joYouTubeConf.toString(4);
            }

            String mmsURL = mmsAPIProtocol + "://" + mmsAPIHostName + ":" + mmsAPIPort + "/catramms/v1/conf/youtube/" + confKey;

            mLogger.info("modifyYouTubeConf"
                    + ", mmsURL: " + mmsURL
                    + ", jsonYouTubeConf: " + jsonYouTubeConf
            );

            Date now = new Date();
            mmsInfo = HttpFeedFetcher.fetchPutHttpsJson(mmsURL, timeoutInSeconds, maxRetriesNumber,
                    username, password, jsonYouTubeConf);
            mLogger.info("Elapsed time login (@" + mmsURL + "@): @" + (new Date().getTime() - now.getTime()) + "@ millisecs.");
        }
        catch (Exception e)
        {
            String errorMessage = "modifyYouTubeConf MMS failed. Exception: " + e;
            mLogger.error(errorMessage);

            throw new Exception(errorMessage);
        }
    }

    public void removeYouTubeConf(String username, String password,
                                        Long confKey)
            throws Exception
    {

        String mmsInfo;
        try
        {
            String mmsURL = mmsAPIProtocol + "://" + mmsAPIHostName + ":" + mmsAPIPort + "/catramms/v1/conf/youtube/" + confKey;

            mLogger.info("removeYouTubeConf"
                            + ", mmsURL: " + mmsURL
                            + ", confKey: " + confKey
            );

            Date now = new Date();
            mmsInfo = HttpFeedFetcher.fetchDeleteHttpsJson(mmsURL, timeoutInSeconds, maxRetriesNumber,
                    username, password);
            mLogger.info("Elapsed time login (@" + mmsURL + "@): @" + (new Date().getTime() - now.getTime()) + "@ millisecs.");
        }
        catch (Exception e)
        {
            String errorMessage = "removeYouTubeConf MMS failed. Exception: " + e;
            mLogger.error(errorMessage);

            throw new Exception(errorMessage);
        }
    }

    public List<YouTubeConf> getYouTubeConf(String username, String password)
            throws Exception
    {
        List<YouTubeConf> youTubeConfList = new ArrayList<>();

        String mmsInfo;
        try
        {
            String mmsURL = mmsAPIProtocol + "://" + mmsAPIHostName + ":" + mmsAPIPort + "/catramms/v1/conf/youtube";

            mLogger.info("mmsURL: " + mmsURL);

            Date now = new Date();
            mmsInfo = HttpFeedFetcher.fetchGetHttpsJson(mmsURL, timeoutInSeconds, maxRetriesNumber,
                    username, password);
            mLogger.info("Elapsed time getYouTubeConf (@" + mmsURL + "@): @" + (new Date().getTime() - now.getTime()) + "@ millisecs.");
        }
        catch (Exception e)
        {
            String errorMessage = "MMS API failed. Exception: " + e;
            mLogger.error(errorMessage);

            throw new Exception(errorMessage);
        }

        try
        {
            JSONObject joMMSInfo = new JSONObject(mmsInfo);
            JSONObject joResponse = joMMSInfo.getJSONObject("response");
            JSONArray jaYouTubeConf = joResponse.getJSONArray("youTubeConf");

            mLogger.info("jaYouTubeConf.length(): " + jaYouTubeConf.length());

            youTubeConfList.clear();

            for (int youTubeConfIndex = 0;
                 youTubeConfIndex < jaYouTubeConf.length();
                 youTubeConfIndex++)
            {
                YouTubeConf youTubeConf = new YouTubeConf();

                JSONObject youTubeConfInfo = jaYouTubeConf.getJSONObject(youTubeConfIndex);

                fillYouTubeConf(youTubeConf, youTubeConfInfo);

                youTubeConfList.add(youTubeConf);
            }
        }
        catch (Exception e)
        {
            String errorMessage = "Parsing youTubeConf failed. Exception: " + e;
            mLogger.error(errorMessage);

            throw new Exception(errorMessage);
        }

        return youTubeConfList;
    }

    public void addFacebookConf(String username, String password,
                               String label, String pageToken)
            throws Exception
    {

        String mmsInfo;
        try
        {
            String jsonFacebookConf;
            {
                JSONObject joFacebookConf = new JSONObject();

                joFacebookConf.put("Label", label);
                joFacebookConf.put("PageToken", pageToken);

                jsonFacebookConf = joFacebookConf.toString(4);
            }

            String mmsURL = mmsAPIProtocol + "://" + mmsAPIHostName + ":" + mmsAPIPort + "/catramms/v1/conf/facebook";

            mLogger.info("addFacebookConf"
                    + ", mmsURL: " + mmsURL
                    + ", jsonFacebookConf: " + jsonFacebookConf
            );

            Date now = new Date();
            String contentType = null;
            mmsInfo = HttpFeedFetcher.fetchPostHttpsJson(mmsURL, contentType, timeoutInSeconds, maxRetriesNumber,
                    username, password, jsonFacebookConf);
            mLogger.info("Elapsed time login (@" + mmsURL + "@): @" + (new Date().getTime() - now.getTime()) + "@ millisecs.");
        }
        catch (Exception e)
        {
            String errorMessage = "addFacebookConf MMS failed. Exception: " + e;
            mLogger.error(errorMessage);

            throw new Exception(errorMessage);
        }
    }

    public void modifyFacebookConf(String username, String password,
                                  Long confKey, String label, String pageToken)
            throws Exception
    {

        String mmsInfo;
        try
        {
            String jsonFacebookConf;
            {
                JSONObject joFacebookConf = new JSONObject();

                joFacebookConf.put("Label", label);
                joFacebookConf.put("PageToken", pageToken);

                jsonFacebookConf = joFacebookConf.toString(4);
            }

            String mmsURL = mmsAPIProtocol + "://" + mmsAPIHostName + ":" + mmsAPIPort + "/catramms/v1/conf/facebook/" + confKey;

            mLogger.info("modifyFacebookConf"
                    + ", mmsURL: " + mmsURL
                    + ", jsonFacebookConf: " + jsonFacebookConf
            );

            Date now = new Date();
            mmsInfo = HttpFeedFetcher.fetchPutHttpsJson(mmsURL, timeoutInSeconds, maxRetriesNumber,
                    username, password, jsonFacebookConf);
            mLogger.info("Elapsed time login (@" + mmsURL + "@): @" + (new Date().getTime() - now.getTime()) + "@ millisecs.");
        }
        catch (Exception e)
        {
            String errorMessage = "modifyFacebookConf MMS failed. Exception: " + e;
            mLogger.error(errorMessage);

            throw new Exception(errorMessage);
        }
    }

    public void removeFacebookConf(String username, String password,
                                  Long confKey)
            throws Exception
    {

        String mmsInfo;
        try
        {
            String mmsURL = mmsAPIProtocol + "://" + mmsAPIHostName + ":" + mmsAPIPort + "/catramms/v1/conf/facebook/" + confKey;

            mLogger.info("removeFacebookConf"
                    + ", mmsURL: " + mmsURL
                    + ", confKey: " + confKey
            );

            Date now = new Date();
            mmsInfo = HttpFeedFetcher.fetchDeleteHttpsJson(mmsURL, timeoutInSeconds, maxRetriesNumber,
                    username, password);
            mLogger.info("Elapsed time login (@" + mmsURL + "@): @" + (new Date().getTime() - now.getTime()) + "@ millisecs.");
        }
        catch (Exception e)
        {
            String errorMessage = "removeFacebookConf MMS failed. Exception: " + e;
            mLogger.error(errorMessage);

            throw new Exception(errorMessage);
        }
    }

    public List<FacebookConf> getFacebookConf(String username, String password)
            throws Exception
    {
        List<FacebookConf> facebookConfList = new ArrayList<>();

        String mmsInfo;
        try
        {
            String mmsURL = mmsAPIProtocol + "://" + mmsAPIHostName + ":" + mmsAPIPort + "/catramms/v1/conf/facebook";

            mLogger.info("mmsURL: " + mmsURL);

            Date now = new Date();
            mmsInfo = HttpFeedFetcher.fetchGetHttpsJson(mmsURL, timeoutInSeconds, maxRetriesNumber,
                    username, password);
            mLogger.info("Elapsed time getFacebookConf (@" + mmsURL + "@): @" + (new Date().getTime() - now.getTime()) + "@ millisecs.");
        }
        catch (Exception e)
        {
            String errorMessage = "MMS API failed. Exception: " + e;
            mLogger.error(errorMessage);

            throw new Exception(errorMessage);
        }

        try
        {
            JSONObject joMMSInfo = new JSONObject(mmsInfo);
            JSONObject joResponse = joMMSInfo.getJSONObject("response");
            JSONArray jaFacebookConf = joResponse.getJSONArray("facebookConf");

            mLogger.info("jaFacebookConf.length(): " + jaFacebookConf.length());

            facebookConfList.clear();

            for (int facebookConfIndex = 0;
                 facebookConfIndex < jaFacebookConf.length();
                 facebookConfIndex++)
            {
                FacebookConf facebookConf = new FacebookConf();

                JSONObject facebookConfInfo = jaFacebookConf.getJSONObject(facebookConfIndex);

                fillFacebookConf(facebookConf, facebookConfInfo);

                facebookConfList.add(facebookConf);
            }
        }
        catch (Exception e)
        {
            String errorMessage = "Parsing facebookConf failed. Exception: " + e;
            mLogger.error(errorMessage);

            throw new Exception(errorMessage);
        }

        return facebookConfList;
    }

    public void addLiveURLConf(String username, String password,
                                String label, String liveURL)
            throws Exception
    {

        String mmsInfo;
        try
        {
            String jsonLiveURLConf;
            {
                JSONObject joLiveURLConf = new JSONObject();

                joLiveURLConf.put("Label", label);
                joLiveURLConf.put("LiveURL", liveURL);

                jsonLiveURLConf = joLiveURLConf.toString(4);
            }

            String mmsURL = mmsAPIProtocol + "://" + mmsAPIHostName + ":" + mmsAPIPort + "/catramms/v1/conf/liveURL";

            mLogger.info("addLiveURLConf"
                    + ", mmsURL: " + mmsURL
                    + ", jsonLiveURLConf: " + jsonLiveURLConf
            );

            Date now = new Date();
            String contentType = null;
            mmsInfo = HttpFeedFetcher.fetchPostHttpsJson(mmsURL, contentType, timeoutInSeconds, maxRetriesNumber,
                    username, password, jsonLiveURLConf);
            mLogger.info("Elapsed time login (@" + mmsURL + "@): @" + (new Date().getTime() - now.getTime()) + "@ millisecs.");
        }
        catch (Exception e)
        {
            String errorMessage = "addLiveURLConf MMS failed. Exception: " + e;
            mLogger.error(errorMessage);

            throw new Exception(errorMessage);
        }
    }

    public void modifyLiveURLConf(String username, String password,
                                   Long confKey, String label, String liveURL)
            throws Exception
    {

        String mmsInfo;
        try
        {
            String jsonLiveURLConf;
            {
                JSONObject joLiveURLConf = new JSONObject();

                joLiveURLConf.put("Label", label);
                joLiveURLConf.put("LiveURL", liveURL);

                jsonLiveURLConf = joLiveURLConf.toString(4);
            }

            String mmsURL = mmsAPIProtocol + "://" + mmsAPIHostName + ":" + mmsAPIPort + "/catramms/v1/conf/liveURL/" + confKey;

            mLogger.info("modifyLiveURLConf"
                    + ", mmsURL: " + mmsURL
                    + ", jsonLiveURLConf: " + jsonLiveURLConf
            );

            Date now = new Date();
            mmsInfo = HttpFeedFetcher.fetchPutHttpsJson(mmsURL, timeoutInSeconds, maxRetriesNumber,
                    username, password, jsonLiveURLConf);
            mLogger.info("Elapsed time login (@" + mmsURL + "@): @" + (new Date().getTime() - now.getTime()) + "@ millisecs.");
        }
        catch (Exception e)
        {
            String errorMessage = "modifyLiveURLConf MMS failed. Exception: " + e;
            mLogger.error(errorMessage);

            throw new Exception(errorMessage);
        }
    }

    public void removeLiveURLConf(String username, String password,
                                   Long confKey)
            throws Exception
    {

        String mmsInfo;
        try
        {
            String mmsURL = mmsAPIProtocol + "://" + mmsAPIHostName + ":" + mmsAPIPort + "/catramms/v1/conf/liveURL/" + confKey;

            mLogger.info("removeLiveURLConf"
                    + ", mmsURL: " + mmsURL
                    + ", confKey: " + confKey
            );

            Date now = new Date();
            mmsInfo = HttpFeedFetcher.fetchDeleteHttpsJson(mmsURL, timeoutInSeconds, maxRetriesNumber,
                    username, password);
            mLogger.info("Elapsed time login (@" + mmsURL + "@): @" + (new Date().getTime() - now.getTime()) + "@ millisecs.");
        }
        catch (Exception e)
        {
            String errorMessage = "removeLiveURLConf MMS failed. Exception: " + e;
            mLogger.error(errorMessage);

            throw new Exception(errorMessage);
        }
    }

    public List<LiveURLConf> getLiveURLConf(String username, String password)
            throws Exception
    {
        List<LiveURLConf> liveURLConfList = new ArrayList<>();

        String mmsInfo;
        try
        {
            String mmsURL = mmsAPIProtocol + "://" + mmsAPIHostName + ":" + mmsAPIPort + "/catramms/v1/conf/liveURL";

            mLogger.info("mmsURL: " + mmsURL);

            Date now = new Date();
            mmsInfo = HttpFeedFetcher.fetchGetHttpsJson(mmsURL, timeoutInSeconds, maxRetriesNumber,
                    username, password);
            mLogger.info("Elapsed time getLiveURLConf (@" + mmsURL + "@): @" + (new Date().getTime() - now.getTime()) + "@ millisecs.");
        }
        catch (Exception e)
        {
            String errorMessage = "MMS API failed. Exception: " + e;
            mLogger.error(errorMessage);

            throw new Exception(errorMessage);
        }

        try
        {
            JSONObject joMMSInfo = new JSONObject(mmsInfo);
            JSONObject joResponse = joMMSInfo.getJSONObject("response");
            JSONArray jaLiveURLConf = joResponse.getJSONArray("liveURLConf");

            mLogger.info("jaLiveURLConf.length(): " + jaLiveURLConf.length());

            liveURLConfList.clear();

            for (int liveURLConfIndex = 0;
                 liveURLConfIndex < jaLiveURLConf.length();
                 liveURLConfIndex++)
            {
                LiveURLConf liveURLConf = new LiveURLConf();

                JSONObject liveURLConfInfo = jaLiveURLConf.getJSONObject(liveURLConfIndex);

                fillLiveURLConf(liveURLConf, liveURLConfInfo);

                liveURLConfList.add(liveURLConf);
            }
        }
        catch (Exception e)
        {
            String errorMessage = "Parsing liveURLConf failed. Exception: " + e;
            mLogger.error(errorMessage);

            throw new Exception(errorMessage);
        }

        return liveURLConfList;
    }

    public void addFTPConf(String username, String password,
                               String label, String ftpServer,
                           Long ftpPort, String ftpUserName, String ftpPassword,
                           String ftpRemoteDirectory)
            throws Exception
    {

        String mmsInfo;
        try
        {
            String jsonFTPConf;
            {
                JSONObject joFTPConf = new JSONObject();

                joFTPConf.put("Label", label);
                joFTPConf.put("Server", ftpServer);
                joFTPConf.put("Port", ftpPort);
                joFTPConf.put("UserName", ftpUserName);
                joFTPConf.put("Password", ftpPassword);
                joFTPConf.put("RemoteDirectory", ftpRemoteDirectory);

                jsonFTPConf = joFTPConf.toString(4);
            }

            String mmsURL = mmsAPIProtocol + "://" + mmsAPIHostName + ":" + mmsAPIPort + "/catramms/v1/conf/ftp";

            mLogger.info("addFTPConf"
                            + ", mmsURL: " + mmsURL
                            + ", jsonFTPConf: " + jsonFTPConf
            );

            Date now = new Date();
            String contentType = null;
            mmsInfo = HttpFeedFetcher.fetchPostHttpsJson(mmsURL, contentType, timeoutInSeconds, maxRetriesNumber,
                    username, password, jsonFTPConf);
            mLogger.info("Elapsed time login (@" + mmsURL + "@): @" + (new Date().getTime() - now.getTime()) + "@ millisecs.");
        }
        catch (Exception e)
        {
            String errorMessage = "addFTPConf MMS failed. Exception: " + e;
            mLogger.error(errorMessage);

            throw new Exception(errorMessage);
        }
    }

    public void modifyFTPConf(String username, String password,
                                  Long confKey, String label, String ftpServer,
                              Long ftpPort, String ftpUserName, String ftpPassword,
                              String ftpRemoteDirectory)
            throws Exception
    {

        String mmsInfo;
        try
        {
            String jsonFTPConf;
            {
                JSONObject joFTPConf = new JSONObject();

                joFTPConf.put("Label", label);
                joFTPConf.put("Server", ftpServer);
                joFTPConf.put("Port", ftpPort);
                joFTPConf.put("UserName", ftpUserName);
                joFTPConf.put("Password", ftpPassword);
                joFTPConf.put("RemoteDirectory", ftpRemoteDirectory);

                jsonFTPConf = joFTPConf.toString(4);
            }

            String mmsURL = mmsAPIProtocol + "://" + mmsAPIHostName + ":" + mmsAPIPort + "/catramms/v1/conf/ftp/" + confKey;

            mLogger.info("modifyFTPConf"
                            + ", mmsURL: " + mmsURL
                            + ", jsonFTPConf: " + jsonFTPConf
            );

            Date now = new Date();
            mmsInfo = HttpFeedFetcher.fetchPutHttpsJson(mmsURL, timeoutInSeconds, maxRetriesNumber,
                    username, password, jsonFTPConf);
            mLogger.info("Elapsed time login (@" + mmsURL + "@): @" + (new Date().getTime() - now.getTime()) + "@ millisecs.");
        }
        catch (Exception e)
        {
            String errorMessage = "modifyFTPConf MMS failed. Exception: " + e;
            mLogger.error(errorMessage);

            throw new Exception(errorMessage);
        }
    }

    public void removeFTPConf(String username, String password,
                                  Long confKey)
            throws Exception
    {

        String mmsInfo;
        try
        {
            String mmsURL = mmsAPIProtocol + "://" + mmsAPIHostName + ":" + mmsAPIPort + "/catramms/v1/conf/ftp/" + confKey;

            mLogger.info("removeFTPConf"
                            + ", mmsURL: " + mmsURL
                            + ", confKey: " + confKey
            );

            Date now = new Date();
            mmsInfo = HttpFeedFetcher.fetchDeleteHttpsJson(mmsURL, timeoutInSeconds, maxRetriesNumber,
                    username, password);
            mLogger.info("Elapsed time login (@" + mmsURL + "@): @" + (new Date().getTime() - now.getTime()) + "@ millisecs.");
        }
        catch (Exception e)
        {
            String errorMessage = "removeFTPConf MMS failed. Exception: " + e;
            mLogger.error(errorMessage);

            throw new Exception(errorMessage);
        }
    }

    public List<FTPConf> getFTPConf(String username, String password)
            throws Exception
    {
        List<FTPConf> ftpConfList = new ArrayList<>();

        String mmsInfo;
        try
        {
            String mmsURL = mmsAPIProtocol + "://" + mmsAPIHostName + ":" + mmsAPIPort + "/catramms/v1/conf/ftp";

            mLogger.info("mmsURL: " + mmsURL);

            Date now = new Date();
            mmsInfo = HttpFeedFetcher.fetchGetHttpsJson(mmsURL, timeoutInSeconds, maxRetriesNumber,
                    username, password);
            mLogger.info("Elapsed time getLiveURLConf (@" + mmsURL + "@): @" + (new Date().getTime() - now.getTime()) + "@ millisecs.");
        }
        catch (Exception e)
        {
            String errorMessage = "MMS API failed. Exception: " + e;
            mLogger.error(errorMessage);

            throw new Exception(errorMessage);
        }

        try
        {
            JSONObject joMMSInfo = new JSONObject(mmsInfo);
            JSONObject joResponse = joMMSInfo.getJSONObject("response");
            JSONArray jaFTPConf = joResponse.getJSONArray("ftpConf");

            mLogger.info("jaFTPConf.length(): " + jaFTPConf.length());

            ftpConfList.clear();

            for (int ftpConfIndex = 0;
                 ftpConfIndex < jaFTPConf.length();
                 ftpConfIndex++)
            {
                FTPConf ftpConf = new FTPConf();

                JSONObject ftpConfInfo = jaFTPConf.getJSONObject(ftpConfIndex);

                fillFTPConf(ftpConf, ftpConfInfo);

                ftpConfList.add(ftpConf);
            }
        }
        catch (Exception e)
        {
            String errorMessage = "Parsing ftpConf failed. Exception: " + e;
            mLogger.error(errorMessage);

            throw new Exception(errorMessage);
        }

        return ftpConfList;
    }

    public void addEMailConf(String username, String password,
                           String label, String address,
                           String subject, String message)
            throws Exception
    {

        String mmsInfo;
        try
        {
            String jsonEMailConf;
            {
                JSONObject joEMailConf = new JSONObject();

                joEMailConf.put("Label", label);
                joEMailConf.put("Address", address);
                joEMailConf.put("Subject", subject);
                joEMailConf.put("Message", message);

                jsonEMailConf = joEMailConf.toString(4);
            }

            String mmsURL = mmsAPIProtocol + "://" + mmsAPIHostName + ":" + mmsAPIPort + "/catramms/v1/conf/email";

            mLogger.info("addEMailConf"
                            + ", mmsURL: " + mmsURL
                            + ", jsonEMailConf: " + jsonEMailConf
            );

            Date now = new Date();
            String contentType = null;
            mmsInfo = HttpFeedFetcher.fetchPostHttpsJson(mmsURL, contentType, timeoutInSeconds, maxRetriesNumber,
                    username, password, jsonEMailConf);
            mLogger.info("Elapsed time login (@" + mmsURL + "@): @" + (new Date().getTime() - now.getTime()) + "@ millisecs.");
        }
        catch (Exception e)
        {
            String errorMessage = "addEMailConf MMS failed. Exception: " + e;
            mLogger.error(errorMessage);

            throw new Exception(errorMessage);
        }
    }

    public void modifyEMailConf(String username, String password,
                              Long confKey, String label, String address,
                                String subject, String message)
            throws Exception
    {

        String mmsInfo;
        try
        {
            String jsonEMailConf;
            {
                JSONObject joEMailConf = new JSONObject();

                joEMailConf.put("Label", label);
                joEMailConf.put("Address", address);
                joEMailConf.put("Subject", subject);
                joEMailConf.put("Message", message);

                jsonEMailConf = joEMailConf.toString(4);
            }

            String mmsURL = mmsAPIProtocol + "://" + mmsAPIHostName + ":" + mmsAPIPort + "/catramms/v1/conf/email/" + confKey;

            mLogger.info("modifyEMailConf"
                            + ", mmsURL: " + mmsURL
                            + ", jsonEMailConf: " + jsonEMailConf
            );

            Date now = new Date();
            mmsInfo = HttpFeedFetcher.fetchPutHttpsJson(mmsURL, timeoutInSeconds, maxRetriesNumber,
                    username, password, jsonEMailConf);
            mLogger.info("Elapsed time login (@" + mmsURL + "@): @" + (new Date().getTime() - now.getTime()) + "@ millisecs.");
        }
        catch (Exception e)
        {
            String errorMessage = "modifyEMailConf MMS failed. Exception: " + e;
            mLogger.error(errorMessage);

            throw new Exception(errorMessage);
        }
    }

    public void removeEMailConf(String username, String password,
                              Long confKey)
            throws Exception
    {

        String mmsInfo;
        try
        {
            String mmsURL = mmsAPIProtocol + "://" + mmsAPIHostName + ":" + mmsAPIPort + "/catramms/v1/conf/email/" + confKey;

            mLogger.info("removeEMailConf"
                            + ", mmsURL: " + mmsURL
                            + ", confKey: " + confKey
            );

            Date now = new Date();
            mmsInfo = HttpFeedFetcher.fetchDeleteHttpsJson(mmsURL, timeoutInSeconds, maxRetriesNumber,
                    username, password);
            mLogger.info("Elapsed time login (@" + mmsURL + "@): @" + (new Date().getTime() - now.getTime()) + "@ millisecs.");
        }
        catch (Exception e)
        {
            String errorMessage = "removeEMailConf MMS failed. Exception: " + e;
            mLogger.error(errorMessage);

            throw new Exception(errorMessage);
        }
    }

    public List<EMailConf> getEMailConf(String username, String password)
            throws Exception
    {
        List<EMailConf> emailConfList = new ArrayList<>();

        String mmsInfo;
        try
        {
            String mmsURL = mmsAPIProtocol + "://" + mmsAPIHostName + ":" + mmsAPIPort + "/catramms/v1/conf/email";

            mLogger.info("mmsURL: " + mmsURL);

            Date now = new Date();
            mmsInfo = HttpFeedFetcher.fetchGetHttpsJson(mmsURL, timeoutInSeconds, maxRetriesNumber,
                    username, password);
            mLogger.info("Elapsed time getLiveURLConf (@" + mmsURL + "@): @" + (new Date().getTime() - now.getTime()) + "@ millisecs.");
        }
        catch (Exception e)
        {
            String errorMessage = "MMS API failed. Exception: " + e;
            mLogger.error(errorMessage);

            throw new Exception(errorMessage);
        }

        try
        {
            JSONObject joMMSInfo = new JSONObject(mmsInfo);
            JSONObject joResponse = joMMSInfo.getJSONObject("response");
            JSONArray jaEMailConf = joResponse.getJSONArray("emailConf");

            mLogger.info("jaEMailConf.length(): " + jaEMailConf.length());

            emailConfList.clear();

            for (int emailConfIndex = 0;
                 emailConfIndex < jaEMailConf.length();
                 emailConfIndex++)
            {
                EMailConf emailConf = new EMailConf();

                JSONObject emailConfInfo = jaEMailConf.getJSONObject(emailConfIndex);

                fillEMailConf(emailConf, emailConfInfo);

                emailConfList.add(emailConf);
            }
        }
        catch (Exception e)
        {
            String errorMessage = "Parsing emailConf failed. Exception: " + e;
            mLogger.error(errorMessage);

            throw new Exception(errorMessage);
        }

        return emailConfList;
    }

    private void fillUserProfile(UserProfile userProfile, JSONObject joUserProfileInfo)
            throws Exception
    {
        SimpleDateFormat simpleDateFormat = new SimpleDateFormat("yyyy-MM-dd'T'HH:mm:ss'Z'");
        simpleDateFormat.setTimeZone(TimeZone.getTimeZone("GMT"));

        try
        {
            userProfile.setUserKey(joUserProfileInfo.getLong("userKey"));
            userProfile.setName(joUserProfileInfo.getString("name"));
            userProfile.setCountry(joUserProfileInfo.getString("country"));
            userProfile.setEmailAddress(joUserProfileInfo.getString("eMailAddress"));
            userProfile.setCreationDate(simpleDateFormat.parse(joUserProfileInfo.getString("creationDate")));
            userProfile.setExpirationDate(simpleDateFormat.parse(joUserProfileInfo.getString("expirationDate")));
        }
        catch (Exception e)
        {
            String errorMessage = "fillUserProfile failed. Exception: " + e;
            mLogger.error(errorMessage);

            throw new Exception(errorMessage);
        }
    }

    private void fillWorkspaceDetailsList(List<WorkspaceDetails> workspaceDetailsList, JSONArray jaWorkspacesInfo)
            throws Exception
    {
        try
        {
            for (int workspaceIndex = 0; workspaceIndex < jaWorkspacesInfo.length(); workspaceIndex++)
            {
                JSONObject joWorkspaceInfo = jaWorkspacesInfo.getJSONObject(workspaceIndex);

                WorkspaceDetails workspaceDetails = new WorkspaceDetails();

                fillWorkspaceDetails(workspaceDetails, joWorkspaceInfo);

                workspaceDetailsList.add(workspaceDetails);
            }
        }
        catch (Exception e)
        {
            String errorMessage = "fillWorkspaceDetailsList failed. Exception: " + e;
            mLogger.error(errorMessage);

            throw new Exception(errorMessage);
        }
    }

    private void fillWorkspaceDetails(WorkspaceDetails workspaceDetails, JSONObject jaWorkspaceInfo)
            throws Exception
    {
        SimpleDateFormat simpleDateFormat = new SimpleDateFormat("yyyy-MM-dd'T'HH:mm:ss'Z'");
        simpleDateFormat.setTimeZone(TimeZone.getTimeZone("GMT"));

        try
        {
            workspaceDetails.setWorkspaceKey(jaWorkspaceInfo.getLong("workspaceKey"));
            workspaceDetails.setEnabled(jaWorkspaceInfo.getBoolean("isEnabled"));
            workspaceDetails.setName(jaWorkspaceInfo.getString("workspaceName"));
            workspaceDetails.setMaxEncodingPriority(jaWorkspaceInfo.getString("maxEncodingPriority"));
            workspaceDetails.setEncodingPeriod(jaWorkspaceInfo.getString("encodingPeriod"));
            workspaceDetails.setMaxIngestionsNumber(jaWorkspaceInfo.getLong("maxIngestionsNumber"));
            workspaceDetails.setMaxStorageInMB(jaWorkspaceInfo.getLong("maxStorageInMB"));
            workspaceDetails.setLanguageCode(jaWorkspaceInfo.getString("languageCode"));
            workspaceDetails.setCreationDate(simpleDateFormat.parse(jaWorkspaceInfo.getString("creationDate")));
            workspaceDetails.setApiKey(jaWorkspaceInfo.getString("apiKey"));
            workspaceDetails.setOwner(jaWorkspaceInfo.getBoolean("owner"));
            workspaceDetails.setAdmin(jaWorkspaceInfo.getBoolean("admin"));
            workspaceDetails.setIngestWorkflow(jaWorkspaceInfo.getBoolean("ingestWorkflow"));
            workspaceDetails.setCreateProfiles(jaWorkspaceInfo.getBoolean("createProfiles"));
            workspaceDetails.setDeliveryAuthorization(jaWorkspaceInfo.getBoolean("deliveryAuthorization"));
            workspaceDetails.setShareWorkspace(jaWorkspaceInfo.getBoolean("shareWorkspace"));
            workspaceDetails.setEditMedia(jaWorkspaceInfo.getBoolean("editMedia"));
        }
        catch (Exception e)
        {
            String errorMessage = "fillWorkspaceDetails failed. Exception: " + e;
            mLogger.error(errorMessage);

            throw new Exception(errorMessage);
        }
    }

    private void fillEncodingJob(EncodingJob encodingJob, JSONObject encodingJobInfo)
            throws Exception
    {
        SimpleDateFormat simpleDateFormat = new SimpleDateFormat("yyyy-MM-dd'T'HH:mm:ss'Z'");
        simpleDateFormat.setTimeZone(TimeZone.getTimeZone("GMT"));

        try {
            encodingJob.setEncodingJobKey(encodingJobInfo.getLong("encodingJobKey"));
            encodingJob.setType(encodingJobInfo.getString("type"));
            encodingJob.setStatus(encodingJobInfo.getString("status"));

            encodingJob.setEncodingPriority(encodingJobInfo.getString("encodingPriority"));
            encodingJob.setEncodingPriorityCode(encodingJobInfo.getInt("encodingPriorityCode"));
            encodingJob.setMaxEncodingPriorityCode(encodingJobInfo.getInt("maxEncodingPriorityCode"));

            if (encodingJobInfo.isNull("start"))
                encodingJob.setStart(null);
            else
                encodingJob.setStart(simpleDateFormat.parse(encodingJobInfo.getString("start")));
            if (encodingJobInfo.isNull("end"))
                encodingJob.setEnd(null);
            else
                encodingJob.setEnd(simpleDateFormat.parse(encodingJobInfo.getString("end")));
            if (encodingJobInfo.isNull("progress"))
                encodingJob.setProgress(null);
            else
                encodingJob.setProgress(encodingJobInfo.getLong("progress"));
            if (encodingJobInfo.isNull("failuresNumber"))
                encodingJob.setFailuresNumber(null);
            else
                encodingJob.setFailuresNumber(encodingJobInfo.getLong("failuresNumber"));
            if (encodingJobInfo.has("parameters"))
            {
                encodingJob.setParameters(encodingJobInfo.getJSONObject("parameters").toString(4));

                JSONObject joParameters = new JSONObject(encodingJob.getParameters());

                if (encodingJob.getType().equalsIgnoreCase("EncodeVideoAudio")
                        || encodingJob.getType().equalsIgnoreCase("EncodeImage"))
                {
                    encodingJob.setEncodingProfileKey(joParameters.getLong("encodingProfileKey"));
                    encodingJob.setSourcePhysicalPathKey(joParameters.getLong("sourcePhysicalPathKey"));
                }
                else if (encodingJob.getType().equalsIgnoreCase("OverlayImageOnVideo"))
                {
                    encodingJob.setSourceVideoPhysicalPathKey(joParameters.getLong("sourceVideoPhysicalPathKey"));
                    encodingJob.setSourceImagePhysicalPathKey(joParameters.getLong("sourceImagePhysicalPathKey"));
                }
                else if (encodingJob.getType().equalsIgnoreCase("OverlayTextOnVideo"))
                {
                    encodingJob.setSourceVideoPhysicalPathKey(joParameters.getLong("sourceVideoPhysicalPathKey"));
                }
                else if (encodingJob.getType().equalsIgnoreCase("GenerateFrames")
                        )
                {
                    encodingJob.setSourceVideoPhysicalPathKey(joParameters.getLong("sourceVideoPhysicalPathKey"));
                }
                else if (encodingJob.getType().equalsIgnoreCase("LiveRecorder")
                )
                {
                    encodingJob.setLiveURL(joParameters.getString("liveURL"));
                    encodingJob.setOutputFileFormat(joParameters.getString("outputFileFormat"));
                    encodingJob.setSegmentDurationInSeconds(joParameters.getLong("segmentDurationInSeconds"));
                    encodingJob.setRecordingPeriodEnd(new Date(1000 * joParameters.getLong("utcRecordingPeriodEnd")));
                    encodingJob.setRecordingPeriodStart(new Date(1000 * joParameters.getLong("utcRecordingPeriodStart")));
                }
                else
                {
                    mLogger.error("Wrong encodingJob.getType(): " + encodingJob.getType());
                }
            }
        }
        catch (Exception e)
        {
            String errorMessage = "fillEncodingJob failed. Exception: " + e;
            mLogger.error(errorMessage);

            throw new Exception(errorMessage);
        }
    }

    private void fillMediaItem(MediaItem mediaItem, JSONObject mediaItemInfo, boolean deep)
            throws Exception
    {
        SimpleDateFormat simpleDateFormat = new SimpleDateFormat("yyyy-MM-dd'T'HH:mm:ss'Z'");
        simpleDateFormat.setTimeZone(TimeZone.getTimeZone("GMT"));

        try {
            mediaItem.setMediaItemKey(mediaItemInfo.getLong("mediaItemKey"));
            mediaItem.setContentType(mediaItemInfo.getString("contentType"));
            mediaItem.setTitle(mediaItemInfo.getString("title"));
            if (!mediaItemInfo.isNull("deliveryFileName"))
                mediaItem.setDeliveryFileName(mediaItemInfo.getString("deliveryFileName"));
            mediaItem.setIngestionDate(simpleDateFormat.parse(mediaItemInfo.getString("ingestionDate")));
            mediaItem.setStartPublishing(simpleDateFormat.parse(mediaItemInfo.getString("startPublishing")));
            mediaItem.setEndPublishing(simpleDateFormat.parse(mediaItemInfo.getString("endPublishing")));
            if (!mediaItemInfo.isNull("ingester"))
                mediaItem.setIngester(mediaItemInfo.getString("ingester"));
            {
                JSONArray jaTags = mediaItemInfo.getJSONArray("tags");

                for (int tagIndex = 0; tagIndex < jaTags.length(); tagIndex++)
                    mediaItem.getTags().add(jaTags.getString(tagIndex));
            }
            if (!mediaItemInfo.isNull("userData"))
                mediaItem.setUserData(mediaItemInfo.getString("userData"));
            mediaItem.setProviderName(mediaItemInfo.getString("providerName"));
            mediaItem.setRetentionInMinutes(mediaItemInfo.getLong("retentionInMinutes"));

            if (deep)
            {
                JSONArray jaPhysicalPaths = mediaItemInfo.getJSONArray("physicalPaths");

                mediaItem.setSourcePhysicalPath(null);

                for (int physicalPathIndex = 0; physicalPathIndex < jaPhysicalPaths.length(); physicalPathIndex++)
                {
                    JSONObject physicalPathInfo = jaPhysicalPaths.getJSONObject(physicalPathIndex);

                    PhysicalPath physicalPath = new PhysicalPath();
                    physicalPath.setPhysicalPathKey(physicalPathInfo.getLong("physicalPathKey"));
                    if (physicalPathInfo.isNull("fileFormat"))
                        physicalPath.setFileFormat(null);
                    else
                        physicalPath.setFileFormat(physicalPathInfo.getString("fileFormat"));

                    // partitionNumber, relativePath and fileName are present only if the APIKey has the admin rights
                    try {
                        physicalPath.setPartitionNumber(physicalPathInfo.getLong("partitionNumber"));
                        physicalPath.setRelativePath(physicalPathInfo.getString("relativePath"));
                        physicalPath.setFileName(physicalPathInfo.getString("fileName"));
                    }
                    catch (Exception e)
                    {

                    }

                    physicalPath.setCreationDate(simpleDateFormat.parse(physicalPathInfo.getString("creationDate")));
                    if (physicalPathInfo.isNull("encodingProfileKey"))
                    {
                        physicalPath.setEncodingProfileKey(null);
                        mediaItem.setSourcePhysicalPath(physicalPath);
                    }
                    else
                        physicalPath.setEncodingProfileKey(physicalPathInfo.getLong("encodingProfileKey"));
                    physicalPath.setSizeInBytes(physicalPathInfo.getLong("sizeInBytes"));

                    // physicalPath.setMediaItem(mediaItem);

                    if (mediaItem.getContentType().equalsIgnoreCase("video"))
                    {
                        JSONObject videoDetailsInfo = physicalPathInfo.getJSONObject("videoDetails");

                        physicalPath.getVideoDetails().setDurationInMilliseconds(videoDetailsInfo.getLong("durationInMilliSeconds"));
                        physicalPath.getVideoDetails().setBitRate(videoDetailsInfo.getLong("bitRate"));

                        physicalPath.getVideoDetails().setVideoCodecName(videoDetailsInfo.getString("videoCodecName"));
                        physicalPath.getVideoDetails().setVideoBitRate(videoDetailsInfo.getLong("videoBitRate"));
                        physicalPath.getVideoDetails().setVideoProfile(videoDetailsInfo.getString("videoProfile"));
                        physicalPath.getVideoDetails().setVideoAvgFrameRate(videoDetailsInfo.getString("videoAvgFrameRate"));
                        physicalPath.getVideoDetails().setVideoWidth(videoDetailsInfo.getLong("videoWidth"));
                        physicalPath.getVideoDetails().setVideoHeight(videoDetailsInfo.getLong("videoHeight"));

                        physicalPath.getVideoDetails().setAudioCodecName(videoDetailsInfo.getString("audioCodecName"));
                        physicalPath.getVideoDetails().setAudioBitRate(videoDetailsInfo.getLong("audioBitRate"));
                        physicalPath.getVideoDetails().setAudioChannels(videoDetailsInfo.getLong("audioChannels"));
                        physicalPath.getVideoDetails().setAudioSampleRate(videoDetailsInfo.getLong("audioSampleRate"));
                    }
                    else if (mediaItem.getContentType().equalsIgnoreCase("audio"))
                    {
                        JSONObject audioDetailsInfo = physicalPathInfo.getJSONObject("audioDetails");

                        physicalPath.getAudioDetails().setDurationInMilliseconds(audioDetailsInfo.getLong("durationInMilliSeconds"));
                        physicalPath.getAudioDetails().setCodecName(audioDetailsInfo.getString("codecName"));
                        physicalPath.getAudioDetails().setBitRate(audioDetailsInfo.getLong("bitRate"));
                        physicalPath.getAudioDetails().setSampleRate(audioDetailsInfo.getLong("sampleRate"));
                        physicalPath.getAudioDetails().setChannels(audioDetailsInfo.getLong("channels"));
                    }
                    else if (mediaItem.getContentType().equalsIgnoreCase("image"))
                    {
                        JSONObject imageDetailsInfo = physicalPathInfo.getJSONObject("imageDetails");

                        physicalPath.getImageDetails().setWidth(imageDetailsInfo.getLong("width"));
                        physicalPath.getImageDetails().setHeight(imageDetailsInfo.getLong("height"));
                        physicalPath.getImageDetails().setFormat(imageDetailsInfo.getString("format"));
                        physicalPath.getImageDetails().setQuality(imageDetailsInfo.getLong("quality"));
                    }

                    mediaItem.getPhysicalPathList().add(physicalPath);
                }
            }
        }
        catch (Exception e)
        {
            String errorMessage = "fillMediaItem failed. Exception: " + e;
            mLogger.error(errorMessage);

            throw new Exception(errorMessage);
        }
    }

    private void fillIngestionWorkflow(IngestionWorkflow ingestionWorkflow, JSONObject ingestionWorkflowInfo, boolean deep)
            throws Exception
    {
        SimpleDateFormat simpleDateFormat = new SimpleDateFormat("yyyy-MM-dd'T'HH:mm:ss'Z'");
        simpleDateFormat.setTimeZone(TimeZone.getTimeZone("GMT"));

        try {
            ingestionWorkflow.setIngestionRootKey(ingestionWorkflowInfo.getLong("ingestionRootKey"));
            ingestionWorkflow.setLabel(ingestionWorkflowInfo.getString("label"));
            ingestionWorkflow.setStatus(ingestionWorkflowInfo.getString("status"));
            ingestionWorkflow.setIngestionDate(simpleDateFormat.parse(ingestionWorkflowInfo.getString("ingestionDate")));
            ingestionWorkflow.setLastUpdate(simpleDateFormat.parse(ingestionWorkflowInfo.getString("lastUpdate")));

            if (deep)
            {
                JSONArray jaIngestionJobs = ingestionWorkflowInfo.getJSONArray("ingestionJobs");

                for (int ingestionJobIndex = 0; ingestionJobIndex < jaIngestionJobs.length(); ingestionJobIndex++)
                {
                    JSONObject ingestionJobInfo = jaIngestionJobs.getJSONObject(ingestionJobIndex);

                    IngestionJob ingestionJob = new IngestionJob();

                    fillIngestionJob(ingestionJob, ingestionJobInfo);

                    ingestionWorkflow.getIngestionJobList().add(ingestionJob);
                }
            }
        }
        catch (Exception e)
        {
            String errorMessage = "fillIngestionWorkflow failed. Exception: " + e;
            mLogger.error(errorMessage);

            throw new Exception(errorMessage);
        }
    }

    private void fillIngestionJob(IngestionJob ingestionJob, JSONObject ingestionJobInfo)
            throws Exception
    {
        SimpleDateFormat simpleDateFormat = new SimpleDateFormat("yyyy-MM-dd'T'HH:mm:ss'Z'");
        simpleDateFormat.setTimeZone(TimeZone.getTimeZone("GMT"));

        try {
            ingestionJob.setIngestionJobKey(ingestionJobInfo.getLong("ingestionJobKey"));
            ingestionJob.setLabel(ingestionJobInfo.getString("label"));
            ingestionJob.setIngestionType(ingestionJobInfo.getString("ingestionType"));
            ingestionJob.setMetaDataContent(ingestionJobInfo.getString("metaDataContent"));
            if (ingestionJobInfo.isNull("startProcessing"))
                ingestionJob.setStartProcessing(null);
            else
                ingestionJob.setStartProcessing(simpleDateFormat.parse(ingestionJobInfo.getString("startProcessing")));
            if (ingestionJobInfo.isNull("endProcessing"))
                ingestionJob.setEndProcessing(null);
            else
                ingestionJob.setEndProcessing(simpleDateFormat.parse(ingestionJobInfo.getString("endProcessing")));
            ingestionJob.setStatus(ingestionJobInfo.getString("status"));
            if (ingestionJobInfo.isNull("errorMessage"))
                ingestionJob.setErrorMessage(null);
            else
                ingestionJob.setErrorMessage(ingestionJobInfo.getString("errorMessage"));
            if (ingestionJobInfo.isNull("downloadingProgress"))
                ingestionJob.setDownloadingProgress(null);
            else
                ingestionJob.setDownloadingProgress(ingestionJobInfo.getLong("downloadingProgress"));
            if (ingestionJobInfo.isNull("uploadingProgress"))
                ingestionJob.setUploadingProgress(null);
            else
                ingestionJob.setUploadingProgress(ingestionJobInfo.getLong("uploadingProgress"));

            if (ingestionJobInfo.isNull("ingestionRootKey"))
                ingestionJob.setIngestionRookKey(null);
            else
                ingestionJob.setIngestionRookKey(ingestionJobInfo.getLong("ingestionRootKey"));

            JSONArray jaMediaItems = ingestionJobInfo.getJSONArray("mediaItems");
            for (int mediaItemIndex = 0; mediaItemIndex < jaMediaItems.length(); mediaItemIndex++)
            {
                JSONObject joMediaItem = jaMediaItems.getJSONObject(mediaItemIndex);

                IngestionJobMediaItem ingestionJobMediaItem = new IngestionJobMediaItem();
                ingestionJobMediaItem.setMediaItemKey(joMediaItem.getLong("mediaItemKey"));
                ingestionJobMediaItem.setPhysicalPathKey(joMediaItem.getLong("physicalPathKey"));

                ingestionJob.getIngestionJobMediaItemList().add(ingestionJobMediaItem);
            }

            if (ingestionJobInfo.has("encodingJob")) {
                JSONObject encodingJobInfo = ingestionJobInfo.getJSONObject("encodingJob");

                EncodingJob encodingJob = new EncodingJob();

                fillEncodingJob(encodingJob, encodingJobInfo);

                ingestionJob.setEncodingJob(encodingJob);
            }
        }
        catch (Exception e)
        {
            String errorMessage = "fillIngestionJob failed. Exception: " + e;
            mLogger.error(errorMessage);

            throw new Exception(errorMessage);
        }
    }

    private void fillEncodingProfile(EncodingProfile encodingProfile, JSONObject encodingProfileInfo, boolean deep)
            throws Exception
    {
        SimpleDateFormat simpleDateFormat = new SimpleDateFormat("yyyy-MM-dd'T'HH:mm:ss'Z'");
        simpleDateFormat.setTimeZone(TimeZone.getTimeZone("GMT"));

        try
        {
            encodingProfile.setEncodingProfileKey(encodingProfileInfo.getLong("encodingProfileKey"));
            encodingProfile.setLabel(encodingProfileInfo.getString("label"));
            encodingProfile.setContentType(encodingProfileInfo.getString("contentType"));

            JSONObject joProfileInfo = encodingProfileInfo.getJSONObject("profile");
            encodingProfile.setFileFormat(joProfileInfo.getString("FileFormat"));

            if (deep)
            {
                if (encodingProfile.getContentType().equalsIgnoreCase("video"))
                {
                    JSONObject joVideoInfo = joProfileInfo.getJSONObject("Video");

                    encodingProfile.getVideoDetails().setCodec(joVideoInfo.getString("Codec"));
                    if (joVideoInfo.isNull("Profile"))
                        encodingProfile.getVideoDetails().setProfile(null);
                    else
                        encodingProfile.getVideoDetails().setProfile(joVideoInfo.getString("Profile"));
                    encodingProfile.getVideoDetails().setWidth(joVideoInfo.getLong("Width"));
                    encodingProfile.getVideoDetails().setHeight(joVideoInfo.getLong("Height"));
                    encodingProfile.getVideoDetails().setTwoPasses(joVideoInfo.getBoolean("TwoPasses"));
                    if (joVideoInfo.isNull("KBitRate"))
                        encodingProfile.getVideoDetails().setkBitRate(null);
                    else
                        encodingProfile.getVideoDetails().setkBitRate(joVideoInfo.getLong("KBitRate"));
                    if (joVideoInfo.isNull("OtherOutputParameters"))
                        encodingProfile.getVideoDetails().setOtherOutputParameters(null);
                    else
                        encodingProfile.getVideoDetails().setOtherOutputParameters(joVideoInfo.getString("OtherOutputParameters"));
                    if (joVideoInfo.isNull("KMaxRate"))
                        encodingProfile.getVideoDetails().setkMaxRate(null);
                    else
                        encodingProfile.getVideoDetails().setkMaxRate(joVideoInfo.getLong("KMaxRate"));
                    if (joVideoInfo.isNull("KBufSize"))
                        encodingProfile.getVideoDetails().setkBufSize(null);
                    else
                        encodingProfile.getVideoDetails().setkBufSize(joVideoInfo.getLong("KBufSize"));
                    if (joVideoInfo.isNull("FrameRate"))
                        encodingProfile.getVideoDetails().setFrameRate(null);
                    else
                        encodingProfile.getVideoDetails().setFrameRate(joVideoInfo.getLong("FrameRate"));
                    if (joVideoInfo.isNull("KeyFrameIntervalInSeconds"))
                        encodingProfile.getVideoDetails().setKeyFrameIntervalInSeconds(null);
                    else
                        encodingProfile.getVideoDetails().setKeyFrameIntervalInSeconds(joVideoInfo.getLong("KeyFrameIntervalInSeconds"));

                    JSONObject joAudioInfo = joProfileInfo.getJSONObject("Audio");

                    encodingProfile.getAudioDetails().setCodec(joAudioInfo.getString("Codec"));
                    if (joAudioInfo.isNull("KBitRate"))
                        encodingProfile.getAudioDetails().setkBitRate(null);
                    else
                        encodingProfile.getAudioDetails().setkBitRate(joAudioInfo.getLong("KBitRate"));
                    if (joAudioInfo.isNull("OtherOutputParameters"))
                        encodingProfile.getAudioDetails().setOtherOutputParameters(null);
                    else
                        encodingProfile.getAudioDetails().setOtherOutputParameters(joAudioInfo.getString("OtherOutputParameters"));
                    if (joAudioInfo.isNull("ChannelsNumber"))
                        encodingProfile.getAudioDetails().setChannelsNumber(null);
                    else
                        encodingProfile.getAudioDetails().setChannelsNumber(joAudioInfo.getLong("ChannelsNumber"));
                    if (joAudioInfo.isNull("SampleRate"))
                        encodingProfile.getAudioDetails().setSampleRate(null);
                    else
                        encodingProfile.getAudioDetails().setSampleRate(joAudioInfo.getLong("SampleRate"));
                }
                else if (encodingProfile.getContentType().equalsIgnoreCase("audio"))
                {
                    JSONObject joAudioInfo = joProfileInfo.getJSONObject("Audio");

                    encodingProfile.getAudioDetails().setCodec(joAudioInfo.getString("Codec"));
                    if (joAudioInfo.isNull("KBitRate"))
                        encodingProfile.getAudioDetails().setkBitRate(null);
                    else
                        encodingProfile.getAudioDetails().setkBitRate(joAudioInfo.getLong("KBitRate"));
                    if (joAudioInfo.isNull("OtherOutputParameters"))
                        encodingProfile.getAudioDetails().setOtherOutputParameters(null);
                    else
                        encodingProfile.getAudioDetails().setOtherOutputParameters(joAudioInfo.getString("OtherOutputParameters"));
                    if (joAudioInfo.isNull("ChannelsNumber"))
                        encodingProfile.getAudioDetails().setChannelsNumber(null);
                    else
                        encodingProfile.getAudioDetails().setChannelsNumber(joAudioInfo.getLong("ChannelsNumber"));
                    if (joAudioInfo.isNull("SampleRate"))
                        encodingProfile.getAudioDetails().setSampleRate(null);
                    else
                        encodingProfile.getAudioDetails().setSampleRate(joAudioInfo.getLong("SampleRate"));
                }
                else if (encodingProfile.getContentType().equalsIgnoreCase("image"))
                {
                    JSONObject joImageInfo = joProfileInfo.getJSONObject("Image");

                    encodingProfile.getImageDetails().setWidth(joImageInfo.getLong("Width"));
                    encodingProfile.getImageDetails().setHeight(joImageInfo.getLong("Height"));
                    encodingProfile.getImageDetails().setAspectRatio(joImageInfo.getBoolean("AspectRatio"));
                    encodingProfile.getImageDetails().setInterlaceType(joImageInfo.getString("InterlaceType"));
                }
            }
        }
        catch (Exception e)
        {
            String errorMessage = "fillEncodingProfile failed. Exception: " + e;
            mLogger.error(errorMessage);

            throw new Exception(errorMessage);
        }
    }

    private void fillEncodingProfilesSet(EncodingProfilesSet encodingProfilesSet,
                                         JSONObject encodingProfilesSetInfo, boolean deep)
            throws Exception
    {
        SimpleDateFormat simpleDateFormat = new SimpleDateFormat("yyyy-MM-dd'T'HH:mm:ss'Z'");
        simpleDateFormat.setTimeZone(TimeZone.getTimeZone("GMT"));

        try {
            encodingProfilesSet.setEncodingProfilesSetKey(encodingProfilesSetInfo.getLong("encodingProfilesSetKey"));
            encodingProfilesSet.setContentType(encodingProfilesSetInfo.getString("contentType"));
            encodingProfilesSet.setLabel(encodingProfilesSetInfo.getString("label"));

            if (deep)
            {
                JSONArray jaEncodingProfiles = encodingProfilesSetInfo.getJSONArray("encodingProfiles");

                for (int encodingProfileIndex = 0; encodingProfileIndex < jaEncodingProfiles.length(); encodingProfileIndex++)
                {
                    JSONObject encodingProfileInfo = jaEncodingProfiles.getJSONObject(encodingProfileIndex);

                    EncodingProfile encodingProfile = new EncodingProfile();

                    fillEncodingProfile(encodingProfile, encodingProfileInfo, deep);

                    encodingProfilesSet.getEncodingProfileList().add(encodingProfile);
                }
            }
        }
        catch (Exception e)
        {
            String errorMessage = "fillEncodingProfilesSet failed. Exception: " + e;
            mLogger.error(errorMessage);

            throw new Exception(errorMessage);
        }
    }

    private void fillYouTubeConf(YouTubeConf youTubeConf, JSONObject youTubeConfInfo)
            throws Exception
    {
        try {
            youTubeConf.setConfKey(youTubeConfInfo.getLong("confKey"));
            youTubeConf.setLabel(youTubeConfInfo.getString("label"));
            youTubeConf.setRefreshToken(youTubeConfInfo.getString("refreshToken"));
        }
        catch (Exception e)
        {
            String errorMessage = "fillYouTubeConf failed. Exception: " + e;
            mLogger.error(errorMessage);

            throw new Exception(errorMessage);
        }
    }

    private void fillFacebookConf(FacebookConf facebookConf, JSONObject facebookConfInfo)
            throws Exception
    {
        try {
            facebookConf.setConfKey(facebookConfInfo.getLong("confKey"));
            facebookConf.setLabel(facebookConfInfo.getString("label"));
            facebookConf.setPageToken(facebookConfInfo.getString("pageToken"));
        }
        catch (Exception e)
        {
            String errorMessage = "fillFacebookConf failed. Exception: " + e;
            mLogger.error(errorMessage);

            throw new Exception(errorMessage);
        }
    }

    private void fillLiveURLConf(LiveURLConf liveURLConf, JSONObject liveURLConfInfo)
            throws Exception
    {
        try {
            liveURLConf.setConfKey(liveURLConfInfo.getLong("confKey"));
            liveURLConf.setLabel(liveURLConfInfo.getString("label"));
            liveURLConf.setLiveURL(liveURLConfInfo.getString("liveURL"));
        }
        catch (Exception e)
        {
            String errorMessage = "fillLiveURLConf failed. Exception: " + e;
            mLogger.error(errorMessage);

            throw new Exception(errorMessage);
        }
    }

    private void fillFTPConf(FTPConf ftpConf, JSONObject ftpConfInfo)
            throws Exception
    {
        try {
            ftpConf.setConfKey(ftpConfInfo.getLong("confKey"));
            ftpConf.setLabel(ftpConfInfo.getString("label"));
            ftpConf.setServer(ftpConfInfo.getString("server"));
            ftpConf.setPort(ftpConfInfo.getLong("port"));
            ftpConf.setUserName(ftpConfInfo.getString("userName"));
            ftpConf.setPassword(ftpConfInfo.getString("password"));
            ftpConf.setRemoteDirectory(ftpConfInfo.getString("remoteDirectory"));
        }
        catch (Exception e)
        {
            String errorMessage = "fillFTPConf failed. Exception: " + e;
            mLogger.error(errorMessage);

            throw new Exception(errorMessage);
        }
    }

    private void fillEMailConf(EMailConf emailConf, JSONObject emailConfInfo)
            throws Exception
    {
        try {
            emailConf.setConfKey(emailConfInfo.getLong("confKey"));
            emailConf.setLabel(emailConfInfo.getString("label"));
            emailConf.setAddress(emailConfInfo.getString("address"));
            emailConf.setSubject(emailConfInfo.getString("subject"));
            emailConf.setMessage(emailConfInfo.getString("message"));
        }
        catch (Exception e)
        {
            String errorMessage = "fillEMailConf failed. Exception: " + e;
            mLogger.error(errorMessage);

            throw new Exception(errorMessage);
        }
    }

    private void loadConfigurationParameters()
    {
        try
        {
            Properties properties = getConfigurationParameters();

            {
                {
                    String tmpTimeoutInSeconds = properties.getProperty("catramms.mms.timeoutInSeconds");
                    if (tmpTimeoutInSeconds == null)
                    {
                        String errorMessage = "No catramms.mms.timeoutInSeconds found. ConfigurationFileName: " + configFileName;
                        mLogger.error(errorMessage);

                        return;
                    }
                    timeoutInSeconds = Integer.parseInt(tmpTimeoutInSeconds);

                    String tmpMaxRetriesNumber = properties.getProperty("catramms.mms.maxRetriesNumber");
                    if (tmpMaxRetriesNumber == null)
                    {
                        String errorMessage = "No catramms.mms.maxRetriesNumber found. ConfigurationFileName: " + configFileName;
                        mLogger.error(errorMessage);

                        return;
                    }
                    maxRetriesNumber = Integer.parseInt(tmpMaxRetriesNumber);

                    mmsAPIProtocol = properties.getProperty("catramms.mms.api.protocol");
                    if (mmsAPIProtocol == null)
                    {
                        String errorMessage = "No catramms.mms.api.protocol found. ConfigurationFileName: " + configFileName;
                        mLogger.error(errorMessage);

                        return;
                    }

                    mmsAPIHostName = properties.getProperty("catramms.mms.api.hostname");
                    if (mmsAPIHostName == null)
                    {
                        String errorMessage = "No catramms.mms.api.hostname found. ConfigurationFileName: " + configFileName;
                        mLogger.error(errorMessage);

                        return;
                    }

                    String tmpMmsAPIPort = properties.getProperty("catramms.mms.api.port");
                    if (tmpMmsAPIPort == null)
                    {
                        String errorMessage = "No catramms.mms.api.port found. ConfigurationFileName: " + configFileName;
                        mLogger.error(errorMessage);

                        return;
                    }
                    mmsAPIPort = Integer.parseInt(tmpMmsAPIPort);

                    mmsBinaryProtocol = properties.getProperty("catramms.mms.binary.protocol");
                    if (mmsBinaryProtocol == null)
                    {
                        String errorMessage = "No catramms.mms.binary.protocol found. ConfigurationFileName: " + configFileName;
                        mLogger.error(errorMessage);

                        return;
                    }

                    mmsBinaryHostName = properties.getProperty("catramms.mms.binary.hostname");
                    if (mmsBinaryHostName == null)
                    {
                        String errorMessage = "No catramms.mms.binary.hostname found. ConfigurationFileName: " + configFileName;
                        mLogger.error(errorMessage);

                        return;
                    }

                    String tmpMmsBinaryPort = properties.getProperty("catramms.mms.binary.port");
                    if (tmpMmsBinaryPort == null)
                    {
                        String errorMessage = "No catramms.mms.binary.port found. ConfigurationFileName: " + configFileName;
                        mLogger.error(errorMessage);

                        return;
                    }
                    mmsBinaryPort = Integer.parseInt(tmpMmsBinaryPort);
                }
            }
        }
        catch (Exception e)
        {
            String errorMessage = "Problems to load the configuration file. Exception: " + e + ", ConfigurationFileName: " + configFileName;
            mLogger.error(errorMessage);

            return;
        }
    }

    public Properties getConfigurationParameters()
    {
        Properties properties = new Properties();

        try
        {
            {
                InputStream inputStream;
                String commonPath = "/mnt/common/mp";
                String tomcatPath = System.getProperty("catalina.base");

                File configFile = new File(commonPath + "/conf/" + configFileName);
                if (configFile.exists())
                {
                    mLogger.info("Configuration file: " + configFile.getAbsolutePath());
                    inputStream = new FileInputStream(configFile);
                }
                else
                {
                    configFile = new File(tomcatPath + "/conf/" + configFileName);
                    if (configFile.exists())
                    {
                        mLogger.info("Configuration file: " + configFile.getAbsolutePath());
                        inputStream = new FileInputStream(configFile);
                    }
                    else
                    {
                        mLogger.info("Using the internal configuration file");
                        inputStream = CatraMMS.class.getClassLoader().getResourceAsStream(configFileName);
                    }
                }

                if (inputStream == null)
                {
                    String errorMessage = "Login configuration file not found. ConfigurationFileName: " + configFileName;
                    mLogger.error(errorMessage);

                    return properties;
                }
                properties.load(inputStream);
            }
        }
        catch (Exception e)
        {
            String errorMessage = "Problems to load the configuration file. Exception: " + e + ", ConfigurationFileName: " + configFileName;
            mLogger.error(errorMessage);

            return properties;
        }

        return properties;
    }

}
