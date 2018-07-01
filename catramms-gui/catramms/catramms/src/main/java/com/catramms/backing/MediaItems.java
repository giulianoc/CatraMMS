package com.catramms.backing;

import com.catramms.backing.common.SessionUtils;
import com.catramms.backing.entity.MediaItem;
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
import java.util.*;

/**
 * Created with IntelliJ IDEA.
 * User: multi
 * Date: 27/09/15
 * Time: 20:28
 * To change this template use File | Settings | File Templates.
 */
@ManagedBean
@ViewScoped
public class MediaItems implements Serializable {

    // static because the class is Serializable
    private static final Logger mLogger = Logger.getLogger(MediaItems.class);

    private int autoRefreshPeriodInSeconds;
    private boolean autoRefresh;
    private Date begin;
    private Date end;

    private String contentType;
    private List<String> contentTypesList;

    private Long maxMediaItemsNumber = new Long(100);

    private Long mediaItemsNumber = new Long(0);
    private Long workSpaceUsageInMB = new Long(0);
    private Long maxStorageInMB = new Long(0);
    private List<MediaItem> mediaItemsList = new ArrayList<>();

    private String shareWorkspaceName;
    private String shareWorkspaceEMail;
    private String shareWorkspacePassword;
    private String shareWorkspaceCountry;
    private boolean shareWorkspaceIngestWorkflow;
    private boolean shareWorkspaceCreateProfiles;
    private boolean shareWorkspaceDeliveryAuthorization;
    private boolean shareWorkspaceShareWorkspace;
    private Long shareWorkspaceUserKey;

    @PostConstruct
    public void init()
    {
        mLogger.debug("init");

        autoRefresh = false;
        autoRefreshPeriodInSeconds = 30;

        {
            contentTypesList = new ArrayList<>();
            contentTypesList.add("video");
            contentTypesList.add("audio");
            contentTypesList.add("image");

            contentType = contentTypesList.get(0);
        }
        {
            Calendar calendar = Calendar.getInstance();

            calendar.add(Calendar.DAY_OF_MONTH, -1);

            calendar.set(Calendar.HOUR_OF_DAY, 0);
            calendar.set(Calendar.MINUTE, 0);
            calendar.set(Calendar.SECOND, 0);
            calendar.set(Calendar.MILLISECOND, 0);

            begin = calendar.getTime();
        }

        {
            Calendar calendar = Calendar.getInstance();

            calendar.add(Calendar.HOUR_OF_DAY, 5);

            calendar.set(Calendar.MINUTE, 0);
            calendar.set(Calendar.SECOND, 0);
            calendar.set(Calendar.MILLISECOND, 0);

            end = calendar.getTime();
        }
    }

    public void contentTypeChanged()
    {
        fillList(true);
    }

    public void beginChanged()
    {
        SimpleDateFormat simpleDateFormat = new SimpleDateFormat("yyyy-MM-dd HH:mm:ss");

        mLogger.debug("beginChanged. begin: " + simpleDateFormat.format(begin) + ", end: " + simpleDateFormat.format(end));
        if (begin.getTime() >= end.getTime())
        {
            Calendar calendar = Calendar.getInstance();

            calendar.setTime(begin);

            calendar.add(Calendar.DAY_OF_MONTH, 1);

            end.setTime(calendar.getTime().getTime());

            // really face message is useless because of the redirection
            FacesMessage message = new FacesMessage(FacesMessage.SEVERITY_ERROR,
                    "Traffic System Update", "BeginDate cannot be later the EndDate");
            FacesContext context = FacesContext.getCurrentInstance();
            context.addMessage(null, message);
        }
        /*
        else if ((end.getTime() - begin.getTime()) / 1000 > maxDaysDisplayed * 24 * 3600)
        {
            Calendar calendar = Calendar.getInstance();

            calendar.setTime(begin);

            calendar.add(Calendar.DAY_OF_MONTH, maxDaysDisplayed);

            end.setTime(calendar.getTime().getTime());

            // really face message is useless because of the redirection
            FacesMessage message = new FacesMessage(FacesMessage.SEVERITY_ERROR,
                "Traffic System Update", "Max " + maxDaysDisplayed + " days can be displayed");
            FacesContext context = FacesContext.getCurrentInstance();
            context.addMessage(null, message);
        }
        */

        mLogger.debug("beginChanged. begin: " + simpleDateFormat.format(begin) + ", end: " + simpleDateFormat.format(end));

        fillList(true);
    }

