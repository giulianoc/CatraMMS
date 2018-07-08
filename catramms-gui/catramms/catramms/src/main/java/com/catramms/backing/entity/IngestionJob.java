package com.catramms.backing.entity;

import java.io.Serializable;
import java.util.ArrayList;
import java.util.Date;
import java.util.List;

/**
 * Created by multi on 09.06.18.
 */
public class IngestionJob implements Serializable {
    private Long ingestionJobKey;
    private String label;
    private String ingestionType;
    private Date startProcessing;
    private Date endProcessing;
    private String status;
    private String errorMessage;
    private Long downloadingProgress;
    private Long uploadingProgress;
    private List<IngestionJobMediaItem> ingestionJobMediaItemList = new ArrayList<>();
    private EncodingJob encodingJob = null;
    private Long ingestionRookKey;

    public Long getIngestionRookKey() {
        return ingestionRookKey;
    }

    public void setIngestionRookKey(Long ingestionRookKey) {
        this.ingestionRookKey = ingestionRookKey;
    }

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

    public Date getStartProcessing() {
        return startProcessing;
    }

    public void setStartProcessing(Date startProcessing) {
        this.startProcessing = startProcessing;
    }

    public Date getEndProcessing() {
        return endProcessing;
    }

    public void setEndProcessing(Date endProcessing) {
        this.endProcessing = endProcessing;
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

    public List<IngestionJobMediaItem> getIngestionJobMediaItemList() {
        return ingestionJobMediaItemList;
    }

    public void setIngestionJobMediaItemList(List<IngestionJobMediaItem> ingestionJobMediaItemList) {
        this.ingestionJobMediaItemList = ingestionJobMediaItemList;
    }

    public EncodingJob getEncodingJob() {
        return encodingJob;
    }

    public void setEncodingJob(EncodingJob encodingJob) {
        this.encodingJob = encodingJob;
    }
}
