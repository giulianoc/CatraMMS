package com.catramms.backing.conf;

import com.catramms.backing.common.SessionUtils;
import com.catramms.backing.common.Workspace;
import com.catramms.backing.entity.EMailConf;
import com.catramms.backing.entity.WorkspaceDetails;
import com.catramms.utility.catramms.CatraMMS;
import org.apache.log4j.Logger;

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
public class EmailView extends Workspace implements Serializable {

    // static because the class is Serializable
    private static final Logger mLogger = Logger.getLogger(EmailView.class);

    private Long editEMailConfKey;
    private String editEMailLabel;
    private String editEMailAddress;
    private String editEMailSubject;
    private String editEMailMessage;

    private List<EMailConf> emailConfList = new ArrayList<>();

    @PostConstruct
    public void init()
    {
        mLogger.debug("init");

    }

    public void prepareEditEMailConf(int rowId)
    {
        if (rowId != -1)
        {
            editEMailConfKey = emailConfList.get(rowId).getConfKey();
            editEMailLabel = emailConfList.get(rowId).getLabel();
            editEMailAddress = emailConfList.get(rowId).getAddress();
            editEMailSubject = emailConfList.get(rowId).getSubject();
            editEMailMessage = emailConfList.get(rowId).getMessage();
        }
        else
        {
            editEMailConfKey = new Long(-1);
            editEMailLabel = "";
            editEMailAddress = "";
            editEMailSubject = "";
            editEMailMessage = "";
        }
    }

    public void addModifyEMailConf()
    {
        try
        {
            Long userKey = SessionUtils.getUserProfile().getUserKey();
            String apiKey = SessionUtils.getCurrentWorkspaceDetails().getApiKey();

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
                WorkspaceDetails currentWorkspaceDetails = SessionUtils.getCurrentWorkspaceDetails();

                CatraMMS catraMMS = new CatraMMS();
                if (editEMailConfKey == -1)
                    catraMMS.addEMailConf(
                            username, password,
                            editEMailLabel, editEMailAddress,
                            editEMailSubject, editEMailMessage);
                else
                    catraMMS.modifyEMailConf(
                            username, password,
                            editEMailConfKey, editEMailLabel, editEMailAddress,
                            editEMailSubject, editEMailMessage);

                fillList(false);

                FacesMessage message = new FacesMessage(FacesMessage.SEVERITY_ERROR, "EMail",
                        "Added successful");
                FacesContext.getCurrentInstance().addMessage(null, message);
            }
        }
        catch (Exception e)
        {
            String errorMessage = "Exception: " + e;
            mLogger.error(errorMessage);

            FacesMessage message = new FacesMessage(FacesMessage.SEVERITY_ERROR, "EMail",
                    "Add failed");
            FacesContext.getCurrentInstance().addMessage(null, message);
        }
    }

    public void removeEMailConf(Long confKey)
    {
        try
        {
            Long userKey = SessionUtils.getUserProfile().getUserKey();
            String apiKey = SessionUtils.getCurrentWorkspaceDetails().getApiKey();

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
                catraMMS.removeEMailConf(
                        username, password, confKey);

                fillList(false);

                FacesMessage message = new FacesMessage(FacesMessage.SEVERITY_ERROR, "EMail",
                        "Removed successful");
                FacesContext.getCurrentInstance().addMessage(null, message);
            }
        }
        catch (Exception e)
        {
            String errorMessage = "Exception: " + e;
            mLogger.error(errorMessage);

            FacesMessage message = new FacesMessage(FacesMessage.SEVERITY_ERROR, "EMail",
                    "Remove failed");
            FacesContext.getCurrentInstance().addMessage(null, message);
        }
    }

    public void fillList(boolean toBeRedirected)
    {
        SimpleDateFormat simpleDateFormat = new SimpleDateFormat("yyyy-MM-dd HH:mm:ss");

        mLogger.info("fillList"
                + ", toBeRedirected: " + toBeRedirected);

        if (toBeRedirected)
        {
            try
            {
                // SimpleDateFormat simpleDateFormat_1 = new SimpleDateFormat("yyyy-MM-dd'T'HH:mm:ss");

                String url = "email.xhtml"
                        ;
                mLogger.info("Redirect to " + url);
                FacesContext.getCurrentInstance().getExternalContext().redirect(url);
            }
            catch (Exception e)
            {
                String errorMessage = "Exception: " + e;
                mLogger.error(errorMessage);
            }
        }
        else
        {
            try
            {
                Long userKey = SessionUtils.getUserProfile().getUserKey();
                String apiKey = SessionUtils.getCurrentWorkspaceDetails().getApiKey();

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

                    emailConfList.clear();

                    CatraMMS catraMMS = new CatraMMS();
                    emailConfList = catraMMS.getEMailConf(
                            username, password);
                }
            }
            catch (Exception e)
            {
                String errorMessage = "Exception: " + e;
                mLogger.error(errorMessage);
            }
        }
    }

    public Long getEditEMailConfKey() {
        return editEMailConfKey;
    }

    public void setEditEMailConfKey(Long editEMailConfKey) {
        this.editEMailConfKey = editEMailConfKey;
    }

    public String getEditEMailLabel() {
        return editEMailLabel;
    }

    public void setEditEMailLabel(String editEMailLabel) {
        this.editEMailLabel = editEMailLabel;
    }

    public String getEditEMailAddress() {
        return editEMailAddress;
    }

    public void setEditEMailAddress(String editEMailAddress) {
        this.editEMailAddress = editEMailAddress;
    }

    public String getEditEMailSubject() {
        return editEMailSubject;
    }

    public void setEditEMailSubject(String editEMailSubject) {
        this.editEMailSubject = editEMailSubject;
    }

    public String getEditEMailMessage() {
        return editEMailMessage;
    }

    public void setEditEMailMessage(String editEMailMessage) {
        this.editEMailMessage = editEMailMessage;
    }

    public List<EMailConf> getEmailConfList() {
        return emailConfList;
    }

    public void setEmailConfList(List<EMailConf> emailConfList) {
        this.emailConfList = emailConfList;
    }
}
