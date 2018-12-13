package com.catramms.backing.conf;

import com.catramms.backing.common.SessionUtils;
import com.catramms.backing.common.Workspace;
import com.catramms.backing.entity.EncodingProfile;
import com.catramms.backing.entity.YouTubeDetails;
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
public class YouTubeDetailsView extends Workspace implements Serializable {

    // static because the class is Serializable
    private static final Logger mLogger = Logger.getLogger(YouTubeDetailsView.class);

    private String addYouTubeDetailsLabel;
    private String addYouTubeDetailsRefreshToken;

    private List<YouTubeDetails> youTubeDetailsList = new ArrayList<>();

    @PostConstruct
    public void init()
    {
        mLogger.debug("init");
    }

    public void addYouTubeDetails()
    {
        String jsonYouTubeDetails;

        try
        {
            JSONObject joYouTubeDetails = new JSONObject();

            joYouTubeDetails.put("Label", addYouTubeDetailsLabel);
            joYouTubeDetails.put("RefreshToken", addYouTubeDetailsRefreshToken);

            jsonYouTubeDetails = joYouTubeDetails.toString(4);

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
                catraMMS.addYouTubeDetails(
                        username, password,
                        jsonYouTubeDetails);

                fillList(false);

                FacesMessage message = new FacesMessage(FacesMessage.SEVERITY_ERROR, "YouTube Details",
                        "Add successful");
                FacesContext.getCurrentInstance().addMessage(null, message);
            }
        }
        catch (Exception e)
        {
            String errorMessage = "Exception: " + e;
            mLogger.error(errorMessage);

            FacesMessage message = new FacesMessage(FacesMessage.SEVERITY_ERROR, "YouTube Details",
                    "Add failed");
            FacesContext.getCurrentInstance().addMessage(null, message);
        }
    }

    public void removeYouTubeDetails(String label)
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
                catraMMS.removeEncodingProfile(
                        username, password, label);

                fillList(false);

                FacesMessage message = new FacesMessage(FacesMessage.SEVERITY_ERROR, "YouTube Details",
                        "Remove successful");
                FacesContext.getCurrentInstance().addMessage(null, message);
            }
        }
        catch (Exception e)
        {
            String errorMessage = "Exception: " + e;
            mLogger.error(errorMessage);

            FacesMessage message = new FacesMessage(FacesMessage.SEVERITY_ERROR, "YouTube Details",
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
                SimpleDateFormat simpleDateFormat_1 = new SimpleDateFormat("yyyy-MM-dd'T'HH:mm:ss");

                String url = "youTubeDetails.xhtml"
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

                        youTubeDetailsList.clear();

                        CatraMMS catraMMS = new CatraMMS();
                        catraMMS.getYouTubeDetails(
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
    }

    public String getAddYouTubeDetailsLabel() {
        return addYouTubeDetailsLabel;
    }

    public void setAddYouTubeDetailsLabel(String addYouTubeDetailsLabel) {
        this.addYouTubeDetailsLabel = addYouTubeDetailsLabel;
    }

    public String getAddYouTubeDetailsRefreshToken() {
        return addYouTubeDetailsRefreshToken;
    }

    public void setAddYouTubeDetailsRefreshToken(String addYouTubeDetailsRefreshToken) {
        this.addYouTubeDetailsRefreshToken = addYouTubeDetailsRefreshToken;
    }

    public List<YouTubeDetails> getYouTubeDetailsList() {
        return youTubeDetailsList;
    }

    public void setYouTubeDetailsList(List<YouTubeDetails> youTubeDetailsList) {
        this.youTubeDetailsList = youTubeDetailsList;
    }
}
