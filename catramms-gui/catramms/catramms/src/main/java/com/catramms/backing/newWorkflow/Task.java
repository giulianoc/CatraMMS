package com.catramms.backing.newWorkflow;

import org.primefaces.model.UploadedFile;

import java.io.Serializable;
import java.util.Date;

/**
 * Created by multi on 13.06.18.
 */
public class Task implements Serializable
{
    private String label;
    private String type;

    private String references;

    // type: "GroupOfTasks"
    private String groupOfTaskExecutionType;  // parallel or sequential

    // type: "Add-Content"
    private String sourceDownloadType;  // pull or push
    private String pullSourceURL;
    private String pushBinaryPathName;
    private String fileFormat;
    private String userData;
    private String retention;
    private String title;
    private String uniqueName;
    private String ingester;
    private String keywords;
    private String md5FileCheckSum;
    private Long fileSizeInBytes;
    private String contentProviderName;
    private String deliveryFileName;
    private Date startPublishing;
    private Date endPublishing;
    private Float startTimeInSeconds;
    private Float endTimeInSeconds;
    private Long framesNumber;
    private String cutEndType;  // endTimeInSeconds or framesNumber
    private String encodingPriority;
    private String encodingProfileType;
    private String encodingProfilesSetLabel;
    private String encodingProfileLabel;
    private String emailAddress;
    private String subject;
    private String message;
    private String overlayPositionXInPixel;
    private String overlayPositionYInPixel;
    private String overlayText;
    private String overlayFontType;
    private Long overlayFontSize;
    private String overlayFontColor;
    private Long overlayTextPercentageOpacity;
    private Boolean overlayBoxEnable;
    private String overlayBoxColor;
    private Long overlayBoxPercentageOpacity;
    private Float frameInstantInSeconds;
    private Long frameWidth;
    private Long frameHeight;
    private Long framePeriodInSeconds;
    private Long frameMaxFramesNumber;
    private Float frameDurationOfEachSlideInSeconds;
    private Long frameOutputFrameRate;
    private String ftpDeliveryServer;
    private Long ftpDeliveryPort;
    private String ftpDeliveryUserName;
    private String ftpDeliveryPassword;
    private String ftpDeliveryRemoteDirectory;
    private String httpCallbackProtocol;
    private String httpCallbackHostName;
    private Long httpCallbackPort;
    private String httpCallbackURI;
    private String httpCallbackParameters;
    private String httpCallbackMethod;
    private String httpCallbackHeaders;

    private boolean childTaskCreated;
    private boolean childEventOnSuccessCreated;
    private boolean childEventOnErrorCreated;
    private boolean childEventOnCompleteCreated;

    @Override
    public String toString()
    {
        if (type.equalsIgnoreCase("GroupOfTasks"))
            return "<" + type + ">-" + groupOfTaskExecutionType;
        else
            return label + " <Task: " + type + ">";
    }

    public String getLabel() {
        return label;
    }

    public void setLabel(String label) {
        this.label = label;
    }

    public String getType() {
        return type;
    }

    public void setType(String type) {
        this.type = type;
    }

    public String getTitle() {
        return title;
    }

    public void setTitle(String title) {
        this.title = title;
    }

    public String getPullSourceURL() {
        return pullSourceURL;
    }

    public void setPullSourceURL(String pullSourceURL) {
        this.pullSourceURL = pullSourceURL;
    }

    public String getPushBinaryPathName() {
        return pushBinaryPathName;
    }

    public void setPushBinaryPathName(String pushBinaryPathName) {
        this.pushBinaryPathName = pushBinaryPathName;
    }

    public String getUserData() {
        return userData;
    }

    public void setUserData(String userData) {
        this.userData = userData;
    }

    public String getFileFormat() {
        return fileFormat;
    }

    public void setFileFormat(String fileFormat) {
        this.fileFormat = fileFormat;
    }

    public String getMd5FileCheckSum() {
        return md5FileCheckSum;
    }

