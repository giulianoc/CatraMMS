package com.catramms.backing;

import com.catramms.backing.common.Player;
import com.catramms.backing.common.SessionUtils;
import com.catramms.backing.common.Workspace;
import com.catramms.backing.entity.MediaItem;
import com.catramms.backing.entity.PhysicalPath;
import com.catramms.backing.entity.WorkspaceDetails;
import com.catramms.utility.catramms.CatraMMS;
import org.apache.log4j.Logger;
import org.json.JSONArray;
import org.json.JSONObject;

import javax.annotation.PostConstruct;
import javax.faces.application.FacesMessage;
import javax.faces.bean.ManagedBean;
import javax.faces.bean.ViewScoped;
import javax.faces.context.FacesContext;
import javax.servlet.http.HttpSession;
import java.io.Serializable;
import java.text.SimpleDateFormat;
import java.util.*;

/**
 * Created with IntelliJ IDEA.
 * User: multi
 * Date: 27/09/15
 * Time: 20:28
 * To change this template use File | Settings | File Templates.
 */
@ManagedBean
@ViewScoped
public class MediaItems extends Workspace implements Serializable {

    // static because the class is Serializable
    private static final Logger mLogger = Logger.getLogger(MediaItems.class);

    private Player player;

    private int autoRefreshPeriodInSeconds;
    private boolean autoRefresh;
    private Date begin;
    private Date end;
    private String title;
    private String tags;

    private String contentType;
    private List<String> contentTypesList;

    private Long maxMediaItemsNumber = new Long(100);

    private Long mediaItemsNumber = new Long(0);
    private Long workSpaceUsageInMB = new Long(0);
    private Long maxStorageInMB = new Long(0);
    private List<MediaItem> mediaItemsList = new ArrayList<>();

    @PostConstruct
    public void init()
    {
        mLogger.debug("init");

        player = new Player();

        autoRefresh = true;
        autoRefreshPeriodInSeconds = 30;

        {
            contentTypesList = new ArrayList<>();
            contentTypesList.add("video");
            contentTypesList.add("audio");
            contentTypesList.add("image");

            contentType = contentTypesList.get(0);
        }
        {
            Calendar calendar = Calendar.getInstance();

            calendar.add(Calendar.DAY_OF_MONTH, -7);

            calendar.set(Calendar.HOUR_OF_DAY, 0);
            calendar.set(Calendar.MINUTE, 0);
            calendar.set(Calendar.SECOND, 0);
            calendar.set(Calendar.MILLISECOND, 0);

            begin = calendar.getTime();
        }

        {
            Calendar calendar = Calendar.getInstance();

            calendar.add(Calendar.HOUR_OF_DAY, 5);

            calendar.set(Calendar.MINUTE, 0);
            calendar.set(Calendar.SECOND, 0);
            calendar.set(Calendar.MILLISECOND, 0);

            end = calendar.getTime();
        }
    }

    public void contentTypeChanged()
    {
        fillList(true);
    }

    public void beginChanged()
    {
        SimpleDateFormat simpleDateFormat = new SimpleDateFormat("yyyy-MM-dd HH:mm:ss");

        mLogger.debug("beginChanged. begin: " + simpleDateFormat.format(begin) + ", end: " + simpleDateFormat.format(end));
        if (begin.getTime() >= end.getTime())
        {
            Calendar calendar = Calendar.getInstance();

            calendar.setTime(begin);

            calendar.add(Calendar.DAY_OF_MONTH, 1);

            end.setTime(calendar.getTime().getTime());

            // really face message is useless because of the redirection
            FacesMessage message = new FacesMessage(FacesMessage.SEVERITY_ERROR,
                    "Media items", "BeginDate cannot be later the EndDate");
            FacesContext context = FacesContext.getCurrentInstance();
            context.addMessage(null, message);
        }
        /*
        else if ((end.getTime() - begin.getTime()) / 1000 > maxDaysDisplayed * 24 * 3600)
        {
            Calendar calendar = Calendar.getInstance();

            calendar.setTime(begin);

            calendar.add(Calendar.DAY_OF_MONTH, maxDaysDisplayed);

            end.setTime(calendar.getTime().getTime());

            // really face message is useless because of the redirection
            FacesMessage message = new FacesMessage(FacesMessage.SEVERITY_ERROR,
                "Traffic System Update", "Max " + maxDaysDisplayed + " days can be displayed");
            FacesContext context = FacesContext.getCurrentInstance();
            context.addMessage(null, message);
        }
        */

        mLogger.debug("beginChanged. begin: " + simpleDateFormat.format(begin) + ", end: " + simpleDateFormat.format(end));

        fillList(true);
    }

