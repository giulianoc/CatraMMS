package com.catramms.backing.workspaceEditor.utility;

import java.io.Serializable;
import java.util.ArrayList;
import java.util.Date;
import java.util.List;

public class RemoveContentProperties extends WorkflowProperties implements Serializable {

    private String sourceDownloadType;
    private String pullSourceURL;
    private String pushBinaryFileName;
    private String fileFormat;
    private List<String> fileFormatsList = new ArrayList<>();
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

    public RemoveContentProperties(int elementId, String label)
    {
        super(elementId, label, "Remove-Content" + "-icon.png", "Task", "Remove-Content");

        sourceDownloadType = "pull";
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
}
