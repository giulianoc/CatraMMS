package com.catramms.backing.entity;

import java.io.Serializable;
import java.util.Locale;
import java.util.LongSummaryStatistics;

/**
 * Created by multi on 09.06.18.
 */
public class EncodingProfileVideo implements Serializable {
    private String codec;
    private String profile;
    private Long width;
    private Long height;
    private Boolean twoPasses;
    private Long kBitRate;
    private String otherOutputParameters;
    private Long kMaxRate;
    private Long kBufSize;
    private Long frameRate;
    private Long keyFrameIntervalInSeconds;

    public String getCodec() {
        return codec;
    }

    public void setCodec(String codec) {
        this.codec = codec;
    }

    public String getProfile() {
        return profile;
    }

    public void setProfile(String profile) {
        this.profile = profile;
    }

    public Long getWidth() {
        return width;
    }

    public void setWidth(Long width) {
        this.width = width;
    }

    public Long getHeight() {
        return height;
    }

    public void setHeight(Long height) {
        this.height = height;
    }

    public Boolean getTwoPasses() {
        return twoPasses;
    }

    public void setTwoPasses(Boolean twoPasses) {
        this.twoPasses = twoPasses;
    }

    public Long getkBitRate() {
        return kBitRate;
    }

    public void setkBitRate(Long kBitRate) {
        this.kBitRate = kBitRate;
    }

    public String getOtherOutputParameters() {
        return otherOutputParameters;
    }

    public void setOtherOutputParameters(String otherOutputParameters) {
        this.otherOutputParameters = otherOutputParameters;
    }

    public Long getkMaxRate() {
        return kMaxRate;
    }

    public void setkMaxRate(Long kMaxRate) {
        this.kMaxRate = kMaxRate;
    }

    public Long getkBufSize() {
        return kBufSize;
    }

    public void setkBufSize(Long kBufSize) {
        this.kBufSize = kBufSize;
    }

    public Long getFrameRate() {
        return frameRate;
    }

    public void setFrameRate(Long frameRate) {
        this.frameRate = frameRate;
    }

    public Long getKeyFrameIntervalInSeconds() {
        return keyFrameIntervalInSeconds;
    }

    public void setKeyFrameIntervalInSeconds(Long keyFrameIntervalInSeconds) {
        this.keyFrameIntervalInSeconds = keyFrameIntervalInSeconds;
    }
}
