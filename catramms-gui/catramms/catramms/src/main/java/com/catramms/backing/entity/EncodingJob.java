package com.catramms.backing.entity;

import java.io.Serializable;
import java.util.Date;

/**
 * Created by multi on 09.06.18.
 */
public class EncodingJob implements Serializable {
    private Long encodingJobKey;
    private String status;
    private Date start;
    private Date end;
    private Long progress;
    private Long failuresNumber;
    private String encodingPriority;
    private int encodingPriorityCode;
    private int maxEncodingPriorityCode;
    private String type;
    private String parameters;  // the content of this field depend on the 'type' field

    // type is 'EncodeVideoAudio' or 'EncodeImage'
    private Long encodingProfileKey;
    private Long sourcePhysicalPathKey;

    // type is 'OverlayImageOnVideo'
    private Long sourceVideoPhysicalPathKey;
    private Long sourceImagePhysicalPathKey;

    // type is 'OverlayTextOnVideo'
    // private Long sourceVideoPhysicalPathKey; already present

    // type is 'LiveRecorder'
    private String liveURL;
    private String outputFileFormat;
    private Long segmentDurationInSeconds;
    private Date recordingPeriodStart;
    private Date recordingPeriodEnd;


    public Long getEncodingJobKey() {
        return encodingJobKey;
    }

    public void setEncodingJobKey(Long encodingJobKey) {
        this.encodingJobKey = encodingJobKey;
    }

    public String getType() {
        return type;
    }

    public void setType(String type) {
        this.type = type;
    }

    public String getStatus() {
        return status;
    }

    public void setStatus(String status) {
        this.status = status;
    }

    public Date getStart() {
        return start;
    }

    public void setStart(Date start) {
        this.start = start;
    }

    public Date getEnd() {
        return end;
    }

    public void setEnd(Date end) {
        this.end = end;
    }

    public Long getProgress() {
        return progress;
    }

    public void setProgress(Long progress) {
        this.progress = progress;
    }

    public Long getFailuresNumber() {
        return failuresNumber;
    }

    public void setFailuresNumber(Long failuresNumber) {
        this.failuresNumber = failuresNumber;
    }

    public String getParameters() {
        return parameters;
    }

    public void setParameters(String parameters) {
        this.parameters = parameters;
    }

    public Long getEncodingProfileKey() {
        return encodingProfileKey;
    }

    public void setEncodingProfileKey(Long encodingProfileKey) {
        this.encodingProfileKey = encodingProfileKey;
    }

    public Long getSourcePhysicalPathKey() {
        return sourcePhysicalPathKey;
    }

    public void setSourcePhysicalPathKey(Long sourcePhysicalPathKey) {
        this.sourcePhysicalPathKey = sourcePhysicalPathKey;
    }

    public Long getSourceVideoPhysicalPathKey() {
        return sourceVideoPhysicalPathKey;
    }

    public void setSourceVideoPhysicalPathKey(Long sourceVideoPhysicalPathKey) {
        this.sourceVideoPhysicalPathKey = sourceVideoPhysicalPathKey;
    }

    public Long getSourceImagePhysicalPathKey() {
        return sourceImagePhysicalPathKey;
    }

    public void setSourceImagePhysicalPathKey(Long sourceImagePhysicalPathKey) {
        this.sourceImagePhysicalPathKey = sourceImagePhysicalPathKey;
    }

    public String getEncodingPriority() {
        return encodingPriority;
    }

    public void setEncodingPriority(String encodingPriority) {
        this.encodingPriority = encodingPriority;
    }

    public int getEncodingPriorityCode() {
        return encodingPriorityCode;
    }

    public void setEncodingPriorityCode(int encodingPriorityCode) {
        this.encodingPriorityCode = encodingPriorityCode;
    }

    public int getMaxEncodingPriorityCode() {
        return maxEncodingPriorityCode;
    }

    public void setMaxEncodingPriorityCode(int maxEncodingPriorityCode) {
        this.maxEncodingPriorityCode = maxEncodingPriorityCode;
    }

    public String getLiveURL() {
        return liveURL;
    }

    public void setLiveURL(String liveURL) {
        this.liveURL = liveURL;
    }

    public String getOutputFileFormat() {
        return outputFileFormat;
    }

    public void setOutputFileFormat(String outputFileFormat) {
        this.outputFileFormat = outputFileFormat;
    }

    public Long getSegmentDurationInSeconds() {
        return segmentDurationInSeconds;
    }

    public void setSegmentDurationInSeconds(Long segmentDurationInSeconds) {
        this.segmentDurationInSeconds = segmentDurationInSeconds;
    }

    public Date getRecordingPeriodStart() {
        return recordingPeriodStart;
    }

    public void setRecordingPeriodStart(Date recordingPeriodStart) {
        this.recordingPeriodStart = recordingPeriodStart;
    }

    public Date getRecordingPeriodEnd() {
        return recordingPeriodEnd;
    }

    public void setRecordingPeriodEnd(Date recordingPeriodEnd) {
        this.recordingPeriodEnd = recordingPeriodEnd;
    }
}
