package com.catramms.backing.workflowEditor.Properties;

import com.catramms.backing.newWorkflow.WorkflowIssue;
import com.catramms.backing.workflowEditor.utility.IngestionData;
import org.apache.log4j.Logger;
import org.json.JSONObject;

import java.io.Serializable;
import java.text.DateFormat;
import java.text.SimpleDateFormat;
import java.util.*;

public class LiveRecorderProperties extends CreateContentProperties implements Serializable {

    private static final Logger mLogger = Logger.getLogger(LiveRecorderProperties.class);

    private String liveURL;
    private Date startRecording;
    private Date endRecording;
    private Long segmentDuration;
    private String outputFileFormat;
    private List<String> outputFileFormatsList;
    private String encodingPriority;

    public LiveRecorderProperties(int elementId, String label)
    {
        super(elementId, label, "Live-Recorder" + "-icon.png", "Task", "Live-Recorder");

        startRecording = new Date();
        {
            Calendar calendar = Calendar.getInstance();
            calendar.setTime(startRecording);
            calendar.add(Calendar.HOUR_OF_DAY, 1);
            endRecording = calendar.getTime();
        }

        segmentDuration = new Long(15 * 60);

        {
            outputFileFormatsList = new ArrayList<>();
            outputFileFormatsList.add("ts");
        }
    }

    public LiveRecorderProperties clone()
    {
        LiveRecorderProperties liveRecorderProperties = new LiveRecorderProperties(
                super.getElementId(), super.getLabel());
        liveRecorderProperties.setOnSuccessChildren(super.getOnSuccessChildren());
        liveRecorderProperties.setOnErrorChildren(super.getOnErrorChildren());
        liveRecorderProperties.setOnCompleteChildren(super.getOnCompleteChildren());

        liveRecorderProperties.setLiveURL(liveURL);
        liveRecorderProperties.setStartRecording(startRecording);
        liveRecorderProperties.setEndRecording(endRecording);
        liveRecorderProperties.setSegmentDuration(segmentDuration);
        liveRecorderProperties.setOutputFileFormat(outputFileFormat);
        liveRecorderProperties.setEncodingPriority(encodingPriority);

        liveRecorderProperties.setTitle(getTitle());
        liveRecorderProperties.setTags(getTags());
        liveRecorderProperties.setRetention(getRetention());
        liveRecorderProperties.setStartPublishing(getStartPublishing());
        liveRecorderProperties.setEndPublishing(getEndPublishing());
        liveRecorderProperties.setUserData(getUserData());
        liveRecorderProperties.setIngester(getIngester());
        liveRecorderProperties.setContentProviderName(getContentProviderName());
        liveRecorderProperties.setDeliveryFileName(getDeliveryFileName());
        liveRecorderProperties.setUniqueName(getUniqueName());


        return liveRecorderProperties;
    }

    public JSONObject buildWorkflowElementJson(IngestionData ingestionData)
            throws Exception
    {
        JSONObject jsonWorkflowElement = new JSONObject();

        try
        {
            DateFormat dateFormat = new SimpleDateFormat("yyyy-MM-dd'T'HH:mm:ss'Z'");
            dateFormat.setTimeZone(TimeZone.getTimeZone("GMT"));

            jsonWorkflowElement.put("Type", super.getType());

            JSONObject joParameters = new JSONObject();
            jsonWorkflowElement.put("Parameters", joParameters);

            mLogger.info("task.getType: " + super.getType());
            mLogger.info("LiveURL 1: " + getLiveURL());

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

            mLogger.info("LiveURL 2: " + getLiveURL());
            if (getLiveURL() != null && !getLiveURL().equalsIgnoreCase(""))
                joParameters.put("LiveURL", getLiveURL());
            else
            {
                WorkflowIssue workflowIssue = new WorkflowIssue();
                workflowIssue.setLabel("");
                workflowIssue.setFieldName("LiveURL");
                workflowIssue.setTaskType(super.getType());
                workflowIssue.setIssue("The field is not initialized");

                ingestionData.getWorkflowIssueList().add(workflowIssue);
            }

            JSONObject joRecordingPeriod = new JSONObject();
            joParameters.put("RecordingPeriod", joRecordingPeriod);

            if (getStartRecording() != null)
                joRecordingPeriod.put("Start", dateFormat.format(getStartRecording()));
            else
            {
                WorkflowIssue workflowIssue = new WorkflowIssue();
                workflowIssue.setLabel("");
                workflowIssue.setFieldName("Start");
                workflowIssue.setTaskType(super.getType());
                workflowIssue.setIssue("The field is not initialized");

                ingestionData.getWorkflowIssueList().add(workflowIssue);
            }

            if (getEndRecording() != null)
                joRecordingPeriod.put("End", dateFormat.format(getEndRecording()));
            else
            {
                WorkflowIssue workflowIssue = new WorkflowIssue();
                workflowIssue.setLabel("");
                workflowIssue.setFieldName("End");
                workflowIssue.setTaskType(super.getType());
                workflowIssue.setIssue("The field is not initialized");

                ingestionData.getWorkflowIssueList().add(workflowIssue);
            }

            if (getSegmentDuration() != null)
                joParameters.put("SegmentDuration", getSegmentDuration());
            else
            {
                WorkflowIssue workflowIssue = new WorkflowIssue();
                workflowIssue.setLabel("");
                workflowIssue.setFieldName("SegmentDuration");
                workflowIssue.setTaskType(super.getType());
                workflowIssue.setIssue("The field is not initialized");

                ingestionData.getWorkflowIssueList().add(workflowIssue);
            }

            if (getOutputFileFormat() != null && !getOutputFileFormat().equalsIgnoreCase(""))
                joParameters.put("OutputFileFormat", getOutputFileFormat());

            if (getEncodingPriority() != null && !getEncodingPriority().equalsIgnoreCase(""))
                joParameters.put("EncodingPriority", getEncodingPriority());

            super.addCreateContentPropertiesToJson(joParameters);

            super.addEventsPropertiesToJson(jsonWorkflowElement, ingestionData);
        }
        catch (Exception e)
        {
            mLogger.error("buildWorkflowJson failed: " + e);

            throw e;
        }

        return jsonWorkflowElement;
    }

    public String getEncodingPriority() {
        return encodingPriority;
    }

    public void setEncodingPriority(String encodingPriority) {
        this.encodingPriority = encodingPriority;
    }

    public String getLiveURL() {
        return liveURL;
    }

    public void setLiveURL(String liveURL) {
        this.liveURL = liveURL;
    }

    public Date getStartRecording() {
        return startRecording;
    }

    public void setStartRecording(Date startRecording) {
        this.startRecording = startRecording;
    }

    public Date getEndRecording() {
        return endRecording;
    }

    public void setEndRecording(Date endRecording) {
        this.endRecording = endRecording;
    }

    public Long getSegmentDuration() {
        return segmentDuration;
    }

    public void setSegmentDuration(Long segmentDuration) {
        this.segmentDuration = segmentDuration;
    }

    public String getOutputFileFormat() {
        return outputFileFormat;
    }

    public void setOutputFileFormat(String outputFileFormat) {
        this.outputFileFormat = outputFileFormat;
    }

    public List<String> getOutputFileFormatsList() {
        return outputFileFormatsList;
    }

    public void setOutputFileFormatsList(List<String> outputFileFormatsList) {
        this.outputFileFormatsList = outputFileFormatsList;
    }
}
