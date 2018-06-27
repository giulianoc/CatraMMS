package com.catramms.backing.entity;

import org.json.JSONObject;

import java.io.Serializable;

/**
 * Created by multi on 09.06.18.
 */
public class EncodingProfile implements Serializable{
    private Long encodingProfileKey;
    private String label;
    private String contentType;
    private String fileFormat;
    private EncodingProfileVideo videoDetails = new EncodingProfileVideo();
    private EncodingProfileAudio audioDetails = new EncodingProfileAudio();
    private EncodingProfileImage imageDetails = new EncodingProfileImage();

    public String getFileFormat() {
        return fileFormat;
    }

    public void setFileFormat(String fileFormat) {
        this.fileFormat = fileFormat;
    }

    public Long getEncodingProfileKey() {
        return encodingProfileKey;
    }

    public void setEncodingProfileKey(Long encodingProfileKey) {
        this.encodingProfileKey = encodingProfileKey;
    }

    public String getLabel() {
        return label;
    }

    public void setLabel(String label) {
        this.label = label;
    }

    public String getContentType() {
        return contentType;
    }

    public void setContentType(String contentType) {
        this.contentType = contentType;
    }

    public EncodingProfileVideo getVideoDetails() {
        return videoDetails;
    }

    public void setVideoDetails(EncodingProfileVideo videoDetails) {
        this.videoDetails = videoDetails;
    }

    public EncodingProfileAudio getAudioDetails() {
        return audioDetails;
    }

    public void setAudioDetails(EncodingProfileAudio audioDetails) {
        this.audioDetails = audioDetails;
    }

    public EncodingProfileImage getImageDetails() {
        return imageDetails;
    }

    public void setImageDetails(EncodingProfileImage imageDetails) {
        this.imageDetails = imageDetails;
    }
}