    public void setMd5FileCheckSum(String md5FileCheckSum) {
        this.md5FileCheckSum = md5FileCheckSum;
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

    public String getIngester() {
        return ingester;
    }

    public void setIngester(String ingester) {
        this.ingester = ingester;
    }

    public String getSourceDownloadType() {
        return sourceDownloadType;
    }

    public void setSourceDownloadType(String sourceDownloadType) {
        this.sourceDownloadType = sourceDownloadType;
    }

    public String getKeywords() {
        return keywords;
    }

    public void setKeywords(String keywords) {
        this.keywords = keywords;
    }

    public String getGroupOfTaskExecutionType() {
        return groupOfTaskExecutionType;
    }

    public void setGroupOfTaskExecutionType(String groupOfTaskExecutionType) {
        this.groupOfTaskExecutionType = groupOfTaskExecutionType;
    }

    public String getUniqueName() {
        return uniqueName;
    }

    public void setUniqueName(String uniqueName) {
        this.uniqueName = uniqueName;
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

    public String getRetention() {
        return retention;
    }

    public void setRetention(String retention) {
        this.retention = retention;
    }

    public boolean isChildEventOnSuccessCreated() {
        return childEventOnSuccessCreated;
    }

    public void setChildEventOnSuccessCreated(boolean childEventOnSuccessCreated) {
        this.childEventOnSuccessCreated = childEventOnSuccessCreated;
    }

    public boolean isChildEventOnErrorCreated() {
        return childEventOnErrorCreated;
    }

    public void setChildEventOnErrorCreated(boolean childEventOnErrorCreated) {
        this.childEventOnErrorCreated = childEventOnErrorCreated;
    }

    public boolean isChildEventOnCompleteCreated() {
        return childEventOnCompleteCreated;
    }

    public void setChildEventOnCompleteCreated(boolean childEventOnCompleteCreated) {
        this.childEventOnCompleteCreated = childEventOnCompleteCreated;
    }

    public Float getStartTimeInSeconds() {
        return startTimeInSeconds;
    }

    public void setStartTimeInSeconds(Float startTimeInSeconds) {
        this.startTimeInSeconds = startTimeInSeconds;
    }

    public Float getEndTimeInSeconds() {
        return endTimeInSeconds;
    }

    public void setEndTimeInSeconds(Float endTimeInSeconds) {
        this.endTimeInSeconds = endTimeInSeconds;
    }

    public Long getFramesNumber() {
        return framesNumber;
    }

    public void setFramesNumber(Long framesNumber) {
        this.framesNumber = framesNumber;
    }

    public String getEncodingPriority() {
        return encodingPriority;
    }

    public void setEncodingPriority(String encodingPriority) {
        this.encodingPriority = encodingPriority;
    }

    public String getEncodingProfilesSetLabel() {
        return encodingProfilesSetLabel;
    }

    public void setEncodingProfilesSetLabel(String encodingProfilesSetLabel) {
        this.encodingProfilesSetLabel = encodingProfilesSetLabel;
    }

    public String getEncodingProfileLabel() {
        return encodingProfileLabel;
    }

    public void setEncodingProfileLabel(String encodingProfileLabel) {
        this.encodingProfileLabel = encodingProfileLabel;
    }

    public String getEncodingProfileType() {
        return encodingProfileType;
    }

    public void setEncodingProfileType(String encodingProfileType) {
        this.encodingProfileType = encodingProfileType;
    }

    public boolean isChildTaskCreated() {
        return childTaskCreated;
    }

    public void setChildTaskCreated(boolean childTaskCreated) {
        this.childTaskCreated = childTaskCreated;
    }

    public String getCutEndType() {
        return cutEndType;
    }

    public void setCutEndType(String cutEndType) {
        this.cutEndType = cutEndType;
    }

    public String getEmailAddress() {
        return emailAddress;
    }

    public void setEmailAddress(String emailAddress) {
        this.emailAddress = emailAddress;
    }

    public String getSubject() {
        return subject;
    }

    public void setSubject(String subject) {
        this.subject = subject;
    }

    public String getMessage() {
        return message;
    }

    public void setMessage(String message) {
        this.message = message;
    }

    public String getOverlayPositionXInPixel() {
        return overlayPositionXInPixel;
    }

    public void setOverlayPositionXInPixel(String overlayPositionXInPixel) {
        this.overlayPositionXInPixel = overlayPositionXInPixel;
    }

    public String getOverlayPositionYInPixel() {
        return overlayPositionYInPixel;
    }

    public void setOverlayPositionYInPixel(String overlayPositionYInPixel) {
        this.overlayPositionYInPixel = overlayPositionYInPixel;
    }

    public String getOverlayText() {
        return overlayText;
    }

    public void setOverlayText(String overlayText) {
        this.overlayText = overlayText;
    }

    public String getOverlayFontType() {
        return overlayFontType;
    }

    public void setOverlayFontType(String overlayFontType) {
        this.overlayFontType = overlayFontType;
    }

    public Long getOverlayFontSize() {
        return overlayFontSize;
    }

    public void setOverlayFontSize(Long overlayFontSize) {
        this.overlayFontSize = overlayFontSize;
    }

    public String getOverlayFontColor() {
        return overlayFontColor;
    }

    public void setOverlayFontColor(String overlayFontColor) {
        this.overlayFontColor = overlayFontColor;
    }

    public Long getOverlayTextPercentageOpacity() {
        return overlayTextPercentageOpacity;
    }

    public void setOverlayTextPercentageOpacity(Long overlayTextPercentageOpacity) {
        this.overlayTextPercentageOpacity = overlayTextPercentageOpacity;
    }

    public Boolean getOverlayBoxEnable() {
        return overlayBoxEnable;
    }

    public void setOverlayBoxEnable(Boolean overlayBoxEnable) {
        this.overlayBoxEnable = overlayBoxEnable;
    }

    public String getOverlayBoxColor() {
        return overlayBoxColor;
    }

    public void setOverlayBoxColor(String overlayBoxColor) {
        this.overlayBoxColor = overlayBoxColor;
    }

    public Long getOverlayBoxPercentageOpacity() {
        return overlayBoxPercentageOpacity;
    }

    public void setOverlayBoxPercentageOpacity(Long overlayBoxPercentageOpacity) {
        this.overlayBoxPercentageOpacity = overlayBoxPercentageOpacity;
    }

    public Float getFrameInstantInSeconds() {
        return frameInstantInSeconds;
    }

    public void setFrameInstantInSeconds(Float frameInstantInSeconds) {
        this.frameInstantInSeconds = frameInstantInSeconds;
    }

    public Long getFrameWidth() {
        return frameWidth;
    }

    public void setFrameWidth(Long frameWidth) {
        this.frameWidth = frameWidth;
    }

    public Long getFrameHeight() {
        return frameHeight;
    }

    public void setFrameHeight(Long frameHeight) {
        this.frameHeight = frameHeight;
    }

    public Long getFrameMaxFramesNumber() {
        return frameMaxFramesNumber;
    }

    public void setFrameMaxFramesNumber(Long frameMaxFramesNumber) {
        this.frameMaxFramesNumber = frameMaxFramesNumber;
    }

    public Long getFramePeriodInSeconds() {
        return framePeriodInSeconds;
    }

    public void setFramePeriodInSeconds(Long framePeriodInSeconds) {
        this.framePeriodInSeconds = framePeriodInSeconds;
    }

    public Float getFrameDurationOfEachSlideInSeconds() {
        return frameDurationOfEachSlideInSeconds;
    }

    public void setFrameDurationOfEachSlideInSeconds(Float frameDurationOfEachSlideInSeconds) {
        this.frameDurationOfEachSlideInSeconds = frameDurationOfEachSlideInSeconds;
    }

    public Long getFrameOutputFrameRate() {
        return frameOutputFrameRate;
    }

    public void setFrameOutputFrameRate(Long frameOutputFrameRate) {
        this.frameOutputFrameRate = frameOutputFrameRate;
    }

    public String getReferences() {
        return references;
    }

    public void setReferences(String references) {
        this.references = references;
    }

    public String getFtpDeliveryServer() {
        return ftpDeliveryServer;
    }

    public void setFtpDeliveryServer(String ftpDeliveryServer) {
        this.ftpDeliveryServer = ftpDeliveryServer;
    }

    public Long getFtpDeliveryPort() {
        return ftpDeliveryPort;
    }

    public void setFtpDeliveryPort(Long ftpDeliveryPort) {
        this.ftpDeliveryPort = ftpDeliveryPort;
    }

    public String getFtpDeliveryUserName() {
        return ftpDeliveryUserName;
    }

    public void setFtpDeliveryUserName(String ftpDeliveryUserName) {
        this.ftpDeliveryUserName = ftpDeliveryUserName;
    }

    public String getFtpDeliveryPassword() {
        return ftpDeliveryPassword;
    }

    public void setFtpDeliveryPassword(String ftpDeliveryPassword) {
        this.ftpDeliveryPassword = ftpDeliveryPassword;
    }

    public String getFtpDeliveryRemoteDirectory() {
        return ftpDeliveryRemoteDirectory;
    }

    public void setFtpDeliveryRemoteDirectory(String ftpDeliveryRemoteDirectory) {
        this.ftpDeliveryRemoteDirectory = ftpDeliveryRemoteDirectory;
    }

    public String getHttpCallbackProtocol() {
        return httpCallbackProtocol;
    }

    public void setHttpCallbackProtocol(String httpCallbackProtocol) {
        this.httpCallbackProtocol = httpCallbackProtocol;
    }

    public String getHttpCallbackHostName() {
        return httpCallbackHostName;
    }

    public void setHttpCallbackHostName(String httpCallbackHostName) {
        this.httpCallbackHostName = httpCallbackHostName;
    }

    public Long getHttpCallbackPort() {
        return httpCallbackPort;
    }

    public void setHttpCallbackPort(Long httpCallbackPort) {
        this.httpCallbackPort = httpCallbackPort;
    }

    public String getHttpCallbackURI() {
        return httpCallbackURI;
    }

    public void setHttpCallbackURI(String httpCallbackURI) {
        this.httpCallbackURI = httpCallbackURI;
    }

    public String getHttpCallbackParameters() {
        return httpCallbackParameters;
    }

    public void setHttpCallbackParameters(String httpCallbackParameters) {
        this.httpCallbackParameters = httpCallbackParameters;
    }

    public String getHttpCallbackMethod() {
        return httpCallbackMethod;
    }

    public void setHttpCallbackMethod(String httpCallbackMethod) {
        this.httpCallbackMethod = httpCallbackMethod;
    }

    public String getHttpCallbackHeaders() {
        return httpCallbackHeaders;
    }

    public void setHttpCallbackHeaders(String httpCallbackHeaders) {
        this.httpCallbackHeaders = httpCallbackHeaders;
    }
}
