package com.catramms.backing.conf;

import com.catramms.backing.common.SessionUtils;
import com.catramms.backing.common.Workspace;
import com.catramms.backing.entity.FacebookConf;
import com.catramms.backing.entity.WorkspaceDetails;
import com.catramms.utility.catramms.CatraMMS;
import org.apache.log4j.Logger;

import javax.annotation.PostConstruct;
import javax.faces.application.FacesMessage;
import javax.faces.bean.ManagedBean;
import javax.faces.bean.ViewScoped;
import javax.faces.context.FacesContext;
import javax.servlet.http.HttpSession;
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
public class FacebookView extends Workspace implements Serializable {

    // static because the class is Serializable
    private static final Logger mLogger = Logger.getLogger(FacebookView.class);

    private Long editFacebookConfKey;
    private String editFacebookLabel;
    private String editFacebookPageToken;

    private List<FacebookConf> facebookConfList = new ArrayList<>();

    private String facebookAccessTokenURL;

    @PostConstruct
    public void init()
    {
        mLogger.debug("init");

        try
        {
            // this URL is configured inside the Facebook credentials
            String mmsFacebookCallbak = "https://mms-gui.catrasoft.cloud/rest/api/facebookCallback";

            // clientId (appId) is retrieved by the facebook app
            String clientId = "1862418063793547";

            // this is the you tube scope to upload a video in a page
            String scope = "manage_pages,publish_pages";

            String state = "{\"{st=state123abc,ds=123456789}\"}";

            /*
                https://www.facebook.com/v3.2/dialog/oauth?client_id=1862418063793547&redirect_uri=https://mms-gui.catrasoft.cloud/rest/api/youTubeCallback
                &state={"{st=state123abc,ds=123456789}"}&response_type=code%20token&scope=manage_pages,publish_pages
             */
            facebookAccessTokenURL = "https://www.facebook.com/v3.2/dialog/oauth"
                    + "?client_id=" + clientId
                    + "&redirect_uri=" + java.net.URLEncoder.encode(mmsFacebookCallbak, "UTF-8")
                    + "&state=" + java.net.URLEncoder.encode(state, "UTF-8")
                    + "&response_type=code"
                    + "&scope=" + java.net.URLEncoder.encode(scope, "UTF-8")
            ;
        }
        catch (Exception e)
        {
            String errorMessage = "Problems to set facebookAccessTokenURL. Exception: " + e;
            mLogger.error(errorMessage);

            return;
        }
    }

    public void prepareEditFacebookConf(int rowId)
    {
        if (rowId != -1)
        {
            editFacebookConfKey = facebookConfList.get(rowId).getConfKey();
            editFacebookLabel = facebookConfList.get(rowId).getLabel();
            editFacebookPageToken = facebookConfList.get(rowId).getPageToken();
        }
        else
        {
            editFacebookConfKey = new Long(-1);
            editFacebookLabel = "";
            editFacebookPageToken = "";
        }
    }

    public void addModifyFacebookConf()
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
                if (editFacebookConfKey == -1)
                    catraMMS.addFacebookConf(
                        username, password,
                        editFacebookLabel, editFacebookPageToken);
                else
                    catraMMS.modifyFacebookConf(
                            username, password,
                            editFacebookConfKey, editFacebookLabel, editFacebookPageToken);

                fillList(false);

                FacesMessage message = new FacesMessage(FacesMessage.SEVERITY_ERROR, "Facebook",
                        "Add successful");
                FacesContext.getCurrentInstance().addMessage(null, message);
            }
        }
        catch (Exception e)
        {
            String errorMessage = "Exception: " + e;
            mLogger.error(errorMessage);

            FacesMessage message = new FacesMessage(FacesMessage.SEVERITY_ERROR, "Facebook",
                    "Add failed");
            FacesContext.getCurrentInstance().addMessage(null, message);
        }
    }

    public void removeFacebookConf(Long confKey)
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
                catraMMS.removeFacebookConf(
                        username, password, confKey);

                fillList(false);

                FacesMessage message = new FacesMessage(FacesMessage.SEVERITY_ERROR, "Facebook",
                        "Remove successful");
                FacesContext.getCurrentInstance().addMessage(null, message);
            }
        }
        catch (Exception e)
        {
            String errorMessage = "Exception: " + e;
            mLogger.error(errorMessage);

            FacesMessage message = new FacesMessage(FacesMessage.SEVERITY_ERROR, "Facebook",
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

                String url = "facebook.xhtml"
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

                    facebookConfList.clear();

                    CatraMMS catraMMS = new CatraMMS();
                    facebookConfList = catraMMS.getFacebookConf(
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

    public Long getEditFacebookConfKey() {
        return editFacebookConfKey;
    }

    public void setEditFacebookConfKey(Long editFacebookConfKey) {
        this.editFacebookConfKey = editFacebookConfKey;
    }

    public String getEditFacebookLabel() {
        return editFacebookLabel;
    }

    public void setEditFacebookLabel(String editFacebookLabel) {
        this.editFacebookLabel = editFacebookLabel;
    }

    public String getEditFacebookPageToken() {
        return editFacebookPageToken;
    }

    public void setEditFacebookPageToken(String editFacebookPageToken) {
        this.editFacebookPageToken = editFacebookPageToken;
    }

    public List<FacebookConf> getFacebookConfList() {
        return facebookConfList;
    }

    public void setFacebookConfList(List<FacebookConf> facebookConfList) {
        this.facebookConfList = facebookConfList;
    }

    public String getFacebookAccessTokenURL() {
        return facebookAccessTokenURL;
    }

    public void setFacebookAccessTokenURL(String facebookAccessTokenURL) {
        this.facebookAccessTokenURL = facebookAccessTokenURL;
    }
}
