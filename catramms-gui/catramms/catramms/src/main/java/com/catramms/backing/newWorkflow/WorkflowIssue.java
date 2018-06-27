package com.catramms.backing.newWorkflow;

import java.io.Serializable;

/**
 * Created by multi on 13.06.18.
 */
public class WorkflowIssue implements Serializable {
    private String label;
    private String taskType;
    private String fieldName;
    private String issue;

    public String getLabel() {
        return label;
    }

    public void setLabel(String label) {
        this.label = label;
    }

    public String getTaskType() {
        return taskType;
    }

    public void setTaskType(String taskType) {
        this.taskType = taskType;
    }

    public String getFieldName() {
        return fieldName;
    }

    public void setFieldName(String fieldName) {
        this.fieldName = fieldName;
    }

    public String getIssue() {
        return issue;
    }

    public void setIssue(String issue) {
        this.issue = issue;
    }
}
