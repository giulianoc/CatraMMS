package com.catramms.backing.workflowEditor.Properties;

import com.catramms.backing.newWorkflow.WorkflowIssue;
import com.catramms.backing.workflowEditor.utility.IngestionData;
import org.apache.log4j.Logger;
import org.json.JSONObject;

import java.io.Serializable;
import java.util.ArrayList;
import java.util.List;

/**
 * Created by multi on 11.01.19.
 */
public class WorkflowProperties implements Serializable
{

    private static final Logger mLogger = Logger.getLogger(WorkflowProperties.class);

    private int elementId;
    private String label;
    private boolean labelChanged;
    private String image;
    private String mainType;
    private String type;

    private List<WorkflowProperties> onSuccessChildren = new ArrayList<>();
    private List<WorkflowProperties> onErrorChildren = new ArrayList<>();
    private List<WorkflowProperties> onCompleteChildren = new ArrayList<>();


    public WorkflowProperties() {
    }

    public WorkflowProperties(int elementId, String label, String image, String mainType, String type) {
        this.elementId = elementId;
        this.label = label;
        this.image = image;
        this.mainType = mainType;
        this.type = type;
        labelChanged = false;
    }


    public WorkflowProperties clone()
    {
        boolean isLabelChanged = isLabelChanged();

        WorkflowProperties workflowProperties = new WorkflowProperties(
                getElementId(), getLabel(), getImage(), getMainType(), getType());

        workflowProperties.setLabelChanged(isLabelChanged);

        return workflowProperties;
    }

