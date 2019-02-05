package com.catramms.backing.workflowEditor.Properties;

import com.catramms.backing.common.SessionUtils;
import com.catramms.backing.entity.LiveURLConf;
import com.catramms.backing.workflowEditor.utility.WorkflowIssue;
import com.catramms.backing.workflowEditor.utility.IngestionData;
import com.catramms.utility.catramms.CatraMMS;
import org.apache.log4j.Logger;
import org.json.JSONObject;

import java.io.Serializable;
import java.text.DateFormat;
import java.text.SimpleDateFormat;
import java.util.*;

public class LiveRecorderProperties extends CreateContentProperties implements Serializable {

    private static final Logger mLogger = Logger.getLogger(LiveRecorderProperties.class);

    private String configurationLabel;
    private List<LiveURLConf> confList;
    private boolean highAvailability;
    private Date startRecording;
    private Date endRecording;
    private Long segmentDuration;
    private String outputFileFormat;
    private List<String> outputFileFormatsList;
    private String encodingPriority;

    public LiveRecorderProperties(String positionX, String positionY,
                                  int elementId, String label)
    {
        super(positionX, positionY, elementId, label, "Live-Recorder" + "-icon.png", "Task", "Live-Recorder");

        highAvailability = false;

        startRecording = new Date();
        {
            Calendar calendar = Calendar.getInstance();
            calendar.setTime(startRecording);
            calendar.add(Calendar.HOUR_OF_DAY, 1);
            endRecording = calendar.getTime();
        }

        segmentDuration = new Long(1 * 60);

        {
            outputFileFormatsList = new ArrayList<>();
            outputFileFormatsList.add("ts");
        }

        try
        {
            Long userKey = SessionUtils.getUserProfile().getUserKey();
            String apiKey = SessionUtils.getCurrentWorkspaceDetails().getApiKey();

            if (userKey == null || apiKey == null || apiKey.equalsIgnoreCase(""))
            {
                mLogger.warn("no input to require encodingProfilesSetKey"
                        + ", userKey: " + userKey
                        + ", apiKey: " + apiKey
                );
            }
            else
            {
                String username = userKey.toString();
                String password = apiKey;

                CatraMMS catraMMS = new CatraMMS();

                confList = catraMMS.getLiveURLConf(username, password);
            }
        }
        catch (Exception e)
        {
            String errorMessage = "Exception: " + e;
            mLogger.error(errorMessage);
        }
    }

    public LiveRecorderProperties(String positionX, String positionY,
                                  int elementId, String label, List<LiveURLConf> confList)
    {
        super(positionX, positionY, elementId, label, "Live-Recorder" + "-icon.png", "Task", "Live-Recorder");

        highAvailability = false;

        startRecording = new Date();
        {
            Calendar calendar = Calendar.getInstance();
            calendar.setTime(startRecording);
            calendar.add(Calendar.HOUR_OF_DAY, 1);
            endRecording = calendar.getTime();
        }

        segmentDuration = new Long(1 * 60);

        {
            outputFileFormatsList = new ArrayList<>();
            outputFileFormatsList.add("ts");
        }

        this.confList = confList;
    }

    public LiveRecorderProperties clone()
    {
        LiveRecorderProperties liveRecorderProperties = new LiveRecorderProperties(
                super.getPositionX(), super.getPositionY(),
                super.getElementId(), super.getLabel(), confList);

        liveRecorderProperties.setConfigurationLabel(configurationLabel);
        liveRecorderProperties.setHighAvailability(highAvailability);
        liveRecorderProperties.setStartRecording(startRecording);
        liveRecorderProperties.setEndRecording(endRecording);
        liveRecorderProperties.setSegmentDuration(segmentDuration);
        liveRecorderProperties.setOutputFileFormat(outputFileFormat);
        liveRecorderProperties.setEncodingPriority(encodingPriority);

        super.getData(liveRecorderProperties);


        return liveRecorderProperties;
    }

