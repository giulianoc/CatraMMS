package com.catramms.backing.workflowEditor.utility;

import org.primefaces.model.UploadedFile;

import java.io.Serializable;

/**
 * Created by multi on 13.06.18.
 */
public class PushContent implements Serializable
{
    private String label;
    private String binaryPathName;


    public String getLabel() {
        return label;
    }

    public void setLabel(String label) {
        this.label = label;
    }

    public String getBinaryPathName() {
        return binaryPathName;
    }

    public void setBinaryPathName(String binaryPathName) {
        this.binaryPathName = binaryPathName;
    }
}