    public JSONObject buildWorkflowElementJson(IngestionData ingestionData)
            throws Exception
    {
        JSONObject jsonWorkflowElement = new JSONObject();

        try
        {
            jsonWorkflowElement.put("Type", getType());

            if (label != null && !label.equalsIgnoreCase(""))
                jsonWorkflowElement.put("Label", label);
            else
            {
                WorkflowIssue workflowIssue = new WorkflowIssue();
                workflowIssue.setLabel("");
                workflowIssue.setFieldName("Label");
                workflowIssue.setTaskType("Workflow");
                workflowIssue.setIssue("The field is not initialized");

                ingestionData.getWorkflowIssueList().add(workflowIssue);
            }

            int onSuccessChildrenNumber = onSuccessChildren.size();
            mLogger.info("WorkflowProperties::buildWorkflowElementJson"
                    + ", onSuccessChildrenNumber: " + onSuccessChildrenNumber
            );
            // inside this object (WorkflowProperties) we can have just one child
            if (onSuccessChildrenNumber == 1)
            {
                // Task
                jsonWorkflowElement.put("Task", onSuccessChildren.get(0).buildWorkflowElementJson(ingestionData));
            }
            else if (onSuccessChildrenNumber > 1)
            {
                mLogger.error("It is not possible to have more than one connection"
                        + ", onSuccessChildrenNumber: " + onSuccessChildrenNumber
                );
            }

                /*
                else if (task.getType().equalsIgnoreCase("Remove-Content"))
                {
                    jsonObject.put("Type", task.getType());

                    JSONObject joParameters = new JSONObject();
                    jsonObject.put("Parameters", joParameters);

                    mLogger.info("task.getType: " + task.getType());

                    if (task.getLabel() != null && !task.getLabel().equalsIgnoreCase(""))
                        jsonObject.put("Label", task.getLabel());
                    else
                    {
                        WorkflowIssue workflowIssue = new WorkflowIssue();
                        workflowIssue.setLabel("");
                        workflowIssue.setFieldName("Label");
                        workflowIssue.setTaskType(task.getType());
                        workflowIssue.setIssue("The field is not initialized");

                        workflowIssueList.add(workflowIssue);
                    }

                    if (task.getReferences() != null && !task.getReferences().equalsIgnoreCase(""))
                    {
                        JSONArray jaReferences = new JSONArray();
                        joParameters.put("References", jaReferences);

                        String [] mediaItemKeyReferences = task.getReferences().split(",");
                        for (String mediaItemKeyReference: mediaItemKeyReferences)
                        {
                            JSONObject joReference = new JSONObject();
                            joReference.put("ReferenceMediaItemKey", Long.parseLong(mediaItemKeyReference.trim()));

                            jaReferences.put(joReference);
                        }
                    }
                }
                else if (task.getType().equalsIgnoreCase("Concat-Demuxer"))
                {
                    jsonObject.put("Type", task.getType());

                    JSONObject joParameters = new JSONObject();
                    jsonObject.put("Parameters", joParameters);

                    mLogger.info("task.getType: " + task.getType());

                    if (task.getLabel() != null && !task.getLabel().equalsIgnoreCase(""))
                        jsonObject.put("Label", task.getLabel());
                    else
                    {
                        WorkflowIssue workflowIssue = new WorkflowIssue();
                        workflowIssue.setLabel("");
                        workflowIssue.setFieldName("Label");
                        workflowIssue.setTaskType(task.getType());
                        workflowIssue.setIssue("The field is not initialized");

                        workflowIssueList.add(workflowIssue);
                    }

                    if (task.getUserData() != null && !task.getUserData().equalsIgnoreCase(""))
                        joParameters.put("UserData", task.getUserData());
                    if (task.getRetention() != null && !task.getRetention().equalsIgnoreCase(""))
                        joParameters.put("Retention", task.getRetention());
                    if (task.getTitle() != null && !task.getTitle().equalsIgnoreCase(""))
                        joParameters.put("Title", task.getTitle());
                    if (task.getUniqueName() != null && !task.getUniqueName().equalsIgnoreCase(""))
                        joParameters.put("UniqueName", task.getUniqueName());
                    if (task.getIngester() != null && !task.getIngester().equalsIgnoreCase(""))
                        joParameters.put("Ingester", task.getIngester());
                    if (task.getTags() != null && !task.getTags().equalsIgnoreCase(""))
                    {
                        JSONArray jsonTagsArray = new JSONArray();
                        joParameters.put("Tags", jsonTagsArray);

                        for (String tag: task.getTags().split(","))
                        {
                            jsonTagsArray.put(tag);
                        }
                    }
                    if (task.getContentProviderName() != null && !task.getContentProviderName().equalsIgnoreCase(""))
                        joParameters.put("ContentProviderName", task.getContentProviderName());
                    if (task.getDeliveryFileName() != null && !task.getDeliveryFileName().equalsIgnoreCase(""))
                        joParameters.put("DeliveryFileName", task.getDeliveryFileName());
                    if (task.getStartPublishing() != null || task.getEndPublishing() != null)
                    {
                        JSONObject joPublishing = new JSONObject();
                        joParameters.put("Publishing", joPublishing);

                        if (task.getStartPublishing() != null)
                            joPublishing.put("StartPublishing", dateFormat.format(task.getStartPublishing()));
                        else
                            joPublishing.put("StartPublishing", "NOW");
                        if (task.getEndPublishing() != null)
                            joPublishing.put("EndPublishing", dateFormat.format(task.getEndPublishing()));
                        else
                            joPublishing.put("EndPublishing", "FOREVER");
                    }

                    if (task.getReferences() != null && !task.getReferences().equalsIgnoreCase(""))
                    {
                        JSONArray jaReferences = new JSONArray();
                        joParameters.put("References", jaReferences);

                        String [] physicalPathKeyReferences = task.getReferences().split(",");
                        for (String physicalPathKeyReference: physicalPathKeyReferences)
                        {
                            JSONObject joReference = new JSONObject();
                            joReference.put("ReferencePhysicalPathKey", Long.parseLong(physicalPathKeyReference.trim()));

                            jaReferences.put(joReference);
                        }
                    }
                }
                else if (task.getType().equalsIgnoreCase("Extract-Tracks"))
                {
                    jsonObject.put("Type", task.getType());

                    JSONObject joParameters = new JSONObject();
                    jsonObject.put("Parameters", joParameters);

                    mLogger.info("task.getType: " + task.getType());

                    if (task.getLabel() != null && !task.getLabel().equalsIgnoreCase(""))
                        jsonObject.put("Label", task.getLabel());
                    else
                    {
                        WorkflowIssue workflowIssue = new WorkflowIssue();
                        workflowIssue.setLabel("");
                        workflowIssue.setFieldName("Label");
                        workflowIssue.setTaskType(task.getType());
                        workflowIssue.setIssue("The field is not initialized");

                        workflowIssueList.add(workflowIssue);
                    }

                    joParameters.put("OutputFileFormat", task.getFileFormat());
                    {
                        JSONArray jaTracks = new JSONArray();
                        joParameters.put("Tracks", jaTracks);

                        if (taskExtractTracksVideoTrackNumber != null)
                        {
                            JSONObject joTrack = new JSONObject();
                            jaTracks.put(joTrack);
                            joTrack.put("TrackType", "video");
                            joTrack.put("TrackNumber", taskExtractTracksVideoTrackNumber);
                        }

                        if (taskExtractTracksAudioTrackNumber != null)
                        {
                            JSONObject joTrack = new JSONObject();
                            jaTracks.put(joTrack);
                            joTrack.put("TrackType", "audio");
                            joTrack.put("TrackNumber", taskExtractTracksAudioTrackNumber);
                        }
                    }
                    if (task.getUserData() != null && !task.getUserData().equalsIgnoreCase(""))
                        joParameters.put("UserData", task.getUserData());
                    if (task.getRetention() != null && !task.getRetention().equalsIgnoreCase(""))
                        joParameters.put("Retention", task.getRetention());
                    if (task.getTitle() != null && !task.getTitle().equalsIgnoreCase(""))
                        joParameters.put("Title", task.getTitle());
                    if (task.getUniqueName() != null && !task.getUniqueName().equalsIgnoreCase(""))
                        joParameters.put("UniqueName", task.getUniqueName());
                    if (task.getIngester() != null && !task.getIngester().equalsIgnoreCase(""))
                        joParameters.put("Ingester", task.getIngester());
                    if (task.getTags() != null && !task.getTags().equalsIgnoreCase(""))
                    {
                        JSONArray jsonTagsArray = new JSONArray();
                        joParameters.put("Tags", jsonTagsArray);

                        for (String tag: task.getTags().split(","))
                        {
                            jsonTagsArray.put(tag);
                        }
                    }
                    if (task.getContentProviderName() != null && !task.getContentProviderName().equalsIgnoreCase(""))
                        joParameters.put("ContentProviderName", task.getContentProviderName());
                    if (task.getDeliveryFileName() != null && !task.getDeliveryFileName().equalsIgnoreCase(""))
                        joParameters.put("DeliveryFileName", task.getDeliveryFileName());
                    if (task.getStartPublishing() != null || task.getEndPublishing() != null)
                    {
                        JSONObject joPublishing = new JSONObject();
                        joParameters.put("Publishing", joPublishing);

                        if (task.getStartPublishing() != null)
                            joPublishing.put("StartPublishing", dateFormat.format(task.getStartPublishing()));
                        else
                            joPublishing.put("StartPublishing", "NOW");
                        if (task.getEndPublishing() != null)
                            joPublishing.put("EndPublishing", dateFormat.format(task.getEndPublishing()));
                        else
                            joPublishing.put("EndPublishing", "FOREVER");
                    }

                    if (task.getReferences() != null && !task.getReferences().equalsIgnoreCase(""))
                    {
                        JSONArray jaReferences = new JSONArray();
                        joParameters.put("References", jaReferences);

                        String [] physicalPathKeyReferences = task.getReferences().split(",");
                        for (String physicalPathKeyReference: physicalPathKeyReferences)
                        {
                            JSONObject joReference = new JSONObject();
                            joReference.put("ReferencePhysicalPathKey", Long.parseLong(physicalPathKeyReference.trim()));

                            jaReferences.put(joReference);
                        }
                    }
                }
                else if (task.getType().equalsIgnoreCase("Cut"))
                {
                    jsonObject.put("Type", task.getType());

                    JSONObject joParameters = new JSONObject();
                    jsonObject.put("Parameters", joParameters);

                    mLogger.info("task.getType: " + task.getType());

                    if (task.getLabel() != null && !task.getLabel().equalsIgnoreCase(""))
                        jsonObject.put("Label", task.getLabel());
                    else
                    {
                        WorkflowIssue workflowIssue = new WorkflowIssue();
                        workflowIssue.setLabel("");
                        workflowIssue.setFieldName("Label");
                        workflowIssue.setTaskType(task.getType());
                        workflowIssue.setIssue("The field is not initialized");

                        workflowIssueList.add(workflowIssue);
                    }

                    if (task.getStartTimeInSeconds() != null)
                        joParameters.put("StartTimeInSeconds", task.getStartTimeInSeconds());
                        // String.format("%." + timeInSecondsDecimalsPrecision + "g",
                        //        task.getStartTimeInSeconds().floatValue()));
                    else
                    {
                        WorkflowIssue workflowIssue = new WorkflowIssue();
                        workflowIssue.setLabel(task.getLabel());
                        workflowIssue.setFieldName("StartTimeInSeconds");
                        workflowIssue.setTaskType(task.getType());
                        workflowIssue.setIssue("The field is not initialized");

                        workflowIssueList.add(workflowIssue);
                    }
                    if (task.getCutEndType().equalsIgnoreCase("endTime"))
                    {
                        if (task.getEndTimeInSeconds() != null)
                            joParameters.put("EndTimeInSeconds", task.getEndTimeInSeconds());
                            // String.format("%." + timeInSecondsDecimalsPrecision + "g",
                            //        task.getEndTimeInSeconds().floatValue()));
                        else
                        {
                            WorkflowIssue workflowIssue = new WorkflowIssue();
                            workflowIssue.setLabel(task.getLabel());
                            workflowIssue.setFieldName("EndTimeInSeconds");
                            workflowIssue.setTaskType(task.getType());
                            workflowIssue.setIssue("The field is not initialized");

                            workflowIssueList.add(workflowIssue);
                        }
                    }
                    else
                    {
                        if (task.getFramesNumber() != null)
                            joParameters.put("FramesNumber", task.getFramesNumber());
                        else
                        {
                            WorkflowIssue workflowIssue = new WorkflowIssue();
                            workflowIssue.setLabel(task.getLabel());
                            workflowIssue.setFieldName("FramesNumber");
                            workflowIssue.setTaskType(task.getType());
                            workflowIssue.setIssue("The field is not initialized");

                            workflowIssueList.add(workflowIssue);
                        }
                    }
                    if (task.getFileFormat() != null && !task.getFileFormat().equalsIgnoreCase(""))
                        joParameters.put("OutputFileFormat", task.getFileFormat());
                    if (task.getUserData() != null && !task.getUserData().equalsIgnoreCase(""))
                        joParameters.put("UserData", task.getUserData());
                    if (task.getRetention() != null && !task.getRetention().equalsIgnoreCase(""))
                        joParameters.put("Retention", task.getRetention());
                    if (task.getTitle() != null && !task.getTitle().equalsIgnoreCase(""))
                        joParameters.put("Title", task.getTitle());
                    if (task.getUniqueName() != null && !task.getUniqueName().equalsIgnoreCase(""))
                        joParameters.put("UniqueName", task.getUniqueName());
                    if (task.getIngester() != null && !task.getIngester().equalsIgnoreCase(""))
                        joParameters.put("Ingester", task.getIngester());
                    if (task.getTags() != null && !task.getTags().equalsIgnoreCase(""))
                    {
                        JSONArray jsonTagsArray = new JSONArray();
                        joParameters.put("Tags", jsonTagsArray);

                        for (String tag: task.getTags().split(","))
                        {
                            jsonTagsArray.put(tag);
                        }
                    }
                    if (task.getContentProviderName() != null && !task.getContentProviderName().equalsIgnoreCase(""))
                        joParameters.put("ContentProviderName", task.getContentProviderName());
                    if (task.getDeliveryFileName() != null && !task.getDeliveryFileName().equalsIgnoreCase(""))
                        joParameters.put("DeliveryFileName", task.getDeliveryFileName());
                    if (task.getStartPublishing() != null || task.getEndPublishing() != null)
                    {
                        JSONObject joPublishing = new JSONObject();
                        joParameters.put("Publishing", joPublishing);

                        if (task.getStartPublishing() != null)
                            joPublishing.put("StartPublishing", dateFormat.format(task.getStartPublishing()));
                        else
                            joPublishing.put("StartPublishing", "NOW");
                        if (task.getEndPublishing() != null)
                            joPublishing.put("EndPublishing", dateFormat.format(task.getEndPublishing()));
                        else
                            joPublishing.put("EndPublishing", "FOREVER");
                    }

                    if (task.getReferences() != null && !task.getReferences().equalsIgnoreCase(""))
                    {
                        JSONArray jaReferences = new JSONArray();
                        joParameters.put("References", jaReferences);

                        String [] physicalPathKeyReferences = task.getReferences().split(",");
                        for (String physicalPathKeyReference: physicalPathKeyReferences)
                        {
                            JSONObject joReference = new JSONObject();
                            joReference.put("ReferencePhysicalPathKey", Long.parseLong(physicalPathKeyReference.trim()));

                            jaReferences.put(joReference);
                        }
                    }
                }
                else if (task.getType().equalsIgnoreCase("Face-Recognition"))
                {
                    jsonObject.put("Type", task.getType());

                    JSONObject joParameters = new JSONObject();
                    jsonObject.put("Parameters", joParameters);

                    mLogger.info("task.getType: " + task.getType());

                    if (task.getLabel() != null && !task.getLabel().equalsIgnoreCase(""))
                        jsonObject.put("Label", task.getLabel());
                    else
                    {
                        WorkflowIssue workflowIssue = new WorkflowIssue();
                        workflowIssue.setLabel("");
                        workflowIssue.setFieldName("Label");
                        workflowIssue.setTaskType(task.getType());
                        workflowIssue.setIssue("The field is not initialized");

                        workflowIssueList.add(workflowIssue);
                    }

                    if (task.getFaceRecognitionCascadeName() != null && !task.getFaceRecognitionCascadeName().equalsIgnoreCase(""))
                        joParameters.put("CascadeName", task.getFaceRecognitionCascadeName());
                    else
                    {
                        WorkflowIssue workflowIssue = new WorkflowIssue();
                        workflowIssue.setLabel(task.getLabel());
                        workflowIssue.setFieldName("CascadeName");
                        workflowIssue.setTaskType(task.getType());
                        workflowIssue.setIssue("The field is not initialized");

                        workflowIssueList.add(workflowIssue);
                    }
                    if (task.getFaceRecognitionOutput() != null && !task.getFaceRecognitionOutput().equalsIgnoreCase(""))
                        joParameters.put("Output", task.getFaceRecognitionOutput());
                    else
                    {
                        WorkflowIssue workflowIssue = new WorkflowIssue();
                        workflowIssue.setLabel(task.getLabel());
                        workflowIssue.setFieldName("Output");
                        workflowIssue.setTaskType(task.getType());
                        workflowIssue.setIssue("The field is not initialized");

                        workflowIssueList.add(workflowIssue);
                    }
                    if (task.getUserData() != null && !task.getUserData().equalsIgnoreCase(""))
                        joParameters.put("UserData", task.getUserData());
                    if (task.getRetention() != null && !task.getRetention().equalsIgnoreCase(""))
                        joParameters.put("Retention", task.getRetention());
                    if (task.getTitle() != null && !task.getTitle().equalsIgnoreCase(""))
                        joParameters.put("Title", task.getTitle());
                    if (task.getEncodingPriority() != null && !task.getEncodingPriority().equalsIgnoreCase(""))
                        joParameters.put("EncodingPriority", task.getEncodingPriority());
                    if (task.getUniqueName() != null && !task.getUniqueName().equalsIgnoreCase(""))
                        joParameters.put("UniqueName", task.getUniqueName());
                    if (task.getIngester() != null && !task.getIngester().equalsIgnoreCase(""))
                        joParameters.put("Ingester", task.getIngester());
                    if (task.getTags() != null && !task.getTags().equalsIgnoreCase(""))
                    {
                        JSONArray jsonTagsArray = new JSONArray();
                        joParameters.put("Tags", jsonTagsArray);

                        for (String tag: task.getTags().split(","))
                        {
                            jsonTagsArray.put(tag);
                        }
                    }
                    if (task.getContentProviderName() != null && !task.getContentProviderName().equalsIgnoreCase(""))
                        joParameters.put("ContentProviderName", task.getContentProviderName());
                    if (task.getDeliveryFileName() != null && !task.getDeliveryFileName().equalsIgnoreCase(""))
                        joParameters.put("DeliveryFileName", task.getDeliveryFileName());
                    if (task.getStartPublishing() != null || task.getEndPublishing() != null)
                    {
                        JSONObject joPublishing = new JSONObject();
                        joParameters.put("Publishing", joPublishing);

                        if (task.getStartPublishing() != null)
                            joPublishing.put("StartPublishing", dateFormat.format(task.getStartPublishing()));
                        else
                            joPublishing.put("StartPublishing", "NOW");
                        if (task.getEndPublishing() != null)
                            joPublishing.put("EndPublishing", dateFormat.format(task.getEndPublishing()));
                        else
                            joPublishing.put("EndPublishing", "FOREVER");
                    }

                    if (task.getReferences() != null && !task.getReferences().equalsIgnoreCase(""))
                    {
                        JSONArray jaReferences = new JSONArray();
                        joParameters.put("References", jaReferences);

                        String [] physicalPathKeyReferences = task.getReferences().split(",");
                        for (String physicalPathKeyReference: physicalPathKeyReferences)
                        {
                            JSONObject joReference = new JSONObject();
                            joReference.put("ReferencePhysicalPathKey", Long.parseLong(physicalPathKeyReference.trim()));

                            jaReferences.put(joReference);
                        }
                    }
                }
                else if (task.getType().equalsIgnoreCase("Face-Identification"))
                {
                    jsonObject.put("Type", task.getType());

                    JSONObject joParameters = new JSONObject();
                    jsonObject.put("Parameters", joParameters);

                    mLogger.info("task.getType: " + task.getType());

                    if (task.getLabel() != null && !task.getLabel().equalsIgnoreCase(""))
                        jsonObject.put("Label", task.getLabel());
                    else
                    {
                        WorkflowIssue workflowIssue = new WorkflowIssue();
                        workflowIssue.setLabel("");
                        workflowIssue.setFieldName("Label");
                        workflowIssue.setTaskType(task.getType());
                        workflowIssue.setIssue("The field is not initialized");

                        workflowIssueList.add(workflowIssue);
                    }

                    if (task.getFaceRecognitionCascadeName() != null && !task.getFaceRecognitionCascadeName().equalsIgnoreCase(""))
                        joParameters.put("CascadeName", task.getFaceRecognitionCascadeName());
                    else
                    {
                        WorkflowIssue workflowIssue = new WorkflowIssue();
                        workflowIssue.setLabel(task.getLabel());
                        workflowIssue.setFieldName("CascadeName");
                        workflowIssue.setTaskType(task.getType());
                        workflowIssue.setIssue("The field is not initialized");

                        workflowIssueList.add(workflowIssue);
                    }
                    if (task.getFaceIdentificationDeepLearnedModelTags() != null && !task.getFaceIdentificationDeepLearnedModelTags().equalsIgnoreCase(""))
                    {
                        JSONArray jaDeepLearnedModelTags = new JSONArray();
                        joParameters.put("DeepLearnedModelTags", jaDeepLearnedModelTags);

                        String [] deepLearnedModelTags = task.getFaceIdentificationDeepLearnedModelTags().split(",");
                        for (String deepLearnedModelTag: deepLearnedModelTags)
                        {
                            jaDeepLearnedModelTags.put(deepLearnedModelTag);
                        }
                    }
                    else
                    {
                        WorkflowIssue workflowIssue = new WorkflowIssue();
                        workflowIssue.setLabel(task.getLabel());
                        workflowIssue.setFieldName("DeepLearnedModelTags");
                        workflowIssue.setTaskType(task.getType());
                        workflowIssue.setIssue("The field is not initialized");

                        workflowIssueList.add(workflowIssue);
                    }
                    if (task.getUserData() != null && !task.getUserData().equalsIgnoreCase(""))
                        joParameters.put("UserData", task.getUserData());
                    if (task.getRetention() != null && !task.getRetention().equalsIgnoreCase(""))
                        joParameters.put("Retention", task.getRetention());
                    if (task.getTitle() != null && !task.getTitle().equalsIgnoreCase(""))
                        joParameters.put("Title", task.getTitle());
                    if (task.getEncodingPriority() != null && !task.getEncodingPriority().equalsIgnoreCase(""))
                        joParameters.put("EncodingPriority", task.getEncodingPriority());
                    if (task.getUniqueName() != null && !task.getUniqueName().equalsIgnoreCase(""))
                        joParameters.put("UniqueName", task.getUniqueName());
                    if (task.getIngester() != null && !task.getIngester().equalsIgnoreCase(""))
                        joParameters.put("Ingester", task.getIngester());
                    if (task.getTags() != null && !task.getTags().equalsIgnoreCase(""))
                    {
                        JSONArray jsonTagsArray = new JSONArray();
                        joParameters.put("Tags", jsonTagsArray);

                        for (String tag: task.getTags().split(","))
                        {
                            jsonTagsArray.put(tag);
                        }
                    }
                    if (task.getContentProviderName() != null && !task.getContentProviderName().equalsIgnoreCase(""))
                        joParameters.put("ContentProviderName", task.getContentProviderName());
                    if (task.getDeliveryFileName() != null && !task.getDeliveryFileName().equalsIgnoreCase(""))
                        joParameters.put("DeliveryFileName", task.getDeliveryFileName());
                    if (task.getStartPublishing() != null || task.getEndPublishing() != null)
                    {
                        JSONObject joPublishing = new JSONObject();
                        joParameters.put("Publishing", joPublishing);

                        if (task.getStartPublishing() != null)
                            joPublishing.put("StartPublishing", dateFormat.format(task.getStartPublishing()));
                        else
                            joPublishing.put("StartPublishing", "NOW");
                        if (task.getEndPublishing() != null)
                            joPublishing.put("EndPublishing", dateFormat.format(task.getEndPublishing()));
                        else
                            joPublishing.put("EndPublishing", "FOREVER");
                    }

                    if (task.getReferences() != null && !task.getReferences().equalsIgnoreCase(""))
                    {
                        JSONArray jaReferences = new JSONArray();
                        joParameters.put("References", jaReferences);

                        String [] physicalPathKeyReferences = task.getReferences().split(",");
                        for (String physicalPathKeyReference: physicalPathKeyReferences)
                        {
                            JSONObject joReference = new JSONObject();
                            joReference.put("ReferencePhysicalPathKey", Long.parseLong(physicalPathKeyReference.trim()));

                            jaReferences.put(joReference);
                        }
                    }
                }
                else if (task.getType().equalsIgnoreCase("Overlay-Image-On-Video"))
                {
                    jsonObject.put("Type", task.getType());

                    JSONObject joParameters = new JSONObject();
                    jsonObject.put("Parameters", joParameters);

                    mLogger.info("task.getType: " + task.getType());

                    if (task.getLabel() != null && !task.getLabel().equalsIgnoreCase(""))
                        jsonObject.put("Label", task.getLabel());
                    else
                    {
                        WorkflowIssue workflowIssue = new WorkflowIssue();
                        workflowIssue.setLabel("");
                        workflowIssue.setFieldName("Label");
                        workflowIssue.setTaskType(task.getType());
                        workflowIssue.setIssue("The field is not initialized");

                        workflowIssueList.add(workflowIssue);
                    }

                    if (task.getOverlayPositionXInPixel() != null && !task.getOverlayPositionXInPixel().equalsIgnoreCase(""))
                        joParameters.put("ImagePosition_X_InPixel", task.getOverlayPositionXInPixel());
                    else
                    {
                        WorkflowIssue workflowIssue = new WorkflowIssue();
                        workflowIssue.setLabel(task.getLabel());
                        workflowIssue.setFieldName("ImagePosition_X_InPixel");
                        workflowIssue.setTaskType(task.getType());
                        workflowIssue.setIssue("The field is not initialized");

                        workflowIssueList.add(workflowIssue);
                    }
                    if (task.getOverlayPositionYInPixel() != null && !task.getOverlayPositionYInPixel().equalsIgnoreCase(""))
                        joParameters.put("ImagePosition_Y_InPixel", task.getOverlayPositionYInPixel());
                    else
                    {
                        WorkflowIssue workflowIssue = new WorkflowIssue();
                        workflowIssue.setLabel(task.getLabel());
                        workflowIssue.setFieldName("ImagePosition_Y_InPixel");
                        workflowIssue.setTaskType(task.getType());
                        workflowIssue.setIssue("The field is not initialized");

                        workflowIssueList.add(workflowIssue);
                    }
                    if (task.getUserData() != null && !task.getUserData().equalsIgnoreCase(""))
                        joParameters.put("UserData", task.getUserData());
                    if (task.getRetention() != null && !task.getRetention().equalsIgnoreCase(""))
                        joParameters.put("Retention", task.getRetention());
                    if (task.getTitle() != null && !task.getTitle().equalsIgnoreCase(""))
                        joParameters.put("Title", task.getTitle());
                    if (task.getEncodingPriority() != null && !task.getEncodingPriority().equalsIgnoreCase(""))
                        joParameters.put("EncodingPriority", task.getEncodingPriority());
                    if (task.getUniqueName() != null && !task.getUniqueName().equalsIgnoreCase(""))
                        joParameters.put("UniqueName", task.getUniqueName());
                    if (task.getIngester() != null && !task.getIngester().equalsIgnoreCase(""))
                        joParameters.put("Ingester", task.getIngester());
                    if (task.getTags() != null && !task.getTags().equalsIgnoreCase(""))
                    {
                        JSONArray jsonTagsArray = new JSONArray();
                        joParameters.put("Tags", jsonTagsArray);

                        for (String tag: task.getTags().split(","))
                        {
                            jsonTagsArray.put(tag);
                        }
                    }
                    if (task.getContentProviderName() != null && !task.getContentProviderName().equalsIgnoreCase(""))
                        joParameters.put("ContentProviderName", task.getContentProviderName());
                    if (task.getDeliveryFileName() != null && !task.getDeliveryFileName().equalsIgnoreCase(""))
                        joParameters.put("DeliveryFileName", task.getDeliveryFileName());
                    if (task.getStartPublishing() != null || task.getEndPublishing() != null)
                    {
                        JSONObject joPublishing = new JSONObject();
                        joParameters.put("Publishing", joPublishing);

                        if (task.getStartPublishing() != null)
                            joPublishing.put("StartPublishing", dateFormat.format(task.getStartPublishing()));
                        else
                            joPublishing.put("StartPublishing", "NOW");
                        if (task.getEndPublishing() != null)
                            joPublishing.put("EndPublishing", dateFormat.format(task.getEndPublishing()));
                        else
                            joPublishing.put("EndPublishing", "FOREVER");
                    }

                    if (task.getReferences() != null && !task.getReferences().equalsIgnoreCase(""))
                    {
                        JSONArray jaReferences = new JSONArray();
                        joParameters.put("References", jaReferences);

                        String [] physicalPathKeyReferences = task.getReferences().split(",");
                        for (String physicalPathKeyReference: physicalPathKeyReferences)
                        {
                            JSONObject joReference = new JSONObject();
                            joReference.put("ReferencePhysicalPathKey", Long.parseLong(physicalPathKeyReference.trim()));

                            jaReferences.put(joReference);
                        }
                    }
                }
                else if (task.getType().equalsIgnoreCase("Overlay-Text-On-Video"))
                {
                    jsonObject.put("Type", task.getType());

                    JSONObject joParameters = new JSONObject();
                    jsonObject.put("Parameters", joParameters);

                    mLogger.info("task.getType: " + task.getType());

                    if (task.getLabel() != null && !task.getLabel().equalsIgnoreCase(""))
                        jsonObject.put("Label", task.getLabel());
                    else
                    {
                        WorkflowIssue workflowIssue = new WorkflowIssue();
                        workflowIssue.setLabel("");
                        workflowIssue.setFieldName("Label");
                        workflowIssue.setTaskType(task.getType());
                        workflowIssue.setIssue("The field is not initialized");

                        workflowIssueList.add(workflowIssue);
                    }

                    if (task.getOverlayText() != null && !task.getOverlayText().equalsIgnoreCase(""))
                        joParameters.put("Text", task.getOverlayText());
                    else
                    {
                        WorkflowIssue workflowIssue = new WorkflowIssue();
                        workflowIssue.setLabel(task.getLabel());
                        workflowIssue.setFieldName("Text");
                        workflowIssue.setTaskType(task.getType());
                        workflowIssue.setIssue("The field is not initialized");

                        workflowIssueList.add(workflowIssue);
                    }

                    if (task.getOverlayPositionXInPixel() != null && !task.getOverlayPositionXInPixel().equalsIgnoreCase(""))
                        joParameters.put("TextPosition_X_InPixel", task.getOverlayPositionXInPixel());
                    if (task.getOverlayPositionYInPixel() != null && !task.getOverlayPositionYInPixel().equalsIgnoreCase(""))
                        joParameters.put("TextPosition_Y_InPixel", task.getOverlayPositionYInPixel());
                    if (task.getOverlayFontType() != null && !task.getOverlayFontType().equalsIgnoreCase(""))
                        joParameters.put("FontType", task.getOverlayFontType());
                    if (task.getOverlayFontSize() != null)
                        joParameters.put("FontSize", task.getOverlayFontSize());
                    if (task.getOverlayFontColor() != null && !task.getOverlayFontColor().equalsIgnoreCase(""))
                        joParameters.put("FontColor", task.getOverlayFontColor());
                    if (task.getOverlayTextPercentageOpacity() != null)
                        joParameters.put("TextPercentageOpacity", task.getOverlayTextPercentageOpacity());
                    if (task.getOverlayBoxColor() != null)
                        joParameters.put("BoxEnable", task.getOverlayBoxEnable());
                    if (task.getOverlayBoxColor() != null && !task.getOverlayBoxColor().equalsIgnoreCase(""))
                        joParameters.put("BoxColor", task.getOverlayBoxColor());
                    if (task.getOverlayBoxPercentageOpacity() != null)
                        joParameters.put("BoxPercentageOpacity", task.getOverlayBoxPercentageOpacity());
                    if (task.getUserData() != null && !task.getUserData().equalsIgnoreCase(""))
                        joParameters.put("UserData", task.getUserData());
                    if (task.getRetention() != null && !task.getRetention().equalsIgnoreCase(""))
                        joParameters.put("Retention", task.getRetention());
                    if (task.getTitle() != null && !task.getTitle().equalsIgnoreCase(""))
                        joParameters.put("Title", task.getTitle());
                    if (task.getEncodingPriority() != null && !task.getEncodingPriority().equalsIgnoreCase(""))
                        joParameters.put("EncodingPriority", task.getEncodingPriority());
                    if (task.getUniqueName() != null && !task.getUniqueName().equalsIgnoreCase(""))
                        joParameters.put("UniqueName", task.getUniqueName());
                    if (task.getIngester() != null && !task.getIngester().equalsIgnoreCase(""))
                        joParameters.put("Ingester", task.getIngester());
                    if (task.getTags() != null && !task.getTags().equalsIgnoreCase(""))
                    {
                        JSONArray jsonTagsArray = new JSONArray();
                        joParameters.put("Tags", jsonTagsArray);

                        for (String tag: task.getTags().split(","))
                        {
                            jsonTagsArray.put(tag);
                        }
                    }
                    if (task.getContentProviderName() != null && !task.getContentProviderName().equalsIgnoreCase(""))
                        joParameters.put("ContentProviderName", task.getContentProviderName());
                    if (task.getDeliveryFileName() != null && !task.getDeliveryFileName().equalsIgnoreCase(""))
                        joParameters.put("DeliveryFileName", task.getDeliveryFileName());
                    if (task.getStartPublishing() != null || task.getEndPublishing() != null)
                    {
                        JSONObject joPublishing = new JSONObject();
                        joParameters.put("Publishing", joPublishing);

                        if (task.getStartPublishing() != null)
                            joPublishing.put("StartPublishing", dateFormat.format(task.getStartPublishing()));
                        else
                            joPublishing.put("StartPublishing", "NOW");
                        if (task.getEndPublishing() != null)
                            joPublishing.put("EndPublishing", dateFormat.format(task.getEndPublishing()));
                        else
                            joPublishing.put("EndPublishing", "FOREVER");
                    }

                    if (task.getReferences() != null && !task.getReferences().equalsIgnoreCase(""))
                    {
                        JSONArray jaReferences = new JSONArray();
                        joParameters.put("References", jaReferences);

                        String [] physicalPathKeyReferences = task.getReferences().split(",");
                        for (String physicalPathKeyReference: physicalPathKeyReferences)
                        {
                            JSONObject joReference = new JSONObject();
                            joReference.put("ReferencePhysicalPathKey", Long.parseLong(physicalPathKeyReference.trim()));

                            jaReferences.put(joReference);
                        }
                    }
                }
                else if (task.getType().equalsIgnoreCase("Frame"))
                {
                    jsonObject.put("Type", task.getType());

                    JSONObject joParameters = new JSONObject();
                    jsonObject.put("Parameters", joParameters);

                    mLogger.info("task.getType: " + task.getType());

                    if (task.getLabel() != null && !task.getLabel().equalsIgnoreCase(""))
                        jsonObject.put("Label", task.getLabel());
                    else
                    {
                        WorkflowIssue workflowIssue = new WorkflowIssue();
                        workflowIssue.setLabel("");
                        workflowIssue.setFieldName("Label");
                        workflowIssue.setTaskType(task.getType());
                        workflowIssue.setIssue("The field is not initialized");

                        workflowIssueList.add(workflowIssue);
                    }

                    if (task.getFrameInstantInSeconds() != null)
                        joParameters.put("InstantInSeconds", task.getFrameInstantInSeconds());
                    if (task.getFrameWidth() != null)
                        joParameters.put("Width", task.getFrameWidth());
                    if (task.getFrameHeight() != null)
                        joParameters.put("Height", task.getFrameHeight());
                    if (task.getUserData() != null && !task.getUserData().equalsIgnoreCase(""))
                        joParameters.put("UserData", task.getUserData());
                    if (task.getRetention() != null && !task.getRetention().equalsIgnoreCase(""))
                        joParameters.put("Retention", task.getRetention());
                    if (task.getTitle() != null && !task.getTitle().equalsIgnoreCase(""))
                        joParameters.put("Title", task.getTitle());
                    if (task.getUniqueName() != null && !task.getUniqueName().equalsIgnoreCase(""))
                        joParameters.put("UniqueName", task.getUniqueName());
                    if (task.getIngester() != null && !task.getIngester().equalsIgnoreCase(""))
                        joParameters.put("Ingester", task.getIngester());
                    if (task.getTags() != null && !task.getTags().equalsIgnoreCase(""))
                    {
                        JSONArray jsonTagsArray = new JSONArray();
                        joParameters.put("Tags", jsonTagsArray);

                        for (String tag: task.getTags().split(","))
                        {
                            jsonTagsArray.put(tag);
                        }
                    }
                    if (task.getContentProviderName() != null && !task.getContentProviderName().equalsIgnoreCase(""))
                        joParameters.put("ContentProviderName", task.getContentProviderName());
                    if (task.getDeliveryFileName() != null && !task.getDeliveryFileName().equalsIgnoreCase(""))
                        joParameters.put("DeliveryFileName", task.getDeliveryFileName());
                    if (task.getStartPublishing() != null || task.getEndPublishing() != null)
                    {
                        JSONObject joPublishing = new JSONObject();
                        joParameters.put("Publishing", joPublishing);

                        if (task.getStartPublishing() != null)
                            joPublishing.put("StartPublishing", dateFormat.format(task.getStartPublishing()));
                        else
                            joPublishing.put("StartPublishing", "NOW");
                        if (task.getEndPublishing() != null)
                            joPublishing.put("EndPublishing", dateFormat.format(task.getEndPublishing()));
                        else
                            joPublishing.put("EndPublishing", "FOREVER");
                    }

                    if (task.getReferences() != null && !task.getReferences().equalsIgnoreCase(""))
                    {
                        JSONArray jaReferences = new JSONArray();
                        joParameters.put("References", jaReferences);

                        String [] physicalPathKeyReferences = task.getReferences().split(",");
                        for (String physicalPathKeyReference: physicalPathKeyReferences)
                        {
                            JSONObject joReference = new JSONObject();
                            joReference.put("ReferencePhysicalPathKey", Long.parseLong(physicalPathKeyReference.trim()));

                            jaReferences.put(joReference);
                        }
                    }
                }
                else if (task.getType().equalsIgnoreCase("Periodical-Frames"))
                {
                    jsonObject.put("Type", task.getType());

                    JSONObject joParameters = new JSONObject();
                    jsonObject.put("Parameters", joParameters);

                    mLogger.info("task.getType: " + task.getType());

                    if (task.getLabel() != null && !task.getLabel().equalsIgnoreCase(""))
                        jsonObject.put("Label", task.getLabel());
                    else
                    {
                        WorkflowIssue workflowIssue = new WorkflowIssue();
                        workflowIssue.setLabel("");
                        workflowIssue.setFieldName("Label");
                        workflowIssue.setTaskType(task.getType());
                        workflowIssue.setIssue("The field is not initialized");

                        workflowIssueList.add(workflowIssue);
                    }

                    if (task.getFramePeriodInSeconds() != null)
                        joParameters.put("PeriodInSeconds", task.getFramePeriodInSeconds());
                    else
                    {
                        WorkflowIssue workflowIssue = new WorkflowIssue();
                        workflowIssue.setLabel(task.getLabel());
                        workflowIssue.setFieldName("Label");
                        workflowIssue.setTaskType(task.getType());
                        workflowIssue.setIssue("The field is not initialized");

                        workflowIssueList.add(workflowIssue);
                    }
                    if (task.getStartTimeInSeconds() != null)
                        joParameters.put("StartTimeInSeconds", task.getStartTimeInSeconds());
                    if (task.getFrameMaxFramesNumber() != null)
                        joParameters.put("MaxFramesNumber", task.getFrameMaxFramesNumber());
                    if (task.getFrameWidth() != null)
                        joParameters.put("Width", task.getFrameWidth());
                    if (task.getFrameHeight() != null)
                        joParameters.put("Height", task.getFrameHeight());
                    if (task.getUserData() != null && !task.getUserData().equalsIgnoreCase(""))
                        joParameters.put("UserData", task.getUserData());
                    if (task.getRetention() != null && !task.getRetention().equalsIgnoreCase(""))
                        joParameters.put("Retention", task.getRetention());
                    if (task.getTitle() != null && !task.getTitle().equalsIgnoreCase(""))
                        joParameters.put("Title", task.getTitle());
                    if (task.getEncodingPriority() != null && !task.getEncodingPriority().equalsIgnoreCase(""))
                        joParameters.put("EncodingPriority", task.getEncodingPriority());
                    if (task.getUniqueName() != null && !task.getUniqueName().equalsIgnoreCase(""))
                        joParameters.put("UniqueName", task.getUniqueName());
                    if (task.getIngester() != null && !task.getIngester().equalsIgnoreCase(""))
                        joParameters.put("Ingester", task.getIngester());
                    if (task.getTags() != null && !task.getTags().equalsIgnoreCase(""))
                    {
                        JSONArray jsonTagsArray = new JSONArray();
                        joParameters.put("Tags", jsonTagsArray);

                        for (String tag: task.getTags().split(","))
                        {
                            jsonTagsArray.put(tag);
                        }
                    }
                    if (task.getContentProviderName() != null && !task.getContentProviderName().equalsIgnoreCase(""))
                        joParameters.put("ContentProviderName", task.getContentProviderName());
                    if (task.getDeliveryFileName() != null && !task.getDeliveryFileName().equalsIgnoreCase(""))
                        joParameters.put("DeliveryFileName", task.getDeliveryFileName());
                    if (task.getStartPublishing() != null || task.getEndPublishing() != null)
                    {
                        JSONObject joPublishing = new JSONObject();
                        joParameters.put("Publishing", joPublishing);

                        if (task.getStartPublishing() != null)
                            joPublishing.put("StartPublishing", dateFormat.format(task.getStartPublishing()));
                        else
                            joPublishing.put("StartPublishing", "NOW");
                        if (task.getEndPublishing() != null)
                            joPublishing.put("EndPublishing", dateFormat.format(task.getEndPublishing()));
                        else
                            joPublishing.put("EndPublishing", "FOREVER");
                    }

                    if (task.getReferences() != null && !task.getReferences().equalsIgnoreCase(""))
                    {
                        JSONArray jaReferences = new JSONArray();
                        joParameters.put("References", jaReferences);

                        String [] physicalPathKeyReferences = task.getReferences().split(",");
                        for (String physicalPathKeyReference: physicalPathKeyReferences)
                        {
                            JSONObject joReference = new JSONObject();
                            joReference.put("ReferencePhysicalPathKey", Long.parseLong(physicalPathKeyReference.trim()));

                            jaReferences.put(joReference);
                        }
                    }
                }
                else if (task.getType().equalsIgnoreCase("I-Frames"))
                {
                    jsonObject.put("Type", task.getType());

                    JSONObject joParameters = new JSONObject();
                    jsonObject.put("Parameters", joParameters);

                    mLogger.info("task.getType: " + task.getType());

                    if (task.getLabel() != null && !task.getLabel().equalsIgnoreCase(""))
                        jsonObject.put("Label", task.getLabel());
                    else
                    {
                        WorkflowIssue workflowIssue = new WorkflowIssue();
                        workflowIssue.setLabel("");
                        workflowIssue.setFieldName("Label");
                        workflowIssue.setTaskType(task.getType());
                        workflowIssue.setIssue("The field is not initialized");

                        workflowIssueList.add(workflowIssue);
                    }

                    if (task.getStartTimeInSeconds() != null)
                        joParameters.put("StartTimeInSeconds", task.getStartTimeInSeconds());
                    if (task.getFrameMaxFramesNumber() != null)
                        joParameters.put("MaxFramesNumber", task.getFrameMaxFramesNumber());
                    if (task.getFrameWidth() != null)
                        joParameters.put("Width", task.getFrameWidth());
                    if (task.getFrameHeight() != null)
                        joParameters.put("Height", task.getFrameHeight());
                    if (task.getUserData() != null && !task.getUserData().equalsIgnoreCase(""))
                        joParameters.put("UserData", task.getUserData());
                    if (task.getRetention() != null && !task.getRetention().equalsIgnoreCase(""))
                        joParameters.put("Retention", task.getRetention());
                    if (task.getTitle() != null && !task.getTitle().equalsIgnoreCase(""))
                        joParameters.put("Title", task.getTitle());
                    if (task.getEncodingPriority() != null && !task.getEncodingPriority().equalsIgnoreCase(""))
                        joParameters.put("EncodingPriority", task.getEncodingPriority());
                    if (task.getUniqueName() != null && !task.getUniqueName().equalsIgnoreCase(""))
                        joParameters.put("UniqueName", task.getUniqueName());
                    if (task.getIngester() != null && !task.getIngester().equalsIgnoreCase(""))
                        joParameters.put("Ingester", task.getIngester());
                    if (task.getTags() != null && !task.getTags().equalsIgnoreCase(""))
                    {
                        JSONArray jsonTagsArray = new JSONArray();
                        joParameters.put("Tags", jsonTagsArray);

                        for (String tag: task.getTags().split(","))
                        {
                            jsonTagsArray.put(tag);
                        }
                    }
                    if (task.getContentProviderName() != null && !task.getContentProviderName().equalsIgnoreCase(""))
                        joParameters.put("ContentProviderName", task.getContentProviderName());
                    if (task.getDeliveryFileName() != null && !task.getDeliveryFileName().equalsIgnoreCase(""))
                        joParameters.put("DeliveryFileName", task.getDeliveryFileName());
                    if (task.getStartPublishing() != null || task.getEndPublishing() != null)
                    {
                        JSONObject joPublishing = new JSONObject();
                        joParameters.put("Publishing", joPublishing);

                        if (task.getStartPublishing() != null)
                            joPublishing.put("StartPublishing", dateFormat.format(task.getStartPublishing()));
                        else
                            joPublishing.put("StartPublishing", "NOW");
                        if (task.getEndPublishing() != null)
                            joPublishing.put("EndPublishing", dateFormat.format(task.getEndPublishing()));
                        else
                            joPublishing.put("EndPublishing", "FOREVER");
                    }

                    if (task.getReferences() != null && !task.getReferences().equalsIgnoreCase(""))
                    {
                        JSONArray jaReferences = new JSONArray();
                        joParameters.put("References", jaReferences);

                        String [] physicalPathKeyReferences = task.getReferences().split(",");
                        for (String physicalPathKeyReference: physicalPathKeyReferences)
                        {
                            JSONObject joReference = new JSONObject();
                            joReference.put("ReferencePhysicalPathKey", Long.parseLong(physicalPathKeyReference.trim()));

                            jaReferences.put(joReference);
                        }
                    }
                }
                else if (task.getType().equalsIgnoreCase("Motion-JPEG-by-Periodical-Frames"))
                {
                    jsonObject.put("Type", task.getType());

                    JSONObject joParameters = new JSONObject();
                    jsonObject.put("Parameters", joParameters);

                    mLogger.info("task.getType: " + task.getType());

                    if (task.getLabel() != null && !task.getLabel().equalsIgnoreCase(""))
                        jsonObject.put("Label", task.getLabel());
                    else
                    {
                        WorkflowIssue workflowIssue = new WorkflowIssue();
                        workflowIssue.setLabel("");
                        workflowIssue.setFieldName("Label");
                        workflowIssue.setTaskType(task.getType());
                        workflowIssue.setIssue("The field is not initialized");

                        workflowIssueList.add(workflowIssue);
                    }

                    if (task.getFramePeriodInSeconds() != null)
                        joParameters.put("PeriodInSeconds", task.getFramePeriodInSeconds());
                    else
                    {
                        WorkflowIssue workflowIssue = new WorkflowIssue();
                        workflowIssue.setLabel(task.getLabel());
                        workflowIssue.setFieldName("Label");
                        workflowIssue.setTaskType(task.getType());
                        workflowIssue.setIssue("The field is not initialized");

                        workflowIssueList.add(workflowIssue);
                    }
                    if (task.getStartTimeInSeconds() != null)
                        joParameters.put("StartTimeInSeconds", task.getStartTimeInSeconds());
                    if (task.getFrameMaxFramesNumber() != null)
                        joParameters.put("MaxFramesNumber", task.getFrameMaxFramesNumber());
                    if (task.getFrameWidth() != null)
                        joParameters.put("Width", task.getFrameWidth());
                    if (task.getFrameHeight() != null)
                        joParameters.put("Height", task.getFrameHeight());
                    if (task.getUserData() != null && !task.getUserData().equalsIgnoreCase(""))
                        joParameters.put("UserData", task.getUserData());
                    if (task.getRetention() != null && !task.getRetention().equalsIgnoreCase(""))
                        joParameters.put("Retention", task.getRetention());
                    if (task.getTitle() != null && !task.getTitle().equalsIgnoreCase(""))
                        joParameters.put("Title", task.getTitle());
                    if (task.getEncodingPriority() != null && !task.getEncodingPriority().equalsIgnoreCase(""))
                        joParameters.put("EncodingPriority", task.getEncodingPriority());
                    if (task.getUniqueName() != null && !task.getUniqueName().equalsIgnoreCase(""))
                        joParameters.put("UniqueName", task.getUniqueName());
                    if (task.getIngester() != null && !task.getIngester().equalsIgnoreCase(""))
                        joParameters.put("Ingester", task.getIngester());
                    if (task.getTags() != null && !task.getTags().equalsIgnoreCase(""))
                    {
                        JSONArray jsonTagsArray = new JSONArray();
                        joParameters.put("Tags", jsonTagsArray);

                        for (String tag: task.getTags().split(","))
                        {
                            jsonTagsArray.put(tag);
                        }
                    }
                    if (task.getContentProviderName() != null && !task.getContentProviderName().equalsIgnoreCase(""))
                        joParameters.put("ContentProviderName", task.getContentProviderName());
                    if (task.getDeliveryFileName() != null && !task.getDeliveryFileName().equalsIgnoreCase(""))
                        joParameters.put("DeliveryFileName", task.getDeliveryFileName());
                    if (task.getStartPublishing() != null || task.getEndPublishing() != null)
                    {
                        JSONObject joPublishing = new JSONObject();
                        joParameters.put("Publishing", joPublishing);

                        if (task.getStartPublishing() != null)
                            joPublishing.put("StartPublishing", dateFormat.format(task.getStartPublishing()));
                        else
                            joPublishing.put("StartPublishing", "NOW");
                        if (task.getEndPublishing() != null)
                            joPublishing.put("EndPublishing", dateFormat.format(task.getEndPublishing()));
                        else
                            joPublishing.put("EndPublishing", "FOREVER");
                    }

                    if (task.getReferences() != null && !task.getReferences().equalsIgnoreCase(""))
                    {
                        JSONArray jaReferences = new JSONArray();
                        joParameters.put("References", jaReferences);

                        String [] physicalPathKeyReferences = task.getReferences().split(",");
                        for (String physicalPathKeyReference: physicalPathKeyReferences)
                        {
                            JSONObject joReference = new JSONObject();
                            joReference.put("ReferencePhysicalPathKey", Long.parseLong(physicalPathKeyReference.trim()));

                            jaReferences.put(joReference);
                        }
                    }
                }
                else if (task.getType().equalsIgnoreCase("Motion-JPEG-by-I-Frames"))
                {
                    jsonObject.put("Type", task.getType());

                    JSONObject joParameters = new JSONObject();
                    jsonObject.put("Parameters", joParameters);

                    mLogger.info("task.getType: " + task.getType());

                    if (task.getLabel() != null && !task.getLabel().equalsIgnoreCase(""))
                        jsonObject.put("Label", task.getLabel());
                    else
                    {
                        WorkflowIssue workflowIssue = new WorkflowIssue();
                        workflowIssue.setLabel("");
                        workflowIssue.setFieldName("Label");
                        workflowIssue.setTaskType(task.getType());
                        workflowIssue.setIssue("The field is not initialized");

                        workflowIssueList.add(workflowIssue);
                    }

                    if (task.getStartTimeInSeconds() != null)
                        joParameters.put("StartTimeInSeconds", task.getStartTimeInSeconds());
                    if (task.getFrameMaxFramesNumber() != null)
                        joParameters.put("MaxFramesNumber", task.getFrameMaxFramesNumber());
                    if (task.getFrameWidth() != null)
                        joParameters.put("Width", task.getFrameWidth());
                    if (task.getFrameHeight() != null)
                        joParameters.put("Height", task.getFrameHeight());
                    if (task.getUserData() != null && !task.getUserData().equalsIgnoreCase(""))
                        joParameters.put("UserData", task.getUserData());
                    if (task.getRetention() != null && !task.getRetention().equalsIgnoreCase(""))
                        joParameters.put("Retention", task.getRetention());
                    if (task.getTitle() != null && !task.getTitle().equalsIgnoreCase(""))
                        joParameters.put("Title", task.getTitle());
                    if (task.getEncodingPriority() != null && !task.getEncodingPriority().equalsIgnoreCase(""))
                        joParameters.put("EncodingPriority", task.getEncodingPriority());
                    if (task.getUniqueName() != null && !task.getUniqueName().equalsIgnoreCase(""))
                        joParameters.put("UniqueName", task.getUniqueName());
                    if (task.getIngester() != null && !task.getIngester().equalsIgnoreCase(""))
                        joParameters.put("Ingester", task.getIngester());
                    if (task.getTags() != null && !task.getTags().equalsIgnoreCase(""))
                    {
                        JSONArray jsonTagsArray = new JSONArray();
                        joParameters.put("Tags", jsonTagsArray);

                        for (String tag: task.getTags().split(","))
                        {
                            jsonTagsArray.put(tag);
                        }
                    }
                    if (task.getContentProviderName() != null && !task.getContentProviderName().equalsIgnoreCase(""))
                        joParameters.put("ContentProviderName", task.getContentProviderName());
                    if (task.getDeliveryFileName() != null && !task.getDeliveryFileName().equalsIgnoreCase(""))
                        joParameters.put("DeliveryFileName", task.getDeliveryFileName());
                    if (task.getStartPublishing() != null || task.getEndPublishing() != null)
                    {
                        JSONObject joPublishing = new JSONObject();
                        joParameters.put("Publishing", joPublishing);

                        if (task.getStartPublishing() != null)
                            joPublishing.put("StartPublishing", dateFormat.format(task.getStartPublishing()));
                        else
                            joPublishing.put("StartPublishing", "NOW");
                        if (task.getEndPublishing() != null)
                            joPublishing.put("EndPublishing", dateFormat.format(task.getEndPublishing()));
                        else
                            joPublishing.put("EndPublishing", "FOREVER");
                    }

                    if (task.getReferences() != null && !task.getReferences().equalsIgnoreCase(""))
                    {
                        JSONArray jaReferences = new JSONArray();
                        joParameters.put("References", jaReferences);

                        String [] physicalPathKeyReferences = task.getReferences().split(",");
                        for (String physicalPathKeyReference: physicalPathKeyReferences)
                        {
                            JSONObject joReference = new JSONObject();
                            joReference.put("ReferencePhysicalPathKey", Long.parseLong(physicalPathKeyReference.trim()));

                            jaReferences.put(joReference);
                        }
                    }
                }
                else if (task.getType().equalsIgnoreCase("Slideshow"))
                {
                    jsonObject.put("Type", task.getType());

                    JSONObject joParameters = new JSONObject();
                    jsonObject.put("Parameters", joParameters);

                    mLogger.info("task.getType: " + task.getType());

                    if (task.getLabel() != null && !task.getLabel().equalsIgnoreCase(""))
                        jsonObject.put("Label", task.getLabel());
                    else
                    {
                        WorkflowIssue workflowIssue = new WorkflowIssue();
                        workflowIssue.setLabel("");
                        workflowIssue.setFieldName("Label");
                        workflowIssue.setTaskType(task.getType());
                        workflowIssue.setIssue("The field is not initialized");

                        workflowIssueList.add(workflowIssue);
                    }

                    if (task.getFrameDurationOfEachSlideInSeconds() != null)
                        joParameters.put("DurationOfEachSlideInSeconds", task.getFrameDurationOfEachSlideInSeconds());
                    if (task.getFrameOutputFrameRate() != null)
                        joParameters.put("OutputFrameRate", task.getFrameOutputFrameRate());
                    if (task.getUserData() != null && !task.getUserData().equalsIgnoreCase(""))
                        joParameters.put("UserData", task.getUserData());
                    if (task.getRetention() != null && !task.getRetention().equalsIgnoreCase(""))
                        joParameters.put("Retention", task.getRetention());
                    if (task.getTitle() != null && !task.getTitle().equalsIgnoreCase(""))
                        joParameters.put("Title", task.getTitle());
                    if (task.getEncodingPriority() != null && !task.getEncodingPriority().equalsIgnoreCase(""))
                        joParameters.put("EncodingPriority", task.getEncodingPriority());
                    if (task.getUniqueName() != null && !task.getUniqueName().equalsIgnoreCase(""))
                        joParameters.put("UniqueName", task.getUniqueName());
                    if (task.getIngester() != null && !task.getIngester().equalsIgnoreCase(""))
                        joParameters.put("Ingester", task.getIngester());
                    if (task.getTags() != null && !task.getTags().equalsIgnoreCase(""))
                    {
                        JSONArray jsonTagsArray = new JSONArray();
                        joParameters.put("Tags", jsonTagsArray);

                        for (String tag: task.getTags().split(","))
                        {
                            jsonTagsArray.put(tag);
                        }
                    }
                    if (task.getContentProviderName() != null && !task.getContentProviderName().equalsIgnoreCase(""))
                        joParameters.put("ContentProviderName", task.getContentProviderName());
                    if (task.getDeliveryFileName() != null && !task.getDeliveryFileName().equalsIgnoreCase(""))
                        joParameters.put("DeliveryFileName", task.getDeliveryFileName());
                    if (task.getStartPublishing() != null || task.getEndPublishing() != null)
                    {
                        JSONObject joPublishing = new JSONObject();
                        joParameters.put("Publishing", joPublishing);

                        if (task.getStartPublishing() != null)
                            joPublishing.put("StartPublishing", dateFormat.format(task.getStartPublishing()));
                        else
                            joPublishing.put("StartPublishing", "NOW");
                        if (task.getEndPublishing() != null)
                            joPublishing.put("EndPublishing", dateFormat.format(task.getEndPublishing()));
                        else
                            joPublishing.put("EndPublishing", "FOREVER");
                    }

                    if (task.getReferences() != null && !task.getReferences().equalsIgnoreCase(""))
                    {
                        JSONArray jaReferences = new JSONArray();
                        joParameters.put("References", jaReferences);

                        String [] physicalPathKeyReferences = task.getReferences().split(",");
                        for (String physicalPathKeyReference: physicalPathKeyReferences)
                        {
                            JSONObject joReference = new JSONObject();
                            joReference.put("ReferencePhysicalPathKey", Long.parseLong(physicalPathKeyReference.trim()));

                            jaReferences.put(joReference);
                        }
                    }
                }
                else if (task.getType().equalsIgnoreCase("Encode"))
                {
                    jsonObject.put("Type", task.getType());

                    JSONObject joParameters = new JSONObject();
                    jsonObject.put("Parameters", joParameters);

                    mLogger.info("task.getType: " + task.getType());

                    jsonObject.put("Label", task.getLabel());

                    if (task.getEncodingPriority() != null && !task.getEncodingPriority().equalsIgnoreCase(""))
                        joParameters.put("EncodingPriority", task.getEncodingPriority());
                    if (task.getEncodingProfileType().equalsIgnoreCase("profilesSet"))
                    {
                        if (task.getEncodingProfilesSetLabel() != null && !task.getEncodingProfilesSetLabel().equalsIgnoreCase(""))
                            joParameters.put("EncodingProfilesSetLabel", task.getEncodingProfilesSetLabel());
                        else
                        {
                            WorkflowIssue workflowIssue = new WorkflowIssue();
                            workflowIssue.setLabel(task.getLabel());
                            workflowIssue.setFieldName("EncodingProfilesSetLabel");
                            workflowIssue.setTaskType(task.getType());
                            workflowIssue.setIssue("The field is not initialized");

                            workflowIssueList.add(workflowIssue);
                        }
                    }
                    else if (task.getEncodingProfileType().equalsIgnoreCase("singleProfile"))
                    {
                        if (task.getEncodingProfileLabel() != null && !task.getEncodingProfileLabel().equalsIgnoreCase(""))
                            joParameters.put("EncodingProfileLabel", task.getEncodingProfileLabel());
                        else
                        {
                            WorkflowIssue workflowIssue = new WorkflowIssue();
                            workflowIssue.setLabel(task.getLabel());
                            workflowIssue.setFieldName("EncodingProfileLabel");
                            workflowIssue.setTaskType(task.getType());
                            workflowIssue.setIssue("The field is not initialized");

                            workflowIssueList.add(workflowIssue);
                        }
                    }
                    else
                    {
                        mLogger.error("Unknown task.getEncodingProfileType: " + task.getEncodingProfileType());
                    }

                    if (task.getReferences() != null && !task.getReferences().equalsIgnoreCase(""))
                    {
                        JSONArray jaReferences = new JSONArray();
                        joParameters.put("References", jaReferences);

                        String [] physicalPathKeyReferences = task.getReferences().split(",");
                        for (String physicalPathKeyReference: physicalPathKeyReferences)
                        {
                            JSONObject joReference = new JSONObject();
                            joReference.put("ReferencePhysicalPathKey", Long.parseLong(physicalPathKeyReference.trim()));

                            jaReferences.put(joReference);
                        }
                    }
                }
                else if (task.getType().equalsIgnoreCase("Email-Notification"))
                {
                    jsonObject.put("Type", task.getType());

                    JSONObject joParameters = new JSONObject();
                    jsonObject.put("Parameters", joParameters);

                    mLogger.info("task.getType: " + task.getType());

                    jsonObject.put("Label", task.getLabel());

                    if (task.getEmailAddress() != null && !task.getEmailAddress().equalsIgnoreCase(""))
                        joParameters.put("EmailAddress", task.getEmailAddress());
                    else
                    {
                        WorkflowIssue workflowIssue = new WorkflowIssue();
                        workflowIssue.setLabel(task.getLabel());
                        workflowIssue.setFieldName("EmailAddress");
                        workflowIssue.setTaskType(task.getType());
                        workflowIssue.setIssue("The field is not initialized");

                        workflowIssueList.add(workflowIssue);
                    }
                    if (task.getSubject() != null && !task.getSubject().equalsIgnoreCase(""))
                        joParameters.put("Subject", task.getSubject());
                    else
                    {
                        WorkflowIssue workflowIssue = new WorkflowIssue();
                        workflowIssue.setLabel(task.getLabel());
                        workflowIssue.setFieldName("Subject");
                        workflowIssue.setTaskType(task.getType());
                        workflowIssue.setIssue("The field is not initialized");

                        workflowIssueList.add(workflowIssue);
                    }
                    if (task.getMessage() != null && !task.getMessage().equalsIgnoreCase(""))
                        joParameters.put("Message", task.getMessage());
                    else
                    {
                        WorkflowIssue workflowIssue = new WorkflowIssue();
                        workflowIssue.setLabel(task.getLabel());
                        workflowIssue.setFieldName("Message");
                        workflowIssue.setTaskType(task.getType());
                        workflowIssue.setIssue("The field is not initialized");

                        workflowIssueList.add(workflowIssue);
                    }
                }
                else if (task.getType().equalsIgnoreCase("FTP-Delivery"))
                {
                    jsonObject.put("Type", task.getType());

                    JSONObject joParameters = new JSONObject();
                    jsonObject.put("Parameters", joParameters);

                    mLogger.info("task.getType: " + task.getType());

                    if (task.getLabel() != null && !task.getLabel().equalsIgnoreCase(""))
                        jsonObject.put("Label", task.getLabel());
                    else
                    {
                        WorkflowIssue workflowIssue = new WorkflowIssue();
                        workflowIssue.setLabel("");
                        workflowIssue.setFieldName("Label");
                        workflowIssue.setTaskType(task.getType());
                        workflowIssue.setIssue("The field is not initialized");

                        workflowIssueList.add(workflowIssue);
                    }

                    if (task.getFtpDeliveryServer() != null && !task.getFtpDeliveryServer().equalsIgnoreCase(""))
                        joParameters.put("Server", task.getFtpDeliveryServer());
                    else
                    {
                        WorkflowIssue workflowIssue = new WorkflowIssue();
                        workflowIssue.setLabel(task.getLabel());
                        workflowIssue.setFieldName("Server");
                        workflowIssue.setTaskType(task.getType());
                        workflowIssue.setIssue("The field is not initialized");

                        workflowIssueList.add(workflowIssue);
                    }

                    if (task.getFtpDeliveryPort() != null)
                        joParameters.put("Port", task.getFtpDeliveryPort());

                    if (task.getFtpDeliveryUserName() != null && !task.getFtpDeliveryUserName().equalsIgnoreCase(""))
                        joParameters.put("UserName", task.getFtpDeliveryUserName());
                    else
                    {
                        WorkflowIssue workflowIssue = new WorkflowIssue();
                        workflowIssue.setLabel(task.getLabel());
                        workflowIssue.setFieldName("UserName");
                        workflowIssue.setTaskType(task.getType());
                        workflowIssue.setIssue("The field is not initialized");

                        workflowIssueList.add(workflowIssue);
                    }

                    if (task.getFtpDeliveryPassword() != null && !task.getFtpDeliveryPassword().equalsIgnoreCase(""))
                        joParameters.put("Password", task.getFtpDeliveryPassword());
                    else
                    {
                        WorkflowIssue workflowIssue = new WorkflowIssue();
                        workflowIssue.setLabel(task.getLabel());
                        workflowIssue.setFieldName("Password");
                        workflowIssue.setTaskType(task.getType());
                        workflowIssue.setIssue("The field is not initialized");

                        workflowIssueList.add(workflowIssue);
                    }

                    if (task.getFtpDeliveryRemoteDirectory() != null && !task.getFtpDeliveryRemoteDirectory().equalsIgnoreCase(""))
                        joParameters.put("RemoteDirectory", task.getFtpDeliveryRemoteDirectory());

                    if (task.getReferences() != null && !task.getReferences().equalsIgnoreCase(""))
                    {
                        JSONArray jaReferences = new JSONArray();
                        joParameters.put("References", jaReferences);

                        String [] physicalPathKeyReferences = task.getReferences().split(",");
                        for (String physicalPathKeyReference: physicalPathKeyReferences)
                        {
                            JSONObject joReference = new JSONObject();
                            joReference.put("ReferencePhysicalPathKey", Long.parseLong(physicalPathKeyReference.trim()));

                            jaReferences.put(joReference);
                        }
                    }
                }
                else if (task.getType().equalsIgnoreCase("Post-On-Facebook"))
                {
                    jsonObject.put("Type", task.getType());

                    JSONObject joParameters = new JSONObject();
                    jsonObject.put("Parameters", joParameters);

                    mLogger.info("task.getType: " + task.getType());

                    if (task.getLabel() != null && !task.getLabel().equalsIgnoreCase(""))
                        jsonObject.put("Label", task.getLabel());
                    else
                    {
                        WorkflowIssue workflowIssue = new WorkflowIssue();
                        workflowIssue.setLabel("");
                        workflowIssue.setFieldName("Label");
                        workflowIssue.setTaskType(task.getType());
                        workflowIssue.setIssue("The field is not initialized");

                        workflowIssueList.add(workflowIssue);
                    }

                    if (task.getPostOnFacebookConfigurationLabel() != null && !task.getPostOnFacebookConfigurationLabel().equalsIgnoreCase(""))
                        joParameters.put("ConfigurationLabel", task.getPostOnFacebookConfigurationLabel());
                    else
                    {
                        WorkflowIssue workflowIssue = new WorkflowIssue();
                        workflowIssue.setLabel(task.getLabel());
                        workflowIssue.setFieldName("ConfigurationLabel");
                        workflowIssue.setTaskType(task.getType());
                        workflowIssue.setIssue("The field is not initialized");

                        workflowIssueList.add(workflowIssue);
                    }

                    if (task.getPostOnFacebookNodeId() != null && !task.getPostOnFacebookNodeId().equalsIgnoreCase(""))
                        joParameters.put("NodeId", task.getPostOnFacebookNodeId());
                    else
                    {
                        WorkflowIssue workflowIssue = new WorkflowIssue();
                        workflowIssue.setLabel(task.getLabel());
                        workflowIssue.setFieldName("NodeId");
                        workflowIssue.setTaskType(task.getType());
                        workflowIssue.setIssue("The field is not initialized");

                        workflowIssueList.add(workflowIssue);
                    }

                    if (task.getReferences() != null && !task.getReferences().equalsIgnoreCase(""))
                    {
                        JSONArray jaReferences = new JSONArray();
                        joParameters.put("References", jaReferences);

                        String [] physicalPathKeyReferences = task.getReferences().split(",");
                        for (String physicalPathKeyReference: physicalPathKeyReferences)
                        {
                            JSONObject joReference = new JSONObject();
                            joReference.put("ReferencePhysicalPathKey", Long.parseLong(physicalPathKeyReference.trim()));

                            jaReferences.put(joReference);
                        }
                    }
                }
                else if (task.getType().equalsIgnoreCase("Post-On-YouTube"))
                {
                    jsonObject.put("Type", task.getType());

                    JSONObject joParameters = new JSONObject();
                    jsonObject.put("Parameters", joParameters);

                    mLogger.info("task.getType: " + task.getType());

                    if (task.getLabel() != null && !task.getLabel().equalsIgnoreCase(""))
                        jsonObject.put("Label", task.getLabel());
                    else
                    {
                        WorkflowIssue workflowIssue = new WorkflowIssue();
                        workflowIssue.setLabel("");
                        workflowIssue.setFieldName("Label");
                        workflowIssue.setTaskType(task.getType());
                        workflowIssue.setIssue("The field is not initialized");

                        workflowIssueList.add(workflowIssue);
                    }

                    if (task.getPostOnYouTubeConfigurationLabel() != null && !task.getPostOnYouTubeConfigurationLabel().equalsIgnoreCase(""))
                        joParameters.put("ConfigurationLabel", task.getPostOnYouTubeConfigurationLabel());
                    else
                    {
                        WorkflowIssue workflowIssue = new WorkflowIssue();
                        workflowIssue.setLabel(task.getLabel());
                        workflowIssue.setFieldName("ConfigurationLabel");
                        workflowIssue.setTaskType(task.getType());
                        workflowIssue.setIssue("The field is not initialized");

                        workflowIssueList.add(workflowIssue);
                    }

                    if (task.getPostOnYouTubeTitle() != null && !task.getPostOnYouTubeTitle().equalsIgnoreCase(""))
                        joParameters.put("Title", task.getPostOnYouTubeTitle());

                    if (task.getPostOnYouTubeDescription() != null && !task.getPostOnYouTubeDescription().equalsIgnoreCase(""))
                        joParameters.put("Description", task.getPostOnYouTubeDescription());

                    if (task.getPostOnYouTubeTags() != null && !task.getPostOnYouTubeTags().equalsIgnoreCase(""))
                    {
                        String[] tags = task.getPostOnYouTubeTags().split(",");

                        JSONArray jaTags = new JSONArray();
                        for (String tag: tags)
                            jaTags.put(tag);

                        joParameters.put("Tags", jaTags);
                    }

                    if (task.getPostOnYouTubeCategoryId() != null)
                        joParameters.put("CategoryId", task.getPostOnYouTubeCategoryId());

                    if (task.getPostOnYouTubePrivacy() != null && !task.getPostOnYouTubePrivacy().equalsIgnoreCase(""))
                        joParameters.put("Privacy", task.getPostOnYouTubePrivacy());

                    if (task.getReferences() != null && !task.getReferences().equalsIgnoreCase(""))
                    {
                        JSONArray jaReferences = new JSONArray();
                        joParameters.put("References", jaReferences);

                        String [] physicalPathKeyReferences = task.getReferences().split(",");
                        for (String physicalPathKeyReference: physicalPathKeyReferences)
                        {
                            JSONObject joReference = new JSONObject();
                            joReference.put("ReferencePhysicalPathKey", Long.parseLong(physicalPathKeyReference.trim()));

                            jaReferences.put(joReference);
                        }
                    }
                }
                else if (task.getType().equalsIgnoreCase("Local-Copy"))
                {
                    jsonObject.put("Type", task.getType());

                    JSONObject joParameters = new JSONObject();
                    jsonObject.put("Parameters", joParameters);

                    mLogger.info("task.getType: " + task.getType());

                    if (task.getLabel() != null && !task.getLabel().equalsIgnoreCase(""))
                        jsonObject.put("Label", task.getLabel());
                    else
                    {
                        WorkflowIssue workflowIssue = new WorkflowIssue();
                        workflowIssue.setLabel("");
                        workflowIssue.setFieldName("Label");
                        workflowIssue.setTaskType(task.getType());
                        workflowIssue.setIssue("The field is not initialized");

                        workflowIssueList.add(workflowIssue);
                    }

                    if (task.getLocalCopyLocalPath() != null && !task.getLocalCopyLocalPath().equalsIgnoreCase(""))
                        joParameters.put("LocalPath", task.getLocalCopyLocalPath());
                    else
                    {
                        WorkflowIssue workflowIssue = new WorkflowIssue();
                        workflowIssue.setLabel(task.getLabel());
                        workflowIssue.setFieldName("LocalPath");
                        workflowIssue.setTaskType(task.getType());
                        workflowIssue.setIssue("The field is not initialized");

                        workflowIssueList.add(workflowIssue);
                    }

                    if (task.getLocalCopyLocalFileName() != null && !task.getLocalCopyLocalFileName().equalsIgnoreCase(""))
                        joParameters.put("LocalFileName", task.getLocalCopyLocalFileName());

                    if (task.getReferences() != null && !task.getReferences().equalsIgnoreCase(""))
                    {
                        JSONArray jaReferences = new JSONArray();
                        joParameters.put("References", jaReferences);

                        String [] physicalPathKeyReferences = task.getReferences().split(",");
                        for (String physicalPathKeyReference: physicalPathKeyReferences)
                        {
                            JSONObject joReference = new JSONObject();
                            joReference.put("ReferencePhysicalPathKey", Long.parseLong(physicalPathKeyReference.trim()));

                            jaReferences.put(joReference);
                        }
                    }
                }
                else if (task.getType().equalsIgnoreCase("HTTP-Callback"))
                {
                    jsonObject.put("Type", task.getType());

                    JSONObject joParameters = new JSONObject();
                    jsonObject.put("Parameters", joParameters);

                    mLogger.info("task.getType: " + task.getType());

                    if (task.getLabel() != null && !task.getLabel().equalsIgnoreCase(""))
                        jsonObject.put("Label", task.getLabel());
                    else
                    {
                        WorkflowIssue workflowIssue = new WorkflowIssue();
                        workflowIssue.setLabel("");
                        workflowIssue.setFieldName("Label");
                        workflowIssue.setTaskType(task.getType());
                        workflowIssue.setIssue("The field is not initialized");

                        workflowIssueList.add(workflowIssue);
                    }

                    if (task.getHttpCallbackProtocol() != null && !task.getHttpCallbackProtocol().equalsIgnoreCase(""))
                        joParameters.put("Protocol", task.getHttpCallbackProtocol());

                    if (task.getHttpCallbackHostName() != null && !task.getHttpCallbackHostName().equalsIgnoreCase(""))
                        joParameters.put("HostName", task.getHttpCallbackHostName());
                    else
                    {
                        WorkflowIssue workflowIssue = new WorkflowIssue();
                        workflowIssue.setLabel(task.getLabel());
                        workflowIssue.setFieldName("HostName");
                        workflowIssue.setTaskType(task.getType());
                        workflowIssue.setIssue("The field is not initialized");

                        workflowIssueList.add(workflowIssue);
                    }

                    if (task.getHttpCallbackPort() != null)
                        joParameters.put("Port", task.getHttpCallbackPort());

                    if (task.getHttpCallbackURI() != null && !task.getHttpCallbackURI().equalsIgnoreCase(""))
                        joParameters.put("URI", task.getHttpCallbackURI());
                    else
                    {
                        WorkflowIssue workflowIssue = new WorkflowIssue();
                        workflowIssue.setLabel(task.getLabel());
                        workflowIssue.setFieldName("URI");
                        workflowIssue.setTaskType(task.getType());
                        workflowIssue.setIssue("The field is not initialized");

                        workflowIssueList.add(workflowIssue);
                    }

                    if (task.getHttpCallbackParameters() != null && !task.getHttpCallbackParameters().equalsIgnoreCase(""))
                        joParameters.put("Parameters", task.getHttpCallbackParameters());

                    if (task.getHttpCallbackMethod() != null && !task.getHttpCallbackMethod().equalsIgnoreCase(""))
                        joParameters.put("Method", task.getHttpCallbackMethod());

                    {
                        JSONArray jaHeaders = new JSONArray();
                        joParameters.put("Headers", jaHeaders);

                        if (task.getHttpCallbackHeaders() != null && !task.getHttpCallbackHeaders().equalsIgnoreCase(""))
                            jaHeaders.put(task.getHttpCallbackMethod());
                    }

                    if (task.getReferences() != null && !task.getReferences().equalsIgnoreCase(""))
                    {
                        JSONArray jaReferences = new JSONArray();
                        joParameters.put("References", jaReferences);

                        String [] mediaItemKeyReferences = task.getReferences().split(",");
                        for (String mediaItemKeyReference: mediaItemKeyReferences)
                        {
                            JSONObject joReference = new JSONObject();
                            joReference.put("ReferenceMediaItemKey", Long.parseLong(mediaItemKeyReference.trim()));

                            jaReferences.put(joReference);
                        }
                    }
                }
                else
                {
                    mLogger.error("Unknonw task.getType(): " + task.getType());
                }

                for (TreeNode tnEvent: tnTreeNode.getChildren())
                {
                    if (tnEvent.getType().equalsIgnoreCase("Event"))
                    {
                        Event event = (Event) tnEvent.getData();

                        JSONObject joEvent = new JSONObject();;
                        if (event.getType().equalsIgnoreCase("OnSuccess"))
                        {
                            jsonObject.put("OnSuccess", joEvent);
                        }
                        else if (event.getType().equalsIgnoreCase("OnError"))
                        {
                            jsonObject.put("OnError", joEvent);
                        }
                        else if (event.getType().equalsIgnoreCase("OnComplete"))
                        {
                            jsonObject.put("OnComplete", joEvent);
                        }
                        else
                        {
                            mLogger.error("Unknonw task.getType(): " + task.getType());
                        }

                        if (tnEvent.getChildren().size() > 0)
                            joEvent.put("Task", buildTask(tnEvent.getChildren().get(0)));
                    }
                }

                return jsonObject;
                */
        }
        catch (Exception e)
        {
            mLogger.error("buildWorkflowJson failed: " + e);

            throw e;
        }

        return jsonWorkflowElement;
    }

