package com.catramms.backing.workflowEditor.Properties;

import com.catramms.backing.newWorkflow.WorkflowIssue;
import com.catramms.backing.workflowEditor.utility.IngestionData;
import org.apache.log4j.Logger;
import org.json.JSONArray;
import org.json.JSONObject;

import java.io.Serializable;

public class PeriodicalFramesProperties extends CreateContentProperties implements Serializable {

    private static final Logger mLogger = Logger.getLogger(PeriodicalFramesProperties.class);

    private Long timeInSecondsDecimalsPrecision;
    private Float startTimeInSeconds;
    private Long periodInSeconds;
    private Long maxFramesNumber;
    private Long width;
    private Long height;
    private String encodingPriority;

    private StringBuilder taskReferences = new StringBuilder();

    public PeriodicalFramesProperties(int elementId, String label)
    {
        super(elementId, label, "Periodical-Frames" + "-icon.png", "Task", "Periodical-Frames");

        timeInSecondsDecimalsPrecision = new Long(6);
    }

    public PeriodicalFramesProperties clone()
    {
        PeriodicalFramesProperties periodicalFramesProperties = new PeriodicalFramesProperties(
                super.getElementId(), super.getLabel());

        periodicalFramesProperties.setStartTimeInSeconds(startTimeInSeconds);
        periodicalFramesProperties.setPeriodInSeconds(periodInSeconds);
        periodicalFramesProperties.setMaxFramesNumber(maxFramesNumber);
        periodicalFramesProperties.setWidth(width);
        periodicalFramesProperties.setHeight(height);
        periodicalFramesProperties.setEncodingPriority(encodingPriority);

        periodicalFramesProperties.setStringBuilderTaskReferences(taskReferences);

        super.getData(periodicalFramesProperties);

        return periodicalFramesProperties;
    }

    public void setData(PeriodicalFramesProperties workflowProperties)
    {
        super.setData(workflowProperties);

        setStartTimeInSeconds(workflowProperties.getStartTimeInSeconds());
        setPeriodInSeconds(workflowProperties.getPeriodInSeconds());
        setMaxFramesNumber(workflowProperties.getMaxFramesNumber());
        setWidth(workflowProperties.getWidth());
        setHeight(workflowProperties.getHeight());
        setEncodingPriority(workflowProperties.getEncodingPriority());

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

            if (getPeriodInSeconds() != null)
                joParameters.put("PeriodInSeconds", getPeriodInSeconds());
            else
            {
                WorkflowIssue workflowIssue = new WorkflowIssue();
                workflowIssue.setLabel(getLabel());
                workflowIssue.setFieldName("Label");
                workflowIssue.setTaskType(getType());
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

    public Long getPeriodInSeconds() {
        return periodInSeconds;
    }

    public void setPeriodInSeconds(Long periodInSeconds) {
        this.periodInSeconds = periodInSeconds;
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
