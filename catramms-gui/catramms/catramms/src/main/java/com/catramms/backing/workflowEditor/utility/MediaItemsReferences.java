package com.catramms.backing.workflowEditor.utility;

import com.catramms.backing.common.SessionUtils;
import com.catramms.backing.entity.MediaItem;
import com.catramms.backing.entity.PhysicalPath;
import com.catramms.utility.catramms.CatraMMS;
import org.apache.log4j.Logger;

import javax.faces.application.FacesMessage;
import javax.faces.context.FacesContext;
import java.io.Serializable;
import java.util.*;

public class MediaItemsReferences implements Serializable {

    // static because the class is Serializable
    private static final Logger mLogger = Logger.getLogger(MediaItemsReferences.class);

    private List<MediaItem> mediaItemsList = new ArrayList<>();
    private List<MediaItem> mediaItemsSelectedList = new ArrayList<>();
    private String mediaItemsSelectionMode;
    private Long mediaItemsNumber = new Long(0);
    private String mediaItemsContentType;
    private List<String> mediaItemsContentTypesList = new ArrayList<>();
    private Date mediaItemsBegin;
    private Date mediaItemsEnd;
    private String mediaItemsTitle;
    private Long mediaItemsMaxMediaItemsNumber = new Long(100);
    private String mediaItemsToBeAddedOrReplaced;
    private StringBuilder taskReferences;

    private String currentElementType;

    public MediaItemsReferences()
    {
        {
            Calendar calendar = Calendar.getInstance();

            calendar.add(Calendar.DAY_OF_MONTH, -1);

            calendar.set(Calendar.HOUR_OF_DAY, 0);
            calendar.set(Calendar.MINUTE, 0);
            calendar.set(Calendar.SECOND, 0);
            calendar.set(Calendar.MILLISECOND, 0);

            mediaItemsBegin = calendar.getTime();
        }

        {
            Calendar calendar = Calendar.getInstance();

            calendar.add(Calendar.HOUR_OF_DAY, 5);

            calendar.set(Calendar.MINUTE, 0);
            calendar.set(Calendar.SECOND, 0);
            calendar.set(Calendar.MILLISECOND, 0);

            mediaItemsEnd = calendar.getTime();
        }

        mediaItemsTitle = "";

        mediaItemsToBeAddedOrReplaced = "toBeReplaced";
    }

    public void prepareToSelectMediaItems(
            String currentElementType, String mediaItemsSelectionMode,
            boolean videoContentType, boolean audioContentType, boolean imageContentType,
            StringBuilder taskReferences)
    {
        mLogger.info("prepareToSelectMediaItems...");

        mediaItemsList.clear();
        mediaItemsSelectedList.clear();

        // it is used StringBuilder to pass it as references. This is because the same variable
        // is initialized here (in MediaItemsReferences) and is used in MediaItemsReferences dialog
        // refereed as workflowEditor.current....Properties.taskReferences
        this.taskReferences = taskReferences;

        // single or multiple
        this.mediaItemsSelectionMode = mediaItemsSelectionMode;

        mediaItemsMaxMediaItemsNumber = new Long(100);

        {
            mediaItemsContentTypesList.clear();
            if (videoContentType)
                mediaItemsContentTypesList.add("video");
            if (audioContentType)
                mediaItemsContentTypesList.add("audio");
            if (imageContentType)
                mediaItemsContentTypesList.add("image");

            mediaItemsContentType = mediaItemsContentTypesList.get(0);
        }

        // i.e.: Remove-Content, Add-Content, ...
        this.currentElementType = currentElementType;

        fillMediaItems();
    }

    public void fillMediaItems()
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

                mLogger.info("Calling catraMMS.getMediaItems"
                        + ", mediaItemsMaxMediaItemsNumber: " + mediaItemsMaxMediaItemsNumber
                        + ", mediaItemsContentType: " + mediaItemsContentType
                        + ", mediaItemsBegin: " + mediaItemsBegin
                        + ", mediaItemsEnd: " + mediaItemsEnd
                        + ", mediaItemsTitle: " + mediaItemsTitle
                );

