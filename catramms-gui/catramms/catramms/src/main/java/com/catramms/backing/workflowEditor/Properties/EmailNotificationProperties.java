package com.catramms.backing.workflowEditor.Properties;

import com.catramms.backing.common.SessionUtils;
import com.catramms.backing.entity.EMailConf;
import com.catramms.backing.workflowEditor.utility.WorkflowIssue;
import com.catramms.backing.workflowEditor.utility.IngestionData;
import com.catramms.utility.catramms.CatraMMS;
import org.apache.log4j.Logger;
import org.json.JSONObject;

import java.io.*;
import java.util.ArrayList;
import java.util.List;

public class EmailNotificationProperties extends WorkflowProperties implements Serializable {

    private static final Logger mLogger = Logger.getLogger(EmailNotificationProperties.class);

    private String configurationLabel;
    private List<EMailConf> confList;

    public EmailNotificationProperties(String positionX, String positionY,
                                       int elementId, String label)
    {
        super(positionX, positionY, elementId, label, "Email-Notification" + "-icon.png", "Task", "Email-Notification");

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

                confList = catraMMS.getEMailConf(username, password);
            }
        }
        catch (Exception e)
        {
            String errorMessage = "Exception: " + e;
            mLogger.error(errorMessage);
        }
    }

    public EmailNotificationProperties(String positionX, String positionY,
                                       int elementId, String label,
                                       List<EMailConf> confList)
    {
        super(positionX, positionY, elementId, label, "Email-Notification" + "-icon.png", "Task", "Email-Notification");

        this.confList = confList;
    }

    public EmailNotificationProperties clone()
    {
        EmailNotificationProperties emailNotificationProperties = new EmailNotificationProperties(
                super.getPositionX(), super.getPositionY(),
                super.getElementId(), super.getLabel(), confList);

        emailNotificationProperties.setConfigurationLabel(getConfigurationLabel());

        return emailNotificationProperties;
    }

    public void setData(EmailNotificationProperties workflowProperties)
    {
        super.setData(workflowProperties);

        setConfigurationLabel(workflowProperties.getConfigurationLabel());
    }

    public void setData(JSONObject jsonWorkflowElement)
    {
        try {
            super.setData(jsonWorkflowElement);

            setLabel(jsonWorkflowElement.getString("Label"));

            JSONObject joParameters = jsonWorkflowElement.getJSONObject("Parameters");

            if (joParameters.has("ConfigurationLabel") && !joParameters.getString("ConfigurationLabel").equalsIgnoreCase(""))
                setConfigurationLabel(joParameters.getString("ConfigurationLabel"));
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

        for (EMailConf emailConf: confList)
            configurationLabels.add(emailConf.getLabel());

        return configurationLabels;
    }

    public String getConfigurationLabel() {
        return configurationLabel;
    }

    public void setConfigurationLabel(String configurationLabel) {
        this.configurationLabel = configurationLabel;
    }
}
