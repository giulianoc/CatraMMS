package com.catramms.backing.conf;

import com.catramms.backing.common.SessionUtils;
import com.catramms.backing.common.Workspace;
import com.catramms.backing.entity.UserProfile;
import com.catramms.utility.catramms.CatraMMS;
import org.apache.log4j.Logger;

import javax.annotation.PostConstruct;
import javax.faces.application.FacesMessage;
import javax.faces.bean.ManagedBean;
import javax.faces.bean.ViewScoped;
import javax.faces.context.FacesContext;
import javax.servlet.http.HttpSession;
import java.io.Serializable;

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

    private UserProfile userProfile;

    private String newName;
    private String newEmailAddress;
    private String newPassword;
    private String newCountry;

    @PostConstruct
    public void init()
    {
        mLogger.debug("init");

        userProfile = SessionUtils.getUserProfile();

        newName = userProfile.getName();
        newEmailAddress = userProfile.getEmailAddress();
        newPassword = userProfile.getPassword();
        newCountry = userProfile.getCountry();
    }

    public void updateUserProfile()
    {
        if (newName != userProfile.getName()
            || newEmailAddress != userProfile.getEmailAddress()
            || newPassword != userProfile.getPassword()
            || newCountry != userProfile.getCountry()
        )
        {
            try
            {
                Long userKey = SessionUtils.getUserProfile().getUserKey();
                String apiKey = SessionUtils.getCurrentWorkspaceDetails().getApiKey();

                if (userKey == null || apiKey == null || apiKey.equalsIgnoreCase(""))
                {
                    mLogger.warn("no input to require encodingJobs"
                                    + ", userKey: " + userKey
                                    + ", apiKey: " + apiKey
                    );
                }
                else
                {
                    String username = userKey.toString();
                    String password = apiKey;

                    CatraMMS catraMMS = new CatraMMS();
                    UserProfile userProfile = catraMMS.updateUserProfile(
                        username, password,
                        newName, newEmailAddress, newPassword, newCountry);

                    HttpSession session = SessionUtils.getSession();
                    session.setAttribute("userProfile", userProfile);
                }
            }
            catch (Exception e)
            {
                String errorMessage = "Exception: " + e;
                mLogger.error(errorMessage);

                FacesMessage message = new FacesMessage(FacesMessage.SEVERITY_ERROR,
                        "Update User Profile", errorMessage);
                FacesContext context = FacesContext.getCurrentInstance();
                context.addMessage(null, message);
            }
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

    public UserProfile getUserProfile() {
        return userProfile;
    }

    public void setUserProfile(UserProfile userProfile) {
        this.userProfile = userProfile;
    }
}
