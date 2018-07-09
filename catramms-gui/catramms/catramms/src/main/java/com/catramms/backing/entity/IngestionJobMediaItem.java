package com.catramms.backing.entity;

import java.io.Serializable;

/**
 * Created by multi on 09.06.18.
 */
public class IngestionJobMediaItem implements Serializable {
    private Long mediaItemKey;
    private Long physicalPathKey;

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
}
