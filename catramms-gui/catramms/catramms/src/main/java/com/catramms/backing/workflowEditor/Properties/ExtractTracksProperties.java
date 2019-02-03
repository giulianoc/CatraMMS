package com.catramms.backing.workflowEditor.Properties;

import com.catramms.backing.workflowEditor.utility.WorkflowIssue;
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

    public ExtractTracksProperties(String positionX, String positionY,
                                   int elementId, String label)
    {
        super(positionX, positionY, elementId, label, "Extract-Tracks" + "-icon.png", "Task", "Extract-Tracks");
    }

    public ExtractTracksProperties clone()
    {
        ExtractTracksProperties extractTracksProperties = new ExtractTracksProperties(
                super.getPositionX(), super.getPositionY(),
                super.getElementId(), super.getLabel());

        extractTracksProperties.setFileFormat(fileFormat);
        extractTracksProperties.setVideoTrackNumber(videoTrackNumber);
        extractTracksProperties.setAudioTrackNumber(audioTrackNumber);

        extractTracksProperties.setStringBuilderTaskReferences(taskReferences);

        super.getData(extractTracksProperties);


        return extractTracksProperties;
    }

    public void setData(ExtractTracksProperties workflowProperties)
    {
        super.setData(workflowProperties);

        setFileFormat(workflowProperties.getFileFormat());
        setVideoTrackNumber(workflowProperties.getVideoTrackNumber());
        setAudioTrackNumber(workflowProperties.getAudioTrackNumber());

        setStringBuilderTaskReferences(workflowProperties.getStringBuilderTaskReferences());
    }

    public void setData(JSONObject jsonWorkflowElement)
    {
        try {
            super.setData(jsonWorkflowElement);

            JSONObject joParameters = jsonWorkflowElement.getJSONObject("Parameters");

            if (joParameters.has("FileFormat") && !joParameters.getString("FileFormat").equalsIgnoreCase(""))
                setFileFormat(joParameters.getString("FileFormat"));
            if (joParameters.has("VideoTrackNumber"))
                setVideoTrackNumber(joParameters.getLong("VideoTrackNumber"));
            if (joParameters.has("AudioTrackNumber"))
                setAudioTrackNumber(joParameters.getLong("AudioTrackNumber"));

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
