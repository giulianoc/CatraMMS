package com.catramms.backing.conf;

import com.catramms.backing.common.SessionUtils;
import com.catramms.backing.common.Workspace;
import com.catramms.backing.entity.EncodingProfile;
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

    @PostConstruct
    public void init()
    {
        mLogger.debug("init");

    }

    public void createWorkspace()
    {
        String jsonCreateWorkspace;

        try
        {
            JSONObject joCreateWorkspace = new JSONObject();

            joCreateWorkspace.put("WorkspaceName", createWorkspaceName);

            jsonCreateWorkspace = joCreateWorkspace.toString(4);
        }
        catch (Exception e)
        {
            String errorMessage = "Exception: " + e;
            mLogger.error(errorMessage);

            FacesMessage message = new FacesMessage(FacesMessage.SEVERITY_ERROR, "Create Workspace",
                    "Add failed");
            FacesContext.getCurrentInstance().addMessage(null, message);

            return;
        }

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
                        jsonCreateWorkspace);

                FacesMessage message = new FacesMessage(FacesMessage.SEVERITY_ERROR, "Encoding Profile",
                        "Add successful");
                FacesContext.getCurrentInstance().addMessage(null, message);
            }
        }
        catch (Exception e)
        {
            String errorMessage = "Exception: " + e;
            mLogger.error(errorMessage);

            FacesMessage message = new FacesMessage(FacesMessage.SEVERITY_ERROR, "Encoding Profile",
                    "Add failed");
            FacesContext.getCurrentInstance().addMessage(null, message);
        }
    }

    public String getCreateWorkspaceName() {
        return createWorkspaceName;
    }

    public void setCreateWorkspaceName(String createWorkspaceName) {
        this.createWorkspaceName = createWorkspaceName;
    }
}