    public void endChanged()
    {
        SimpleDateFormat simpleDateFormat = new SimpleDateFormat("yyyy-MM-dd HH:mm:ss");

        mLogger.debug("endChanged. begin: " + simpleDateFormat.format(begin) + ", end: " + simpleDateFormat.format(end));
        if (begin.getTime() >= end.getTime())
        {
            Calendar calendar = Calendar.getInstance();

            calendar.setTime(end);

            calendar.add(Calendar.DAY_OF_MONTH, -1);

            begin.setTime(calendar.getTime().getTime());

            // really face message is useless because of the redirection
            FacesMessage message = new FacesMessage(FacesMessage.SEVERITY_ERROR,
                "Media Items", "BeginDate cannot be later the EndDate");
            FacesContext context = FacesContext.getCurrentInstance();
            context.addMessage(null, message);
        }
        /*
        else if ((end.getTime() - begin.getTime()) / 1000 > maxDaysDisplayed * 24 * 3600)
        {
            Calendar calendar = Calendar.getInstance();

            calendar.setTime(end);

            calendar.add(Calendar.DAY_OF_MONTH, -maxDaysDisplayed);

            begin.setTime(calendar.getTime().getTime());

            // really face message is useless because of the redirection
            FacesMessage message = new FacesMessage(FacesMessage.SEVERITY_ERROR,
                "Traffic System Update", "Max " + maxDaysDisplayed + " days can be displayed");
            FacesContext context = FacesContext.getCurrentInstance();
            context.addMessage(null, message);
        }
        */

        mLogger.debug("endChanged. begin: " + simpleDateFormat.format(begin) + ", end: " + simpleDateFormat.format(end));

        fillList(true);
    }

    public List<MediaItem> getMediaItemsList()
    {
        mLogger.debug("Received getMediaItemsList: " + (mediaItemsList == null ? "null" : mediaItemsList.size()));


        return mediaItemsList;
    }

