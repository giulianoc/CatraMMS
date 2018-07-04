package com.catramms.backing;

import com.catramms.backing.common.SessionUtils;
import com.catramms.backing.entity.WorkspaceDetails;
import com.catramms.utility.catramms.CatraMMS;
import org.apache.log4j.Logger;

import java.io.Serializable;
import java.util.ArrayList;
import java.util.List;

import javax.annotation.PostConstruct;
import javax.faces.application.FacesMessage;
import javax.faces.bean.ManagedBean;
import javax.faces.bean.SessionScoped;
import javax.faces.context.FacesContext;
import javax.servlet.http.HttpSession;


@ManagedBean
@SessionScoped
public class Login implements Serializable {

    private static final Logger mLogger = Logger.getLogger(Login.class);

    private static final long serialVersionUID = 1094801825228386363L;

    private String originURI;
    private String pwd;
    private String msg;
    private String emailAddress;
    private String userName;

    private String registrationName;
    private String registrationEMail;
    private String registrationPassword;
    private String registrationCountry;
    private Long registrationUserKey;
    private String registrationConfirmationCode;
    private String registrationWorkspaceName;

    @PostConstruct
    public void init()
    {
        mLogger.debug("init");

        originURI = "";
        pwd = "";
        msg = "";
        emailAddress = "";
        userName = "";

        registrationName = "";
        registrationEMail = "";
        registrationPassword = "";
        registrationCountry = "";
        registrationUserKey = new Long(0);
        registrationConfirmationCode = "";
        registrationWorkspaceName = "";
    }

    //validate login
    public String validateUsernamePassword()
    {
        boolean valid;
        Long userKey = null;
        List<WorkspaceDetails> workspaceDetailsList = new ArrayList<>();

        try
        {
            CatraMMS catraMMS = new CatraMMS();
            List<Object> userKeyAndPassword = catraMMS.login(emailAddress, pwd, workspaceDetailsList);

            userKey = (Long) userKeyAndPassword.get(0);
            userName = (String) userKeyAndPassword.get(1);

            if (workspaceDetailsList.size() == 0)
                throw new Exception("No Workspace available");

            valid = true;
        }
        catch (Exception e)
        {
            String errorMessage = "Login MMS failed. Exception: " + e;
            mLogger.error(errorMessage);

            valid = false;
        }

        mLogger.info("OriginURI: " + originURI);
        if (valid)
        {
            HttpSession session = SessionUtils.getSession();
            session.setAttribute("username", userName);
            session.setAttribute("userKey", userKey);
            session.setAttribute("workspaceDetailsList", workspaceDetailsList);
            session.setAttribute("currentWorkspaceDetails", workspaceDetailsList.get(0));

            try
            {
                if (originURI == null || originURI.equalsIgnoreCase("")
                        || originURI.equalsIgnoreCase("/catramms/login.xhtml"))
                    originURI = "mediaItems.xhtml";

                mLogger.info("Redirect to " + originURI);
                FacesContext.getCurrentInstance().getExternalContext().redirect(originURI);
            }
            catch (Exception e)
            {
                mLogger.error("Redirect failed: " + originURI);
            }
        }
        else
        {
            FacesContext.getCurrentInstance().addMessage(
                    null,
                    new FacesMessage(FacesMessage.SEVERITY_WARN,
                            "Incorrect Username and Passowrd",
                            "Please enter correct username and Password"));

            try
            {
                originURI = "/catramms/login.xhtml";

                mLogger.info("Redirect to " + originURI);
                FacesContext.getCurrentInstance().getExternalContext().redirect(originURI);
            }
            catch (Exception e)
            {
                mLogger.error("Redirect failed: " + originURI);
            }
            // return "login";
        }

        return "";
    }

