package com.catramms.backing.workflowEditor.Properties;

import com.catramms.backing.common.SessionUtils;
import org.apache.log4j.Logger;
import org.json.JSONArray;
import org.json.JSONObject;

import java.io.Serializable;
import java.text.DateFormat;
import java.text.SimpleDateFormat;
import java.util.Date;
import java.util.TimeZone;

public class CreateContentProperties extends WorkflowProperties implements Serializable {

    private static final Logger mLogger = Logger.getLogger(CreateContentProperties.class);

    private String title;
    private String tags;
    private String retention;
    private Date startPublishing;
    private Date endPublishing;
    private String userData;
    private String ingester;
    private String contentProviderName;
    private String deliveryFileName;
    private String uniqueName;

    public CreateContentProperties(String positionX, String positionY,
                                   int elementId, String label, String image, String mainType, String type)
    {
        super(positionX, positionY, elementId, label, image, mainType, type);

        setIngester(SessionUtils.getUserProfile().getName());
    }

    public void getData(CreateContentProperties workflowProperties)
    {
        workflowProperties.setTitle(getTitle());
        workflowProperties.setTags(getTags());
        workflowProperties.setRetention(getRetention());
        workflowProperties.setStartPublishing(getStartPublishing());
        workflowProperties.setEndPublishing(getEndPublishing());
        workflowProperties.setUserData(getUserData());
        workflowProperties.setIngester(getIngester());
        workflowProperties.setContentProviderName(getContentProviderName());
        workflowProperties.setDeliveryFileName(getDeliveryFileName());
        workflowProperties.setUniqueName(getUniqueName());
    }

    public void setData(CreateContentProperties workflowProperties)
    {
        super.setData(workflowProperties);

        // mLogger.info("CreateContentProperties::setData");

        setTitle(workflowProperties.getTitle());
        setTags(workflowProperties.getTags());
        setRetention(workflowProperties.getRetention());
        setStartPublishing(workflowProperties.getStartPublishing());
        setEndPublishing(workflowProperties.getEndPublishing());
        setUserData(workflowProperties.getUserData());
        setIngester(workflowProperties.getIngester());
        setContentProviderName(workflowProperties.getContentProviderName());
        setDeliveryFileName(workflowProperties.getDeliveryFileName());
        setUniqueName(workflowProperties.getUniqueName());
    }

    public void setData(JSONObject jsonWorkflowElement)
    {
        DateFormat dateFormat = new SimpleDateFormat("yyyy-MM-dd'T'HH:mm:ss'Z'");
        dateFormat.setTimeZone(TimeZone.getTimeZone("GMT"));

        try {
            super.setData(jsonWorkflowElement);

            setLabel(jsonWorkflowElement.getString("Label"));

            JSONObject joParameters = jsonWorkflowElement.getJSONObject("Parameters");

            if (joParameters.has("Title") && !joParameters.getString("Title").equalsIgnoreCase(""))
                setTitle(joParameters.getString("Title"));
            if (joParameters.has("Tags") && !joParameters.getString("Tags").equalsIgnoreCase(""))
                setTags(joParameters.getString("Tags"));
            if (joParameters.has("Retention") && !joParameters.getString("Retention").equalsIgnoreCase(""))
                setRetention(joParameters.getString("Retention"));
            if (joParameters.has("StartPublishing") && !joParameters.getString("StartPublishing").equalsIgnoreCase(""))
                setStartPublishing(dateFormat.parse(joParameters.getString("StartPublishing")));
            if (joParameters.has("EndPublishing") && !joParameters.getString("EndPublishing").equalsIgnoreCase(""))
                setEndPublishing(dateFormat.parse(joParameters.getString("EndPublishing")));
            if (joParameters.has("UserData") && !joParameters.getString("UserData").equalsIgnoreCase(""))
                setUserData(joParameters.getString("UserData"));
            if (joParameters.has("Ingester") && !joParameters.getString("Ingester").equalsIgnoreCase(""))
                setIngester(joParameters.getString("Ingester"));
            if (joParameters.has("ContentProviderName") && !joParameters.getString("ContentProviderName").equalsIgnoreCase(""))
                setContentProviderName(joParameters.getString("ContentProviderName"));
            if (joParameters.has("DeliveryFileName") && !joParameters.getString("DeliveryFileName").equalsIgnoreCase(""))
                setDeliveryFileName(joParameters.getString("DeliveryFileName"));
            if (joParameters.has("UniqueName") && !joParameters.getString("UniqueName").equalsIgnoreCase(""))
                setUniqueName(joParameters.getString("UniqueName"));
        }
        catch (Exception e)
        {
            mLogger.error("WorkflowProperties:setData failed, exception: " + e);
        }
    }

