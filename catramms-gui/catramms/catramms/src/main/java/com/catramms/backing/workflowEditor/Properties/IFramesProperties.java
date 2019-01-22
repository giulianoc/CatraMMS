package com.catramms.backing.workflowEditor.Properties;

import com.catramms.backing.newWorkflow.WorkflowIssue;
import com.catramms.backing.workflowEditor.utility.IngestionData;
import org.apache.log4j.Logger;
import org.json.JSONArray;
import org.json.JSONObject;

import java.io.Serializable;

public class IFramesProperties extends CreateContentProperties implements Serializable {

    private static final Logger mLogger = Logger.getLogger(IFramesProperties.class);

    private Long timeInSecondsDecimalsPrecision;
    private Float startTimeInSeconds;
    private Long maxFramesNumber;
    private Long width;
    private Long height;
    private String encodingPriority;

    private StringBuilder taskReferences = new StringBuilder();

    public IFramesProperties(int elementId, String label)
    {
        super(elementId, label, "I-Frames" + "-icon.png", "Task", "I-Frames");

        timeInSecondsDecimalsPrecision = new Long(6);
    }

    public IFramesProperties clone()
    {
        IFramesProperties iFramesProperties = new IFramesProperties(
                super.getElementId(), super.getLabel());
        iFramesProperties.setOnSuccessChildren(super.getOnSuccessChildren());
        iFramesProperties.setOnErrorChildren(super.getOnErrorChildren());
        iFramesProperties.setOnCompleteChildren(super.getOnCompleteChildren());

        iFramesProperties.setStartTimeInSeconds(startTimeInSeconds);
        iFramesProperties.setMaxFramesNumber(maxFramesNumber);
        iFramesProperties.setWidth(width);
        iFramesProperties.setHeight(height);
        iFramesProperties.setEncodingPriority(encodingPriority);

        iFramesProperties.setTitle(getTitle());
        iFramesProperties.setTags(getTags());
        iFramesProperties.setRetention(getRetention());
        iFramesProperties.setStartPublishing(getStartPublishing());
        iFramesProperties.setEndPublishing(getEndPublishing());
        iFramesProperties.setUserData(getUserData());
        iFramesProperties.setIngester(getIngester());
        iFramesProperties.setContentProviderName(getContentProviderName());
        iFramesProperties.setDeliveryFileName(getDeliveryFileName());
        iFramesProperties.setUniqueName(getUniqueName());

        iFramesProperties.setStringBuilderTaskReferences(taskReferences);

        return iFramesProperties;
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

            if (getStartTimeInSeconds() != null)
                joParameters.put("StartTimeInSeconds", getStartTimeInSeconds());
            if (getMaxFramesNumber() != null)
                joParameters.put("MaxFramesNumber", getMaxFramesNumber());
            if (getWidth() != null)
                joParameters.put("Width", getWidth());
            if (getHeight() != null)
                joParameters.put("Height", getHeight());
            if (getEncodingPriority() != null && !getEncodingPriority().equalsIgnoreCase(""))
                joParameters.put("EncodingPriority", getEncodingPriority());

            super.addCreateContentPropertiesToJson(joParameters);

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

    public Long getTimeInSecondsDecimalsPrecision() {
        return timeInSecondsDecimalsPrecision;
    }

    public void setTimeInSecondsDecimalsPrecision(Long timeInSecondsDecimalsPrecision) {
        this.timeInSecondsDecimalsPrecision = timeInSecondsDecimalsPrecision;
    }

    public Long getMaxFramesNumber() {
        return maxFramesNumber;
    }

    public void setMaxFramesNumber(Long maxFramesNumber) {
        this.maxFramesNumber = maxFramesNumber;
    }

    public Float getStartTimeInSeconds() {
        return startTimeInSeconds;
    }

    public void setStartTimeInSeconds(Float startTimeInSeconds) {
        this.startTimeInSeconds = startTimeInSeconds;
    }

    public String getEncodingPriority() {
        return encodingPriority;
    }

    public void setEncodingPriority(String encodingPriority) {
        this.encodingPriority = encodingPriority;
    }

    public Long getWidth() {
        return width;
    }

    public void setWidth(Long width) {
        this.width = width;
    }

    public Long getHeight() {
        return height;
    }

    public void setHeight(Long height) {
        this.height = height;
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
