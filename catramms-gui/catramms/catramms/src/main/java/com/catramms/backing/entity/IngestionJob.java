package com.catramms.backing.entity;

import java.io.Serializable;
import java.util.Date;

/**
 * Created by multi on 09.06.18.
 */
public class IngestionJob implements Serializable {
    private Long ingestionJobKey;
    private String label;
    private String ingestionType;
    private Date startIngestion;
    private Date endIngestion;
    private String status;
    private String errorMessage;
    private Long downloadingProgress;
    private Long uploadingProgress;
    private Long mediaItemKey;
    private Long physicalPathKey;
    private EncodingJob encodingJob = null;

    public Long getIngestionJobKey() {
        return ingestionJobKey;
    }

    public void setIngestionJobKey(Long ingestionJobKey) {
        this.ingestionJobKey = ingestionJobKey;
    }

    public String getLabel() {
        return label;
    }

    public void setLabel(String label) {
        this.label = label;
    }

    public String getIngestionType() {
        return ingestionType;
    }

    public void setIngestionType(String ingestionType) {
        this.ingestionType = ingestionType;
    }

    public Date getStartIngestion() {
        return startIngestion;
    }

    public void setStartIngestion(Date startIngestion) {
        this.startIngestion = startIngestion;
    }

    public Date getEndIngestion() {
        return endIngestion;
    }

    public void setEndIngestion(Date endIngestion) {
        this.endIngestion = endIngestion;
    }

    public String getStatus() {
        return status;
    }

    public void setStatus(String status) {
        this.status = status;
    }

    public String getErrorMessage() {
        return errorMessage;
    }

    public void setErrorMessage(String errorMessage) {
        this.errorMessage = errorMessage;
    }

    public Long getDownloadingProgress() {
        return downloadingProgress;
    }

    public void setDownloadingProgress(Long downloadingProgress) {
        this.downloadingProgress = downloadingProgress;
    }

    public Long getUploadingProgress() {
        return uploadingProgress;
    }

    public void setUploadingProgress(Long uploadingProgress) {
        this.uploadingProgress = uploadingProgress;
    }

    public Long getMediaItemKey() {
        return mediaItemKey;
    }

    public void setMediaItemKey(Long mediaItemKey) {
        this.mediaItemKey = mediaItemKey;
    }

    public Long getPhysicalPathKey() {
        return physicalPathKey;
    }

    public void setPhysicalPathKey(Long physicalPathKey) {
        this.physicalPathKey = physicalPathKey;
    }

    public EncodingJob getEncodingJob() {
        return encodingJob;
    }

    public void setEncodingJob(EncodingJob encodingJob) {
        this.encodingJob = encodingJob;
    }
}
