package com.catramms.backing.workflowEditor.Properties;

import com.catramms.backing.workflowEditor.utility.WorkflowIssue;
import com.catramms.backing.workflowEditor.utility.IngestionData;
import org.apache.log4j.Logger;
import org.json.JSONArray;
import org.json.JSONObject;

import java.io.Serializable;
import java.util.ArrayList;
import java.util.List;

public class HTTPCallbackProperties extends WorkflowProperties implements Serializable {

    private static final Logger mLogger = Logger.getLogger(HTTPCallbackProperties.class);

    private String protocol;
    private List<String> protocolsList;

    private String method;
    private List<String> methodsList;

    private String hostName;
    private Long port;
    private String uri;
    private String parameters;
    private String headers;

    private StringBuilder taskReferences = new StringBuilder();

    public HTTPCallbackProperties(String positionX, String positionY,
                                  int elementId, String label)
    {
        super(positionX, positionY, elementId, label, "HTTP-Callback" + "-icon.png", "Task", "HTTP-Callback");

        {
            protocolsList = new ArrayList<>();
            protocolsList.add("http");
            protocolsList.add("https");
        }

        {
            methodsList = new ArrayList<>();
            methodsList.add("POST");
            methodsList.add("GET");
        }
    }

    public HTTPCallbackProperties clone()
    {
        HTTPCallbackProperties httpCallbackProperties = new HTTPCallbackProperties(
                super.getPositionX(), super.getPositionY(),
                super.getElementId(), super.getLabel());

        httpCallbackProperties.setProtocol(getProtocol());
        httpCallbackProperties.setMethod(getMethod());
        httpCallbackProperties.setHostName(getHostName());
        httpCallbackProperties.setPort(getPort());
        httpCallbackProperties.setUri(getUri());
        httpCallbackProperties.setParameters(getParameters());
        httpCallbackProperties.setHeaders(getHeaders());

        httpCallbackProperties.setStringBuilderTaskReferences(taskReferences);

        return httpCallbackProperties;
    }

    public void setData(HTTPCallbackProperties workflowProperties)
    {
        super.setData(workflowProperties);

        setProtocol(workflowProperties.getProtocol());
        setMethod(workflowProperties.getMethod());
        setHostName(workflowProperties.getHostName());
        setPort(workflowProperties.getPort());
        setUri(workflowProperties.getUri());
        setParameters(workflowProperties.getParameters());
        setHeaders(workflowProperties.getHeaders());

        setStringBuilderTaskReferences(workflowProperties.getStringBuilderTaskReferences());
    }

    public void setData(JSONObject jsonWorkflowElement)
    {
        try {
            super.setData(jsonWorkflowElement);

            JSONObject joParameters = jsonWorkflowElement.getJSONObject("Parameters");

            if (joParameters.has("Protocol") && !joParameters.getString("Protocol").equalsIgnoreCase(""))
                setProtocol(joParameters.getString("Protocol"));
            if (joParameters.has("Method") && !joParameters.getString("Method").equalsIgnoreCase(""))
                setMethod(joParameters.getString("Method"));
            if (joParameters.has("HostName") && !joParameters.getString("HostName").equalsIgnoreCase(""))
                setHostName(joParameters.getString("HostName"));
            if (joParameters.has("Port"))
                setPort(joParameters.getLong("Port"));
            if (joParameters.has("URI") && !joParameters.getString("URI").equalsIgnoreCase(""))
                setUri(joParameters.getString("URI"));
            if (joParameters.has("Parameters") && !joParameters.getString("Parameters").equalsIgnoreCase(""))
                setParameters(joParameters.getString("Parameters"));
            if (joParameters.has("Headers") && !joParameters.getString("Headers").equalsIgnoreCase(""))
                setHeaders(joParameters.getString("Headers"));

            if (joParameters.has("References"))
            {
                String references = "";
                JSONArray jaReferences = joParameters.getJSONArray("References");
                for (int referenceIndex = 0; referenceIndex < jaReferences.length(); referenceIndex++)
                {
                    JSONObject joReference = jaReferences.getJSONObject(referenceIndex);

                    if (joReference.has("ReferenceMediaItemKey"))
                    {
                        if (references.equalsIgnoreCase(""))
                            references = new Long(joReference.getLong("ReferenceMediaItemKey")).toString();
                        else
                            references += ("," + new Long(joReference.getLong("ReferenceMediaItemKey")).toString());
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

            if (getProtocol() != null && !getProtocol().equalsIgnoreCase(""))
                joParameters.put("Protocol", getProtocol());

            if (getHostName() != null && !getHostName().equalsIgnoreCase(""))
                joParameters.put("HostName", getHostName());
            else
            {
                WorkflowIssue workflowIssue = new WorkflowIssue();
                workflowIssue.setLabel(getLabel());
                workflowIssue.setFieldName("HostName");
                workflowIssue.setTaskType(getType());
                workflowIssue.setIssue("The field is not initialized");

                ingestionData.getWorkflowIssueList().add(workflowIssue);
            }

            if (getPort() != null)
                joParameters.put("Port", getPort());

            if (getUri() != null && !getUri().equalsIgnoreCase(""))
                joParameters.put("URI", getUri());
            else
            {
                WorkflowIssue workflowIssue = new WorkflowIssue();
                workflowIssue.setLabel(getLabel());
                workflowIssue.setFieldName("URI");
                workflowIssue.setTaskType(getType());
                workflowIssue.setIssue("The field is not initialized");

                ingestionData.getWorkflowIssueList().add(workflowIssue);
            }

            if (getParameters() != null && !getParameters().equalsIgnoreCase(""))
                joParameters.put("Parameters", getParameters());

            if (getMethod() != null && !getMethod().equalsIgnoreCase(""))
                joParameters.put("Method", getMethod());

            {
                JSONArray jaHeaders = new JSONArray();
                joParameters.put("Headers", jaHeaders);

                if (getHeaders() != null && !getHeaders().equalsIgnoreCase(""))
                    jaHeaders.put(getMethod());
            }

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

    public String getProtocol() {
        return protocol;
    }

    public void setProtocol(String protocol) {
        this.protocol = protocol;
    }

    public List<String> getProtocolsList() {
        return protocolsList;
    }

    public void setProtocolsList(List<String> protocolsList) {
        this.protocolsList = protocolsList;
    }

    public String getMethod() {
        return method;
    }

    public void setMethod(String method) {
        this.method = method;
    }

    public List<String> getMethodsList() {
        return methodsList;
    }

    public void setMethodsList(List<String> methodsList) {
        this.methodsList = methodsList;
    }

    public String getHostName() {
        return hostName;
    }

    public void setHostName(String hostName) {
        this.hostName = hostName;
    }

    public Long getPort() {
        return port;
    }

    public void setPort(Long port) {
        this.port = port;
    }

    public String getUri() {
        return uri;
    }

    public void setUri(String uri) {
        this.uri = uri;
    }

    public String getParameters() {
        return parameters;
    }

    public void setParameters(String parameters) {
        this.parameters = parameters;
    }

    public String getHeaders() {
        return headers;
    }

    public void setHeaders(String headers) {
        this.headers = headers;
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
