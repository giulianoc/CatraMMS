package com.catramms.backing.conf;

import com.catramms.backing.common.SessionUtils;
import com.catramms.backing.common.Workspace;
import com.catramms.backing.entity.FacebookConf;
import com.catramms.backing.entity.LiveURLConf;
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
public class LiveURLView extends Workspace implements Serializable {

    // static because the class is Serializable
    private static final Logger mLogger = Logger.getLogger(LiveURLView.class);

    private Long editLiveURLConfKey;
    private String editLiveURLLabel;
    private String editLiveURLLiveURL;

    private List<LiveURLConf> liveURLConfList = new ArrayList<>();

    @PostConstruct
    public void init()
    {
        mLogger.debug("init");

    }

    public void prepareEditLiveURLConf(int rowId)
    {
        if (rowId != -1)
        {
            editLiveURLConfKey = liveURLConfList.get(rowId).getConfKey();
            editLiveURLLabel = liveURLConfList.get(rowId).getLabel();
            editLiveURLLiveURL = liveURLConfList.get(rowId).getLiveURL();
        }
        else
        {
            editLiveURLConfKey = new Long(-1);
            editLiveURLLabel = "";
            editLiveURLLiveURL = "";
        }
    }

    public void addModifyLiveURLConf()
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
                if (editLiveURLConfKey == -1)
                    catraMMS.addLiveURLConf(
                        username, password,
                        editLiveURLLabel, editLiveURLLiveURL);
                else
                    catraMMS.modifyLiveURLConf(
                            username, password,
                            editLiveURLConfKey, editLiveURLLabel, editLiveURLLiveURL);

                fillList(false);

                FacesMessage message = new FacesMessage(FacesMessage.SEVERITY_ERROR, "LiveURL",
                        "Added successful");
                FacesContext.getCurrentInstance().addMessage(null, message);
            }
        }
        catch (Exception e)
        {
            String errorMessage = "Exception: " + e;
            mLogger.error(errorMessage);

            FacesMessage message = new FacesMessage(FacesMessage.SEVERITY_ERROR, "LiveURL",
                    "Add failed");
            FacesContext.getCurrentInstance().addMessage(null, message);
        }
    }

    public void removeLiveURLConf(Long confKey)
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
                catraMMS.removeLiveURLConf(
                        username, password, confKey);

                fillList(false);

                FacesMessage message = new FacesMessage(FacesMessage.SEVERITY_ERROR, "LiveURL",
                        "Removed successful");
                FacesContext.getCurrentInstance().addMessage(null, message);
            }
        }
        catch (Exception e)
        {
            String errorMessage = "Exception: " + e;
            mLogger.error(errorMessage);

            FacesMessage message = new FacesMessage(FacesMessage.SEVERITY_ERROR, "LiveURL",
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

                String url = "liveURL.xhtml"
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

                    liveURLConfList.clear();

                    CatraMMS catraMMS = new CatraMMS();
                    liveURLConfList = catraMMS.getLiveURLConf(
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

    public Long getEditLiveURLConfKey() {
        return editLiveURLConfKey;
    }

    public void setEditLiveURLConfKey(Long editLiveURLConfKey) {
        this.editLiveURLConfKey = editLiveURLConfKey;
    }

    public String getEditLiveURLLabel() {
        return editLiveURLLabel;
    }

    public void setEditLiveURLLabel(String editLiveURLLabel) {
        this.editLiveURLLabel = editLiveURLLabel;
    }

    public String getEditLiveURLLiveURL() {
        return editLiveURLLiveURL;
    }

    public void setEditLiveURLLiveURL(String editLiveURLLiveURL) {
        this.editLiveURLLiveURL = editLiveURLLiveURL;
    }

    public List<LiveURLConf> getLiveURLConfList() {
        return liveURLConfList;
    }

    public void setLiveURLConfList(List<LiveURLConf> liveURLConfList) {
        this.liveURLConfList = liveURLConfList;
    }
}
