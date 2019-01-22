package com.catramms.backing.workflowEditor.Properties;

import com.catramms.backing.newWorkflow.WorkflowIssue;
import com.catramms.backing.workflowEditor.utility.IngestionData;
import org.apache.log4j.Logger;
import org.json.JSONArray;
import org.json.JSONObject;

import java.io.Serializable;

public class OverlayImageOnVideoProperties extends CreateContentProperties implements Serializable {

    private static final Logger mLogger = Logger.getLogger(OverlayImageOnVideoProperties.class);

    private String positionXInPixel;
    private String positionYInPixel;
    private String encodingPriority;

    private StringBuilder taskReferences = new StringBuilder();

    public OverlayImageOnVideoProperties(int elementId, String label)
    {
        super(elementId, label, "Overlay-Image-On-Video" + "-icon.png", "Task", "Overlay-Image-On-Video");
    }

    public OverlayImageOnVideoProperties clone()
    {
        OverlayImageOnVideoProperties overlayImageOnVideoProperties = new OverlayImageOnVideoProperties(
                super.getElementId(), super.getLabel());
        overlayImageOnVideoProperties.setOnSuccessChildren(super.getOnSuccessChildren());
        overlayImageOnVideoProperties.setOnErrorChildren(super.getOnErrorChildren());
        overlayImageOnVideoProperties.setOnCompleteChildren(super.getOnCompleteChildren());

        overlayImageOnVideoProperties.setPositionXInPixel(getPositionXInPixel());
        overlayImageOnVideoProperties.setPositionYInPixel(getPositionYInPixel());
        overlayImageOnVideoProperties.setEncodingPriority(getEncodingPriority());

        overlayImageOnVideoProperties.setTitle(getTitle());
        overlayImageOnVideoProperties.setTags(getTags());
        overlayImageOnVideoProperties.setRetention(getRetention());
        overlayImageOnVideoProperties.setStartPublishing(getStartPublishing());
        overlayImageOnVideoProperties.setEndPublishing(getEndPublishing());
        overlayImageOnVideoProperties.setUserData(getUserData());
        overlayImageOnVideoProperties.setIngester(getIngester());
        overlayImageOnVideoProperties.setContentProviderName(getContentProviderName());
        overlayImageOnVideoProperties.setDeliveryFileName(getDeliveryFileName());
        overlayImageOnVideoProperties.setUniqueName(getUniqueName());

        overlayImageOnVideoProperties.setStringBuilderTaskReferences(taskReferences);

        return overlayImageOnVideoProperties;
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

            if (getPositionXInPixel() != null && !getPositionXInPixel().equalsIgnoreCase(""))
                joParameters.put("ImagePosition_X_InPixel", getPositionXInPixel());
            else
            {
                WorkflowIssue workflowIssue = new WorkflowIssue();
                workflowIssue.setLabel(getLabel());
                workflowIssue.setFieldName("ImagePosition_X_InPixel");
                workflowIssue.setTaskType(getType());
                workflowIssue.setIssue("The field is not initialized");

                ingestionData.getWorkflowIssueList().add(workflowIssue);
            }
            if (getPositionYInPixel() != null && !getPositionYInPixel().equalsIgnoreCase(""))
                joParameters.put("ImagePosition_Y_InPixel", getPositionYInPixel());
            else
            {
                WorkflowIssue workflowIssue = new WorkflowIssue();
                workflowIssue.setLabel(getLabel());
                workflowIssue.setFieldName("ImagePosition_Y_InPixel");
                workflowIssue.setTaskType(getType());
                workflowIssue.setIssue("The field is not initialized");

                ingestionData.getWorkflowIssueList().add(workflowIssue);
            }
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

    public String getPositionXInPixel() {
        return positionXInPixel;
    }

    public void setPositionXInPixel(String positionXInPixel) {
        this.positionXInPixel = positionXInPixel;
    }

    public String getPositionYInPixel() {
        return positionYInPixel;
    }

    public void setPositionYInPixel(String positionYInPixel) {
        this.positionYInPixel = positionYInPixel;
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
