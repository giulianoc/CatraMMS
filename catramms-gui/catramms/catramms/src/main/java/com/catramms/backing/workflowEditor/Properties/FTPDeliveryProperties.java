package com.catramms.backing.workflowEditor.Properties;

import com.catramms.backing.newWorkflow.WorkflowIssue;
import com.catramms.backing.workflowEditor.utility.IngestionData;
import org.apache.log4j.Logger;
import org.json.JSONArray;
import org.json.JSONObject;

import java.io.Serializable;

public class FTPDeliveryProperties extends WorkflowProperties implements Serializable {

    private static final Logger mLogger = Logger.getLogger(FTPDeliveryProperties.class);

    private String server;
    private Long port;
    private String userName;
    private String password;
    private String remoteDirectory;

    private StringBuilder taskReferences = new StringBuilder();

    public FTPDeliveryProperties(int elementId, String label)
    {
        super(elementId, label, "FTP-Delivery" + "-icon.png", "Task", "FTP-Delivery");
    }

    public FTPDeliveryProperties clone()
    {
        FTPDeliveryProperties ftpDeliveryProperties = new FTPDeliveryProperties(
                super.getElementId(), super.getLabel());
        ftpDeliveryProperties.setOnSuccessChildren(super.getOnSuccessChildren());
        ftpDeliveryProperties.setOnErrorChildren(super.getOnErrorChildren());
        ftpDeliveryProperties.setOnCompleteChildren(super.getOnCompleteChildren());

        ftpDeliveryProperties.setServer(getServer());
        ftpDeliveryProperties.setPort(getPort());
        ftpDeliveryProperties.setUserName(getUserName());
        ftpDeliveryProperties.setPassword(getPassword());
        ftpDeliveryProperties.setRemoteDirectory(getRemoteDirectory());

        ftpDeliveryProperties.setStringBuilderTaskReferences(taskReferences);

        return ftpDeliveryProperties;
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

            if (getServer() != null && !getServer().equalsIgnoreCase(""))
                joParameters.put("Server", getServer());
            else
            {
                WorkflowIssue workflowIssue = new WorkflowIssue();
                workflowIssue.setLabel(getLabel());
                workflowIssue.setFieldName("Server");
                workflowIssue.setTaskType(getType());
                workflowIssue.setIssue("The field is not initialized");

                ingestionData.getWorkflowIssueList().add(workflowIssue);
            }

            if (getPort() != null)
                joParameters.put("Port", getPort());

            if (getUserName() != null && !getUserName().equalsIgnoreCase(""))
                joParameters.put("UserName", getUserName());
            else
            {
                WorkflowIssue workflowIssue = new WorkflowIssue();
                workflowIssue.setLabel(getLabel());
                workflowIssue.setFieldName("UserName");
                workflowIssue.setTaskType(getType());
                workflowIssue.setIssue("The field is not initialized");

                ingestionData.getWorkflowIssueList().add(workflowIssue);
            }

            if (getPassword() != null && !getPassword().equalsIgnoreCase(""))
                joParameters.put("Password", getPassword());
            else
            {
                WorkflowIssue workflowIssue = new WorkflowIssue();
                workflowIssue.setLabel(getLabel());
                workflowIssue.setFieldName("Password");
                workflowIssue.setTaskType(getType());
                workflowIssue.setIssue("The field is not initialized");

                ingestionData.getWorkflowIssueList().add(workflowIssue);
            }

            if (getRemoteDirectory() != null && !getRemoteDirectory().equalsIgnoreCase(""))
                joParameters.put("RemoteDirectory", getRemoteDirectory());

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

    public String getServer() {
        return server;
    }

    public void setServer(String server) {
        this.server = server;
    }

    public Long getPort() {
        return port;
    }

    public void setPort(Long port) {
        this.port = port;
    }

    public String getUserName() {
        return userName;
    }

    public void setUserName(String userName) {
        this.userName = userName;
    }

    public String getPassword() {
        return password;
    }

    public void setPassword(String password) {
        this.password = password;
    }

    public String getRemoteDirectory() {
        return remoteDirectory;
    }

    public void setRemoteDirectory(String remoteDirectory) {
        this.remoteDirectory = remoteDirectory;
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
