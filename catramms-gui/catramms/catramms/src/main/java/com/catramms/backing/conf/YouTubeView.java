package com.catramms.backing.conf;

import com.catramms.backing.common.SessionUtils;
import com.catramms.backing.common.Workspace;
import com.catramms.backing.entity.WorkspaceDetails;
import com.catramms.backing.entity.YouTubeConf;
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
public class YouTubeView extends Workspace implements Serializable {

    // static because the class is Serializable
    private static final Logger mLogger = Logger.getLogger(YouTubeView.class);

    private Long editYouTubeConfKey;
    private String editYouTubeLabel;
    private String editYouTubeRefreshToken;

    private List<YouTubeConf> youTubeConfList = new ArrayList<>();

    private String youTubeAccessTokenURL;

    @PostConstruct
    public void init()
    {
        mLogger.debug("init");

        try
        {
            // this URL is configured inside the YouTube credentials
            String mmsYouTubeCallbak = "https://mms-gui.catrasoft.cloud/rest/api/youTubeCallback";

            // clientId is retrieved by the credentials
            String clientId = "700586767360-96om12ccsf16m41qijrdagkk0oqf2o7m.apps.googleusercontent.com";

            // this is the you tube scope to upload a video
            String scope = "https://www.googleapis.com/auth/youtube https://www.googleapis.com/auth/youtube.upload";

            youTubeAccessTokenURL = "https://accounts.google.com/o/oauth2/v2/auth"
                    + "?redirect_uri=" + java.net.URLEncoder.encode(mmsYouTubeCallbak, "UTF-8")
                    + "&prompt=consent"
                    + "&response_type=code"
                    + "&client_id=" + clientId
                    + "&scope=" + java.net.URLEncoder.encode(scope, "UTF-8")
                    + "&access_type=offline"
            ;
        }
        catch (Exception e)
        {
            String errorMessage = "Problems to set youTubeAccessTokenURL. Exception: " + e;
            mLogger.error(errorMessage);

            return;
        }
    }

    public void prepareEditYouTubeConf(int rowId)
    {
        if (rowId != -1)
        {
            editYouTubeConfKey = youTubeConfList.get(rowId).getConfKey();
            editYouTubeLabel = youTubeConfList.get(rowId).getLabel();
            editYouTubeRefreshToken = youTubeConfList.get(rowId).getRefreshToken();
        }
        else
        {
            editYouTubeConfKey = new Long(-1);
            editYouTubeLabel = "";
            editYouTubeRefreshToken = "";
        }
    }

    public void addModifyYouTubeConf()
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
                HttpSession session = SessionUtils.getSession();

                WorkspaceDetails currentWorkspaceDetails = (WorkspaceDetails) session.getAttribute("currentWorkspaceDetails");

                CatraMMS catraMMS = new CatraMMS();
                if (editYouTubeConfKey == -1)
                    catraMMS.addYouTubeConf(
                        username, password,
                        editYouTubeLabel, editYouTubeRefreshToken);
                else
                    catraMMS.modifyYouTubeConf(
                            username, password,
                            editYouTubeConfKey, editYouTubeLabel, editYouTubeRefreshToken);

                fillList(false);

                FacesMessage message = new FacesMessage(FacesMessage.SEVERITY_ERROR, "YouTube",
                        "Add successful");
                FacesContext.getCurrentInstance().addMessage(null, message);
            }
        }
        catch (Exception e)
        {
            String errorMessage = "Exception: " + e;
            mLogger.error(errorMessage);

            FacesMessage message = new FacesMessage(FacesMessage.SEVERITY_ERROR, "YouTube",
                    "Add failed");
            FacesContext.getCurrentInstance().addMessage(null, message);
        }
    }

    public void removeYouTubeConf(Long confKey)
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
                catraMMS.removeYouTubeConf(
                        username, password, confKey);

                fillList(false);

                FacesMessage message = new FacesMessage(FacesMessage.SEVERITY_ERROR, "YouTube",
                        "Remove successful");
                FacesContext.getCurrentInstance().addMessage(null, message);
            }
        }
        catch (Exception e)
        {
            String errorMessage = "Exception: " + e;
            mLogger.error(errorMessage);

            FacesMessage message = new FacesMessage(FacesMessage.SEVERITY_ERROR, "YouTube",
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

                String url = "youTube.xhtml"
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

                    youTubeConfList.clear();

                    CatraMMS catraMMS = new CatraMMS();
                    youTubeConfList = catraMMS.getYouTubeConf(
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

    public Long getEditYouTubeConfKey() {
        return editYouTubeConfKey;
    }

    public void setEditYouTubeConfKey(Long editYouTubeConfKey) {
        this.editYouTubeConfKey = editYouTubeConfKey;
    }

    public String getEditYouTubeLabel() {
        return editYouTubeLabel;
    }

    public void setEditYouTubeLabel(String editYouTubeLabel) {
        this.editYouTubeLabel = editYouTubeLabel;
    }

    public String getEditYouTubeRefreshToken() {
        return editYouTubeRefreshToken;
    }

    public void setEditYouTubeRefreshToken(String editYouTubeRefreshToken) {
        this.editYouTubeRefreshToken = editYouTubeRefreshToken;
    }

    public List<YouTubeConf> getYouTubeConfList() {
        return youTubeConfList;
    }

    public void setYouTubeConfList(List<YouTubeConf> youTubeConfList) {
        this.youTubeConfList = youTubeConfList;
    }

    public String getYouTubeAccessTokenURL() {
        return youTubeAccessTokenURL;
    }

    public void setYouTubeAccessTokenURL(String youTubeAccessTokenURL) {
        this.youTubeAccessTokenURL = youTubeAccessTokenURL;
    }
}
