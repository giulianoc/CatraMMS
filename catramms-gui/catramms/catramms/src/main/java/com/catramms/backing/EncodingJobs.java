package com.catramms.backing;

import com.catramms.backing.common.SessionUtils;
import com.catramms.backing.common.Workspace;
import com.catramms.backing.entity.EncodingJob;
import com.catramms.backing.entity.IngestionJob;
import com.catramms.backing.entity.MediaItem;
import com.catramms.backing.entity.PhysicalPath;
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
import java.util.Calendar;
import java.util.Date;
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
public class EncodingJobs extends Workspace implements Serializable {

    // static because the class is Serializable
    private static final Logger mLogger = Logger.getLogger(EncodingJobs.class);

    private int autoRefreshPeriodInSeconds;
    private boolean autoRefresh;
    private Date begin;
    private Date end;

    private String status;
    private List<String> statusOptions;

    private boolean ascending;
    private Long maxEncodingJobsNumber = new Long(100);

    private Long encodingJobsNumber = new Long(0);
    private List<EncodingJob> encodingJobsList = new ArrayList<>();
    private List<String> encodingJobsTitlesList = new ArrayList<>();
    private List<String> encodingJobsDurationsList = new ArrayList<>();

    @PostConstruct
    public void init()
    {
        mLogger.debug("init");

        ascending = false;

        autoRefresh = true;
        autoRefreshPeriodInSeconds = 30;

        {
            statusOptions = new ArrayList<>();

            statusOptions.add("all");
            statusOptions.add("notCompleted");
            statusOptions.add("completed");

            status = statusOptions.get(0);
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

            calendar.add(Calendar.HOUR_OF_DAY, 1);

            calendar.set(Calendar.MINUTE, 0);
            calendar.set(Calendar.SECOND, 0);
            calendar.set(Calendar.MILLISECOND, 0);

            end = calendar.getTime();
        }
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

    public void fillList(boolean toBeRedirected)
    {
        SimpleDateFormat simpleDateFormat = new SimpleDateFormat("yyyy-MM-dd HH:mm:ss");

        mLogger.info("fillList"
                + ", toBeRedirected: " + toBeRedirected
                + ", maxEncodingJobsNumber: " + maxEncodingJobsNumber
                + ", autoRefresh: " + autoRefresh
                + ", begin: " + simpleDateFormat.format(begin)
            + ", end: " + simpleDateFormat.format(end));

        if (toBeRedirected)
        {
            try
            {
                SimpleDateFormat simpleDateFormat_1 = new SimpleDateFormat("yyyy-MM-dd'T'HH:mm:ss");

                String url = "encodingJobs.xhtml?maxEncodingJobsNumber=" + maxEncodingJobsNumber
                        + "&ascending=" + ascending
                        + "&autoRefresh=" + autoRefresh
                        + "&status=" + status
                        + "&begin=" + simpleDateFormat_1.format(begin)
                        + "&end=" + simpleDateFormat_1.format(end)
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
                        mLogger.warn("no input to require encodingJobs"
                                + ", userKey: " + userKey
                                + ", apiKey: " + apiKey
                        );
                    }
                    else
                    {
                        String username = userKey.toString();
                        String password = apiKey;

                        encodingJobsList.clear();
                        encodingJobsTitlesList.clear();

                        CatraMMS catraMMS = new CatraMMS();
                        encodingJobsNumber = catraMMS.getEncodingJobs(
                                username, password, maxEncodingJobsNumber,
                                begin, end, status, ascending, encodingJobsList);

                        mLogger.info("Retrieved " + encodingJobsList.size() + " encoding jobs");
                        for (EncodingJob encodingJob: encodingJobsList)
                        {
                            mLogger.info("getTitle for " + encodingJob.getEncodingJobKey() + " encoding job key ...");
                            addTitleAndDuration(catraMMS, username, password, encodingJob);
                        }
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

    private void addTitleAndDuration(CatraMMS catraMMS, String username, String password, EncodingJob encodingJob)
    {
        String title = "";
        String duration = "";

        try
        {
            Long physicalPathKey = null;

            if (encodingJob.getType().equalsIgnoreCase("EncodeVideoAudio")
                    || encodingJob.getType().equalsIgnoreCase("EncodeImage"))
            {
                physicalPathKey = encodingJob.getSourcePhysicalPathKey();
            }
            else if (encodingJob.getType().equalsIgnoreCase("OverlayImageOnVideo"))
            {
                physicalPathKey = encodingJob.getSourceVideoPhysicalPathKey();
            }
            else if (encodingJob.getType().equalsIgnoreCase("OverlayTextOnVideo"))
            {
                physicalPathKey = encodingJob.getSourceVideoPhysicalPathKey();
            }
            else if (encodingJob.getType().equalsIgnoreCase("GenerateFrames"))
            {
                physicalPathKey = encodingJob.getSourceVideoPhysicalPathKey();
            }

            if (physicalPathKey != null)
            {
                Long mediaItemKey = null;

                mLogger.info("catraMMS.getMediaItem"
                                + ", mediaItemKey: " + mediaItemKey
                                + ", physicalPathKey: " + physicalPathKey
                );
                MediaItem mediaItem = catraMMS.getMediaItem(
                        username, password, mediaItemKey, physicalPathKey);
                if (mediaItem != null)
                {
                    title = mediaItem.getTitle();

                    String contentType = mediaItem.getContentType();

                    for (PhysicalPath physicalPath: mediaItem.getPhysicalPathList())
                    {
                        if (physicalPath.getPhysicalPathKey().longValue() == physicalPathKey.longValue())
                        {
                            if (contentType.equalsIgnoreCase("video"))
                                duration = getDurationAsString(
                                        physicalPath.getVideoDetails().getDurationInMilliseconds());
                            else if (contentType.equalsIgnoreCase("audio"))
                                duration = getDurationAsString(
                                        physicalPath.getAudioDetails().getDurationInMilliseconds());

                            break;
                        }
                    }
                }
            }
        }
        catch (Exception e)
        {
            mLogger.error("Exception: " + e);
        }

        encodingJobsTitlesList.add(title);
        encodingJobsDurationsList.add(duration);
    }

    public String getDurationAsString(Long durationInMilliseconds)
    {
        if (durationInMilliseconds == null)
            return "";

        String duration;

        int hours = (int) (durationInMilliseconds / 3600000);
        String sHours = String.format("%02d", hours);

        int minutes = (int) ((durationInMilliseconds - (hours * 3600000)) / 60000);
        String sMinutes = String.format("%02d", minutes);

        int seconds = (int) ((durationInMilliseconds - ((hours * 3600000) + (minutes * 60000))) / 1000);
        String sSeconds = String.format("%02d", seconds);

        int milliSeconds = (int) (durationInMilliseconds - ((hours * 3600000) + (minutes * 60000) + (seconds * 1000)));
        String sMilliSeconds = String.format("%03d", milliSeconds);

        return sHours.concat(":").concat(sMinutes).concat(":").concat(sSeconds).concat(".").concat(sMilliSeconds);
    }

    public String getEncodingJobStyleClass(int rowId)
    {
        String styleClass = "";

        String status = encodingJobsList.get(rowId).getStatus();

        if (status.equalsIgnoreCase("End_ProcessedSuccessful"))
            styleClass = "successFullColor";
        else if (status.equalsIgnoreCase("End_Failed"))
            styleClass = "failureColor";
        else if (status.equalsIgnoreCase("Processing"))
            styleClass = "processingColor";
        else if (status.equalsIgnoreCase("ToBeProcessed"))
            styleClass = "toBeProcessedColor";

        return styleClass;
    }

    public List<String> getEncodingJobsTitlesList() {
        return encodingJobsTitlesList;
    }

    public void setEncodingJobsTitlesList(List<String> encodingJobsTitlesList) {
        this.encodingJobsTitlesList = encodingJobsTitlesList;
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

    public Long getMaxEncodingJobsNumber() {
        return maxEncodingJobsNumber;
    }

    public void setMaxEncodingJobsNumber(Long maxEncodingJobsNumber) {
        this.maxEncodingJobsNumber = maxEncodingJobsNumber;
    }

    public Long getEncodingJobsNumber() {
        return encodingJobsNumber;
    }

    public void setEncodingJobsNumber(Long encodingJobsNumber) {
        this.encodingJobsNumber = encodingJobsNumber;
    }

    public List<EncodingJob> getEncodingJobsList() {
        return encodingJobsList;
    }

    public void setEncodingJobsList(List<EncodingJob> encodingJobsList) {
        this.encodingJobsList = encodingJobsList;
    }

    public String getStatus() {
        return status;
    }

    public void setStatus(String status) {
        this.status = status;
    }

    public List<String> getStatusOptions() {
        return statusOptions;
    }

    public void setStatusOptions(List<String> statusOptions) {
        this.statusOptions = statusOptions;
    }

    public boolean isAscending() {
        return ascending;
    }

    public void setAscending(boolean ascending) {
        this.ascending = ascending;
    }

    public List<String> getEncodingJobsDurationsList() {
        return encodingJobsDurationsList;
    }

    public void setEncodingJobsDurationsList(List<String> encodingJobsDurationsList) {
        this.encodingJobsDurationsList = encodingJobsDurationsList;
    }
}