    public void fillList(boolean toBeRedirected)
    {
        SimpleDateFormat simpleDateFormat = new SimpleDateFormat("yyyy-MM-dd HH:mm:ss");

        mLogger.info("fillList"
                + ", toBeRedirected: " + toBeRedirected
                + ", maxMediaItemsNumber: " + maxMediaItemsNumber
                + ", autoRefresh: " + autoRefresh
                + ", contentType: " + contentType
                + ", begin: " + simpleDateFormat.format(begin)
            + ", end: " + simpleDateFormat.format(end));

        if (toBeRedirected)
        {
            try
            {
                SimpleDateFormat simpleDateFormat_1 = new SimpleDateFormat("yyyy-MM-dd'T'HH:mm:ss");

                String url = "mediaItems.xhtml?maxMediaItemsNumber=" + maxMediaItemsNumber
                        + "&autoRefresh=" + autoRefresh
                        + "&begin=" + simpleDateFormat_1.format(begin)
                        + "&end=" + simpleDateFormat_1.format(end)
                        + "&title=" + java.net.URLEncoder.encode(title, "UTF-8")
                        + "&tags=" + java.net.URLEncoder.encode(tags, "UTF-8")
                        + "&contentType=" + contentType
                        ;
                mLogger.info("Redirect to " + url);
                FacesContext.getCurrentInstance().getExternalContext().redirect(url);
            }
            catch (Exception e)
            {
                String errorMessage = "Exception: " + e;
                mLogger.error(errorMessage);
            }
        }
        else
        {
            {
                try
                {
                    Long userKey = SessionUtils.getUserProfile().getUserKey();
                    String apiKey = SessionUtils.getCurrentWorkspaceDetails().getApiKey();

                    if (userKey == null || apiKey == null || apiKey.equalsIgnoreCase(""))
                    {
                        mLogger.warn("no input to require mediaItemsKey"
                                + ", userKey: " + userKey
                                + ", apiKey: " + apiKey
                        );
                    }
                    else
                    {
                        String username = userKey.toString();
                        String password = apiKey;

                        mediaItemsList.clear();

                        String jsonCondition = null;
                        String jsonOrderBy = null;
                        String ingestionDateAndTitleOrder = "desc";
                        CatraMMS catraMMS = new CatraMMS();
                        Vector<Long> mediaItemsData = catraMMS.getMediaItems(
                                username, password, maxMediaItemsNumber,
                                contentType, begin, end, title, tags, jsonCondition,
                                ingestionDateAndTitleOrder, jsonOrderBy,
                                mediaItemsList);

                        mediaItemsNumber = mediaItemsData.get(0);
                        workSpaceUsageInMB = mediaItemsData.get(1);
                        maxStorageInMB = mediaItemsData.get(2);
                    }
                }
                catch (Exception e)
                {
                    String errorMessage = "Exception: " + e;
                    mLogger.error(errorMessage);
                }
            }
        }
    }

    public Date willBeRemovedAt(Date ingestionDate, Long retentionInMinutes)
    {
        Calendar calendar = Calendar.getInstance();

        calendar.setTime(ingestionDate);
        calendar.add(Calendar.MINUTE, retentionInMinutes.intValue());

        return calendar.getTime();
    }

    public void removeMediaItem(MediaItem mediaItem)
    {
        try
        {
            JSONObject joWorkflow = new JSONObject();
            joWorkflow.put("Label", "Remove Media Item");
            joWorkflow.put("Type", "Workflow");

            JSONObject joRemoveTask = new JSONObject();
            joWorkflow.put("Task", joRemoveTask);

            joRemoveTask.put("Label", "Remove Task");
            joRemoveTask.put("Type", "Remove-Content");

            JSONObject joParameters = new JSONObject();
            joRemoveTask.put("Parameters", joParameters);

            joParameters.put("Ingester", SessionUtils.getUserProfile().getName());

            JSONArray jaReferences = new JSONArray();
            joParameters.put("References", jaReferences);

            JSONObject joReference = new JSONObject();
            jaReferences.put(joReference);

            joReference.put("ReferenceMediaItemKey", mediaItem.getMediaItemKey());

            String url = "workflowEditor/workflowEditor.xhtml?loadType=metaDataContent"
                    + "&data=" + java.net.URLEncoder.encode(joWorkflow.toString(), "UTF-8")
                    ;

            mLogger.info("Redirect to " + url);
            FacesContext.getCurrentInstance().getExternalContext().redirect(url);
        }
        catch (Exception e)
        {
            mLogger.error("removeMediaItem, exception: " + e);
        }
    }

