package com.catramms.backing.entity;

import java.io.Serializable;

/**
 * Created by multi on 08.06.18.
 */
public class FacebookConf implements Serializable{

    private Long confKey;
    private String label;
    private String pageToken;

    public Long getConfKey() {
        return confKey;
    }

    public void setConfKey(Long confKey) {
        this.confKey = confKey;
    }

    public String getLabel() {
        return label;
    }

    public void setLabel(String label) {
        this.label = label;
    }

    public String getPageToken() {
        return pageToken;
    }

    public void setPageToken(String pageToken) {
        this.pageToken = pageToken;
    }
}
