package com.catramms.backing;

import com.catramms.backing.common.SessionUtils;
import com.catramms.backing.entity.MediaItem;
import com.catramms.backing.entity.PhysicalPath;
import com.catramms.utility.catramms.CatraMMS;
import org.apache.log4j.Logger;
import org.json.JSONArray;
import org.json.JSONObject;

import javax.faces.application.FacesMessage;
import javax.faces.bean.ManagedBean;
import javax.faces.bean.ManagedProperty;
import javax.faces.bean.ViewScoped;
import javax.faces.context.FacesContext;
import java.io.Serializable;
import java.util.ArrayList;
import java.util.List;
import java.util.Map;

/**
 * Created with IntelliJ IDEA.
 * User: multi
 * Date: 27/09/15
 * Time: 20:28
 * To change this template use File | Settings | File Templates.
 */
@ManagedBean
@ViewScoped
public class MediaItemDetails implements Serializable {

    // static because the class is Serializable
    private static final Logger mLogger = Logger.getLogger(MediaItemDetails.class);

    private MediaItem mediaItem = null;
    private Long framesPerSecond;
    private Long mediaItemKey;
    private PhysicalPath currentPhysicalPath;
    private String currentMediaURL;
    private String videoCurrentTime;
    private String videoMarkIn = "";
    private String videoMarkOut = "";
    private Boolean markButtonDisabled;
    private Boolean addMarkButtonDisabled;
    private List<Mark> markList = new ArrayList<>();

