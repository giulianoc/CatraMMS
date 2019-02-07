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
    private MediaItem mediaItemSelected = new MediaItem();
    private String propertyFieldToBeUpdated;
    private String mediaItemsSelectionMode;
    private Long mediaItemsNumber = new Long(0);
    private String mediaItemsContentType;
    private List<String> mediaItemsContentTypesList = new ArrayList<>();
    private Date mediaItemsBegin;
    private Date mediaItemsEnd;
    private String mediaItemsTitle;
    private String mediaItemsTags;
    private Long mediaItemsMaxMediaItemsNumber = new Long(100);
    private String mediaItemsToBeAddedOrReplaced;
    private String mediaItemsSortedBy;
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
        mediaItemsTags = "";

        mediaItemsToBeAddedOrReplaced = "toBeReplaced";
        mediaItemsSortedBy = "userSelection";
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

        propertyFieldToBeUpdated = currentElementType + "Form:taskReferences";

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
                                + ", mediaItemsTags: " + mediaItemsTags
                );

                String jsonCondition = null;
                String jsonOrderBy = null;
                String ingestionDateOrder = "desc";
                CatraMMS catraMMS = new CatraMMS();
                Vector<Long> mediaItemsData = catraMMS.getMediaItems(
                        username, password, mediaItemsMaxMediaItemsNumber,
                        mediaItemsContentType,
                        mediaItemsBegin, mediaItemsEnd,
                        mediaItemsTitle, mediaItemsTags, jsonCondition,
                        ingestionDateOrder, jsonOrderBy, mediaItemsList);
                mLogger.info("mediaItemsData: " + mediaItemsData.toString()
                        + ", mediaItemsList.size: " + mediaItemsList.size()
                                + ", mediaItemsSelectedList.size: " + mediaItemsSelectedList.size()
                );
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
                        + ", mediaItemsSortedBy: " + mediaItemsSortedBy
                        + ", taskReferences: " + taskReferences
        );

        List<Long> keysList = new ArrayList<>();

        if (mediaItemsToBeAddedOrReplaced.equalsIgnoreCase("toBeAdded"))
        {
            for (String key: taskReferences.toString().split(","))
                keysList.add(Long.parseLong(key));
        }

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
                if (!keysList.contains(mediaItem.getMediaItemKey()))
                    keysList.add(mediaItem.getMediaItemKey());
            }
            else
            {
                mLogger.info("Looking for the SourcePhysicalPath. MediaItemKey: " + mediaItem.getMediaItemKey());
                PhysicalPath sourcePhysicalPath = mediaItem.getSourcePhysicalPath();

                if (sourcePhysicalPath != null)
                {
                    if (!keysList.contains(sourcePhysicalPath.getPhysicalPathKey()))
                        keysList.add(sourcePhysicalPath.getPhysicalPathKey());
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

        if (mediaItemsSortedBy.equalsIgnoreCase("keyAscending"))
        {
            Collections.sort(keysList);
        }
        else if (mediaItemsSortedBy.equalsIgnoreCase("keyDescending"))
        {
            Collections.sort(keysList);
            Collections.reverse(keysList);
        }

        taskReferences.delete(0, taskReferences.length());
        for (Long key: keysList)
        {
            if (taskReferences.length() == 0)
                taskReferences.append(key);
            else
                taskReferences.append("," + key);
        }

        mLogger.info("taskReferences: " + taskReferences);
    }

    public void setMediaItemSelected(MediaItem mediaItemSelected) {
        this.mediaItemSelected = mediaItemSelected;

        mLogger.info("taskReferences initialization (single)"
                        + ", taskReferences: " + taskReferences
        );

        // mediaItemsSelectionMode == 'single' means "toBeReplaced" behaviour
        // if (mediaItemsToBeAddedOrReplaced.equalsIgnoreCase("toBeReplaced"))
            taskReferences.delete(0, taskReferences.length());

        mLogger.info("taskReferences initialization"
                        + ", currentElementType: " + currentElementType
                        + ", taskReferences: " + taskReferences
                        + ", taskReferences.toString: " + taskReferences.toString()
        );

        {
            if (currentElementType.equalsIgnoreCase("Remove-Content") ||
                    currentElementType.equalsIgnoreCase("HTTP-Callback"))
            {
                taskReferences.append(mediaItemSelected.getMediaItemKey().toString());
            }
            else
            {
                mLogger.info("Looking for the SourcePhysicalPath. MediaItemKey: " + mediaItemSelected.getMediaItemKey());
                PhysicalPath sourcePhysicalPath = mediaItemSelected.getSourcePhysicalPath();

                if (sourcePhysicalPath != null)
                {
                    taskReferences.append(sourcePhysicalPath.getPhysicalPathKey().toString());
                }
                else
                {
                    String errorMessage = "No sourcePhysicalPath found"
                            + ", mediaItemKey: " + mediaItemSelected.getMediaItemKey();
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

    public MediaItem getMediaItemSelected() {
        return mediaItemSelected;
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

    public String getMediaItemsTags() {
        return mediaItemsTags;
    }

    public void setMediaItemsTags(String mediaItemsTags) {
        this.mediaItemsTags = mediaItemsTags;
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

    public String getMediaItemsSortedBy() {
        return mediaItemsSortedBy;
    }

    public void setMediaItemsSortedBy(String mediaItemsSortedBy) {
        this.mediaItemsSortedBy = mediaItemsSortedBy;
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

    public String getPropertyFieldToBeUpdated() {
        return propertyFieldToBeUpdated;
    }

    public void setPropertyFieldToBeUpdated(String propertyFieldToBeUpdated) {
        this.propertyFieldToBeUpdated = propertyFieldToBeUpdated;
    }
}
