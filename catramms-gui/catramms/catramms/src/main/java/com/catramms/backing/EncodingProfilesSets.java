package com.catramms.backing;

import com.catramms.backing.common.SessionUtils;
import com.catramms.backing.common.Workspace;
import com.catramms.backing.entity.EncodingProfile;
import com.catramms.backing.entity.EncodingProfilesSet;
import com.catramms.utility.catramms.CatraMMS;
import org.apache.log4j.Logger;
import org.json.JSONArray;
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
public class EncodingProfilesSets extends Workspace implements Serializable {

    // static because the class is Serializable
    private static final Logger mLogger = Logger.getLogger(EncodingProfilesSets.class);

    private String contentType;
    private List<String> contentTypesList;

    private List<EncodingProfilesSet> encodingProfilesSetList = new ArrayList<>();

    private String addProfilesSetLabel;
    private List<EncodingProfile> addProfilesSetEncodingProfileList = new ArrayList<>();
    private List<EncodingProfile> addProfilesSetEncodingProfileSelectedList = new ArrayList<>();

    @PostConstruct
    public void init()
    {
        mLogger.debug("init");

        {
            contentTypesList = new ArrayList<>();
            contentTypesList.add("video");
            contentTypesList.add("audio");
            contentTypesList.add("image");

            contentType = contentTypesList.get(0);
        }

        updateEncodingProfileList();
    }

    public void contentTypeChanged()
    {

        fillList(true);
        updateEncodingProfileList();
    }

    public void updateEncodingProfileList()
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

                addProfilesSetEncodingProfileList.clear();

                CatraMMS catraMMS = new CatraMMS();
                catraMMS.getEncodingProfiles(
                        username, password,
                        contentType, addProfilesSetEncodingProfileList);
            }
        }
        catch (Exception e)
        {
            String errorMessage = "Exception: " + e;
            mLogger.error(errorMessage);
        }
    }

    public void prepareAddEncodingProfilesSet()
    {
        addProfilesSetEncodingProfileSelectedList.clear();
    }

    public void addEncodingProfilesSet()
    {
        String jsonEncodingProfilesSet;

        try
        {
            mLogger.info("addEncodingProfilesSet. Label: " + addProfilesSetLabel);

            JSONObject joEncodingProfilesSet = new JSONObject();
            joEncodingProfilesSet.put("Label", addProfilesSetLabel);

            JSONArray jaEncodingProfiles = new JSONArray();
            joEncodingProfilesSet.put("Profiles", jaEncodingProfiles);

            for (EncodingProfile encodingProfile: addProfilesSetEncodingProfileSelectedList)
            {
                jaEncodingProfiles.put(encodingProfile.getLabel());
            }

            jsonEncodingProfilesSet = joEncodingProfilesSet.toString(4);
        }
        catch (Exception e)
        {
            String errorMessage = "Exception: " + e;
            mLogger.error(errorMessage);

            FacesMessage message = new FacesMessage(FacesMessage.SEVERITY_ERROR, "Encoding Profiles Set",
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
                catraMMS.addEncodingProfilesSet(
                        username, password,
                        contentType, jsonEncodingProfilesSet);

                fillList(false);

                FacesMessage message = new FacesMessage(FacesMessage.SEVERITY_ERROR, "Encoding Profiles Set",
                        "Add successful");
                FacesContext.getCurrentInstance().addMessage(null, message);
            }
        }
        catch (Exception e)
        {
            String errorMessage = "Exception: " + e;
            mLogger.error(errorMessage);

            FacesMessage message = new FacesMessage(FacesMessage.SEVERITY_ERROR, "Encoding Profiles Set",
                    "Add failed");
            FacesContext.getCurrentInstance().addMessage(null, message);
        }
    }

    public void removeEncodingProfilesSet(Long encodingProfilesSetKey)
    {
        try
        {
            Long userKey = SessionUtils.getUserKey();
            String apiKey = SessionUtils.getAPIKey();

            if (userKey == null || apiKey == null || apiKey.equalsIgnoreCase(""))
            {
                mLogger.warn("no input to require removeEncodingProfilesSet"
                                + ", userKey: " + userKey
                                + ", apiKey: " + apiKey
                );
            }
            else
            {
                String username = userKey.toString();
                String password = apiKey;

                CatraMMS catraMMS = new CatraMMS();
                catraMMS.removeEncodingProfilesSet(
                        username, password, encodingProfilesSetKey);

                fillList(false);

                FacesMessage message = new FacesMessage(FacesMessage.SEVERITY_ERROR, "Encoding Profiles Set",
                        "Remove successful");
                FacesContext.getCurrentInstance().addMessage(null, message);
            }
        }
        catch (Exception e)
        {
            String errorMessage = "Exception: " + e;
            mLogger.error(errorMessage);

            FacesMessage message = new FacesMessage(FacesMessage.SEVERITY_ERROR, "Encoding Profiles Set",
                    "Remove failed");
            FacesContext.getCurrentInstance().addMessage(null, message);
        }
    }

    public void fillList(boolean toBeRedirected)
    {
        SimpleDateFormat simpleDateFormat = new SimpleDateFormat("yyyy-MM-dd HH:mm:ss");

        mLogger.info("fillList"
                + ", toBeRedirected: " + toBeRedirected
                + ", contentType: " + contentType
        );

        if (toBeRedirected)
        {
            try
            {
                SimpleDateFormat simpleDateFormat_1 = new SimpleDateFormat("yyyy-MM-dd'T'HH:mm:ss");

                String url = "encodingProfilesSets.xhtml?contentType=" + contentType
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

                        encodingProfilesSetList.clear();

                        CatraMMS catraMMS = new CatraMMS();
                        catraMMS.getEncodingProfilesSets(username, password,
                                contentType, encodingProfilesSetList);
                    }
                }
                catch (Exception e)
                {
                    String errorMessage = "Exception: " + e;
                    mLogger.error(errorMessage);

                    encodingProfilesSetList.clear();
                }
            }
        }
    }

    public String getContentType() {
        return contentType;
    }

    public void setContentType(String contentType) {
        this.contentType = contentType;
    }

    public List<String> getContentTypesList() {
        return contentTypesList;
    }

    public void setContentTypesList(List<String> contentTypesList) {
        this.contentTypesList = contentTypesList;
    }

    public List<EncodingProfilesSet> getEncodingProfilesSetList() {
        return encodingProfilesSetList;
    }

    public void setEncodingProfilesSetList(List<EncodingProfilesSet> encodingProfilesSetList) {
        this.encodingProfilesSetList = encodingProfilesSetList;
    }

    public List<EncodingProfile> getAddProfilesSetEncodingProfileList() {
        return addProfilesSetEncodingProfileList;
    }

    public void setAddProfilesSetEncodingProfileList(List<EncodingProfile> addProfilesSetEncodingProfileList) {
        this.addProfilesSetEncodingProfileList = addProfilesSetEncodingProfileList;
    }

    public List<EncodingProfile> getAddProfilesSetEncodingProfileSelectedList() {
        return addProfilesSetEncodingProfileSelectedList;
    }

    public void setAddProfilesSetEncodingProfileSelectedList(List<EncodingProfile> addProfilesSetEncodingProfileSelectedList) {
        this.addProfilesSetEncodingProfileSelectedList = addProfilesSetEncodingProfileSelectedList;
    }

    public String getAddProfilesSetLabel() {
        return addProfilesSetLabel;
    }

    public void setAddProfilesSetLabel(String addProfilesSetLabel) {
        this.addProfilesSetLabel = addProfilesSetLabel;
    }
}
