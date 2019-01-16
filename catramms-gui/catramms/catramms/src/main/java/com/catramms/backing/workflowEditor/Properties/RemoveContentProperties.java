package com.catramms.backing.workflowEditor.Properties;

import com.catramms.backing.newWorkflow.WorkflowIssue;
import com.catramms.backing.workflowEditor.utility.IngestionData;
import org.apache.log4j.Logger;
import org.json.JSONArray;
import org.json.JSONObject;

import java.io.Serializable;

public class RemoveContentProperties extends WorkflowProperties implements Serializable {

    private static final Logger mLogger = Logger.getLogger(AddContentProperties.class);

    private StringBuilder taskReferences = new StringBuilder();

    public RemoveContentProperties(int elementId, String label)
    {
        super(elementId, label, "Remove-Content" + "-icon.png", "Task", "Remove-Content");
    }

    public RemoveContentProperties clone()
    {
        RemoveContentProperties removeContentProperties = new RemoveContentProperties(
                super.getElementId(), super.getLabel());

        removeContentProperties.setStringBuilderTaskReferences(taskReferences);

        return removeContentProperties;
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

            // OnSuccess
            {
                int onSuccessChildrenNumber = getOnSuccessChildren().size();
                mLogger.info("AddContentProperties::buildWorkflowElementJson"
                        + ", onSuccessChildrenNumber: " + onSuccessChildrenNumber
                );
                if (onSuccessChildrenNumber == 1)
                {
                    JSONObject joOnSuccess = new JSONObject();
                    jsonWorkflowElement.put("OnSuccess", joOnSuccess);

                    // Task
                    joOnSuccess.put("Task", getOnSuccessChildren().get(0).buildWorkflowElementJson(ingestionData));
                }
                else if (onSuccessChildrenNumber > 1)
                {
                    mLogger.error("It is not possible to have more than one connection"
                            + ", onSuccessChildrenNumber: " + onSuccessChildrenNumber
                    );
                }
            }

            // OnError
            {
                int onErrorChildrenNumber = getOnErrorChildren().size();
                mLogger.info("AddContentProperties::buildWorkflowElementJson"
                        + ", onErrorChildrenNumber: " + onErrorChildrenNumber
                );
                if (onErrorChildrenNumber == 1)
                {
                    JSONObject joOnError = new JSONObject();
                    jsonWorkflowElement.put("OnError", joOnError);

                    // Task
                    joOnError.put("Task", getOnErrorChildren().get(0).buildWorkflowElementJson(ingestionData));
                }
                else if (onErrorChildrenNumber > 1)
                {
                    mLogger.error("It is not possible to have more than one connection"
                            + ", onErrorChildrenNumber: " + onErrorChildrenNumber
                    );
                }
            }

            // OnComplete
            {
                int onCompleteChildrenNumber = getOnCompleteChildren().size();
                mLogger.info("AddContentProperties::buildWorkflowElementJson"
                        + ", onCompleteChildrenNumber: " + onCompleteChildrenNumber
                );
                if (onCompleteChildrenNumber == 1)
                {
                    JSONObject joOnComplete = new JSONObject();
                    jsonWorkflowElement.put("OnComplete", joOnComplete);

                    // Task
                    joOnComplete.put("Task", getOnCompleteChildren().get(0).buildWorkflowElementJson(ingestionData));
                }
                else if (onCompleteChildrenNumber > 1)
                {
                    mLogger.error("It is not possible to have more than one connection"
                            + ", onCompleteChildrenNumber: " + onCompleteChildrenNumber
                    );
                }
            }
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
