package com.catramms.backing.workflowEditor.Properties;

import com.catramms.backing.newWorkflow.WorkflowIssue;
import com.catramms.backing.workflowEditor.utility.IngestionData;
import org.apache.log4j.Logger;
import org.json.JSONArray;
import org.json.JSONObject;

import java.io.Serializable;
import java.util.ArrayList;
import java.util.List;

public class FaceIdentificationProperties extends CreateContentProperties implements Serializable {

    private static final Logger mLogger = Logger.getLogger(FaceIdentificationProperties.class);

    private String cascadeName;
    private List<String> cascadeNamesList;
    private String deepLearnedModelTags;
    private String encodingPriority;

    private StringBuilder taskReferences = new StringBuilder();

    public FaceIdentificationProperties(String positionX, String positionY,
                                        int elementId, String label)
    {
        super(positionX, positionY, elementId, label, "Face-Identification" + "-icon.png", "Task", "Face-Identification");

        {
            cascadeNamesList = new ArrayList<>();
            cascadeNamesList.add("haarcascade_frontalface_alt");
            cascadeNamesList.add("haarcascade_frontalface_alt2");
            cascadeNamesList.add("haarcascade_frontalface_alt_tree");
            cascadeNamesList.add("haarcascade_frontalface_default");
        }
    }

    public FaceIdentificationProperties clone()
    {
        FaceIdentificationProperties faceIdentificationProperties = new FaceIdentificationProperties(
                super.getPositionX(), super.getPositionY(),
                super.getElementId(), super.getLabel());

        faceIdentificationProperties.setCascadeName(getCascadeName());
        faceIdentificationProperties.setDeepLearnedModelTags(getDeepLearnedModelTags());
        faceIdentificationProperties.setEncodingPriority(getEncodingPriority());

        faceIdentificationProperties.setStringBuilderTaskReferences(taskReferences);

        super.getData(faceIdentificationProperties);


        return faceIdentificationProperties;
    }

    public void setData(FaceIdentificationProperties workflowProperties)
    {
        super.setData(workflowProperties);

        setCascadeName(workflowProperties.getCascadeName());
        setDeepLearnedModelTags(workflowProperties.getDeepLearnedModelTags());
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
            if (joParameters.has("DeepLearnedModelTags") && !joParameters.getString("DeepLearnedModelTags").equalsIgnoreCase(""))
                setDeepLearnedModelTags(joParameters.getString("DeepLearnedModelTags"));
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

            if (getDeepLearnedModelTags() != null && !getDeepLearnedModelTags().equalsIgnoreCase(""))
            {
                JSONArray jaDeepLearnedModelTags = new JSONArray();
                joParameters.put("DeepLearnedModelTags", jaDeepLearnedModelTags);

                String [] deepLearnedModelTags = getDeepLearnedModelTags().split(",");
                for (String deepLearnedModelTag: deepLearnedModelTags)
                {
                    jaDeepLearnedModelTags.put(deepLearnedModelTag);
                }
            }
            else
            {
                WorkflowIssue workflowIssue = new WorkflowIssue();
                workflowIssue.setLabel(getLabel());
                workflowIssue.setFieldName("DeepLearnedModelTags");
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

    public String getDeepLearnedModelTags() {
        return deepLearnedModelTags;
    }

    public void setDeepLearnedModelTags(String deepLearnedModelTags) {
        this.deepLearnedModelTags = deepLearnedModelTags;
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
