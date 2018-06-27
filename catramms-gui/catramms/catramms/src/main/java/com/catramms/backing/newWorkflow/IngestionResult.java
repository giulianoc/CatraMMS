package com.catramms.backing.newWorkflow;

import java.io.Serializable;

/**
 * Created by multi on 13.06.18.
 */
public class IngestionResult implements Serializable {
    private Long key;
    private String label;

    public Long getKey() {
        return key;
    }

    public void setKey(Long key) {
        this.key = key;
    }

    public String getLabel() {
        return label;
    }

    public void setLabel(String label) {
        this.label = label;
    }
}