    public void init()
    {
        videoCurrentTime = "";
        markButtonDisabled = true;
        addMarkButtonDisabled = true;


        if (mediaItemKey == null)
        {
            String errorMessage = "mediaItemKey is null";
            mLogger.error(errorMessage);

            return;
        }

        try {
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
                mediaItem = catraMMS.getMediaItem(
                        username, password, mediaItemKey);
            }
        }
        catch (Exception e)
        {
            String errorMessage = "Exception: " + e;
            mLogger.error(errorMessage);
        }
    }

    public void updateVideoTimeCode()
    {
        mLogger.info("updateVideoTimeCode is called");
        Map<String, String> requestParamMap = FacesContext.getCurrentInstance().getExternalContext().getRequestParameterMap();
        if (requestParamMap.containsKey("newTimeCode"))
        {
            videoCurrentTime = requestParamMap.get("newTimeCode");
            markButtonDisabled = false;

            mLogger.info("videoCurrentTime: " + videoCurrentTime);
        }
    }

    private Float smpteTimeCodeToSeconds(String smpteTimecode)
    {
        String[] smpteTimecodeArray = smpteTimecode.split(":");
        float localHours = Long.parseLong(smpteTimecodeArray[0]);
        float localMinutes = Long.parseLong(smpteTimecodeArray[1]);
        float localSeconds = Long.parseLong(smpteTimecodeArray[2]);
        float localFramesPerSeconds = Long.parseLong(smpteTimecodeArray[3]);

        Float seconds = new Float(
                        (localHours * 3600)
                        + (localMinutes * 60)
                        + localSeconds
                        + (localFramesPerSeconds / framesPerSecond));

        mLogger.info("smpteTimeCodeToSeconds"
                + ", smpteTimecode: " + smpteTimecode
                + ", seconds: " + seconds);

        return seconds;
    }

    public void markIn()
    {
        if (videoCurrentTime.equalsIgnoreCase(""))
        {
            FacesContext.getCurrentInstance().addMessage(
                    null,
                    new FacesMessage(FacesMessage.SEVERITY_WARN,
                            "Mark",
                            "Current Time is not initialized"));
            return;
        }

        if (!videoMarkOut.equalsIgnoreCase(""))
        {
            Float fVideoMarkIn = smpteTimeCodeToSeconds(videoCurrentTime);
            Float fVideoMarkOut = smpteTimeCodeToSeconds(videoMarkOut);

            if (fVideoMarkIn >= fVideoMarkOut)
            {
                FacesContext.getCurrentInstance().addMessage(
                        null,
                        new FacesMessage(FacesMessage.SEVERITY_WARN,
                                "Mark",
                                "Mark In cannot be greater than Mark Out"));
                return;
            }

            addMarkButtonDisabled = false;
        }

        videoMarkIn = videoCurrentTime;

        mLogger.info("markIn"
                        + ", videoMarkIn: " + videoMarkIn
        );
    }

    public void markOut()
    {
        if (videoCurrentTime.equalsIgnoreCase(""))
        {
            FacesContext.getCurrentInstance().addMessage(
                    null,
                    new FacesMessage(FacesMessage.SEVERITY_WARN,
                            "Mark",
                            "Current Time is not initialized"));
            return;
        }

        if (!videoMarkIn.equalsIgnoreCase(""))
        {
            Float fVideoMarkIn = smpteTimeCodeToSeconds(videoMarkIn);
            Float fVideoMarkOut = smpteTimeCodeToSeconds(videoCurrentTime);

            if (fVideoMarkIn >= fVideoMarkOut)
            {
                FacesContext.getCurrentInstance().addMessage(
                        null,
                        new FacesMessage(FacesMessage.SEVERITY_WARN,
                                "Mark",
                                "Mark In cannot be greater than Mark Out"));
                return;
            }

            addMarkButtonDisabled = false;
        }

        videoMarkOut = videoCurrentTime;

        mLogger.info("markOut"
                        + ", videoMarkOut: " + videoMarkOut
        );
    }

    public void addMark()
    {
        mLogger.info("addMark");

        if (videoMarkIn.equalsIgnoreCase(""))
        {
            FacesContext.getCurrentInstance().addMessage(
                    null,
                    new FacesMessage(FacesMessage.SEVERITY_WARN,
                            "Mark",
                            "Mark In is not initialized"));
            return;
        }
        else if (videoMarkOut.equalsIgnoreCase(""))
        {
            FacesContext.getCurrentInstance().addMessage(
                    null,
                    new FacesMessage(FacesMessage.SEVERITY_WARN,
                            "Mark",
                            "Mark Out is not initialized"));
            return;
        }

        Float fVideoMarkIn = smpteTimeCodeToSeconds(videoMarkIn);
        Float fVideoMarkOut = smpteTimeCodeToSeconds(videoMarkOut);

        if (fVideoMarkIn >= fVideoMarkOut)
        {
            FacesContext.getCurrentInstance().addMessage(
                    null,
                    new FacesMessage(FacesMessage.SEVERITY_WARN,
                            "Mark",
                            "Mark In cannot be greater than Mark Out"));
            return;
        }

        {
            boolean markFound = false;

            for (Mark mark: markList)
            {
                if (mark.getIn().equalsIgnoreCase(videoMarkIn))
                {
                    markFound = true;
                    break;
                }
            }

            if (markFound)
            {
                FacesContext.getCurrentInstance().addMessage(
                        null,
                        new FacesMessage(FacesMessage.SEVERITY_WARN,
                                "Mark",
                                "Mark In was already added"));
                return;
            }
        }

        Mark mark = new Mark();
        mark.setIn(videoMarkIn);
        mark.setOut(videoMarkOut);

        markList.add(mark);

        videoMarkIn = "";
        videoMarkOut = "";

        addMarkButtonDisabled = true;
    }

    public void removeMark(String markIn)
    {
        Mark markToBeRemoved = null;

        for (Mark mark: markList)
        {
            if (mark.getIn().equalsIgnoreCase(markIn))
            {
                markToBeRemoved = mark;
                break;
            }
        }

        if (markToBeRemoved == null)
        {
            FacesContext.getCurrentInstance().addMessage(
                    null,
                    new FacesMessage(FacesMessage.SEVERITY_WARN,
                            "Mark",
                            "Mark to be removed was not found"));
            return;
        }

        markList.remove(markToBeRemoved);
    }

    public void prepareWorkflowToIngest()
    {
        if (markList.size() == 0)
        {
            String errorMessage = "No marks were saved";
            mLogger.error(errorMessage);

            FacesContext.getCurrentInstance().addMessage(
                    null,
                    new FacesMessage(FacesMessage.SEVERITY_WARN,
                            "Mark",
                            errorMessage));

            return;
        }

        try {
            String predefined = "cut";

            JSONObject joCut = new JSONObject();
            joCut.put("key", currentPhysicalPath.getPhysicalPathKey());

            JSONArray jaMarks = new JSONArray();
            joCut.put("marks", jaMarks);

            for (Mark mark: markList)
            {
                JSONObject jsonObject = new JSONObject();
                jaMarks.put(jsonObject);

                jsonObject.put("s", smpteTimeCodeToSeconds(mark.getIn()).toString());
                jsonObject.put("e", smpteTimeCodeToSeconds(mark.getOut()).toString());
            }

            String url = "newWorkflow/newWorkflow.xhtml?predefined=" + predefined
                    + "&data=" + java.net.URLEncoder.encode(joCut.toString(), "UTF-8")
                    ;
            mLogger.info("Redirect to " + url);
            FacesContext.getCurrentInstance().getExternalContext().redirect(url);
        }
        catch (Exception e)
        {
            String errorMessage = "Exception: " + e;
            mLogger.error(errorMessage);

            FacesContext.getCurrentInstance().addMessage(
                    null,
                    new FacesMessage(FacesMessage.SEVERITY_WARN,
                            "Mark",
                            "Workflow preparation failed"));
        }
    }

    public void prepareCurrentMediaURL(PhysicalPath physicalPath)
    {
        long ttlInSeconds = 60 * 60;
        int maxRetries = 20;

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
                currentMediaURL = catraMMS.getDeliveryURL(
                        username, password, physicalPath.getPhysicalPathKey(),
                        ttlInSeconds, maxRetries);

                setCurrentPhysicalPath(physicalPath);
            }

            markList.clear();
        }
        catch (Exception e)
        {
            String errorMessage = "Exception: " + e;
            mLogger.error(errorMessage);
        }

        long defaultFramesPerSeconds = 25;
        try
        {
            framesPerSecond = null;

            String videoAvgFrameRate = physicalPath.getVideoDetails().getVideoAvgFrameRate();
            int endIndexOfFrameRate = videoAvgFrameRate.indexOf('/');
            if (endIndexOfFrameRate != -1)
                framesPerSecond = Long.parseLong(videoAvgFrameRate.substring(0, endIndexOfFrameRate));

            if (framesPerSecond == null)
            {
                mLogger.error("No frame rate found");

                framesPerSecond = new Long(defaultFramesPerSeconds);
            }
        }
        catch (Exception e)
        {
            String errorMessage = "Exception: " + e;
            mLogger.error(errorMessage);

            framesPerSecond = new Long(defaultFramesPerSeconds);
        }

        mLogger.info("framesPerSecond: " + framesPerSecond);
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

    public String getVideoCurrentTime() {
        return videoCurrentTime;
    }

    public void setVideoCurrentTime(String videoCurrentTime) {
        this.videoCurrentTime = videoCurrentTime;
    }

    public MediaItem getMediaItem() {
        return mediaItem;
    }

    public void setMediaItem(MediaItem mediaItem) {
        this.mediaItem = mediaItem;
    }

    public Long getMediaItemKey() {
        return mediaItemKey;
    }

    public void setMediaItemKey(Long mediaItemKey) {
        this.mediaItemKey = mediaItemKey;
    }

    public String getCurrentMediaURL() {
        return currentMediaURL;
    }

    public void setCurrentMediaURL(String currentMediaURL) {
        this.currentMediaURL = currentMediaURL;
    }

    public PhysicalPath getCurrentPhysicalPath() {
        return currentPhysicalPath;
    }

    public void setCurrentPhysicalPath(PhysicalPath currentPhysicalPath) {
        this.currentPhysicalPath = currentPhysicalPath;
    }

    public String getVideoMarkIn() {
        return videoMarkIn;
    }

    public void setVideoMarkIn(String videoMarkIn) {
        this.videoMarkIn = videoMarkIn;
    }

    public String getVideoMarkOut() {
        return videoMarkOut;
    }

    public void setVideoMarkOut(String videoMarkOut) {
        this.videoMarkOut = videoMarkOut;
    }

    public List<Mark> getMarkList() {
        return markList;
    }

    public void setMarkList(List<Mark> markList) {
        this.markList = markList;
    }

    public Boolean getMarkButtonDisabled() {
        return markButtonDisabled;
    }

    public void setMarkButtonDisabled(Boolean markButtonDisabled) {
        this.markButtonDisabled = markButtonDisabled;
    }

    public Boolean getAddMarkButtonDisabled() {
        return addMarkButtonDisabled;
    }

    public void setAddMarkButtonDisabled(Boolean addMarkButtonDisabled) {
        this.addMarkButtonDisabled = addMarkButtonDisabled;
    }

    public Long getFramesPerSecond() {
        return framesPerSecond;
    }

    public void setFramesPerSecond(Long framesPerSecond) {
        this.framesPerSecond = framesPerSecond;
    }
}