    public void addCreateContentPropertiesToJson(JSONObject joParameters)
            throws Exception
    {
        try
        {
            DateFormat dateFormat = new SimpleDateFormat("yyyy-MM-dd'T'HH:mm:ss'Z'");
            dateFormat.setTimeZone(TimeZone.getTimeZone("GMT"));

            if (getTitle() != null && !getTitle().equalsIgnoreCase(""))
                joParameters.put("Title", getTitle());
            if (getTags() != null && !getTags().equalsIgnoreCase(""))
            {
                JSONArray jsonTagsArray = new JSONArray();
                joParameters.put("Tags", jsonTagsArray);

                for (String tag: getTags().split(","))
                {
                    jsonTagsArray.put(tag);
                }
            }
            if (getRetention() != null && !getRetention().equalsIgnoreCase(""))
                joParameters.put("Retention", getRetention());
            if (getStartPublishing() != null || getEndPublishing() != null)
            {
                JSONObject joPublishing = new JSONObject();
                joParameters.put("Publishing", joPublishing);

                if (getStartPublishing() != null)
                    joPublishing.put("StartPublishing", dateFormat.format(getStartPublishing()));
                else
                    joPublishing.put("StartPublishing", "NOW");
                if (getEndPublishing() != null)
                    joPublishing.put("EndPublishing", dateFormat.format(getEndPublishing()));
                else
                    joPublishing.put("EndPublishing", "FOREVER");
            }
            if (getUserData() != null && !getUserData().equalsIgnoreCase(""))
                joParameters.put("UniqueData", getUserData());
            if (getIngester() != null && !getIngester().equalsIgnoreCase(""))
                joParameters.put("Ingester", getIngester());
            if (getContentProviderName() != null && !getContentProviderName().equalsIgnoreCase(""))
                joParameters.put("ContentProviderName", getContentProviderName());
            if (getDeliveryFileName() != null && !getDeliveryFileName().equalsIgnoreCase(""))
                joParameters.put("DeliveryFileName", getDeliveryFileName());
            if (getUniqueName() != null && !getUniqueName().equalsIgnoreCase(""))
                joParameters.put("UniqueName", getUniqueName());
        }
        catch (Exception e)
        {
            mLogger.error("buildWorkflowJson failed: " + e);

            throw e;
        }
    }

    public String getTitle() {
        return title;
    }

    public void setTitle(String title) {
        this.title = title;
    }

    public String getTags() {
        return tags;
    }

    public void setTags(String tags) {
        this.tags = tags;
    }

    public String getRetention() {
        return retention;
    }

    public void setRetention(String retention) {
        this.retention = retention;
    }

    public Date getStartPublishing() {
        return startPublishing;
    }

    public void setStartPublishing(Date startPublishing) {
        this.startPublishing = startPublishing;
    }

    public Date getEndPublishing() {
        return endPublishing;
    }

    public void setEndPublishing(Date endPublishing) {
        this.endPublishing = endPublishing;
    }

    public String getUserData() {
        return userData;
    }

    public void setUserData(String userData) {
        this.userData = userData;
    }

    public String getIngester() {
        return ingester;
    }

    public void setIngester(String ingester) {
        this.ingester = ingester;
    }

    public String getContentProviderName() {
        return contentProviderName;
    }

    public void setContentProviderName(String contentProviderName) {
        this.contentProviderName = contentProviderName;
    }

    public String getDeliveryFileName() {
        return deliveryFileName;
    }

    public void setDeliveryFileName(String deliveryFileName) {
        this.deliveryFileName = deliveryFileName;
    }

    public String getUniqueName() {
        return uniqueName;
    }

    public void setUniqueName(String uniqueName) {
        this.uniqueName = uniqueName;
    }

}
