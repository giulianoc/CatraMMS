package com.catramms.backing.workflowEditor.Properties;

import com.catramms.backing.workflowEditor.utility.WorkflowIssue;
import com.catramms.backing.workflowEditor.utility.IngestionData;
import org.apache.log4j.Logger;
import org.json.JSONObject;

import java.io.Serializable;
import java.util.ArrayList;
import java.util.List;

/**
 * Created by multi on 11.01.19.
 */
public class WorkflowProperties implements Serializable
{

    private static final Logger mLogger = Logger.getLogger(WorkflowProperties.class);

    // common list of constants
    private List<String> fileFormatsList;
    private List<String> encodingPrioritiesList;
    private List<String> contentTypesList;
    private List<String> fontTypesList;
    List<String> fontSizesList;
    List<String> colorsList;

    private int elementId;
    private String label;
    private boolean labelChanged;
    private String image;
    private String mainType;
    private String type;
    private String positionX;
    private String positionY;

    private List<WorkflowProperties> onSuccessChildren = new ArrayList<>();
    private List<WorkflowProperties> onErrorChildren = new ArrayList<>();
    private List<WorkflowProperties> onCompleteChildren = new ArrayList<>();


    public WorkflowProperties(String positionX, String positionY,
                              int elementId, String label, String image, String mainType, String type)
    {
        this.positionX = positionX;
        this.positionY = positionY;
        this.elementId = elementId;
        this.label = label;
        this.image = image;
        this.mainType = mainType;
        this.type = type;
        labelChanged = false;

        {
            fileFormatsList = new ArrayList<>();
            fileFormatsList.add("mp4");
            fileFormatsList.add("mov");
            fileFormatsList.add("ts");
            fileFormatsList.add("wmv");
            fileFormatsList.add("mpeg");
            fileFormatsList.add("mxf");
            fileFormatsList.add("avi");
            fileFormatsList.add("webm");
            fileFormatsList.add("mp3");
            fileFormatsList.add("aac");
            fileFormatsList.add("png");
            fileFormatsList.add("jpg");
        }

        {
            encodingPrioritiesList = new ArrayList<>();
            encodingPrioritiesList.add("Low");
            encodingPrioritiesList.add("Medium");
            encodingPrioritiesList.add("High");
        }

        {
            contentTypesList = new ArrayList<>();
            contentTypesList.add("video");
            contentTypesList.add("audio");
            contentTypesList.add("image");
        }

        {
            fontTypesList = new ArrayList<>();
            fontTypesList.add("cac_champagne.ttf");
            fontTypesList.add("DancingScript-Regular.otf");
            fontTypesList.add("OpenSans-BoldItalic.ttf");
            fontTypesList.add("OpenSans-Bold.ttf");
            fontTypesList.add("OpenSans-ExtraBoldItalic.ttf");
            fontTypesList.add("OpenSans-ExtraBold.ttf");
            fontTypesList.add("OpenSans-Italic.ttf");
            fontTypesList.add("OpenSans-LightItalic.ttf");
            fontTypesList.add("OpenSans-Light.ttf");
            fontTypesList.add("OpenSans-Regular.ttf");
            fontTypesList.add("OpenSans-SemiboldItalic.ttf");
            fontTypesList.add("OpenSans-Semibold.ttf");
            fontTypesList.add("Pacifico.ttf");
            fontTypesList.add("Sofia-Regular.otf");
            fontTypesList.add("Windsong.ttf");
        }

        {
            fontSizesList = new ArrayList<>();
            fontSizesList.add("10");
            fontSizesList.add("12");
            fontSizesList.add("14");
            fontSizesList.add("18");
            fontSizesList.add("24");
            fontSizesList.add("30");
            fontSizesList.add("36");
            fontSizesList.add("48");
            fontSizesList.add("60");
        }

        {
            colorsList = new ArrayList<>();
            colorsList.add("black");
            colorsList.add("blue");
            colorsList.add("gray");
            colorsList.add("green");
            colorsList.add("orange");
            colorsList.add("purple");
            colorsList.add("red");
            colorsList.add("violet");
            colorsList.add("white");
            colorsList.add("yellow");
        }
    }


    public WorkflowProperties clone()
    {
        boolean isLabelChanged = isLabelChanged();

        WorkflowProperties workflowProperties = new WorkflowProperties(
                getPositionX(), getPositionY(),
                getElementId(), getLabel(), getImage(), getMainType(), getType());

        workflowProperties.setLabelChanged(isLabelChanged);


        return workflowProperties;
    }

    public void setData(WorkflowProperties workflowProperties)
    {
        // mLogger.info("WorkflowProperties::setData");
        setLabel(workflowProperties.getLabel());
        setPositionX(workflowProperties.getPositionX());
        setPositionY(workflowProperties.getPositionY());
    }

