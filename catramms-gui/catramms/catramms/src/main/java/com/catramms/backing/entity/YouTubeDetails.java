package com.catramms.backing.entity;

import java.io.Serializable;

/**
 * Created by multi on 08.06.18.
 */
public class YouTubeDetails implements Serializable{

    private Long userKey;
    private String label;
    private String refreshToken;

    public Long getUserKey() {
        return userKey;
    }

    public void setUserKey(Long userKey) {
        this.userKey = userKey;
    }

    public String getLabel() {
        return label;
    }

    public void setLabel(String label) {
        this.label = label;
    }

    public String getRefreshToken() {
        return refreshToken;
    }

    public void setRefreshToken(String refreshToken) {
        this.refreshToken = refreshToken;
    }
}
