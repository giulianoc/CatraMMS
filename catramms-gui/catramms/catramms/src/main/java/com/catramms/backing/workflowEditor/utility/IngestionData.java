package com.catramms.backing.workflowEditor.utility;

import com.catramms.backing.newWorkflow.IngestionResult;
import com.catramms.backing.newWorkflow.PushContent;
import com.catramms.backing.newWorkflow.WorkflowIssue;

import java.io.Serializable;
import java.util.ArrayList;
import java.util.List;

public class IngestionData implements Serializable {

    private String ingestWorkflowErrorMessage;
    private List<IngestionResult> ingestionJobList = new ArrayList<>();
    private List<PushContent> pushContentList = new ArrayList<>();
    private IngestionResult workflowRoot = new IngestionResult();
    private List<WorkflowIssue> workflowIssueList = new ArrayList<>();
    private String jsonWorkflow;

    public IngestionData()
    {
        // needed otherwise when the ingestionWorkflowDetails is built at the beginning will generate
        // the excetion ...at java.net.URLEncoder.encode(URLEncoder.java:204)
        workflowRoot.setKey(new Long(0));

    }

    public String getIngestWorkflowErrorMessage() {
        return ingestWorkflowErrorMessage;
    }

    public void setIngestWorkflowErrorMessage(String ingestWorkflowErrorMessage) {
        this.ingestWorkflowErrorMessage = ingestWorkflowErrorMessage;
    }

    public List<IngestionResult> getIngestionJobList() {
        return ingestionJobList;
    }

    public void setIngestionJobList(List<IngestionResult> ingestionJobList) {
        this.ingestionJobList = ingestionJobList;
    }

    public List<PushContent> getPushContentList() {
        return pushContentList;
    }

    public void setPushContentList(List<PushContent> pushContentList) {
        this.pushContentList = pushContentList;
    }

    public IngestionResult getWorkflowRoot() {
        return workflowRoot;
    }

    public void setWorkflowRoot(IngestionResult workflowRoot) {
        this.workflowRoot = workflowRoot;
    }

    public List<WorkflowIssue> getWorkflowIssueList() {
        return workflowIssueList;
    }

    public void setWorkflowIssueList(List<WorkflowIssue> workflowIssueList) {
        this.workflowIssueList = workflowIssueList;
    }

    public String getJsonWorkflow() {
        return jsonWorkflow;
    }

    public void setJsonWorkflow(String jsonWorkflow) {
        this.jsonWorkflow = jsonWorkflow;
    }
}
