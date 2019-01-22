package com.catramms.backing.workflowEditor.Properties;

import com.catramms.backing.common.SessionUtils;
import com.catramms.backing.entity.EncodingProfile;
import com.catramms.backing.entity.EncodingProfilesSet;
import com.catramms.backing.newWorkflow.WorkflowIssue;
import com.catramms.backing.workflowEditor.utility.IngestionData;
import com.catramms.utility.catramms.CatraMMS;
import org.apache.log4j.Logger;
import org.json.JSONArray;
import org.json.JSONObject;

import java.io.Serializable;
import java.util.ArrayList;
import java.util.List;

public class EncodeProperties extends WorkflowProperties implements Serializable {

    private static final Logger mLogger = Logger.getLogger(EncodeProperties.class);

    private String encodingPriority;
    private String contentType;
    private String encodingProfileType;
    private String encodingProfileLabel;
    private String encodingProfilesSetLabel;

    private List<String> encodingProfilesLabelList = new ArrayList<>();
    private List<EncodingProfile> videoEncodingProfilesList = new ArrayList<>();
    private List<EncodingProfile> audioEncodingProfilesList = new ArrayList<>();
    private List<EncodingProfile> imageEncodingProfilesList = new ArrayList<>();

    private List<String> encodingProfilesLabelSetList = new ArrayList<>();
    private List<EncodingProfilesSet> videoEncodingProfilesSetList = new ArrayList<>();
    private List<EncodingProfilesSet> audioEncodingProfilesSetList = new ArrayList<>();
    private List<EncodingProfilesSet> imageEncodingProfilesSetList = new ArrayList<>();


    private StringBuilder taskReferences = new StringBuilder();


    public EncodeProperties(int elementId, String label)
    {
        super(elementId, label, "Encode" + "-icon.png", "Task", "Encode");

        encodingProfileType = "profilesSet";

        contentType = getContentTypesList().get(0);

        try
        {
            Long userKey = SessionUtils.getUserProfile().getUserKey();
            String apiKey = SessionUtils.getCurrentWorkspaceDetails().getApiKey();

            if (userKey == null || apiKey == null || apiKey.equalsIgnoreCase(""))
            {
                mLogger.warn("no input to require encodingProfilesSetKey"
                        + ", userKey: " + userKey
                        + ", apiKey: " + apiKey
                );
            }
            else
            {
                String username = userKey.toString();
                String password = apiKey;

                CatraMMS catraMMS = new CatraMMS();

                catraMMS.getEncodingProfilesSets(username, password,
                        "video", videoEncodingProfilesSetList);
                mLogger.info("videoEncodingProfilesSetList.size(): " + videoEncodingProfilesSetList.size());
                catraMMS.getEncodingProfilesSets(username, password,
                        "audio", audioEncodingProfilesSetList);
                mLogger.info("audioEncodingProfilesSetList.size(): " + audioEncodingProfilesSetList.size());
                catraMMS.getEncodingProfilesSets(username, password,
                        "image", imageEncodingProfilesSetList);
                mLogger.info("imageEncodingProfilesSetList.size(): " + imageEncodingProfilesSetList.size());

                catraMMS.getEncodingProfiles(username, password,
                        "video", videoEncodingProfilesList);
                mLogger.info("videoEncodingProfilesList.size(): " + videoEncodingProfilesList.size());
                catraMMS.getEncodingProfiles(username, password,
                        "audio", audioEncodingProfilesList);
                mLogger.info("audioEncodingProfilesList.size(): " + audioEncodingProfilesList.size());
                catraMMS.getEncodingProfiles(username, password,
                        "image", imageEncodingProfilesList);
                mLogger.info("imageEncodingProfilesList.size(): " + imageEncodingProfilesList.size());
            }
        }
        catch (Exception e)
        {
            String errorMessage = "Exception: " + e;
            mLogger.error(errorMessage);
        }
    }

    public EncodeProperties(int elementId, String label,
                            List<EncodingProfile> videoEncodingProfilesList,
                            List<EncodingProfile> audioEncodingProfilesList,
                            List<EncodingProfile> imageEncodingProfilesList,
                            List<EncodingProfilesSet> videoEncodingProfilesSetList,
                            List<EncodingProfilesSet> audioEncodingProfilesSetList,
                            List<EncodingProfilesSet> imageEncodingProfilesSetList
    )
    {
        super(elementId, label, "Encode" + "-icon.png", "Task", "Encode");

        encodingProfileType = "profilesSet";

        contentType = getContentTypesList().get(0);

        this.videoEncodingProfilesList = videoEncodingProfilesList;
        this.audioEncodingProfilesList = audioEncodingProfilesList;
        this.imageEncodingProfilesList = imageEncodingProfilesList;
        this.videoEncodingProfilesSetList = videoEncodingProfilesSetList;
        this.audioEncodingProfilesSetList = audioEncodingProfilesSetList;
        this.imageEncodingProfilesSetList = imageEncodingProfilesSetList;
    }

