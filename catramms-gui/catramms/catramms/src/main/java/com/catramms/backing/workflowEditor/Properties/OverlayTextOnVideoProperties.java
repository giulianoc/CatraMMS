package com.catramms.backing.workflowEditor.Properties;

import com.catramms.backing.workflowEditor.utility.WorkflowIssue;
import com.catramms.backing.workflowEditor.utility.IngestionData;
import org.apache.log4j.Logger;
import org.json.JSONArray;
import org.json.JSONObject;

import java.io.Serializable;

public class OverlayTextOnVideoProperties extends CreateContentProperties implements Serializable {

    private static final Logger mLogger = Logger.getLogger(OverlayTextOnVideoProperties.class);

    private String text;
    private String positionXInPixel;
    private String positionYInPixel;
    private String fontType;
    private String fontSize; //it's String because I need taskFontSizesList as String
    private String fontColor;
    private Long textPercentageOpacity;
    private Boolean boxEnable;
    private String boxColor;
    private Long boxPercentageOpacity;
    private String encodingPriority;

    private StringBuilder taskReferences = new StringBuilder();

    public OverlayTextOnVideoProperties(String positionX, String positionY,
                                        int elementId, String label)
    {
        super(positionX, positionY, elementId, label, "Overlay-Text-On-Video" + "-icon.png", "Task", "Overlay-Text-On-Video");
    }

    public OverlayTextOnVideoProperties clone()
    {
        OverlayTextOnVideoProperties overlayTextOnVideoProperties = new OverlayTextOnVideoProperties(
                super.getPositionX(), super.getPositionY(),
                super.getElementId(), super.getLabel());

        overlayTextOnVideoProperties.setText(getText());
        overlayTextOnVideoProperties.setPositionXInPixel(getPositionXInPixel());
        overlayTextOnVideoProperties.setPositionYInPixel(getPositionYInPixel());
        overlayTextOnVideoProperties.setFontType(getFontType());
        overlayTextOnVideoProperties.setFontSize(getFontSize());
        overlayTextOnVideoProperties.setFontColor(getFontColor());
        overlayTextOnVideoProperties.setTextPercentageOpacity(getTextPercentageOpacity());
        overlayTextOnVideoProperties.setBoxEnable(getBoxEnable());
        overlayTextOnVideoProperties.setBoxColor(getBoxColor());
        overlayTextOnVideoProperties.setBoxPercentageOpacity(getBoxPercentageOpacity());
        overlayTextOnVideoProperties.setEncodingPriority(getEncodingPriority());

        overlayTextOnVideoProperties.setStringBuilderTaskReferences(taskReferences);

        super.getData(overlayTextOnVideoProperties);


        return overlayTextOnVideoProperties;
    }

    public void setData(OverlayTextOnVideoProperties workflowProperties)
    {
        super.setData(workflowProperties);

        setText(workflowProperties.getText());
        setPositionXInPixel(workflowProperties.getPositionXInPixel());
        setPositionYInPixel(workflowProperties.getPositionYInPixel());
        setFontType(workflowProperties.getFontType());
        setFontSize(workflowProperties.getFontSize());
        setFontColor(workflowProperties.getFontColor());
        setTextPercentageOpacity(workflowProperties.getTextPercentageOpacity());
        setBoxEnable(workflowProperties.getBoxEnable());
        setBoxColor(workflowProperties.getBoxColor());
        setBoxPercentageOpacity(workflowProperties.getBoxPercentageOpacity());
        setEncodingPriority(workflowProperties.getEncodingPriority());

        setStringBuilderTaskReferences(workflowProperties.getStringBuilderTaskReferences());
    }