                String ingestionDateOrder = "desc";
                CatraMMS catraMMS = new CatraMMS();
                Vector<Long> mediaItemsData = catraMMS.getMediaItems(
                        username, password, mediaItemsMaxMediaItemsNumber,
                        mediaItemsContentType,
                        mediaItemsBegin, mediaItemsEnd,
                        mediaItemsTitle,
                        ingestionDateOrder, mediaItemsList);
                mediaItemsNumber = mediaItemsData.get(0);
            }
        }
        catch (Exception e)
        {
            String errorMessage = "Exception: " + e;
            mLogger.error(errorMessage);
        }
    }

    public void setMediaItemsSelectedList(List<MediaItem> mediaItemsSelectedList)
    {
        this.mediaItemsSelectedList = mediaItemsSelectedList;

        mLogger.info("taskReferences initialization"
                + ", mediaItemsSelectedList.size: " + mediaItemsSelectedList.size()
                + ", mediaItemsToBeAddedOrReplaced: " + mediaItemsToBeAddedOrReplaced
                + ", taskReferences: " + taskReferences
        );

        if (mediaItemsToBeAddedOrReplaced.equalsIgnoreCase("toBeReplaced"))
            taskReferences.delete(0, taskReferences.length());

        mLogger.info("taskReferences initialization"
                        + ", currentElementType: " + currentElementType
                        + ", taskReferences: " + taskReferences
                        + ", taskReferences.toString: " + taskReferences.toString()
        );

        for (MediaItem mediaItem: mediaItemsSelectedList)
        {
            if (currentElementType.equalsIgnoreCase("Remove-Content") ||
                    currentElementType.equalsIgnoreCase("HTTP-Callback"))
            {
                if (taskReferences.length() == 0)
                    taskReferences.append(mediaItem.getMediaItemKey().toString());
                else
                    taskReferences.append("," + mediaItem.getMediaItemKey().toString());
            }
            else
            {
                mLogger.info("Looking for the SourcePhysicalPath. MediaItemKey: " + mediaItem.getMediaItemKey());
                PhysicalPath sourcePhysicalPath = mediaItem.getSourcePhysicalPath();

                if (sourcePhysicalPath != null)
                {
                    if (taskReferences.length() == 0)
                        taskReferences.append(sourcePhysicalPath.getPhysicalPathKey().toString());
                    else
                        taskReferences.append("," + sourcePhysicalPath.getPhysicalPathKey().toString());
                }
                else
                {
                    String errorMessage = "No sourcePhysicalPath found"
                            + ", mediaItemKey: " + mediaItem.getMediaItemKey();
                    mLogger.error(errorMessage);

                    FacesMessage message = new FacesMessage(FacesMessage.SEVERITY_INFO, "Workflow",
                            errorMessage);
                    FacesContext.getCurrentInstance().addMessage(null, message);
                }
            }
        }
        mLogger.info("taskReferences: " + taskReferences);
    }

    public List<MediaItem> getMediaItemsSelectedList() {
        return mediaItemsSelectedList;
    }

    public List<MediaItem> getMediaItemsList() {
        return mediaItemsList;
    }

    public void setMediaItemsList(List<MediaItem> mediaItemsList) {
        this.mediaItemsList = mediaItemsList;
    }

    public Long getMediaItemsNumber() {
        return mediaItemsNumber;
    }

    public void setMediaItemsNumber(Long mediaItemsNumber) {
        this.mediaItemsNumber = mediaItemsNumber;
    }

    public String getMediaItemsContentType() {
        return mediaItemsContentType;
    }

    public void setMediaItemsContentType(String mediaItemsContentType) {
        this.mediaItemsContentType = mediaItemsContentType;
    }

    public List<String> getMediaItemsContentTypesList() {
        return mediaItemsContentTypesList;
    }

    public void setMediaItemsContentTypesList(List<String> mediaItemsContentTypesList) {
        this.mediaItemsContentTypesList = mediaItemsContentTypesList;
    }

    public Date getMediaItemsBegin() {
        return mediaItemsBegin;
    }

    public void setMediaItemsBegin(Date mediaItemsBegin) {
        this.mediaItemsBegin = mediaItemsBegin;
    }

    public Date getMediaItemsEnd() {
        return mediaItemsEnd;
    }

    public void setMediaItemsEnd(Date mediaItemsEnd) {
        this.mediaItemsEnd = mediaItemsEnd;
    }

    public String getMediaItemsTitle() {
        return mediaItemsTitle;
    }

    public void setMediaItemsTitle(String mediaItemsTitle) {
        this.mediaItemsTitle = mediaItemsTitle;
    }

    public Long getMediaItemsMaxMediaItemsNumber() {
        return mediaItemsMaxMediaItemsNumber;
    }

    public void setMediaItemsMaxMediaItemsNumber(Long mediaItemsMaxMediaItemsNumber) {
        this.mediaItemsMaxMediaItemsNumber = mediaItemsMaxMediaItemsNumber;
    }

    public String getMediaItemsToBeAddedOrReplaced() {
        return mediaItemsToBeAddedOrReplaced;
    }

    public void setMediaItemsToBeAddedOrReplaced(String mediaItemsToBeAddedOrReplaced) {
        this.mediaItemsToBeAddedOrReplaced = mediaItemsToBeAddedOrReplaced;
    }

    public StringBuilder getTaskReferences() {
        return taskReferences;
    }

    public String getCurrentElementType() {
        return currentElementType;
    }

    public void setCurrentElementType(String currentElementType) {
        this.currentElementType = currentElementType;
    }

    public String getMediaItemsSelectionMode() {
        return mediaItemsSelectionMode;
    }

    public void setMediaItemsSelectionMode(String mediaItemsSelectionMode) {
        this.mediaItemsSelectionMode = mediaItemsSelectionMode;
    }
}