    public EncodeProperties clone()
    {
        EncodeProperties encodeProperties = new EncodeProperties(
                super.getElementId(), super.getLabel(),
                videoEncodingProfilesList, audioEncodingProfilesList, imageEncodingProfilesList,
                videoEncodingProfilesSetList, audioEncodingProfilesSetList, imageEncodingProfilesSetList);

        encodeProperties.setEncodingPriority(getEncodingPriority());
        encodeProperties.setContentType(getContentType());
        encodeProperties.setEncodingProfileType(getEncodingProfileType());
        encodeProperties.setEncodingProfileLabel(getEncodingProfileLabel());
        encodeProperties.setEncodingProfilesSetLabel(getEncodingProfilesSetLabel());

        encodeProperties.setStringBuilderTaskReferences(taskReferences);

        return encodeProperties;
    }

    public void setData(EncodeProperties workflowProperties)
    {
        super.setData(workflowProperties);

        setEncodingPriority(workflowProperties.getEncodingPriority());
        setContentType(workflowProperties.getContentType());
        setEncodingProfileType(workflowProperties.getEncodingProfileType());
        setEncodingProfileLabel(workflowProperties.getEncodingProfileLabel());
        setEncodingProfilesSetLabel(workflowProperties.getEncodingProfilesSetLabel());

        setStringBuilderTaskReferences(workflowProperties.getStringBuilderTaskReferences());
    }

    public JSONObject buildWorkflowElementJson(IngestionData ingestionData)
            throws Exception
    {
        JSONObject jsonWorkflowElement = new JSONObject();

        try
        {
            jsonWorkflowElement.put("Type", super.getType());

            JSONObject joParameters = new JSONObject();
            jsonWorkflowElement.put("Parameters", joParameters);

            mLogger.info("task.getType: " + super.getType());

            if (super.getLabel() != null && !super.getLabel().equalsIgnoreCase(""))
                jsonWorkflowElement.put("Label", super.getLabel());
            else
            {
                WorkflowIssue workflowIssue = new WorkflowIssue();
                workflowIssue.setLabel("");
                workflowIssue.setFieldName("Label");
                workflowIssue.setTaskType(super.getType());
                workflowIssue.setIssue("The field is not initialized");

                ingestionData.getWorkflowIssueList().add(workflowIssue);
            }

            if (getEncodingPriority() != null && !getEncodingPriority().equalsIgnoreCase(""))
                joParameters.put("EncodingPriority", getEncodingPriority());
            if (getEncodingProfileType().equalsIgnoreCase("profilesSet"))
            {
                if (getEncodingProfilesSetLabel() != null && !getEncodingProfilesSetLabel().equalsIgnoreCase(""))
                    joParameters.put("EncodingProfilesSetLabel", getEncodingProfilesSetLabel());
                else
                {
                    WorkflowIssue workflowIssue = new WorkflowIssue();
                    workflowIssue.setLabel(getLabel());
                    workflowIssue.setFieldName("EncodingProfilesSetLabel");
                    workflowIssue.setTaskType(getType());
                    workflowIssue.setIssue("The field is not initialized");

                    ingestionData.getWorkflowIssueList().add(workflowIssue);
                }
            }
            else if (getEncodingProfileType().equalsIgnoreCase("singleProfile"))
            {
                if (getEncodingProfileLabel() != null && !getEncodingProfileLabel().equalsIgnoreCase(""))
                    joParameters.put("EncodingProfileLabel", getEncodingProfileLabel());
                else
                {
                    WorkflowIssue workflowIssue = new WorkflowIssue();
                    workflowIssue.setLabel(getLabel());
                    workflowIssue.setFieldName("EncodingProfileLabel");
                    workflowIssue.setTaskType(getType());
                    workflowIssue.setIssue("The field is not initialized");

                    ingestionData.getWorkflowIssueList().add(workflowIssue);
                }
            }
            else
            {
                mLogger.error("Unknown task.getEncodingProfileType: " + getEncodingProfileType());
            }

            if (taskReferences != null && !taskReferences.toString().equalsIgnoreCase(""))
            {
                JSONArray jaReferences = new JSONArray();
                joParameters.put("References", jaReferences);

                String [] physicalPathKeyReferences = taskReferences.toString().split(",");
                for (String physicalPathKeyReference: physicalPathKeyReferences)
                {
                    JSONObject joReference = new JSONObject();
                    joReference.put("ReferencePhysicalPathKey", Long.parseLong(physicalPathKeyReference.trim()));

                    jaReferences.put(joReference);
                }
            }

            super.addEventsPropertiesToJson(jsonWorkflowElement, ingestionData);
        }
        catch (Exception e)
        {
            mLogger.error("buildWorkflowJson failed: " + e);

            throw e;
        }

        return jsonWorkflowElement;
    }

    public List<String> getEncodingProfilesLabelList()
    {
        encodingProfilesLabelList.clear();

        if (contentType != null && contentType.equalsIgnoreCase("video"))
        {
            for (EncodingProfile encodingProfile: videoEncodingProfilesList)
                encodingProfilesLabelList.add(encodingProfile.getLabel());

        }
        else if (contentType != null && contentType.equalsIgnoreCase("audio"))
        {
            for (EncodingProfile encodingProfile: audioEncodingProfilesList)
                encodingProfilesLabelList.add(encodingProfile.getLabel());

        }
        else if (contentType != null && contentType.equalsIgnoreCase("image"))
        {
            for (EncodingProfile encodingProfile: imageEncodingProfilesList)
                encodingProfilesLabelList.add(encodingProfile.getLabel());

        }
        else
            mLogger.error("Unknown contentType: " + contentType);


        return encodingProfilesLabelList;
    }

