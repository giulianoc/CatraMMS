package com.catramms.backing.entity;

import java.io.Serializable;

/**
 * Created by multi on 08.06.18.
 */
public class VideoDetails implements Serializable{

    private Long durationInMilliseconds;
    private Long bitRate;

    private String videoCodecName;
    private Long videoBitRate;
    private String videoProfile;
    private String videoAvgFrameRate;
    private Long videoWidth;
    private Long videoHeight;

    private String audioCodecName;
    private Long audioBitRate;
    private Long audioChannels;
    private Long audioSampleRate;

    public String getVideoCodecName() {
        return videoCodecName;
    }

    public void setVideoCodecName(String videoCodecName) {
        this.videoCodecName = videoCodecName;
    }

    public Long getVideoBitRate() {
        return videoBitRate;
    }

    public void setVideoBitRate(Long videoBitRate) {
        this.videoBitRate = videoBitRate;
    }

    public String getVideoProfile() {
        return videoProfile;
    }

    public void setVideoProfile(String videoProfile) {
        this.videoProfile = videoProfile;
    }

    public String getVideoAvgFrameRate() {
        return videoAvgFrameRate;
    }

    public void setVideoAvgFrameRate(String videoAvgFrameRate) {
        this.videoAvgFrameRate = videoAvgFrameRate;
    }

    public Long getVideoWidth() {
        return videoWidth;
    }

    public void setVideoWidth(Long videoWidth) {
        this.videoWidth = videoWidth;
    }

    public Long getVideoHeight() {
        return videoHeight;
    }

    public void setVideoHeight(Long videoHeight) {
        this.videoHeight = videoHeight;
    }

    public Long getDurationInMilliseconds() {
        return durationInMilliseconds;
    }

    public void setDurationInMilliseconds(Long durationInMilliseconds) {
        this.durationInMilliseconds = durationInMilliseconds;
    }

    public Long getBitRate() {
        return bitRate;
    }

    public void setBitRate(Long bitRate) {
        this.bitRate = bitRate;
    }

    public String getAudioCodecName() {
        return audioCodecName;
    }

    public void setAudioCodecName(String audioCodecName) {
        this.audioCodecName = audioCodecName;
    }

    public Long getAudioBitRate() {
        return audioBitRate;
    }

    public void setAudioBitRate(Long audioBitRate) {
        this.audioBitRate = audioBitRate;
    }

    public Long getAudioChannels() {
        return audioChannels;
    }

    public void setAudioChannels(Long audioChannels) {
        this.audioChannels = audioChannels;
    }

    public Long getAudioSampleRate() {
        return audioSampleRate;
    }

    public void setAudioSampleRate(Long audioSampleRate) {
        this.audioSampleRate = audioSampleRate;
    }
}
