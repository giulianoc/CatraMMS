package com.catramms.backing.workflowEditor.Properties;

import com.catramms.backing.workflowEditor.utility.WorkflowIssue;
import com.catramms.backing.workflowEditor.utility.IngestionData;
import org.apache.log4j.Logger;
import org.json.JSONArray;
import org.json.JSONObject;

import java.io.Serializable;

public class CutProperties extends CreateContentProperties implements Serializable {

    private static final Logger mLogger = Logger.getLogger(CutProperties.class);

    private Long timeInSecondsDecimalsPrecision;
    private Float startTimeInSeconds;
    private String endType;
    private Float endTimeInSeconds;
    private Long framesNumber;
    private String fileFormat;

    private StringBuilder taskReferences = new StringBuilder();

    public CutProperties(String positionX, String positionY,
                         int elementId, String label)
    {
        super(positionX, positionY, elementId, label, "Cut" + "-icon.png", "Task", "Cut");

        endType = "endTime";
        timeInSecondsDecimalsPrecision = new Long(6);
    }

    public CutProperties clone()
    {
        CutProperties cutProperties = new CutProperties(
                super.getPositionX(), super.getPositionY(),
                super.getElementId(), super.getLabel());

        cutProperties.setStartTimeInSeconds(startTimeInSeconds);
        cutProperties.setEndType(endType);
        cutProperties.setEndTimeInSeconds(endTimeInSeconds);
        cutProperties.setFramesNumber(framesNumber);
        cutProperties.setFileFormat(fileFormat);

        cutProperties.setStringBuilderTaskReferences(taskReferences);

        super.getData(cutProperties);


        return cutProperties;
    }

    public void setData(CutProperties workflowProperties)
    {
        super.setData(workflowProperties);

        setStartTimeInSeconds(workflowProperties.getStartTimeInSeconds());
        setEndType(workflowProperties.getEndType());
        setEndTimeInSeconds(workflowProperties.getEndTimeInSeconds());
        setFramesNumber(workflowProperties.getFramesNumber());
        setFileFormat(workflowProperties.getFileFormat());

        setStringBuilderTaskReferences(workflowProperties.getStringBuilderTaskReferences());
    }

    public void setData(JSONObject jsonWorkflowElement)
    {
        try {
            super.setData(jsonWorkflowElement);

            JSONObject joParameters = jsonWorkflowElement.getJSONObject("Parameters");

            if (joParameters.has("StartTimeInSeconds"))
                setStartTimeInSeconds(new Float(joParameters.getDouble("StartTimeInSeconds")));
            if (joParameters.has("EndTimeInSeconds"))
            {
                setEndTimeInSeconds(new Float(joParameters.getDouble("EndTimeInSeconds")));
                setEndType("endTime");
            }
            if (joParameters.has("FramesNumber")) {
                setFramesNumber(joParameters.getLong("FramesNumber"));
                setEndType("framesNumber");
            }
            if (joParameters.has("FileFormat") && !joParameters.getString("FileFormat").equalsIgnoreCase(""))
                setFileFormat(joParameters.getString("FileFormat"));

            if (joParameters.has("References"))
            {
                String references = "";
                JSONArray jaReferences = joParameters.getJSONArray("References");
                for (int referenceIndex = 0; referenceIndex < jaReferences.length(); referenceIndex++)
                {
                    JSONObject joReference = jaReferences.getJSONObject(referenceIndex);

                    if (joReference.has("ReferencePhysicalPathKey"))
                    {
                        if (references.equalsIgnoreCase(""))
                            references = new Long(joReference.getLong("ReferencePhysicalPathKey")).toString();
                        else
                            references += ("," + new Long(joReference.getLong("ReferencePhysicalPathKey")).toString());
                    }
                }

                setTaskReferences(references);
            }
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
                // String.format("%." + timeInSecondsDecimalsPrecision + "g",
                //        task.getStartTimeInSeconds().floatValue()));
            else
            {
                WorkflowIssue workflowIssue = new WorkflowIssue();
                workflowIssue.setLabel(getLabel());
                workflowIssue.setFieldName("StartTimeInSeconds");
                workflowIssue.setTaskType(getType());
                workflowIssue.setIssue("The field is not initialized");

                ingestionData.getWorkflowIssueList().add(workflowIssue);
            }
            if (getEndType().equalsIgnoreCase("endTime"))
            {
                if (getEndTimeInSeconds() != null)
                    joParameters.put("EndTimeInSeconds", getEndTimeInSeconds());
                    // String.format("%." + timeInSecondsDecimalsPrecision + "g",
                    //        task.getEndTimeInSeconds().floatValue()));
                else
                {
                    WorkflowIssue workflowIssue = new WorkflowIssue();
                    workflowIssue.setLabel(getLabel());
                    workflowIssue.setFieldName("EndTimeInSeconds");
                    workflowIssue.setTaskType(getType());
                    workflowIssue.setIssue("The field is not initialized");

                    ingestionData.getWorkflowIssueList().add(workflowIssue);
                }
            }
            else
            {
                if (getFramesNumber() != null)
                    joParameters.put("FramesNumber", getFramesNumber());
                else
                {
                    WorkflowIssue workflowIssue = new WorkflowIssue();
                    workflowIssue.setLabel(getLabel());
                    workflowIssue.setFieldName("FramesNumber");
                    workflowIssue.setTaskType(getType());
                    workflowIssue.setIssue("The field is not initialized");

                    ingestionData.getWorkflowIssueList().add(workflowIssue);
                }
            }
            if (getFileFormat() != null && !getFileFormat().equalsIgnoreCase(""))
                joParameters.put("OutputFileFormat", getFileFormat());

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

    public Float getStartTimeInSeconds() {
        return startTimeInSeconds;
    }

    public void setStartTimeInSeconds(Float startTimeInSeconds) {
        this.startTimeInSeconds = startTimeInSeconds;
    }

    public Float getEndTimeInSeconds() {
        return endTimeInSeconds;
    }

    public void setEndTimeInSeconds(Float endTimeInSeconds) {
        this.endTimeInSeconds = endTimeInSeconds;
    }

    public Long getFramesNumber() {
        return framesNumber;
    }

    public void setFramesNumber(Long framesNumber) {
        this.framesNumber = framesNumber;
    }

    public String getEndType() {
        return endType;
    }

    public void setEndType(String endType) {
        this.endType = endType;
    }

    public String getFileFormat() {
        return fileFormat;
    }

    public void setFileFormat(String fileFormat) {
        this.fileFormat = fileFormat;
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

    public Long getTimeInSecondsDecimalsPrecision() {
        return timeInSecondsDecimalsPrecision;
    }

    public void setTimeInSecondsDecimalsPrecision(Long timeInSecondsDecimalsPrecision) {
        this.timeInSecondsDecimalsPrecision = timeInSecondsDecimalsPrecision;
    }
}