    public void setData(JSONObject jsonWorkflowElement)
    {
        try {
            if (jsonWorkflowElement.has("Label"))   // GroupOfTasks does not have Label
                setLabel(jsonWorkflowElement.getString("Label"));
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
            jsonWorkflowElement.put("Type", getType());

            if (label != null && !label.equalsIgnoreCase(""))
                jsonWorkflowElement.put("Label", label);
            else
            {
                WorkflowIssue workflowIssue = new WorkflowIssue();
                workflowIssue.setLabel("");
                workflowIssue.setFieldName("Label");
                workflowIssue.setTaskType("Workflow");
                workflowIssue.setIssue("The field is not initialized");

                ingestionData.getWorkflowIssueList().add(workflowIssue);
            }

            int onSuccessChildrenNumber = onSuccessChildren.size();
            mLogger.info("WorkflowProperties::buildWorkflowElementJson"
                    + ", onSuccessChildrenNumber: " + onSuccessChildrenNumber
            );
            // inside this object (WorkflowProperties) we can have just one child
            if (onSuccessChildrenNumber == 1)
            {
                // Task
                jsonWorkflowElement.put("Task", onSuccessChildren.get(0).buildWorkflowElementJson(ingestionData));
            }
            else if (onSuccessChildrenNumber > 1)
            {
                mLogger.error("It is not possible to have more than one connection"
                        + ", onSuccessChildrenNumber: " + onSuccessChildrenNumber
                );
            }
        }
        catch (Exception e)
        {
            mLogger.error("buildWorkflowJson failed: " + e);

            throw e;
        }

        return jsonWorkflowElement;
    }

    public void addEventsPropertiesToJson(JSONObject jsonWorkflowElement, IngestionData ingestionData)
            throws Exception
    {
        try {
            // OnSuccess
            {
                int onSuccessChildrenNumber = getOnSuccessChildren().size();
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
            mLogger.error("addEventsPropertiesToJson failed: " + e);

            throw e;
        }
    }

    @Override
    public String toString() {
        return label;
    }

    public String getLabel() {
        return label;
    }

    public void setLabel(String label) {
        if (!this.label.equals(label))
        {
            this.label = label;
            labelChanged = true;
        }
    }

    public boolean isLabelChanged() {
        return labelChanged;
    }

    public void setLabelChanged(boolean labelChanged) {
        this.labelChanged = labelChanged;
    }

    public String getImage() {
        return image;
    }

    public void setImage(String image) {
        this.image = image;
    }

    public String getType() {
        return type;
    }

    public void setType(String type) {
        this.type = type;
    }

    public int getElementId() {
        return elementId;
    }

    public void setElementId(int elementId) {
        this.elementId = elementId;
    }

    public List<WorkflowProperties> getOnSuccessChildren() {
        return onSuccessChildren;
    }

    public void setOnSuccessChildren(List<WorkflowProperties> onSuccessChildren) {
        this.onSuccessChildren = onSuccessChildren;
    }

    public List<WorkflowProperties> getOnErrorChildren() {
        return onErrorChildren;
    }

    public void setOnErrorChildren(List<WorkflowProperties> onErrorChildren) {
        this.onErrorChildren = onErrorChildren;
    }

    public List<WorkflowProperties> getOnCompleteChildren() {
        return onCompleteChildren;
    }

    public void setOnCompleteChildren(List<WorkflowProperties> onCompleteChildren) {
        this.onCompleteChildren = onCompleteChildren;
    }

    public String getMainType() {
        return mainType;
    }

    public void setMainType(String mainType) {
        this.mainType = mainType;
    }

    public List<String> getFileFormatsList() {
        return fileFormatsList;
    }

    public void setFileFormatsList(List<String> fileFormatsList) {
        this.fileFormatsList = fileFormatsList;
    }

    public List<String> getEncodingPrioritiesList() {
        return encodingPrioritiesList;
    }

    public void setEncodingPrioritiesList(List<String> encodingPrioritiesList) {
        this.encodingPrioritiesList = encodingPrioritiesList;
    }

    public List<String> getContentTypesList() {
        return contentTypesList;
    }

    public void setContentTypesList(List<String> contentTypesList) {
        this.contentTypesList = contentTypesList;
    }

    public List<String> getFontTypesList() {
        return fontTypesList;
    }

    public void setFontTypesList(List<String> fontTypesList) {
        this.fontTypesList = fontTypesList;
    }

    public List<String> getFontSizesList() {
        return fontSizesList;
    }

    public void setFontSizesList(List<String> fontSizesList) {
        this.fontSizesList = fontSizesList;
    }

    public List<String> getColorsList() {
        return colorsList;
    }

    public void setColorsList(List<String> colorsList) {
        this.colorsList = colorsList;
    }

    public String getPositionX() {
        return positionX;
    }

    public void setPositionX(String positionX) {
        this.positionX = positionX;
    }

    public String getPositionY() {
        return positionY;
    }

    public void setPositionY(String positionY) {
        this.positionY = positionY;
    }
}
