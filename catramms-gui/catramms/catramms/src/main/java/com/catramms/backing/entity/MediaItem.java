package com.catramms.backing.entity;

import java.io.Serializable;
import java.util.ArrayList;
import java.util.Date;
import java.util.List;

/**
 * Created by multi on 07.06.18.
 */
public class MediaItem implements Serializable{
    private Long mediaItemKey;
    private String title;
    private String contentType;
    private Date ingestionDate;
    private Date startPublishing;
    private Date endPublishing;
    private String ingester;
    private String keywords;
    private String providerName;
    private Long retentionInMinutes;

    private List<PhysicalPath> physicalPathList = new ArrayList<>();

    private VideoDetails videoDetails = new VideoDetails();
    private AudioDetails audioDetails = new AudioDetails();

    // this is calculated during fillMediaItem
    private PhysicalPath sourcePhysicalPath;

    public PhysicalPath getSourcePhysicalPath() {
        return sourcePhysicalPath;
    }

    public void setSourcePhysicalPath(PhysicalPath sourcePhysicalPath) {
        this.sourcePhysicalPath = sourcePhysicalPath;
    }

    public List<PhysicalPath> getPhysicalPathList() {
        return physicalPathList;
    }

    public void setPhysicalPathList(List<PhysicalPath> physicalPathList) {
        this.physicalPathList = physicalPathList;
    }

    public VideoDetails getVideoDetails() {
        return videoDetails;
    }

    public void setVideoDetails(VideoDetails videoDetails) {
        this.videoDetails = videoDetails;
    }

    public AudioDetails getAudioDetails() {
        return audioDetails;
    }

    public void setAudioDetails(AudioDetails audioDetails) {
        this.audioDetails = audioDetails;
    }

    public Long getMediaItemKey() {
        return mediaItemKey;
    }

    public void setMediaItemKey(Long mediaItemKey) {
        this.mediaItemKey = mediaItemKey;
    }

    public String getContentType() {
        return contentType;
    }

    public void setContentType(String contentType) {
        this.contentType = contentType;
    }

    public Date getIngestionDate() {
        return ingestionDate;
    }

    public void setIngestionDate(Date ingestionDate) {
        this.ingestionDate = ingestionDate;
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

    public String getIngester() {
        return ingester;
    }

    public void setIngester(String ingester) {
        this.ingester = ingester;
    }

    public String getKeywords() {
        return keywords;
    }

    public void setKeywords(String keywords) {
        this.keywords = keywords;
    }

    public String getProviderName() {
        return providerName;
    }

    public void setProviderName(String providerName) {
        this.providerName = providerName;
    }

    public Long getRetentionInMinutes() {
        return retentionInMinutes;
    }

    public void setRetentionInMinutes(Long retentionInMinutes) {
        this.retentionInMinutes = retentionInMinutes;
    }

    public String getTitle() {
        return title;
    }

    public void setTitle(String title) {
        this.title = title;
    }
}