    public void endChanged()
    {
        SimpleDateFormat simpleDateFormat = new SimpleDateFormat("yyyy-MM-dd HH:mm:ss");

        mLogger.debug("endChanged. begin: " + simpleDateFormat.format(begin) + ", end: " + simpleDateFormat.format(end));
        if (begin.getTime() >= end.getTime())
        {
            Calendar calendar = Calendar.getInstance();

            calendar.setTime(end);

            calendar.add(Calendar.DAY_OF_MONTH, -1);

            begin.setTime(calendar.getTime().getTime());

            // really face message is useless because of the redirection
            FacesMessage message = new FacesMessage(FacesMessage.SEVERITY_ERROR,
                "Traffic System Update", "BeginDate cannot be later the EndDate");
            FacesContext context = FacesContext.getCurrentInstance();
            context.addMessage(null, message);
        }
        /*
        else if ((end.getTime() - begin.getTime()) / 1000 > maxDaysDisplayed * 24 * 3600)
        {
            Calendar calendar = Calendar.getInstance();

            calendar.setTime(end);

            calendar.add(Calendar.DAY_OF_MONTH, -maxDaysDisplayed);

            begin.setTime(calendar.getTime().getTime());

            // really face message is useless because of the redirection
            FacesMessage message = new FacesMessage(FacesMessage.SEVERITY_ERROR,
                "Traffic System Update", "Max " + maxDaysDisplayed + " days can be displayed");
            FacesContext context = FacesContext.getCurrentInstance();
            context.addMessage(null, message);
        }
        */

        mLogger.debug("endChanged. begin: " + simpleDateFormat.format(begin) + ", end: " + simpleDateFormat.format(end));

        fillList(true);
    }

    public List<MediaItem> getMediaItemsList()
    {
        mLogger.debug("Received getMediaItemsList: " + (mediaItemsList == null ? "null" : mediaItemsList.size()));


        return mediaItemsList;
    }