    public void addEncodingProfile(MediaItem mediaItem)
    {
        try
        {
            JSONObject joWorkflow = new JSONObject();
            joWorkflow.put("Label", "Add new encoding profile");
            joWorkflow.put("Type", "Workflow");

            JSONObject joEncodeTask = new JSONObject();
            joWorkflow.put("Task", joEncodeTask);

            joEncodeTask.put("Label", "Encode Task");
            joEncodeTask.put("Type", "Encode");

            JSONObject joParameters = new JSONObject();
            joEncodeTask.put("Parameters", joParameters);

            joParameters.put("Ingester", SessionUtils.getUserProfile().getName());

            JSONArray jaReferences = new JSONArray();
            joParameters.put("References", jaReferences);

            JSONObject joReference = new JSONObject();
            jaReferences.put(joReference);

            Long sourcePhysicalPathKey = null;
            for (PhysicalPath physicalPath: mediaItem.getPhysicalPathList())
            {
                if (physicalPath.getEncodingProfileKey() == null)
                {
                    sourcePhysicalPathKey = physicalPath.getPhysicalPathKey();

                    break;
                }
            }

            if (sourcePhysicalPathKey == null)
            {
                mLogger.error("Source Physical Path Key not found");

                FacesMessage message = new FacesMessage(FacesMessage.SEVERITY_ERROR,
                        "Media Items", "Source Physical Path Key not found");
                FacesContext context = FacesContext.getCurrentInstance();
                context.addMessage(null, message);

                return;
            }
            joReference.put("ReferencePhysicalPathKey", sourcePhysicalPathKey);

            String url = "workflowEditor/workflowEditor.xhtml?loadType=metaDataContent"
                    + "&data=" + java.net.URLEncoder.encode(joWorkflow.toString(), "UTF-8")
                    ;

            mLogger.info("Redirect to " + url);
            FacesContext.getCurrentInstance().getExternalContext().redirect(url);
        }
        catch (Exception e)
        {
            mLogger.error("removeMediaItem, exception: " + e);
        }
    }

    public void changeFileFormat(MediaItem mediaItem)
    {
        try
        {
            JSONObject joWorkflow = new JSONObject();
            joWorkflow.put("Label", "Change File Format Workflow");
            joWorkflow.put("Type", "Workflow");

            JSONObject joChangeFileFormatTask = new JSONObject();
            joWorkflow.put("Task", joChangeFileFormatTask);

            joChangeFileFormatTask.put("Label", "Change File Format Task");
            joChangeFileFormatTask.put("Type", "Change-File-Format");

            JSONObject joParameters = new JSONObject();
            joChangeFileFormatTask.put("Parameters", joParameters);

            joParameters.put("Ingester", SessionUtils.getUserProfile().getName());

            joParameters.put("Title", mediaItem.getTitle() + " - New Format");

            JSONArray jaReferences = new JSONArray();
            joParameters.put("References", jaReferences);

            JSONObject joReference = new JSONObject();
            jaReferences.put(joReference);

            Long sourcePhysicalPathKey = null;
            for (PhysicalPath physicalPath: mediaItem.getPhysicalPathList())
            {
                if (physicalPath.getEncodingProfileKey() == null)
                {
                    sourcePhysicalPathKey = physicalPath.getPhysicalPathKey();

                    break;
                }
            }

            if (sourcePhysicalPathKey == null)
            {
                mLogger.error("Source Physical Path Key not found");

                FacesMessage message = new FacesMessage(FacesMessage.SEVERITY_ERROR,
                        "Media Items", "Source Physical Path Key not found");
                FacesContext context = FacesContext.getCurrentInstance();
                context.addMessage(null, message);

                return;
            }
            joReference.put("ReferencePhysicalPathKey", sourcePhysicalPathKey);

            String url = "workflowEditor/workflowEditor.xhtml?loadType=metaDataContent"
                    + "&data=" + java.net.URLEncoder.encode(joWorkflow.toString(), "UTF-8")
                    ;

            mLogger.info("Redirect to " + url);
            FacesContext.getCurrentInstance().getExternalContext().redirect(url);
        }
        catch (Exception e)
        {
            mLogger.error("removeMediaItem, exception: " + e);
        }
    }

