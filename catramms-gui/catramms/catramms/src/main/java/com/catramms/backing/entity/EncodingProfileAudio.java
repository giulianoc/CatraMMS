package com.catramms.backing.entity;

import java.io.Serializable;

/**
 * Created by multi on 09.06.18.
 */
public class EncodingProfileAudio implements Serializable {
    private String codec;
    private Long kBitRate;
    private String otherOutputParameters;
    private Long channelsNumber;
    private Long sampleRate;

    public String getCodec() {
        return codec;
    }

    public void setCodec(String codec) {
        this.codec = codec;
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

    public Long getChannelsNumber() {
        return channelsNumber;
    }

    public void setChannelsNumber(Long channelsNumber) {
        this.channelsNumber = channelsNumber;
    }

    public Long getSampleRate() {
        return sampleRate;
    }

    public void setSampleRate(Long sampleRate) {
        this.sampleRate = sampleRate;
    }
}
