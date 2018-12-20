package com.catramms.backing.conf;

import com.catramms.backing.common.SessionUtils;
import com.catramms.backing.common.Workspace;
import com.catramms.backing.entity.EncodingProfile;
import com.catramms.backing.entity.WorkspaceDetails;
import com.catramms.utility.catramms.CatraMMS;
import org.apache.log4j.Logger;
import org.json.JSONObject;

import javax.annotation.PostConstruct;
import javax.faces.application.FacesMessage;
import javax.faces.bean.ManagedBean;
import javax.faces.bean.ViewScoped;
import javax.faces.context.FacesContext;
import java.io.Serializable;
import java.text.SimpleDateFormat;
import java.util.ArrayList;
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
public class YourWorkspaces extends Workspace implements Serializable {

    // static because the class is Serializable
    private static final Logger mLogger = Logger.getLogger(YourWorkspaces.class);

    private String createWorkspaceName;
    private String registrationConfirmationCode;

    private WorkspaceDetails workspaceDetailsToBeShown;
    private boolean newEnabled;
    private String newMaxEncodingPriority;
    private String newEncodingPeriod;
    private Long newMaxIngestionsNumber;
    private Long newMaxStorageInMB;
    private String newLanguageCode;
    private boolean newIngestWorkflow;
    private boolean newCreateProfiles;
    private boolean newDeliveryAuthorization;
    private boolean newShareWorkspace;
    private boolean newEditMedia;
    private boolean workspaceDetailsReadOnly;
    private List<String> encodingPriorityList = new ArrayList<>();
    private List<String> encodingPeriodList = new ArrayList<>();

    @PostConstruct
    public void init()
    {
        mLogger.debug("init");

        encodingPriorityList.add("Low");
        encodingPriorityList.add("Medium");
        encodingPriorityList.add("High");

        encodingPeriodList.add("Daily");
        encodingPeriodList.add("Weekly");
        encodingPeriodList.add("Monthly");
        encodingPeriodList.add("Yearly");

        workspaceDetailsReadOnly = true;
    }

    public void createWorkspace()
    {
        try
        {
            Long userKey = SessionUtils.getUserKey();
            String apiKey = SessionUtils.getAPIKey();

            if (userKey == null || apiKey == null || apiKey.equalsIgnoreCase(""))
            {
                mLogger.warn("no input to require mediaItemsKey"
                        + ", userKey: " + userKey
                        + ", apiKey: " + apiKey
                );
            }
            else
            {
                String username = userKey.toString();
                String password = apiKey;

                CatraMMS catraMMS = new CatraMMS();
                catraMMS.createWorkspace(
                        username, password,
                        createWorkspaceName);

                FacesMessage message = new FacesMessage(FacesMessage.SEVERITY_ERROR, "Create Workspace",
                        "Add successful");
                FacesContext.getCurrentInstance().addMessage(null, message);
            }
        }
        catch (Exception e)
        {
            String errorMessage = "Exception: " + e;
            mLogger.error(errorMessage);

            FacesMessage message = new FacesMessage(FacesMessage.SEVERITY_ERROR, "Create Workspace",
                    "Add failed");
            FacesContext.getCurrentInstance().addMessage(null, message);
        }
    }