    public void fillList(boolean toBeRedirected)
    {
        SimpleDateFormat simpleDateFormat = new SimpleDateFormat("yyyy-MM-dd HH:mm:ss");

        mLogger.info("fillList"
                + ", toBeRedirected: " + toBeRedirected
                + ", maxMediaItemsNumber: " + maxMediaItemsNumber
                + ", autoRefresh: " + autoRefresh
                + ", contentType: " + contentType
                + ", begin: " + simpleDateFormat.format(begin)
            + ", end: " + simpleDateFormat.format(end));

        if (toBeRedirected)
        {
            try
            {
                SimpleDateFormat simpleDateFormat_1 = new SimpleDateFormat("yyyy-MM-dd'T'HH:mm:ss");

                String url = "mediaItems.xhtml?maxMediaItemsNumber=" + maxMediaItemsNumber
                        + "&autoRefresh=" + autoRefresh
                        + "&begin=" + simpleDateFormat_1.format(begin)
                        + "&end=" + simpleDateFormat_1.format(end)
                        + "&contentType=" + contentType
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

                        mediaItemsList.clear();

                        CatraMMS catraMMS = new CatraMMS();
                        Vector<Long> mediaItemsData = catraMMS.getMediaItems(
                                username, password, maxMediaItemsNumber,
                                contentType, begin, end, mediaItemsList);

                        mediaItemsNumber = mediaItemsData.get(0);
                        workSpaceUsageInMB = mediaItemsData.get(1);
                        maxStorageInMB = mediaItemsData.get(2);
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

    public Date willBeRemovedAt(Date ingestionDate, Long retentionInMinutes)
    {
        Calendar calendar = Calendar.getInstance();

        calendar.setTime(ingestionDate);
        calendar.add(Calendar.MINUTE, retentionInMinutes.intValue());

        return calendar.getTime();
    }

    public void shareWorkspace()
    {
        try {
            Long userKey = SessionUtils.getUserKey();
            String apiKey = SessionUtils.getAPIKey();
            HttpSession session = SessionUtils.getSession();

            WorkspaceDetails currentWorkspaceDetails = (WorkspaceDetails) session.getAttribute("currentWorkspaceDetails");

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
                shareWorkspaceUserKey = catraMMS.shareWorkspace(
                        username, password, shareWorkspaceName, shareWorkspaceEMail, shareWorkspacePassword,
                        shareWorkspaceCountry, shareWorkspaceIngestWorkflow, shareWorkspaceCreateProfiles,
                        shareWorkspaceDeliveryAuthorization, shareWorkspaceShareWorkspace,
                        currentWorkspaceDetails.getWorkspaceKey());
            }
        }
        catch (Exception e)
        {
            String errorMessage = "shareWorkspace failed: " + e;
            mLogger.error(errorMessage);

            FacesMessage message = new FacesMessage(FacesMessage.SEVERITY_ERROR,
                    "Share Workspace", errorMessage);
            FacesContext context = FacesContext.getCurrentInstance();
            context.addMessage(null, message);
        }
    }

    public void workspaceNameChanged()
    {
        HttpSession session = SessionUtils.getSession();

        WorkspaceDetails currentWorkspaceDetails = (WorkspaceDetails) session.getAttribute("currentWorkspaceDetails");
        mLogger.info("workspaceNameChanged"
                        + ", workspaceName: " + currentWorkspaceDetails.getName()
        );

        fillList(true);
    }

    public String getWorkspaceName()
    {
        HttpSession session = SessionUtils.getSession();

        WorkspaceDetails currentWorkspaceDetails = (WorkspaceDetails) session.getAttribute("currentWorkspaceDetails");

        return currentWorkspaceDetails.getName();
    }

    public void setWorkspaceName(String workspaceName)
    {
        HttpSession session = SessionUtils.getSession();

        List<WorkspaceDetails> workspaceDetailsList =
                (List<WorkspaceDetails>) session.getAttribute("workspaceDetailsList");

        for (WorkspaceDetails workspaceDetails: workspaceDetailsList)
        {
            if (workspaceDetails.getName().equalsIgnoreCase(workspaceName))
            {
                session.setAttribute("currentWorkspaceDetails", workspaceDetails);

                break;
            }
        }
    }

    public List<String> getWorkspaceNames()
    {
        List<String> workspaceNames = new ArrayList<>();

        HttpSession session = SessionUtils.getSession();

        List<WorkspaceDetails> workspaceDetailsList =
                (List<WorkspaceDetails>) session.getAttribute("workspaceDetailsList");

        if (workspaceDetailsList != null)
        {
            for (WorkspaceDetails workspaceDetails: workspaceDetailsList)
                workspaceNames.add(workspaceDetails.getName());
        }

        return workspaceNames;
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

    public Date getBegin() {
        return begin;
    }

    public void setBegin(Date begin) {
        this.begin = begin;
    }

    public Date getEnd() {
        return end;
    }

    public void setEnd(Date end) {
        this.end = end;
    }

    public Long getMaxMediaItemsNumber() {
        return maxMediaItemsNumber;
    }

    public void setMaxMediaItemsNumber(Long maxMediaItemsNumber) {
        this.maxMediaItemsNumber = maxMediaItemsNumber;
    }

    public Long getMediaItemsNumber() {
        return mediaItemsNumber;
    }

    public void setMediaItemsNumber(Long mediaItemsNumber) {
        this.mediaItemsNumber = mediaItemsNumber;
    }

    public Long getWorkSpaceUsageInMB() {
        return workSpaceUsageInMB;
    }

    public void setWorkSpaceUsageInMB(Long workSpaceUsageInMB) {
        this.workSpaceUsageInMB = workSpaceUsageInMB;
    }

    public void setMediaItemsList(List<MediaItem> mediaItemsList) {
        this.mediaItemsList = mediaItemsList;
    }

    public boolean isAutoRefresh() {
        return autoRefresh;
    }

    public void setAutoRefresh(boolean autoRefresh) {
        this.autoRefresh = autoRefresh;
    }

    public int getAutoRefreshPeriodInSeconds() {
        return autoRefreshPeriodInSeconds;
    }

    public void setAutoRefreshPeriodInSeconds(int autoRefreshPeriodInSeconds) {
        this.autoRefreshPeriodInSeconds = autoRefreshPeriodInSeconds;
    }

    public Long getMaxStorageInMB() {
        return maxStorageInMB;
    }

    public void setMaxStorageInMB(Long maxStorageInMB) {
        this.maxStorageInMB = maxStorageInMB;
    }

    public String getShareWorkspaceName() {
        return shareWorkspaceName;
    }

    public void setShareWorkspaceName(String shareWorkspaceName) {
        this.shareWorkspaceName = shareWorkspaceName;
    }

    public String getShareWorkspaceEMail() {
        return shareWorkspaceEMail;
    }

    public void setShareWorkspaceEMail(String shareWorkspaceEMail) {
        this.shareWorkspaceEMail = shareWorkspaceEMail;
    }

    public String getShareWorkspacePassword() {
        return shareWorkspacePassword;
    }

    public void setShareWorkspacePassword(String shareWorkspacePassword) {
        this.shareWorkspacePassword = shareWorkspacePassword;
    }

    public String getShareWorkspaceCountry() {
        return shareWorkspaceCountry;
    }

    public void setShareWorkspaceCountry(String shareWorkspaceCountry) {
        this.shareWorkspaceCountry = shareWorkspaceCountry;
    }

    public boolean isShareWorkspaceIngestWorkflow() {
        return shareWorkspaceIngestWorkflow;
    }

    public void setShareWorkspaceIngestWorkflow(boolean shareWorkspaceIngestWorkflow) {
        this.shareWorkspaceIngestWorkflow = shareWorkspaceIngestWorkflow;
    }

    public boolean isShareWorkspaceCreateProfiles() {
        return shareWorkspaceCreateProfiles;
    }

    public void setShareWorkspaceCreateProfiles(boolean shareWorkspaceCreateProfiles) {
        this.shareWorkspaceCreateProfiles = shareWorkspaceCreateProfiles;
    }

    public boolean isShareWorkspaceDeliveryAuthorization() {
        return shareWorkspaceDeliveryAuthorization;
    }

    public void setShareWorkspaceDeliveryAuthorization(boolean shareWorkspaceDeliveryAuthorization) {
        this.shareWorkspaceDeliveryAuthorization = shareWorkspaceDeliveryAuthorization;
    }

    public boolean isShareWorkspaceShareWorkspace() {
        return shareWorkspaceShareWorkspace;
    }

    public void setShareWorkspaceShareWorkspace(boolean shareWorkspaceShareWorkspace) {
        this.shareWorkspaceShareWorkspace = shareWorkspaceShareWorkspace;
    }

    public Long getShareWorkspaceUserKey() {
        return shareWorkspaceUserKey;
    }

    public void setShareWorkspaceUserKey(Long shareWorkspaceUserKey) {
        this.shareWorkspaceUserKey = shareWorkspaceUserKey;
    }
}