    public void setData(LiveRecorderProperties workflowProperties)
    {
        super.setData(workflowProperties);

        // mLogger.info("LiveRecorderProperties::setData");
        setConfigurationLabel(workflowProperties.getConfigurationLabel());
        setHighAvailability(workflowProperties.highAvailability);
        setStartRecording(workflowProperties.getStartRecording());
        setEndRecording(workflowProperties.getEndRecording());
        setSegmentDuration(workflowProperties.getSegmentDuration());
        setOutputFileFormat(workflowProperties.getOutputFileFormat());
        setEncodingPriority(workflowProperties.getEncodingPriority());
    }

    public void setData(JSONObject jsonWorkflowElement)
    {
        DateFormat dateFormat = new SimpleDateFormat("yyyy-MM-dd'T'HH:mm:ss'Z'");
        dateFormat.setTimeZone(TimeZone.getTimeZone("GMT"));

        try {
            super.setData(jsonWorkflowElement);

            JSONObject joParameters = jsonWorkflowElement.getJSONObject("Parameters");

            if (joParameters.has("ConfigurationLabel") && !joParameters.getString("ConfigurationLabel").equalsIgnoreCase(""))
                setConfigurationLabel(joParameters.getString("ConfigurationLabel"));
            if (joParameters.has("HighAvailability"))
                setHighAvailability(joParameters.getBoolean("HighAvailability"));

            if (joParameters.has("RecordingPeriod"))
            {
                JSONObject joRecordingPeriod = joParameters.getJSONObject("RecordingPeriod");

                if (joRecordingPeriod.has("StartRecording") && !joRecordingPeriod.getString("StartRecording").equalsIgnoreCase(""))
                    setStartRecording(dateFormat.parse(joRecordingPeriod.getString("StartRecording")));
                if (joRecordingPeriod.has("EndRecording") && !joRecordingPeriod.getString("EndRecording").equalsIgnoreCase(""))
                    setEndRecording(dateFormat.parse(joRecordingPeriod.getString("EndRecording")));
            }

            if (joParameters.has("SegmentDuration"))
                setSegmentDuration(joParameters.getLong("SegmentDuration"));
            if (joParameters.has("OutputFileFormat") && !joParameters.getString("OutputFileFormat").equalsIgnoreCase(""))
                setOutputFileFormat(joParameters.getString("OutputFileFormat"));
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
            DateFormat dateFormat = new SimpleDateFormat("yyyy-MM-dd'T'HH:mm:ss'Z'");
            dateFormat.setTimeZone(TimeZone.getTimeZone("GMT"));

            jsonWorkflowElement.put("Type", super.getType());

            JSONObject joParameters = new JSONObject();
            jsonWorkflowElement.put("Parameters", joParameters);

            mLogger.info("task.getType: " + super.getType());
            mLogger.info("ConfigurationLabel 1: " + getConfigurationLabel());

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

            mLogger.info("ConfigurationLabel 2: " + getConfigurationLabel());
            if (getConfigurationLabel() != null && !getConfigurationLabel().equalsIgnoreCase(""))
                joParameters.put("ConfigurationLabel", getConfigurationLabel());
            else
            {
                WorkflowIssue workflowIssue = new WorkflowIssue();
                workflowIssue.setLabel("");
                workflowIssue.setFieldName("ConfigurationLabel");
                workflowIssue.setTaskType(super.getType());
                workflowIssue.setIssue("The field is not initialized");

                ingestionData.getWorkflowIssueList().add(workflowIssue);
            }

            if (isHighAvailability())
                joParameters.put("HighAvailability", isHighAvailability());

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

    public List<String> getConfLabels()
    {
        List<String> configurationLabels = new ArrayList<>();

        for (LiveURLConf liveURLConf: confList)
            configurationLabels.add(liveURLConf.getLabel());

        return configurationLabels;
    }

    public String getEncodingPriority() {
        return encodingPriority;
    }

    public void setEncodingPriority(String encodingPriority) {
        this.encodingPriority = encodingPriority;
    }

    public String getConfigurationLabel() {
        return configurationLabel;
    }

    public void setConfigurationLabel(String configurationLabel) {
        this.configurationLabel = configurationLabel;
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

    public boolean isHighAvailability() {
        return highAvailability;
    }

    public void setHighAvailability(boolean highAvailability) {
        this.highAvailability = highAvailability;
    }
}
