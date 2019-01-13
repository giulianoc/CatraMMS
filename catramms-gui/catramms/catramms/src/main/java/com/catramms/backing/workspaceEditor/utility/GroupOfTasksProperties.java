package com.catramms.backing.workspaceEditor.utility;

import com.catramms.backing.common.SessionUtils;
import com.catramms.backing.newWorkflow.PushContent;
import com.catramms.backing.newWorkflow.WorkflowIssue;
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

public class GroupOfTasksProperties extends WorkflowProperties implements Serializable {

    private static final Logger mLogger = Logger.getLogger(GroupOfTasksProperties.class);

    private String groupOfTaskExecutionType;
    private List<WorkflowProperties> tasks = new ArrayList<>();

    public GroupOfTasksProperties(int elementId, String label,
                                  String labelTemplatePrefix, String temporaryPushBinariesPathName)
    {
        super(elementId, label, "GroupOfTasks" + "-icon.png", "GroupOfTasks", "GroupOfTasks");
    }

    public GroupOfTasksProperties clone()
    {
        GroupOfTasksProperties groupOfTasksProperties = new GroupOfTasksProperties(
                super.getElementId(), super.getLabel(), super.getImage(), super.getType());

        groupOfTasksProperties.setGroupOfTaskExecutionType(groupOfTaskExecutionType);

        return groupOfTasksProperties;
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

            joParameters.put("ExecutionType", getGroupOfTaskExecutionType());

            JSONArray jaTasks = new JSONArray();
            joParameters.put("Tasks", jaTasks);

            // tasks
            {
                int tasksNumber = getTasks().size();
                mLogger.info("GroupOfTasksProperties::buildWorkflowElementJson"
                        + ", tasksNumber: " + tasksNumber
                );
                for (int taskIndex = 0; taskIndex < tasksNumber; taskIndex++)
                {
                    WorkflowProperties taskWorkflowProperties = getTasks().get(taskIndex);

                    jaTasks.put(taskWorkflowProperties.buildWorkflowElementJson(ingestionData));
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

    public String getGroupOfTaskExecutionType() {
        return groupOfTaskExecutionType;
    }

    public void setGroupOfTaskExecutionType(String groupOfTaskExecutionType) {
        this.groupOfTaskExecutionType = groupOfTaskExecutionType;
    }

    public List<WorkflowProperties> getTasks() {
        return tasks;
    }

    public void setTasks(List<WorkflowProperties> tasks) {
        this.tasks = tasks;
    }
}
