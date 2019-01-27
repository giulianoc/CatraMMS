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

    public IFramesProperties(String positionX, String positionY,
                             int elementId, String label)
    {
        super(positionX, positionY, elementId, label, "I-Frames" + "-icon.png", "Task", "I-Frames");

        timeInSecondsDecimalsPrecision = new Long(6);
    }

    public IFramesProperties clone()
    {
        IFramesProperties iFramesProperties = new IFramesProperties(
                super.getPositionX(), super.getPositionY(),
                super.getElementId(), super.getLabel());

        iFramesProperties.setStartTimeInSeconds(startTimeInSeconds);
        iFramesProperties.setMaxFramesNumber(maxFramesNumber);
        iFramesProperties.setWidth(width);
        iFramesProperties.setHeight(height);
        iFramesProperties.setEncodingPriority(encodingPriority);

        iFramesProperties.setStringBuilderTaskReferences(taskReferences);

        super.getData(iFramesProperties);


        return iFramesProperties;
    }

    public void setData(IFramesProperties workflowProperties)
    {
        super.setData(workflowProperties);

        setStartTimeInSeconds(workflowProperties.getStartTimeInSeconds());
        setMaxFramesNumber(workflowProperties.getMaxFramesNumber());
        setWidth(workflowProperties.getWidth());
        setHeight(workflowProperties.getHeight());
        setEncodingPriority(workflowProperties.getEncodingPriority());

        setStringBuilderTaskReferences(workflowProperties.getStringBuilderTaskReferences());
    }

    public void setData(JSONObject jsonWorkflowElement)
    {
        try {
            super.setData(jsonWorkflowElement);

            setLabel(jsonWorkflowElement.getString("Label"));

            JSONObject joParameters = jsonWorkflowElement.getJSONObject("Parameters");

            if (joParameters.has("StartTimeInSeconds"))
                setStartTimeInSeconds(new Float(joParameters.getDouble("StartTimeInSeconds")));
            if (joParameters.has("MaxFramesNumber"))
                setMaxFramesNumber(joParameters.getLong("MaxFramesNumber"));
            if (joParameters.has("Width"))
                setWidth(joParameters.getLong("Width"));
            if (joParameters.has("Height"))
                setHeight(joParameters.getLong("Height"));
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