    public void setEncodingProfilesLabelList(List<String> encodingProfilesLabelList) {
        this.encodingProfilesLabelList = encodingProfilesLabelList;
    }

    public List<String> getEncodingProfilesLabelSetList() {
        encodingProfilesLabelSetList.clear();

        if (contentType != null && contentType.equalsIgnoreCase("video"))
        {
            for (EncodingProfilesSet encodingProfilesSet: videoEncodingProfilesSetList)
                encodingProfilesLabelSetList.add(encodingProfilesSet.getLabel());

        }
        else if (contentType != null && contentType.equalsIgnoreCase("audio"))
        {
            for (EncodingProfilesSet encodingProfilesSet: audioEncodingProfilesSetList)
                encodingProfilesLabelSetList.add(encodingProfilesSet.getLabel());

        }
        else if (contentType != null && contentType.equalsIgnoreCase("image"))
        {
            for (EncodingProfilesSet encodingProfilesSet: imageEncodingProfilesSetList)
                encodingProfilesLabelSetList.add(encodingProfilesSet.getLabel());

        }
        else
            mLogger.error("Unknown contentType: " + contentType);

        return encodingProfilesLabelSetList;
    }

    public void setEncodingProfilesLabelSetList(List<String> encodingProfilesLabelSetList) {
        this.encodingProfilesLabelSetList = encodingProfilesLabelSetList;
    }

    public String getEncodingPriority() {
        return encodingPriority;
    }

    public void setEncodingPriority(String encodingPriority) {
        this.encodingPriority = encodingPriority;
    }

    public String getContentType() {
        return contentType;
    }

    public void setContentType(String contentType) {
        this.contentType = contentType;
    }

    public String getEncodingProfileType() {
        return encodingProfileType;
    }

    public void setEncodingProfileType(String encodingProfileType) {
        this.encodingProfileType = encodingProfileType;
    }

    public String getEncodingProfileLabel() {
        return encodingProfileLabel;
    }

    public void setEncodingProfileLabel(String encodingProfileLabel) {
        this.encodingProfileLabel = encodingProfileLabel;
    }

    public String getEncodingProfilesSetLabel() {
        return encodingProfilesSetLabel;
    }

    public void setEncodingProfilesSetLabel(String encodingProfilesSetLabel) {
        this.encodingProfilesSetLabel = encodingProfilesSetLabel;
    }

    public List<EncodingProfile> getVideoEncodingProfilesList() {
        return videoEncodingProfilesList;
    }

    public void setVideoEncodingProfilesList(List<EncodingProfile> videoEncodingProfilesList) {
        this.videoEncodingProfilesList = videoEncodingProfilesList;
    }

    public List<EncodingProfile> getAudioEncodingProfilesList() {
        return audioEncodingProfilesList;
    }

    public void setAudioEncodingProfilesList(List<EncodingProfile> audioEncodingProfilesList) {
        this.audioEncodingProfilesList = audioEncodingProfilesList;
    }

    public List<EncodingProfile> getImageEncodingProfilesList() {
        return imageEncodingProfilesList;
    }

    public void setImageEncodingProfilesList(List<EncodingProfile> imageEncodingProfilesList) {
        this.imageEncodingProfilesList = imageEncodingProfilesList;
    }

    public List<EncodingProfilesSet> getVideoEncodingProfilesSetList() {
        return videoEncodingProfilesSetList;
    }

    public void setVideoEncodingProfilesSetList(List<EncodingProfilesSet> videoEncodingProfilesSetList) {
        this.videoEncodingProfilesSetList = videoEncodingProfilesSetList;
    }

    public List<EncodingProfilesSet> getAudioEncodingProfilesSetList() {
        return audioEncodingProfilesSetList;
    }

    public void setAudioEncodingProfilesSetList(List<EncodingProfilesSet> audioEncodingProfilesSetList) {
        this.audioEncodingProfilesSetList = audioEncodingProfilesSetList;
    }

    public List<EncodingProfilesSet> getImageEncodingProfilesSetList() {
        return imageEncodingProfilesSetList;
    }

    public void setImageEncodingProfilesSetList(List<EncodingProfilesSet> imageEncodingProfilesSetList) {
        this.imageEncodingProfilesSetList = imageEncodingProfilesSetList;
    }

    public void setStringBuilderTaskReferences(StringBuilder taskReferences) {
        this.taskReferences = taskReferences;
    }

    public StringBuilder getStringBuilderTaskReferences() {
        return taskReferences;
    }

    public String getTaskReferences() {
        return taskReferences.toString();
    }

    public void setTaskReferences(String taskReferences) {
        this.taskReferences.replace(0, this.taskReferences.length(), taskReferences);
    }

}
