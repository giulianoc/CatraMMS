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

    public OverlayImageOnVideoProperties(String positionX, String positionY,
                                         int elementId, String label)
    {
        super(positionX, positionY, elementId, label, "Overlay-Image-On-Video" + "-icon.png", "Task", "Overlay-Image-On-Video");
    }

    public OverlayImageOnVideoProperties clone()
    {
        OverlayImageOnVideoProperties overlayImageOnVideoProperties = new OverlayImageOnVideoProperties(
                super.getPositionX(), super.getPositionY(),
                super.getElementId(), super.getLabel());

        overlayImageOnVideoProperties.setPositionXInPixel(getPositionXInPixel());
        overlayImageOnVideoProperties.setPositionYInPixel(getPositionYInPixel());
        overlayImageOnVideoProperties.setEncodingPriority(getEncodingPriority());

        overlayImageOnVideoProperties.setStringBuilderTaskReferences(taskReferences);

        super.getData(overlayImageOnVideoProperties);


        return overlayImageOnVideoProperties;
    }

    public void setData(OverlayImageOnVideoProperties workflowProperties)
    {
        super.setData(workflowProperties);

        setPositionXInPixel(workflowProperties.getPositionXInPixel());
        setPositionYInPixel(workflowProperties.getPositionYInPixel());
        setEncodingPriority(workflowProperties.getEncodingPriority());

        setStringBuilderTaskReferences(workflowProperties.getStringBuilderTaskReferences());
    }

    public void setData(JSONObject jsonWorkflowElement)
    {
        try {
            super.setData(jsonWorkflowElement);

            setLabel(jsonWorkflowElement.getString("Label"));

            JSONObject joParameters = jsonWorkflowElement.getJSONObject("Parameters");

            if (joParameters.has("ImagePosition_X_InPixel") && !joParameters.getString("ImagePosition_X_InPixel").equalsIgnoreCase(""))
                setPositionXInPixel(joParameters.getString("ImagePosition_X_InPixel"));
            if (joParameters.has("ImagePosition_Y_InPixel") && !joParameters.getString("ImagePosition_Y_InPixel").equalsIgnoreCase(""))
                setPositionYInPixel(joParameters.getString("ImagePosition_Y_InPixel"));
            if (joParameters.has("EncodingPriority") && !joParameters.getString("EncodingPriority").equalsIgnoreCase(""))
                setEncodingPriority(joParameters.getString("EncodingPriority"));
        }
        catch (Exception e)
        {
            mLogger.error("WorkflowProperties:setData failed, exception: " + e);
        }
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