    //logout event, invalidate session
    public void logout()
    {
        try
        {
            HttpSession session = SessionUtils.getSession();
            session.invalidate();

            String url = "login.xhtml";

            mLogger.info("Redirect to " + url);
            FacesContext.getCurrentInstance().getExternalContext().redirect(url);
            // return "login";
        }
        catch (Exception e)
        {
            mLogger.error("Redirect failed: " + originURI);
        }
    }

    public void register()
    {
        try
        {
            mLogger.info("register...");

            CatraMMS catraMMS = new CatraMMS();
            registrationUserKey = catraMMS.register(
                    registrationName, registrationEMail, registrationPassword,
                    registrationCountry, registrationWorkspaceName);

            FacesMessage message = new FacesMessage(FacesMessage.SEVERITY_ERROR,
                    "Registration", "Success");
            FacesContext context = FacesContext.getCurrentInstance();
            context.addMessage(null, message);
        }
        catch (Exception e)
        {
            String errorMessage = "registration failed: " + e;
            mLogger.error(errorMessage);

            FacesMessage message = new FacesMessage(FacesMessage.SEVERITY_ERROR,
                    "Registration", errorMessage);
            FacesContext context = FacesContext.getCurrentInstance();
            context.addMessage(null, message);
        }
    }

    public void confirmUser()
    {
        try
        {
            CatraMMS catraMMS = new CatraMMS();
            String apyKey = catraMMS.confirmUser(
                    registrationUserKey, registrationConfirmationCode);

            FacesMessage message = new FacesMessage(FacesMessage.SEVERITY_ERROR,
                    "Confirm", "Confirm Success");
            FacesContext context = FacesContext.getCurrentInstance();
            context.addMessage(null, message);
        }
        catch (Exception e)
        {
            String errorMessage = "confirmUser failed: " + e;
            mLogger.error(errorMessage);

            FacesMessage message = new FacesMessage(FacesMessage.SEVERITY_ERROR,
                    "Confirm", errorMessage);
            FacesContext context = FacesContext.getCurrentInstance();
            context.addMessage(null, message);
        }
    }
    public String getOriginURI() {
        return originURI;
    }

    public void setOriginURI(String originURI) {
        this.originURI = originURI;
    }

    public String getPwd() {
        return pwd;
    }

    public void setPwd(String pwd) {
        this.pwd = pwd;
    }

    public String getMsg() {
        return msg;
    }

    public void setMsg(String msg) {
        this.msg = msg;
    }

    public String getEmailAddress() {
        return emailAddress;
    }

    public void setEmailAddress(String emailAddress) {
        this.emailAddress = emailAddress;
    }

    public String getUserName() {
        return userName;
    }

    public void setUserName(String userName) {
        this.userName = userName;
    }

    public String getRegistrationName() {
        return registrationName;
    }

    public void setRegistrationName(String registrationName) {
        this.registrationName = registrationName;
    }

    public String getRegistrationEMail() {
        return registrationEMail;
    }

    public void setRegistrationEMail(String registrationEMail) {
        this.registrationEMail = registrationEMail;
    }

    public String getRegistrationPassword() {
        return registrationPassword;
    }

    public void setRegistrationPassword(String registrationPassword) {
        this.registrationPassword = registrationPassword;
    }

    public String getRegistrationCountry() {
        return registrationCountry;
    }

    public void setRegistrationCountry(String registrationCountry) {
        this.registrationCountry = registrationCountry;
    }

    public Long getRegistrationUserKey() {
        return registrationUserKey;
    }

    public void setRegistrationUserKey(Long registrationUserKey) {
        this.registrationUserKey = registrationUserKey;
    }

    public String getRegistrationConfirmationCode() {
        return registrationConfirmationCode;
    }

    public void setRegistrationConfirmationCode(String registrationConfirmationCode) {
        this.registrationConfirmationCode = registrationConfirmationCode;
    }

    public String getRegistrationWorkspaceName() {
        return registrationWorkspaceName;
    }

    public void setRegistrationWorkspaceName(String registrationWorkspaceName) {
        this.registrationWorkspaceName = registrationWorkspaceName;
    }
}
