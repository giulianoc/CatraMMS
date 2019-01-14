package com.catramms.backing.workflowEditor.utility;

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

public class AddContentProperties extends WorkflowProperties implements Serializable {

    private static final Logger mLogger = Logger.getLogger(AddContentProperties.class);

    private String sourceDownloadType;
    private String pullSourceURL;
    private String pushBinaryFileName;
    private String fileFormat;
    private List<String> fileFormatsList;
    private String title;
    private String tags;
    private String retention;
    private Date startPublishing;
    private Date endPublishing;
    private String userData;
    private String ingester;
    private String md5FileChecksum;
    private Long fileSizeInBytes;
    private String contentProviderName;
    private String deliveryFileName;
    private String uniqueName;

    private String labelTemplatePrefix;
    private String temporaryPushBinariesPathName;
    private String videoAudioAllowTypes;

    public AddContentProperties(int elementId, String label,
                                String labelTemplatePrefix, String temporaryPushBinariesPathName)
    {
        super(elementId, label, "Add-Content" + "-icon.png", "Task", "Add-Content");

        sourceDownloadType = "pull";

        this.labelTemplatePrefix = labelTemplatePrefix;
        this.temporaryPushBinariesPathName = temporaryPushBinariesPathName;

        {
            fileFormatsList = new ArrayList<>();
            fileFormatsList.add("mp4");
            fileFormatsList.add("mov");
            fileFormatsList.add("ts");
            fileFormatsList.add("wmv");
            fileFormatsList.add("mpeg");
            fileFormatsList.add("avi");
            fileFormatsList.add("webm");
            fileFormatsList.add("mp3");
            fileFormatsList.add("aac");
            fileFormatsList.add("png");
            fileFormatsList.add("jpg");
        }

        {
            String fileExtension = "";   // = "wmv|mp4|ts|mpeg|avi|webm|mp3|aac|png|jpg";

            for (String fileFormat: fileFormatsList)
            {
                if (fileExtension.isEmpty())
                    fileExtension = fileFormat;
                else
                    fileExtension += ("|" + fileFormat);
            }

            videoAudioAllowTypes = ("/(\\.|\\/)(" + fileExtension + ")$/");
        }
    }

    public AddContentProperties clone()
    {
        AddContentProperties addContentProperties = new AddContentProperties(
                super.getElementId(), super.getLabel(), super.getImage(), super.getType());

        addContentProperties.setSourceDownloadType(sourceDownloadType);
        addContentProperties.setPullSourceURL(pullSourceURL);
        addContentProperties.setPushBinaryFileName(pushBinaryFileName);
        addContentProperties.setFileFormat(fileFormat);
        addContentProperties.setFileFormatsList(new ArrayList<>(fileFormatsList));
        addContentProperties.setTitle(title);
        addContentProperties.setTags(tags);
        addContentProperties.setRetention(retention);
        addContentProperties.setStartPublishing(startPublishing);
        addContentProperties.setEndPublishing(endPublishing);
        addContentProperties.setUserData(userData);
        addContentProperties.setIngester(ingester);
        addContentProperties.setMd5FileChecksum(md5FileChecksum);
        addContentProperties.setFileSizeInBytes(fileSizeInBytes);
        addContentProperties.setContentProviderName(contentProviderName);
        addContentProperties.setDeliveryFileName(deliveryFileName);
        addContentProperties.setUniqueName(uniqueName);

        addContentProperties.setLabelTemplatePrefix(labelTemplatePrefix);
        addContentProperties.setTemporaryPushBinariesPathName(temporaryPushBinariesPathName);
        addContentProperties.setVideoAudioAllowTypes(videoAudioAllowTypes);

        return addContentProperties;
    }