    public void setData(JSONObject jsonWorkflowElement)
    {
        try {
            super.setData(jsonWorkflowElement);

            setLabel(jsonWorkflowElement.getString("Label"));

            JSONObject joParameters = jsonWorkflowElement.getJSONObject("Parameters");

            if (joParameters.has("Text") && !joParameters.getString("Text").equalsIgnoreCase(""))
                setText(joParameters.getString("Text"));
            if (joParameters.has("ImagePosition_X_InPixel") && !joParameters.getString("ImagePosition_X_InPixel").equalsIgnoreCase(""))
                setPositionXInPixel(joParameters.getString("ImagePosition_X_InPixel"));
            if (joParameters.has("ImagePosition_Y_InPixel") && !joParameters.getString("ImagePosition_Y_InPixel").equalsIgnoreCase(""))
                setPositionYInPixel(joParameters.getString("ImagePosition_Y_InPixel"));
            if (joParameters.has("FontType") && !joParameters.getString("FontType").equalsIgnoreCase(""))
                setFontType(joParameters.getString("FontType"));
            if (joParameters.has("FontSize") && !joParameters.getString("FontSize").equalsIgnoreCase(""))
                setFontSize(joParameters.getString("FontSize"));
            if (joParameters.has("FontColor") && !joParameters.getString("FontColor").equalsIgnoreCase(""))
                setFontColor(joParameters.getString("FontColor"));
            if (joParameters.has("TextPercentageOpacity"))
                setTextPercentageOpacity(joParameters.getLong("TextPercentageOpacity"));
            if (joParameters.has("BoxEnable"))
                setBoxEnable(joParameters.getBoolean("BoxEnable"));
            if (joParameters.has("BoxColor") && !joParameters.getString("BoxColor").equalsIgnoreCase(""))
                setBoxColor(joParameters.getString("BoxColor"));
            if (joParameters.has("BoxPercentageOpacity"))
                setBoxPercentageOpacity(joParameters.getLong("BoxPercentageOpacity"));
            if (joParameters.has("EncodingPriority") && !joParameters.getString("EncodingPriority").equalsIgnoreCase(""))
                setEncodingPriority(joParameters.getString("EncodingPriority"));

            if (joParameters.has("References"))
            {
                String references = "";
                JSONArray jaReferences = joParameters.getJSONArray("References");
                for (int referenceIndex = 0; referenceIndex < jaReferences.length(); referenceIndex++)
                {
                    JSONObject joReference = jaReferences.getJSONObject(referenceIndex);

                    if (joReference.has("ReferencePhysicalPathKey"))
                    {
                        if (references.equalsIgnoreCase(""))
                            references = new Long(joReference.getLong("ReferencePhysicalPathKey")).toString();
                        else
                            references += ("," + new Long(joReference.getLong("ReferencePhysicalPathKey")).toString());
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

            if (getText() != null && !getText().equalsIgnoreCase(""))
                joParameters.put("Text", getText());
            else
            {
                WorkflowIssue workflowIssue = new WorkflowIssue();
                workflowIssue.setLabel(getLabel());
                workflowIssue.setFieldName("Text");
                workflowIssue.setTaskType(getType());
                workflowIssue.setIssue("The field is not initialized");

                ingestionData.getWorkflowIssueList().add(workflowIssue);
            }

            if (getPositionXInPixel() != null && !getPositionXInPixel().equalsIgnoreCase(""))
                joParameters.put("TextPosition_X_InPixel", getPositionXInPixel());
            if (getPositionYInPixel() != null && !getPositionYInPixel().equalsIgnoreCase(""))
                joParameters.put("TextPosition_Y_InPixel", getPositionYInPixel());
            if (getFontType() != null && !getFontType().equalsIgnoreCase(""))
                joParameters.put("FontType", getFontType());
            if (getFontSize() != null)
                joParameters.put("FontSize", getFontSize());
            if (getFontColor() != null && !getFontColor().equalsIgnoreCase(""))
                joParameters.put("FontColor", getFontColor());
            if (getTextPercentageOpacity() != null)
                joParameters.put("TextPercentageOpacity", getTextPercentageOpacity());
            if (getBoxColor() != null)
                joParameters.put("BoxEnable", getBoxEnable());
            if (getBoxColor() != null && !getBoxColor().equalsIgnoreCase(""))
                joParameters.put("BoxColor", getBoxColor());
            if (getBoxPercentageOpacity() != null)
                joParameters.put("BoxPercentageOpacity", getBoxPercentageOpacity());
            if (getEncodingPriority() != null && !getEncodingPriority().equalsIgnoreCase(""))
                joParameters.put("EncodingPriority", getEncodingPriority());

            super.addCreateContentPropertiesToJson(joParameters);

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

    public String getPositionXInPixel() {
        return positionXInPixel;
    }

    public void setPositionXInPixel(String positionXInPixel) {
        this.positionXInPixel = positionXInPixel;
    }

    public String getPositionYInPixel() {
        return positionYInPixel;
    }

    public void setPositionYInPixel(String positionYInPixel) {
        this.positionYInPixel = positionYInPixel;
    }

    public String getEncodingPriority() {
        return encodingPriority;
    }

    public void setEncodingPriority(String encodingPriority) {
        this.encodingPriority = encodingPriority;
    }

    public String getText() {
        return text;
    }

    public void setText(String text) {
        this.text = text;
    }

    public String getFontType() {
        return fontType;
    }

    public void setFontType(String fontType) {
        this.fontType = fontType;
    }

    public String getFontSize() {
        return fontSize;
    }

    public void setFontSize(String fontSize) {
        this.fontSize = fontSize;
    }

    public String getFontColor() {
        return fontColor;
    }

    public void setFontColor(String fontColor) {
        this.fontColor = fontColor;
    }

    public Long getTextPercentageOpacity() {
        return textPercentageOpacity;
    }

    public void setTextPercentageOpacity(Long textPercentageOpacity) {
        this.textPercentageOpacity = textPercentageOpacity;
    }

    public Boolean getBoxEnable() {
        return boxEnable;
    }

    public void setBoxEnable(Boolean boxEnable) {
        this.boxEnable = boxEnable;
    }

    public String getBoxColor() {
        return boxColor;
    }

    public void setBoxColor(String boxColor) {
        this.boxColor = boxColor;
    }

    public Long getBoxPercentageOpacity() {
        return boxPercentageOpacity;
    }

    public void setBoxPercentageOpacity(Long boxPercentageOpacity) {
        this.boxPercentageOpacity = boxPercentageOpacity;
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
