package com.catramms.backing;

import com.catramms.backing.common.SessionUtils;
import com.catramms.backing.entity.WorkspaceDetails;
import com.catramms.utility.catramms.CatraMMS;
import org.apache.log4j.Logger;

import java.io.Serializable;
import java.util.ArrayList;
import java.util.List;

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
    private String user;

    //validate login
    public String validateUsernamePassword()
    {
        boolean valid;
        Long userKey = null;
        List<WorkspaceDetails> workspaceDetailsList = new ArrayList<>();

        try
        {
            CatraMMS catraMMS = new CatraMMS();
            userKey = catraMMS.login(user, pwd, workspaceDetailsList);

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
            session.setAttribute("username", user);
            session.setAttribute("userKey", userKey);
            session.setAttribute("workspaceDetailsList", workspaceDetailsList);
            session.setAttribute("workspaceName", workspaceDetailsList.get(0).getName());

            try
            {
                if (originURI == null || originURI.equalsIgnoreCase(""))
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
    public String logout()
    {
        HttpSession session = SessionUtils.getSession();
        session.invalidate();

        return "login";
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

    public String getUser() {
        return user;
    }

    public void setUser(String user) {
        this.user = user;
    }

}