    public JSONObject buildWorkflowElementJson(IngestionData ingestionData)
            throws Exception
    {
        JSONObject jsonWorkflowElement = new JSONObject();

        try
        {
            DateFormat dateFormat = new SimpleDateFormat("yyyy-MM-dd'T'HH:mm:ss'Z'");
            dateFormat.setTimeZone(TimeZone.getTimeZone("GMT"));

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

            if (getSourceDownloadType().equalsIgnoreCase("pull"))
            {
                if (getPullSourceURL() != null && !getPullSourceURL().equalsIgnoreCase(""))
                    joParameters.put("SourceURL", getPullSourceURL());
                else
                {
                    WorkflowIssue workflowIssue = new WorkflowIssue();
                    workflowIssue.setLabel(super.getLabel());
                    workflowIssue.setFieldName("SourceURL");
                    workflowIssue.setTaskType(super.getType());
                    workflowIssue.setIssue("The field is not initialized");

                    ingestionData.getWorkflowIssueList().add(workflowIssue);
                }
            }
            else
            {
                if (getPushBinaryFileName() == null || getPushBinaryFileName().equalsIgnoreCase(""))
                {
                    WorkflowIssue workflowIssue = new WorkflowIssue();
                    workflowIssue.setLabel(super.getLabel());
                    workflowIssue.setFieldName("File");
                    workflowIssue.setTaskType(super.getType());
                    workflowIssue.setIssue("The field is not initialized");

                    ingestionData.getWorkflowIssueList().add(workflowIssue);
                }
                else
                {
                    PushContent pushContent = new PushContent();
                    pushContent.setLabel(super.getLabel());
                    pushContent.setBinaryPathName(getLocalBinaryPathName(getPushBinaryFileName()));

                    ingestionData.getPushContentList().add(pushContent);
                }
            }
            joParameters.put("FileFormat", getFileFormat());
            if (getUserData() != null && !getUserData().equalsIgnoreCase(""))
                joParameters.put("UniqueData", getUserData());
            if (getRetention() != null && !getRetention().equalsIgnoreCase(""))
                joParameters.put("Retention", getRetention());
            if (getTitle() != null && !getTitle().equalsIgnoreCase(""))
                joParameters.put("Title", getTitle());
            if (getUniqueName() != null && !getUniqueName().equalsIgnoreCase(""))
                joParameters.put("UniqueName", getUniqueName());
            if (getIngester() != null && !getIngester().equalsIgnoreCase(""))
                joParameters.put("Ingester", getIngester());
            if (getTags() != null && !getTags().equalsIgnoreCase(""))
            {
                JSONArray jsonTagsArray = new JSONArray();
                joParameters.put("Tags", jsonTagsArray);

                for (String tag: getTags().split(","))
                {
                    jsonTagsArray.put(tag);
                }
            }
            if (getMd5FileChecksum() != null && !getMd5FileChecksum().equalsIgnoreCase(""))
                joParameters.put("MD5FileChecksum", getMd5FileChecksum());
            if (getFileSizeInBytes() != null)
                joParameters.put("FileSizeInBytes", getFileSizeInBytes());
            if (getContentProviderName() != null && !getContentProviderName().equalsIgnoreCase(""))
                joParameters.put("ContentProviderName", getContentProviderName());
            if (getDeliveryFileName() != null && !getDeliveryFileName().equalsIgnoreCase(""))
                joParameters.put("DeliveryFileName", getDeliveryFileName());
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

    public void handleFileUpload(FileUploadEvent event)
    {
        mLogger.info("Uploaded binary file name: " + event.getFile().getFileName());

        // save
        File binaryFile = null;
        {
            InputStream input = null;
            OutputStream output = null;

            try
            {
                pushBinaryFileName = event.getFile().getFileName();
                binaryFile = new File(getLocalBinaryPathName(pushBinaryFileName));

                input = event.getFile().getInputstream();
                output = new FileOutputStream(binaryFile);

                IOUtils.copy(input, output);

                String fileName;
                String fileExtension = "";
                int extensionIndex = event.getFile().getFileName().lastIndexOf('.');
                if (extensionIndex == -1)
                    fileName = event.getFile().getFileName();
                else
                {
                    fileName = event.getFile().getFileName().substring(0, extensionIndex);
                    fileExtension = event.getFile().getFileName().substring(extensionIndex + 1);
                }

                if (super.getLabel().startsWith(labelTemplatePrefix))   // not set yet
                    super.setLabel(fileName);

                fileFormat = fileExtension;

                if (title == null || title.isEmpty())   // not set yet
                    title = fileName;
            }
            catch (Exception e)
            {
                mLogger.error("Exception: " + e.getMessage());

                return;
            }
            finally
            {
                if (input != null)
                    IOUtils.closeQuietly(input);
                if (output != null)
                    IOUtils.closeQuietly(output);
            }
        }
    }

    private String getLocalBinaryPathName(String fileName)
    {
        try
        {
            Long userKey = SessionUtils.getUserProfile().getUserKey();

            String localBinaryPathName = temporaryPushBinariesPathName + "/" + userKey + "-" + fileName;

            return localBinaryPathName;
        }
        catch (Exception e)
        {
            String errorMessage = "Exception: " + e;
            mLogger.error(errorMessage);

            return null;
        }
    }

    public String getSourceDownloadType() {
        return sourceDownloadType;
    }

    public void setSourceDownloadType(String sourceDownloadType) {
        this.sourceDownloadType = sourceDownloadType;
    }

    public String getPullSourceURL() {
        return pullSourceURL;
    }

    public void setPullSourceURL(String pullSourceURL) {
        this.pullSourceURL = pullSourceURL;
    }

    public String getPushBinaryFileName() {
        return pushBinaryFileName;
    }

    public void setPushBinaryFileName(String pushBinaryFileName) {
        this.pushBinaryFileName = pushBinaryFileName;
    }

    public String getFileFormat() {
        return fileFormat;
    }

    public void setFileFormat(String fileFormat) {
        this.fileFormat = fileFormat;
    }

    public List<String> getFileFormatsList() {
        return fileFormatsList;
    }

    public void setFileFormatsList(List<String> fileFormatsList) {
        this.fileFormatsList = fileFormatsList;
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

    public String getMd5FileChecksum() {
        return md5FileChecksum;
    }

    public void setMd5FileChecksum(String md5FileChecksum) {
        this.md5FileChecksum = md5FileChecksum;
    }

    public Long getFileSizeInBytes() {
        return fileSizeInBytes;
    }

    public void setFileSizeInBytes(Long fileSizeInBytes) {
        this.fileSizeInBytes = fileSizeInBytes;
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

    public String getVideoAudioAllowTypes() {
        return videoAudioAllowTypes;
    }

    public void setVideoAudioAllowTypes(String videoAudioAllowTypes) {
        this.videoAudioAllowTypes = videoAudioAllowTypes;
    }

    public String getLabelTemplatePrefix() {
        return labelTemplatePrefix;
    }

    public void setLabelTemplatePrefix(String labelTemplatePrefix) {
        this.labelTemplatePrefix = labelTemplatePrefix;
    }

    public String getTemporaryPushBinariesPathName() {
        return temporaryPushBinariesPathName;
    }

    public void setTemporaryPushBinariesPathName(String temporaryPushBinariesPathName) {
        this.temporaryPushBinariesPathName = temporaryPushBinariesPathName;
    }
}
