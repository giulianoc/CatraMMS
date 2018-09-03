package com.catramms.utility.catramms;

import com.catramms.backing.entity.*;
import com.catramms.backing.newWorkflow.IngestionResult;
import com.catramms.utility.httpFetcher.HttpFeedFetcher;
import org.apache.log4j.Logger;
import org.json.JSONArray;
import org.json.JSONObject;
import org.primefaces.model.UploadedFile;

import java.io.File;
import java.text.SimpleDateFormat;
import java.util.*;

/**
 * Created by multi on 08.06.18.
 */
public class CatraMMS {

    private final Logger mLogger = Logger.getLogger(this.getClass());

    private int timeoutInSeconds = 30;
    private int maxRetriesNumber = 1;
    private String mmsAPIProtocol = "https";
    private String mmsAPIHostName = "mms-api.catrasoft.cloud";
    private int mmsAPIPort = 443;
    private String mmsBinaryProtocol = "http";
    private String mmsBinaryHostName = "mms-binary.catrasoft.cloud";
    private int mmsBinaryPort = 80;


    public Long shareWorkspace(String username, String password,
                               String userNameToShare, String emailAddressToShare,
                               String passwordToShare, String countryToShare,
                               Boolean ingestWorkflow, Boolean createProfiles,
                               Boolean deliveryAuthorization, Boolean shareWorkspace,
                               Boolean editMedia, Long workspaceKeyToShare)
            throws Exception
    {
        String mmsInfo;
        try
        {
            String mmsURL = mmsAPIProtocol + "://" + mmsAPIHostName + ":" + mmsAPIPort + "/catramms/v1/workspace/"
                    + workspaceKeyToShare + "/share"
                    + "?ingestWorkflow=" + ingestWorkflow.toString()
                    + "&createProfiles=" + createProfiles.toString()
                    + "&deliveryAuthorization=" + deliveryAuthorization.toString()
                    + "&shareWorkspace=" + shareWorkspace.toString()
                    + "&editMedia=" + editMedia.toString()
            ;

            String postBodyRequest = "{ "
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
            mmsInfo = HttpFeedFetcher.fetchPostHttpsJson(mmsURL, timeoutInSeconds, maxRetriesNumber,
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

    public String confirmUser(Long userKey, String confirmationCode)
            throws Exception
    {
        String mmsInfo;
        try
        {
            String mmsURL = mmsAPIProtocol + "://" + mmsAPIHostName + ":" + mmsAPIPort
                    + "/catramms/v1/user/" + userKey + "/" + confirmationCode;

            String username = null;
            String password = null;
            String postBodyRequest = null;

            mLogger.info("confirmUser"
                            + ", mmsURL: " + mmsURL
            );

            Date now = new Date();
            mmsInfo = HttpFeedFetcher.fetchPutHttpsJson(mmsURL, timeoutInSeconds, maxRetriesNumber,
                    username, password, postBodyRequest);
            mLogger.info("Elapsed time confirmUser (@" + mmsURL + "@): @" + (new Date().getTime() - now.getTime()) + "@ millisecs.");
        }
        catch (Exception e)
        {
            String errorMessage = "shareWorkspace failed. Exception: " + e;
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
            String errorMessage = "Parsing workspaceDetails failed. Exception: " + e;
            mLogger.error(errorMessage);

            throw new Exception(errorMessage);
        }

        return apiKey;
    }

    public List<Object> login(String username, String password, List<WorkspaceDetails> workspaceDetailsList)
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
            mmsInfo = HttpFeedFetcher.fetchPostHttpsJson(mmsURL, timeoutInSeconds, maxRetriesNumber,
                    username, password, postBodyRequest);
            mLogger.info("Elapsed time login (@" + mmsURL + "@): @" + (new Date().getTime() - now.getTime()) + "@ millisecs.");
        }
        catch (Exception e)
        {
            String errorMessage = "Login MMS failed. Exception: " + e;
            mLogger.error(errorMessage);

            throw new Exception(errorMessage);
        }

        List<Object> userKeyAndName = new ArrayList<>();

        Long userKey;
        String userName;

        try
        {
            JSONObject joWMMSInfo = new JSONObject(mmsInfo);
            userKey = joWMMSInfo.getLong("userKey");
            userName = joWMMSInfo.getString("userName");
            JSONArray jaWorkspacesInfo = joWMMSInfo.getJSONArray("workspaces");

            for (int workspaceIndex = 0; workspaceIndex < jaWorkspacesInfo.length(); workspaceIndex++)
            {
                JSONObject workspaceInfo = jaWorkspacesInfo.getJSONObject(workspaceIndex);

                WorkspaceDetails workspaceDetails = new WorkspaceDetails();
                workspaceDetails.setWorkspaceKey(workspaceInfo.getLong("workspaceKey"));
                workspaceDetails.setName(workspaceInfo.getString("workspaceName"));
                workspaceDetails.setApiKey(workspaceInfo.getString("apiKey"));
                workspaceDetails.setOwner(workspaceInfo.getBoolean("owner"));
                workspaceDetails.setAdmin(workspaceInfo.getBoolean("admin"));
                workspaceDetails.setIngestWorkflow(workspaceInfo.getBoolean("ingestWorkflow"));
                workspaceDetails.setCreateProfiles(workspaceInfo.getBoolean("createProfiles"));
                workspaceDetails.setDeliveryAuthorization(workspaceInfo.getBoolean("deliveryAuthorization"));
                workspaceDetails.setShareWorkspace(workspaceInfo.getBoolean("shareWorkspace"));
                workspaceDetails.setEditMedia(workspaceInfo.getBoolean("editMedia"));

                workspaceDetailsList.add(workspaceDetails);
            }
        }
        catch (Exception e)
        {
            String errorMessage = "Parsing workspaceDetails failed. Exception: " + e;
            mLogger.error(errorMessage);

            throw new Exception(errorMessage);
        }

        userKeyAndName.add(userKey);
        userKeyAndName.add(userName);

        return userKeyAndName;
    }

    public IngestionResult ingestWorkflow(String username, String password,
                                          String jsonWorkflow, List<IngestionResult> ingestionJobList)
            throws Exception
    {
        String mmsInfo;
        try
        {
            String mmsURL = mmsAPIProtocol + "://" + mmsAPIHostName + ":" + mmsAPIPort + "/catramms/v1/ingestion";

            mLogger.info("ingestWorkflow"
                    + ", mmsURL: " + mmsURL
                            + ", jsonWorkflow: " + jsonWorkflow
            );

            Date now = new Date();
            mmsInfo = HttpFeedFetcher.fetchPostHttpsJson(mmsURL, timeoutInSeconds, maxRetriesNumber,
                    username, password, jsonWorkflow);
            mLogger.info("Elapsed time login (@" + mmsURL + "@): @" + (new Date().getTime() - now.getTime()) + "@ millisecs.");
        }
        catch (Exception e)
        {
            String errorMessage = "ingestWorkflow MMS failed. Exception: " + e;
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
            String errorMessage = "Parsing workspaceDetails failed. Exception: " + e;
            mLogger.error(errorMessage);

            throw new Exception(errorMessage);
        }

        return workflowRoot;
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
            String errorMessage = "Parsing workspaceDetails failed. Exception: " + e;
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
            String errorMessage = "Parsing workspaceDetails failed. Exception: " + e;
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
                    + "/catramms/v1/encodingJobs/" + encodingJobKey + "/" + newEncodingJobPriorityCode;

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

    public Vector<Long> getMediaItems(String username, String password,
                          Long maxMediaItemsNumber, String contentType,
                          Date start, Date end, String title,
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
            String mmsURL = mmsAPIProtocol + "://" + mmsAPIHostName + ":" + mmsAPIPort + "/catramms/v1/mediaItems"
                    + "?start=0"
                    + "&rows=" + maxMediaItemsNumber
                    + "&contentType=" + contentType
                    + "&title=" + (title == null ? "" : java.net.URLEncoder.encode(title, "UTF-8"))
                    + "&startIngestionDate=" + simpleDateFormat.format(start)
                    + "&endIngestionDate=" + simpleDateFormat.format(end);

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
            String errorMessage = "Parsing workspaceDetails failed. Exception: " + e;
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
            String errorMessage = "Parsing workspaceDetails failed. Exception: " + e;
            mLogger.error(errorMessage);

            throw new Exception(errorMessage);
        }

        return mediaItem;
    }

    public Long getIngestionWorkflows(String username, String password,
                              Long maxIngestionWorkflowsNumber,
                              Date start, Date end, boolean ascending,
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

                boolean deep = false;
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
            String mmsURL = mmsAPIProtocol + "://" + mmsAPIHostName + ":" + mmsAPIPort + "/catramms/v1/ingestionJobs/" + ingestionJobKey;

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
            String errorMessage = "Parsing workspaceDetails failed. Exception: " + e;
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
            String errorMessage = "Parsing workspaceDetails failed. Exception: " + e;
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
            String errorMessage = "Parsing workspaceDetails failed. Exception: " + e;
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
            String errorMessage = "Parsing workspaceDetails failed. Exception: " + e;
            mLogger.error(errorMessage);

            throw new Exception(errorMessage);
        }
    }

    public String getDeliveryURL(String username, String password,
                                 Long physicalPathKey,
                                 long ttlInSeconds, int maxRetries)
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
            String errorMessage = "Parsing workspaceDetails failed. Exception: " + e;
            mLogger.error(errorMessage);

            throw new Exception(errorMessage);
        }

        return deliveryURL;
    }

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
            if (!mediaItemInfo.isNull("keywords"))
                mediaItem.setKeywords(mediaItemInfo.getString("keywords"));
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
}
