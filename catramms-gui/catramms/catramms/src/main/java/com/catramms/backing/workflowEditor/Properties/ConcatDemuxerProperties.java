package com.catramms.backing.workflowEditor.Properties;

import com.catramms.backing.common.SessionUtils;
import com.catramms.backing.newWorkflow.PushContent;
import com.catramms.backing.newWorkflow.WorkflowIssue;
import com.catramms.backing.workflowEditor.utility.IngestionData;
import org.apache.commons.io.IOUtils;
import org.apache.log4j.Logger;
import org.json.JSONArray;
import org.json.JSONObject;
import org.primefaces.event.FileUploadEvent;

import java.io.*;
import java.text.DateFormat;
import java.text.SimpleDateFormat;
import java.util.ArrayList;
import java.util.Date;
import java.util.List;
import java.util.TimeZone;

public class ConcatDemuxerProperties extends CreateContentProperties implements Serializable {

    private static final Logger mLogger = Logger.getLogger(ConcatDemuxerProperties.class);

    private StringBuilder taskReferences = new StringBuilder();

    public ConcatDemuxerProperties(int elementId, String label)
    {
        super(elementId, label, "Concat-Demuxer" + "-icon.png", "Task", "Concat-Demuxer");
    }

    public ConcatDemuxerProperties clone()
    {
        ConcatDemuxerProperties concatDemuxerProperties = new ConcatDemuxerProperties(
                super.getElementId(), super.getLabel());

        concatDemuxerProperties.setTitle(getTitle());
        concatDemuxerProperties.setTags(getTags());
        concatDemuxerProperties.setRetention(getRetention());
        concatDemuxerProperties.setStartPublishing(getStartPublishing());
        concatDemuxerProperties.setEndPublishing(getEndPublishing());
        concatDemuxerProperties.setUserData(getUserData());
        concatDemuxerProperties.setIngester(getIngester());
        concatDemuxerProperties.setContentProviderName(getContentProviderName());
        concatDemuxerProperties.setDeliveryFileName(getDeliveryFileName());
        concatDemuxerProperties.setUniqueName(getUniqueName());

        concatDemuxerProperties.setStringBuilderTaskReferences(taskReferences);

        return concatDemuxerProperties;
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

            super.addCreateContentPropertiesToJson(joParameters);

            if (taskReferences != null && !taskReferences.toString().equalsIgnoreCase(""))
            {
                JSONArray jaReferences = new JSONArray();
                joParameters.put("References", jaReferences);

                String [] mediaItemKeyReferences = taskReferences.toString().split(",");
                for (String mediaItemKeyReference: mediaItemKeyReferences)
                {
                    JSONObject joReference = new JSONObject();
                    joReference.put("ReferenceMediaItemKey", Long.parseLong(mediaItemKeyReference.trim()));

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
