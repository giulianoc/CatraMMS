package com.catramms.backing.entity;

import java.io.Serializable;

/**
 * Created by multi on 08.06.18.
 */
public class WorkspaceDetails implements Serializable {
    private Long workspaceKey;
    private String name;
    private String apiKey;
    private Boolean owner;
    private Boolean admin;
    private Boolean ingestWorkflow;
    private Boolean createProfiles;
    private Boolean deliveryAuthorization;
    private Boolean shareWorkspace;

    public Long getWorkspaceKey() {
        return workspaceKey;
    }

    public void setWorkspaceKey(Long workspaceKey) {
        this.workspaceKey = workspaceKey;
    }

    public Boolean getOwner() {
        return owner;
    }

    public void setOwner(Boolean owner) {
        this.owner = owner;
    }

    public String getName() {
        return name;
    }

    public void setName(String name) {
        this.name = name;
    }

    public String getApiKey() {
        return apiKey;
    }

    public void setApiKey(String apiKey) {
        this.apiKey = apiKey;
    }

    public Boolean getAdmin() {
        return admin;
    }

    public void setAdmin(Boolean admin) {
        this.admin = admin;
    }

    public Boolean getIngestWorkflow() {
        return ingestWorkflow;
    }

    public void setIngestWorkflow(Boolean ingestWorkflow) {
        this.ingestWorkflow = ingestWorkflow;
    }

    public Boolean getCreateProfiles() {
        return createProfiles;
    }

    public void setCreateProfiles(Boolean createProfiles) {
        this.createProfiles = createProfiles;
    }

    public Boolean getDeliveryAuthorization() {
        return deliveryAuthorization;
    }

    public void setDeliveryAuthorization(Boolean deliveryAuthorization) {
        this.deliveryAuthorization = deliveryAuthorization;
    }

    public Boolean getShareWorkspace() {
        return shareWorkspace;
    }

    public void setShareWorkspace(Boolean shareWorkspace) {
        this.shareWorkspace = shareWorkspace;
    }
}
