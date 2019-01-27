package com.catramms.backing.workflowEditor.Properties;

import com.catramms.backing.newWorkflow.WorkflowIssue;
import com.catramms.backing.workflowEditor.utility.IngestionData;
import org.apache.log4j.Logger;
import org.json.JSONArray;
import org.json.JSONObject;

import java.io.Serializable;
import java.util.ArrayList;
import java.util.List;

public class FaceRecognitionProperties extends CreateContentProperties implements Serializable {

    private static final Logger mLogger = Logger.getLogger(FaceRecognitionProperties.class);

    private String cascadeName;
    private List<String> cascadeNamesList;
    private String output;
    private List<String> outputsList;
    private String encodingPriority;

    private StringBuilder taskReferences = new StringBuilder();

    public FaceRecognitionProperties(String positionX, String positionY,
                                     int elementId, String label)
    {
        super(positionX, positionY, elementId, label, "Face-Recognition" + "-icon.png", "Task", "Face-Recognition");

        {
            cascadeNamesList = new ArrayList<>();
            cascadeNamesList.add("haarcascade_frontalface_alt");
            cascadeNamesList.add("haarcascade_frontalface_alt2");
            cascadeNamesList.add("haarcascade_frontalface_alt_tree");
            cascadeNamesList.add("haarcascade_frontalface_default");
        }

        {
            outputsList = new ArrayList<>();
            outputsList.add("VideoWithHighlightedFaces");
            outputsList.add("FacesImagesToBeUsedInDeepLearnedModel");
        }
    }

    public FaceRecognitionProperties clone()
    {
        FaceRecognitionProperties faceRecognitionProperties = new FaceRecognitionProperties(
                super.getPositionX(), super.getPositionY(),
                super.getElementId(), super.getLabel());

        faceRecognitionProperties.setCascadeName(getCascadeName());
        faceRecognitionProperties.setOutput(getOutput());
        faceRecognitionProperties.setEncodingPriority(getEncodingPriority());

        faceRecognitionProperties.setStringBuilderTaskReferences(taskReferences);

        super.getData(faceRecognitionProperties);


        return faceRecognitionProperties;
    }

    public void setData(FaceRecognitionProperties workflowProperties)
    {
        super.setData(workflowProperties);

        setCascadeName(workflowProperties.getCascadeName());
        setOutput(workflowProperties.getOutput());
        setEncodingPriority(workflowProperties.getEncodingPriority());

        setStringBuilderTaskReferences(workflowProperties.getStringBuilderTaskReferences());
    }

    public void setData(JSONObject jsonWorkflowElement)
    {
        try {
            super.setData(jsonWorkflowElement);

            setLabel(jsonWorkflowElement.getString("Label"));

            JSONObject joParameters = jsonWorkflowElement.getJSONObject("Parameters");

            if (joParameters.has("CascadeName") && !joParameters.getString("CascadeName").equalsIgnoreCase(""))
                setCascadeName(joParameters.getString("CascadeName"));
            if (joParameters.has("Output") && !joParameters.getString("Output").equalsIgnoreCase(""))
                setOutput(joParameters.getString("Output"));
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

            if (getCascadeName() != null && !getCascadeName().equalsIgnoreCase(""))
                joParameters.put("CascadeName", getCascadeName());
            else
            {
                WorkflowIssue workflowIssue = new WorkflowIssue();
                workflowIssue.setLabel(getLabel());
                workflowIssue.setFieldName("CascadeName");
                workflowIssue.setTaskType(getType());
                workflowIssue.setIssue("The field is not initialized");

                ingestionData.getWorkflowIssueList().add(workflowIssue);
            }

            if (getOutput() != null && !getOutput().equalsIgnoreCase(""))
                joParameters.put("Output", getOutput());
            else
            {
                WorkflowIssue workflowIssue = new WorkflowIssue();
                workflowIssue.setLabel(getLabel());
                workflowIssue.setFieldName("Output");
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

    public String getCascadeName() {
        return cascadeName;
    }

    public void setCascadeName(String cascadeName) {
        this.cascadeName = cascadeName;
    }

    public List<String> getCascadeNamesList() {
        return cascadeNamesList;
    }

    public void setCascadeNamesList(List<String> cascadeNamesList) {
        this.cascadeNamesList = cascadeNamesList;
    }

    public String getOutput() {
        return output;
    }

    public void setOutput(String output) {
        this.output = output;
    }

    public List<String> getOutputsList() {
        return outputsList;
    }

    public void setOutputsList(List<String> outputsList) {
        this.outputsList = outputsList;
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
