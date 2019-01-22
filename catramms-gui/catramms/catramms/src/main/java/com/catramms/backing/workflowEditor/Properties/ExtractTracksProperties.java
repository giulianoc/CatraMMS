package com.catramms.backing.workflowEditor.Properties;

import com.catramms.backing.newWorkflow.WorkflowIssue;
import com.catramms.backing.workflowEditor.utility.IngestionData;
import org.apache.log4j.Logger;
import org.json.JSONArray;
import org.json.JSONObject;

import java.io.Serializable;

public class ExtractTracksProperties extends CreateContentProperties implements Serializable {

    private static final Logger mLogger = Logger.getLogger(ExtractTracksProperties.class);

    private String fileFormat;
    private Long videoTrackNumber;
    private Long audioTrackNumber;

    private StringBuilder taskReferences = new StringBuilder();

    public ExtractTracksProperties(int elementId, String label)
    {
        super(elementId, label, "Extract-Tracks" + "-icon.png", "Task", "Extract-Tracks");
    }

    public ExtractTracksProperties clone()
    {
        ExtractTracksProperties extractTracksProperties = new ExtractTracksProperties(
                super.getElementId(), super.getLabel());
        extractTracksProperties.setOnSuccessChildren(super.getOnSuccessChildren());
        extractTracksProperties.setOnErrorChildren(super.getOnErrorChildren());
        extractTracksProperties.setOnCompleteChildren(super.getOnCompleteChildren());

        extractTracksProperties.setTitle(getTitle());
        extractTracksProperties.setTags(getTags());
        extractTracksProperties.setRetention(getRetention());
        extractTracksProperties.setStartPublishing(getStartPublishing());
        extractTracksProperties.setEndPublishing(getEndPublishing());
        extractTracksProperties.setUserData(getUserData());
        extractTracksProperties.setIngester(getIngester());
        extractTracksProperties.setContentProviderName(getContentProviderName());
        extractTracksProperties.setDeliveryFileName(getDeliveryFileName());
        extractTracksProperties.setUniqueName(getUniqueName());

        extractTracksProperties.setFileFormat(fileFormat);
        extractTracksProperties.setVideoTrackNumber(videoTrackNumber);
        extractTracksProperties.setAudioTrackNumber(audioTrackNumber);

        extractTracksProperties.setStringBuilderTaskReferences(taskReferences);

        return extractTracksProperties;
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

            joParameters.put("OutputFileFormat", getFileFormat());
            {
                JSONArray jaTracks = new JSONArray();
                joParameters.put("Tracks", jaTracks);

                if (getVideoTrackNumber() != null)
                {
                    JSONObject joTrack = new JSONObject();
                    jaTracks.put(joTrack);
                    joTrack.put("TrackType", "video");
                    joTrack.put("TrackNumber", getVideoTrackNumber());
                }

                if (getAudioTrackNumber() != null)
                {
                    JSONObject joTrack = new JSONObject();
                    jaTracks.put(joTrack);
                    joTrack.put("TrackType", "audio");
                    joTrack.put("TrackNumber", getAudioTrackNumber());
                }
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

    public Long getVideoTrackNumber() {
        return videoTrackNumber;
    }

    public void setVideoTrackNumber(Long videoTrackNumber) {
        this.videoTrackNumber = videoTrackNumber;
    }

    public Long getAudioTrackNumber() {
        return audioTrackNumber;
    }

    public void setAudioTrackNumber(Long audioTrackNumber) {
        this.audioTrackNumber = audioTrackNumber;
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
