package com.catramms.backing.workflowEditor.Properties;

import com.catramms.backing.workflowEditor.utility.IngestionData;
import org.apache.log4j.Logger;
import org.json.JSONArray;
import org.json.JSONObject;

import java.io.*;
import java.util.ArrayList;
import java.util.List;

public class GroupOfTasksProperties extends WorkflowProperties implements Serializable {

    private static final Logger mLogger = Logger.getLogger(GroupOfTasksProperties.class);

    private String groupOfTaskExecutionType;
    private List<WorkflowProperties> tasks = new ArrayList<>();

    public GroupOfTasksProperties(int elementId, String label)
    {
        super(elementId, label, "GroupOfTasks" + "-icon.png", "GroupOfTasks", "GroupOfTasks");

        groupOfTaskExecutionType = "parallel";
    }

    public GroupOfTasksProperties clone()
    {
        String localGroupOfTaskExecutionType = getGroupOfTaskExecutionType();

        GroupOfTasksProperties groupOfTasksProperties = new GroupOfTasksProperties(
                super.getElementId(), super.getLabel());
        groupOfTasksProperties.setOnSuccessChildren(super.getOnSuccessChildren());
        groupOfTasksProperties.setOnErrorChildren(super.getOnErrorChildren());
        groupOfTasksProperties.setOnCompleteChildren(super.getOnCompleteChildren());

        groupOfTasksProperties.setGroupOfTaskExecutionType(localGroupOfTaskExecutionType);
        groupOfTasksProperties.setTasks(getTasks());

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

            super.addEventsPropertiesToJson(jsonWorkflowElement, ingestionData);
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
