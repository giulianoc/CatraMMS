package com.catramms.backing.conf;

import com.catramms.backing.common.SessionUtils;
import com.catramms.backing.common.Workspace;
import com.catramms.backing.entity.WorkspaceDetails;
import com.catramms.utility.catramms.CatraMMS;
import org.apache.log4j.Logger;

import javax.annotation.PostConstruct;
import javax.faces.application.FacesMessage;
import javax.faces.bean.ManagedBean;
import javax.faces.bean.ViewScoped;
import javax.faces.context.FacesContext;
import java.io.Serializable;
import java.util.ArrayList;
import java.util.Date;
import java.util.List;

/**
 * Created with IntelliJ IDEA.
 * User: multi
 * Date: 27/09/15
 * Time: 20:28
 * To change this template use File | Settings | File Templates.
 */
@ManagedBean
@ViewScoped
public class YourProfile extends Workspace implements Serializable {

    // static because the class is Serializable
    private static final Logger mLogger = Logger.getLogger(YourProfile.class);

    private WorkspaceDetails workspaceDetailsToBeShown;
    private String newName;
    private String newEmailAddress;
    private String newPassword;
    private String newCountry;
    private Date newExpirationDate;

    @PostConstruct
    public void init()
    {
        mLogger.debug("init");

        // init of the new fields
    }

    public void saveWorkspace(WorkspaceDetails workspaceDetails)
    {
        if (newEnabled != workspaceDetails.getEnabled()
            || newMaxEncodingPriority != workspaceDetails.getMaxEncodingPriority()
            || newEncodingPeriod != workspaceDetails.getEncodingPeriod()
            || newMaxIngestionsNumber != workspaceDetails.getMaxIngestionsNumber()
            || newMaxStorageInMB != workspaceDetails.getMaxStorageInMB()
            || newLanguageCode != workspaceDetails.getLanguageCode()
            || newIngestWorkflow != workspaceDetails.getIngestWorkflow()
            || newCreateProfiles != workspaceDetails.getCreateProfiles()
            || newDeliveryAuthorization != workspaceDetails.getDeliveryAuthorization()
            || newShareWorkspace != workspaceDetails.getShareWorkspace()
            || newEditMedia != workspaceDetails.getEditMedia()
        )
        {
            // save e reload list in session
        }
    }

    public String getNewName() {
        return newName;
    }

    public void setNewName(String newName) {
        this.newName = newName;
    }

    public String getNewEmailAddress() {
        return newEmailAddress;
    }

    public void setNewEmailAddress(String newEmailAddress) {
        this.newEmailAddress = newEmailAddress;
    }

    public String getNewPassword() {
        return newPassword;
    }

    public void setNewPassword(String newPassword) {
        this.newPassword = newPassword;
    }

    public String getNewCountry() {
        return newCountry;
    }

    public void setNewCountry(String newCountry) {
        this.newCountry = newCountry;
    }

    public Date getNewExpirationDate() {
        return newExpirationDate;
    }

    public void setNewExpirationDate(Date newExpirationDate) {
        this.newExpirationDate = newExpirationDate;
    }
}
