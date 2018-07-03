package com.catramms.backing.entity;

import java.io.Serializable;
import java.util.Date;

/**
 * Created by multi on 08.06.18.
 */
public class PhysicalPath implements Serializable{
    private Long physicalPathKey;
    private String fileFormat;
    private Date creationDate;
    private Long encodingProfileKey;
    private Long sizeInBytes;

    private VideoDetails videoDetails = new VideoDetails();
    private AudioDetails audioDetails = new AudioDetails();
    private ImageDetails imageDetails = new ImageDetails();

    // private MediaItem mediaItem;

    public String getFileFormat() {
        return fileFormat;
    }

    public void setFileFormat(String fileFormat) {
        this.fileFormat = fileFormat;
    }

    public Long getPhysicalPathKey() {
        return physicalPathKey;
    }

    public void setPhysicalPathKey(Long physicalPathKey) {
        this.physicalPathKey = physicalPathKey;
    }

    public Date getCreationDate() {
        return creationDate;
    }

    public void setCreationDate(Date creationDate) {
        this.creationDate = creationDate;
    }

    public Long getEncodingProfileKey() {
        return encodingProfileKey;
    }

    public void setEncodingProfileKey(Long encodingProfileKey) {
        this.encodingProfileKey = encodingProfileKey;
    }

    public Long getSizeInBytes() {
        return sizeInBytes;
    }

    public void setSizeInBytes(Long sizeInBytes) {
        this.sizeInBytes = sizeInBytes;
    }

    /*
    public MediaItem getMediaItem() {
        return mediaItem;
    }

    public void setMediaItem(MediaItem mediaItem) {
        this.mediaItem = mediaItem;
    }
    */

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

    public ImageDetails getImageDetails() {
        return imageDetails;
    }

    public void setImageDetails(ImageDetails imageDetails) {
        this.imageDetails = imageDetails;
    }
}