    public String getDurationAsString(Long durationInMilliseconds)
    {
        if (durationInMilliseconds == null)
            return "";

        String duration;

        int hours = (int) (durationInMilliseconds / 3600000);
        String sHours = String.format("%02d", hours);

        int minutes = (int) ((durationInMilliseconds - (hours * 3600000)) / 60000);
        String sMinutes = String.format("%02d", minutes);

        int seconds = (int) ((durationInMilliseconds - ((hours * 3600000) + (minutes * 60000))) / 1000);
        String sSeconds = String.format("%02d", seconds);

        int milliSeconds = (int) (durationInMilliseconds - ((hours * 3600000) + (minutes * 60000) + (seconds * 1000)));
        String sMilliSeconds = String.format("%03d", milliSeconds);

        return sHours.concat(":").concat(sMinutes).concat(":").concat(sSeconds).concat(".").concat(sMilliSeconds);
    }

    public void workspaceNameChanged()
    {
        WorkspaceDetails currentWorkspaceDetails = SessionUtils.getCurrentWorkspaceDetails();

        mLogger.info("workspaceNameChanged"
                        + ", workspaceName: " + currentWorkspaceDetails.getName()
        );

        fillList(true);
    }

    public List<String> getWorkspaceNames()
    {
        List<String> workspaceNames = new ArrayList<>();

        List<WorkspaceDetails> workspaceDetailsList = SessionUtils.getWorkspaceDetailsList();

        if (workspaceDetailsList != null)
        {
            for (WorkspaceDetails workspaceDetails: workspaceDetailsList)
                workspaceNames.add(workspaceDetails.getName());
        }

        return workspaceNames;
    }

    public String getContentType() {
        return contentType;
    }

    public void setContentType(String contentType) {
        this.contentType = contentType;
    }

    public List<String> getContentTypesList() {
        return contentTypesList;
    }

    public void setContentTypesList(List<String> contentTypesList) {
        this.contentTypesList = contentTypesList;
    }

    public Date getBegin() {
        return begin;
    }

    public void setBegin(Date begin) {
        this.begin = begin;
    }

    public Date getEnd() {
        return end;
    }

    public void setEnd(Date end) {
        this.end = end;
    }

    public Long getMaxMediaItemsNumber() {
        return maxMediaItemsNumber;
    }

    public void setMaxMediaItemsNumber(Long maxMediaItemsNumber) {
        this.maxMediaItemsNumber = maxMediaItemsNumber;
    }

    public Long getMediaItemsNumber() {
        return mediaItemsNumber;
    }

    public void setMediaItemsNumber(Long mediaItemsNumber) {
        this.mediaItemsNumber = mediaItemsNumber;
    }

    public Long getWorkSpaceUsageInMB() {
        return workSpaceUsageInMB;
    }

    public void setWorkSpaceUsageInMB(Long workSpaceUsageInMB) {
        this.workSpaceUsageInMB = workSpaceUsageInMB;
    }

    public void setMediaItemsList(List<MediaItem> mediaItemsList) {
        this.mediaItemsList = mediaItemsList;
    }

    public boolean isAutoRefresh() {
        return autoRefresh;
    }

    public void setAutoRefresh(boolean autoRefresh) {
        this.autoRefresh = autoRefresh;
    }

    public int getAutoRefreshPeriodInSeconds() {
        return autoRefreshPeriodInSeconds;
    }

    public void setAutoRefreshPeriodInSeconds(int autoRefreshPeriodInSeconds) {
        this.autoRefreshPeriodInSeconds = autoRefreshPeriodInSeconds;
    }

    public Long getMaxStorageInMB() {
        return maxStorageInMB;
    }

    public void setMaxStorageInMB(Long maxStorageInMB) {
        this.maxStorageInMB = maxStorageInMB;
    }

    public String getTitle() {
        return title;
    }

    public void setTitle(String title) {
        this.title = title;
    }

    public String getTags() {
        return tags;
    }

    public void setTags(String tags) {
        this.tags = tags;
    }

    public Player getPlayer() {
        return player;
    }

    public void setPlayer(Player player) {
        this.player = player;
    }
}