    public void confirmRegistration()
    {
        try
        {
            Long userKey = SessionUtils.getUserKey();

            CatraMMS catraMMS = new CatraMMS();
            String apyKey = catraMMS.confirmRegistration(
                    userKey, registrationConfirmationCode);

            FacesMessage message = new FacesMessage(FacesMessage.SEVERITY_ERROR,
                    "Confirm", "Confirm Success");
            FacesContext context = FacesContext.getCurrentInstance();
            context.addMessage(null, message);
        }
        catch (Exception e)
        {
            String errorMessage = "confirmRegistration failed: " + e;
            mLogger.error(errorMessage);

            FacesMessage message = new FacesMessage(FacesMessage.SEVERITY_ERROR,
                    "Confirm", errorMessage);
            FacesContext context = FacesContext.getCurrentInstance();
            context.addMessage(null, message);
        }
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

    public void prepareWorkspaceDetailsToBeShown(WorkspaceDetails workspaceDetails, boolean isReadOnly)
    {
        workspaceDetailsReadOnly = isReadOnly;

        this.workspaceDetailsToBeShown = workspaceDetails;

        newEnabled = workspaceDetails.getEnabled();
        newMaxEncodingPriority = workspaceDetails.getMaxEncodingPriority();
        newEncodingPeriod = workspaceDetails.getEncodingPeriod();
        newMaxIngestionsNumber = workspaceDetails.getMaxIngestionsNumber();
        newMaxStorageInMB = workspaceDetails.getMaxStorageInMB();
        newLanguageCode = workspaceDetails.getLanguageCode();
        newIngestWorkflow = workspaceDetails.getIngestWorkflow();
        newCreateProfiles = workspaceDetails.getCreateProfiles();
        newDeliveryAuthorization = workspaceDetails.getDeliveryAuthorization();
        newShareWorkspace = workspaceDetails.getShareWorkspace();
        newEditMedia = workspaceDetails.getEditMedia();

    }

    public String getCreateWorkspaceName() {
        return createWorkspaceName;
    }

    public void setCreateWorkspaceName(String createWorkspaceName) {
        this.createWorkspaceName = createWorkspaceName;
    }

    public String getRegistrationConfirmationCode() {
        return registrationConfirmationCode;
    }

    public void setRegistrationConfirmationCode(String registrationConfirmationCode) {
        this.registrationConfirmationCode = registrationConfirmationCode;
    }

    public WorkspaceDetails getWorkspaceDetailsToBeShown() {
        return workspaceDetailsToBeShown;
    }

    public void setWorkspaceDetailsToBeShown(WorkspaceDetails workspaceDetailsToBeShown) {
        this.workspaceDetailsToBeShown = workspaceDetailsToBeShown;
    }

    public List<String> getEncodingPriorityList() {
        return encodingPriorityList;
    }

    public void setEncodingPriorityList(List<String> encodingPriorityList) {
        this.encodingPriorityList = encodingPriorityList;
    }

    public List<String> getEncodingPeriodList() {
        return encodingPeriodList;
    }

    public void setEncodingPeriodList(List<String> encodingPeriodList) {
        this.encodingPeriodList = encodingPeriodList;
    }

    public boolean isWorkspaceDetailsReadOnly() {
        return workspaceDetailsReadOnly;
    }

    public void setWorkspaceDetailsReadOnly(boolean workspaceDetailsReadOnly) {
        this.workspaceDetailsReadOnly = workspaceDetailsReadOnly;
    }

    public boolean isNewEnabled() {
        return newEnabled;
    }

    public void setNewEnabled(boolean newEnabled) {
        this.newEnabled = newEnabled;
    }

    public String getNewMaxEncodingPriority() {
        return newMaxEncodingPriority;
    }

    public void setNewMaxEncodingPriority(String newMaxEncodingPriority) {
        this.newMaxEncodingPriority = newMaxEncodingPriority;
    }

    public String getNewEncodingPeriod() {
        return newEncodingPeriod;
    }

    public void setNewEncodingPeriod(String newEncodingPeriod) {
        this.newEncodingPeriod = newEncodingPeriod;
    }

    public Long getNewMaxIngestionsNumber() {
        return newMaxIngestionsNumber;
    }

    public void setNewMaxIngestionsNumber(Long newMaxIngestionsNumber) {
        this.newMaxIngestionsNumber = newMaxIngestionsNumber;
    }

    public Long getNewMaxStorageInMB() {
        return newMaxStorageInMB;
    }

    public void setNewMaxStorageInMB(Long newMaxStorageInMB) {
        this.newMaxStorageInMB = newMaxStorageInMB;
    }

    public String getNewLanguageCode() {
        return newLanguageCode;
    }

    public void setNewLanguageCode(String newLanguageCode) {
        this.newLanguageCode = newLanguageCode;
    }

    public boolean isNewIngestWorkflow() {
        return newIngestWorkflow;
    }

    public void setNewIngestWorkflow(boolean newIngestWorkflow) {
        this.newIngestWorkflow = newIngestWorkflow;
    }

    public boolean isNewCreateProfiles() {
        return newCreateProfiles;
    }

    public void setNewCreateProfiles(boolean newCreateProfiles) {
        this.newCreateProfiles = newCreateProfiles;
    }

    public boolean isNewDeliveryAuthorization() {
        return newDeliveryAuthorization;
    }

    public void setNewDeliveryAuthorization(boolean newDeliveryAuthorization) {
        this.newDeliveryAuthorization = newDeliveryAuthorization;
    }

    public boolean isNewShareWorkspace() {
        return newShareWorkspace;
    }

    public void setNewShareWorkspace(boolean newShareWorkspace) {
        this.newShareWorkspace = newShareWorkspace;
    }

    public boolean isNewEditMedia() {
        return newEditMedia;
    }

    public void setNewEditMedia(boolean newEditMedia) {
        this.newEditMedia = newEditMedia;
    }
}
