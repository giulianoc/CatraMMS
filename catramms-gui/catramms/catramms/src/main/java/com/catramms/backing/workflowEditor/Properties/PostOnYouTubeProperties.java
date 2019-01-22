package com.catramms.backing.workflowEditor.Properties;

import com.catramms.backing.common.SessionUtils;
import com.catramms.backing.entity.FacebookConf;
import com.catramms.backing.entity.YouTubeConf;
import com.catramms.backing.newWorkflow.WorkflowIssue;
import com.catramms.backing.workflowEditor.utility.IngestionData;
import com.catramms.utility.catramms.CatraMMS;
import org.apache.log4j.Logger;
import org.json.JSONArray;
import org.json.JSONObject;

import java.io.Serializable;
import java.util.ArrayList;
import java.util.List;

public class PostOnYouTubeProperties extends WorkflowProperties implements Serializable {

    private static final Logger mLogger = Logger.getLogger(PostOnYouTubeProperties.class);

    private String configurationLabel;
    private List<YouTubeConf> confList;
    private String youTubeTitle;
    private String youTubeDescription;
    private String youTubeTags;
    private Long categoryId;
    private String privacy;

    private StringBuilder taskReferences = new StringBuilder();

    public PostOnYouTubeProperties(int elementId, String label)
    {
        super(elementId, label, "Post-On-YouTube" + "-icon.png", "Task", "Post-On-YouTube");

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

                confList = catraMMS.getYouTubeConf(username, password);
            }
        }
        catch (Exception e)
        {
            String errorMessage = "Exception: " + e;
            mLogger.error(errorMessage);
        }
    }

    public PostOnYouTubeProperties(int elementId, String label, List<YouTubeConf> confList)
    {
        super(elementId, label, "Post-On-YouTube" + "-icon.png", "Task", "Post-On-YouTube");

        this.confList = confList;
    }

    public PostOnYouTubeProperties clone()
    {
        PostOnYouTubeProperties postOnYouTubeProperties = new PostOnYouTubeProperties(
                super.getElementId(), super.getLabel(), confList);
        postOnYouTubeProperties.setOnSuccessChildren(super.getOnSuccessChildren());
        postOnYouTubeProperties.setOnErrorChildren(super.getOnErrorChildren());
        postOnYouTubeProperties.setOnCompleteChildren(super.getOnCompleteChildren());

        postOnYouTubeProperties.setConfigurationLabel(getConfigurationLabel());
        postOnYouTubeProperties.setYouTubeTitle(getYouTubeTitle());
        postOnYouTubeProperties.setYouTubeDescription(getYouTubeDescription());
        postOnYouTubeProperties.setYouTubeTags(getYouTubeTags());
        postOnYouTubeProperties.setCategoryId(getCategoryId());
        postOnYouTubeProperties.setPrivacy(getPrivacy());

        postOnYouTubeProperties.setStringBuilderTaskReferences(taskReferences);

        return postOnYouTubeProperties;
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

            if (getConfigurationLabel() != null && !getConfigurationLabel().equalsIgnoreCase(""))
                joParameters.put("ConfigurationLabel", getConfigurationLabel());
            else
            {
                WorkflowIssue workflowIssue = new WorkflowIssue();
                workflowIssue.setLabel(getLabel());
                workflowIssue.setFieldName("ConfigurationLabel");
                workflowIssue.setTaskType(getType());
                workflowIssue.setIssue("The field is not initialized");

                ingestionData.getWorkflowIssueList().add(workflowIssue);
            }

            if (getYouTubeTitle() != null && !getYouTubeTitle().equalsIgnoreCase(""))
                joParameters.put("Title", getYouTubeTitle());

            if (getYouTubeDescription() != null && !getYouTubeDescription().equalsIgnoreCase(""))
                joParameters.put("Description", getYouTubeDescription());

            if (getYouTubeTags() != null && !getYouTubeTags().equalsIgnoreCase(""))
            {
                String[] tags = getYouTubeTags().split(",");

                JSONArray jaTags = new JSONArray();
                for (String tag: tags)
                    jaTags.put(tag);

                joParameters.put("Tags", jaTags);
            }

            if (getCategoryId() != null)
                joParameters.put("CategoryId", getCategoryId());

            if (getPrivacy() != null && !getPrivacy().equalsIgnoreCase(""))
                joParameters.put("Privacy", getPrivacy());

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

    public List<String> getConfLabels()
    {
        List<String> configurationLabels = new ArrayList<>();

        for (YouTubeConf youTubeConf: confList)
            configurationLabels.add(youTubeConf.getLabel());

        return configurationLabels;
    }

    public String getConfigurationLabel() {
        return configurationLabel;
    }

    public void setConfigurationLabel(String configurationLabel) {
        this.configurationLabel = configurationLabel;
    }

    public List<YouTubeConf> getConfList() {
        return confList;
    }

    public void setConfList(List<YouTubeConf> confList) {
        this.confList = confList;
    }

    public String getYouTubeTitle() {
        return youTubeTitle;
    }

    public void setYouTubeTitle(String youTubeTitle) {
        this.youTubeTitle = youTubeTitle;
    }

    public String getYouTubeDescription() {
        return youTubeDescription;
    }

    public void setYouTubeDescription(String youTubeDescription) {
        this.youTubeDescription = youTubeDescription;
    }

    public String getYouTubeTags() {
        return youTubeTags;
    }

    public void setYouTubeTags(String youTubeTags) {
        this.youTubeTags = youTubeTags;
    }

    public Long getCategoryId() {
        return categoryId;
    }

    public void setCategoryId(Long categoryId) {
        this.categoryId = categoryId;
    }

    public String getPrivacy() {
        return privacy;
    }

    public void setPrivacy(String privacy) {
        this.privacy = privacy;
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