    public String getLabel() {
        return label;
    }

    public void setLabel(String label) {
        this.label = label;
        labelChanged = true;
    }

    public boolean isLabelChanged() {
        return labelChanged;
    }

    public void setLabelChanged(boolean labelChanged) {
        this.labelChanged = labelChanged;
    }

    public String getImage() {
        return image;
    }

    public void setImage(String image) {
        this.image = image;
    }

    public String getType() {
        return type;
    }

    public void setType(String type) {
        this.type = type;
    }

    public int getElementId() {
        return elementId;
    }

    public void setElementId(int elementId) {
        this.elementId = elementId;
    }

    public List<WorkflowProperties> getOnSuccessChildren() {
        return onSuccessChildren;
    }

    public void setOnSuccessChildren(List<WorkflowProperties> onSuccessChildren) {
        this.onSuccessChildren = onSuccessChildren;
    }

    public List<WorkflowProperties> getOnErrorChildren() {
        return onErrorChildren;
    }

    public void setOnErrorChildren(List<WorkflowProperties> onErrorChildren) {
        this.onErrorChildren = onErrorChildren;
    }

    public List<WorkflowProperties> getOnCompleteChildren() {
        return onCompleteChildren;
    }

    public void setOnCompleteChildren(List<WorkflowProperties> onCompleteChildren) {
        this.onCompleteChildren = onCompleteChildren;
    }

    public String getMainType() {
        return mainType;
    }

    public void setMainType(String mainType) {
        this.mainType = mainType;
    }

    @Override
    public String toString() {
        return label;
    }
}
