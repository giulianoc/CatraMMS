package com.catramms.backing.workflowEditor.Properties;

import com.catramms.backing.workflowEditor.utility.WorkflowIssue;
import com.catramms.backing.workflowEditor.utility.IngestionData;
import org.apache.log4j.Logger;
import org.json.JSONArray;
import org.json.JSONObject;

import java.io.Serializable;

public class ChangeFileFormatProperties extends CreateContentProperties implements Serializable {

    private static final Logger mLogger = Logger.getLogger(ChangeFileFormatProperties.class);

    private String fileFormat;

    private StringBuilder taskReferences = new StringBuilder();

    public ChangeFileFormatProperties(String positionX, String positionY,
                                      int elementId, String label)
    {
        super(positionX, positionY, elementId, label, "Change-File-Format" + "-icon.png", "Task", "Change-File-Format");
    }

    public ChangeFileFormatProperties clone()
    {
        ChangeFileFormatProperties changeFileFormatProperties = new ChangeFileFormatProperties(
                super.getPositionX(), super.getPositionY(),
                super.getElementId(), super.getLabel());

        changeFileFormatProperties.setFileFormat(fileFormat);

        changeFileFormatProperties.setStringBuilderTaskReferences(taskReferences);

        super.getData(changeFileFormatProperties);


        return changeFileFormatProperties;
    }

    public void setData(ChangeFileFormatProperties workflowProperties)
    {
        super.setData(workflowProperties);

        setFileFormat(workflowProperties.getFileFormat());

        setStringBuilderTaskReferences(workflowProperties.getStringBuilderTaskReferences());
    }

    public void setData(JSONObject jsonWorkflowElement)
    {
        try {
            super.setData(jsonWorkflowElement);

            setLabel(jsonWorkflowElement.getString("Label"));

            JSONObject joParameters = jsonWorkflowElement.getJSONObject("Parameters");

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

            if (getFileFormat() != null && !getFileFormat().equalsIgnoreCase(""))
                joParameters.put("OutputFileFormat", getFileFormat());
            else
            {
                WorkflowIssue workflowIssue = new WorkflowIssue();
                workflowIssue.setLabel("");
                workflowIssue.setFieldName("OutputFileFormat");
                workflowIssue.setTaskType(super.getType());
                workflowIssue.setIssue("The field is not initialized");

                ingestionData.getWorkflowIssueList().add(workflowIssue);
            }

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
}
