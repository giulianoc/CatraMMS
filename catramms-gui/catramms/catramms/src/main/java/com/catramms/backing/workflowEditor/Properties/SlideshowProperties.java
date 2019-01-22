package com.catramms.backing.workflowEditor.Properties;

import com.catramms.backing.newWorkflow.WorkflowIssue;
import com.catramms.backing.workflowEditor.utility.IngestionData;
import org.apache.log4j.Logger;
import org.json.JSONArray;
import org.json.JSONObject;

import java.io.Serializable;

public class SlideshowProperties extends CreateContentProperties implements Serializable {

    private static final Logger mLogger = Logger.getLogger(SlideshowProperties.class);

    private Long timeInSecondsDecimalsPrecision;
    private Float durationOfEachSlideInSeconds;
    private String encodingPriority;

    private StringBuilder taskReferences = new StringBuilder();

    public SlideshowProperties(int elementId, String label)
    {
        super(elementId, label, "Slideshow" + "-icon.png", "Task", "Slideshow");

        timeInSecondsDecimalsPrecision = new Long(6);
    }

    public SlideshowProperties clone()
    {
        SlideshowProperties slideshowProperties = new SlideshowProperties(
                super.getElementId(), super.getLabel());
        slideshowProperties.setOnSuccessChildren(super.getOnSuccessChildren());
        slideshowProperties.setOnErrorChildren(super.getOnErrorChildren());
        slideshowProperties.setOnCompleteChildren(super.getOnCompleteChildren());

        slideshowProperties.setDurationOfEachSlideInSeconds(durationOfEachSlideInSeconds);
        slideshowProperties.setEncodingPriority(encodingPriority);

        slideshowProperties.setTitle(getTitle());
        slideshowProperties.setTags(getTags());
        slideshowProperties.setRetention(getRetention());
        slideshowProperties.setStartPublishing(getStartPublishing());
        slideshowProperties.setEndPublishing(getEndPublishing());
        slideshowProperties.setUserData(getUserData());
        slideshowProperties.setIngester(getIngester());
        slideshowProperties.setContentProviderName(getContentProviderName());
        slideshowProperties.setDeliveryFileName(getDeliveryFileName());
        slideshowProperties.setUniqueName(getUniqueName());

        slideshowProperties.setStringBuilderTaskReferences(taskReferences);

        return slideshowProperties;
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

            if (getDurationOfEachSlideInSeconds() != null)
                joParameters.put("DurationOfEachSlideInSeconds", getDurationOfEachSlideInSeconds());
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

    public Float getDurationOfEachSlideInSeconds() {
        return durationOfEachSlideInSeconds;
    }

    public void setDurationOfEachSlideInSeconds(Float durationOfEachSlideInSeconds) {
        this.durationOfEachSlideInSeconds = durationOfEachSlideInSeconds;
    }

    public String getEncodingPriority() {
        return encodingPriority;
    }

    public void setEncodingPriority(String encodingPriority) {
        this.encodingPriority = encodingPriority;
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
