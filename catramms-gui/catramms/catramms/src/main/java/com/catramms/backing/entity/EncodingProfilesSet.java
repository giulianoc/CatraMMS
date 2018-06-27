package com.catramms.backing.entity;

import java.io.Serializable;
import java.util.ArrayList;
import java.util.Date;
import java.util.List;

/**
 * Created by multi on 07.06.18.
 */
public class EncodingProfilesSet implements Serializable {

    private Long encodingProfilesSetKey;
    private String label;
    private String contentType;

    private List<EncodingProfile> encodingProfileList = new ArrayList<>();


    public Long getEncodingProfilesSetKey() {
        return encodingProfilesSetKey;
    }

    public void setEncodingProfilesSetKey(Long encodingProfilesSetKey) {
        this.encodingProfilesSetKey = encodingProfilesSetKey;
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

    public List<EncodingProfile> getEncodingProfileList() {
        return encodingProfileList;
    }

    public void setEncodingProfileList(List<EncodingProfile> encodingProfileList) {
        this.encodingProfileList = encodingProfileList;
    }
}
