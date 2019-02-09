package com.catramms.backing.common;

import com.catramms.backing.Mark;
import com.catramms.backing.entity.MediaItem;
import com.catramms.backing.entity.PhysicalPath;
import com.catramms.utility.catramms.CatraMMS;
import org.apache.log4j.Logger;
import org.json.JSONArray;
import org.json.JSONObject;
import org.primefaces.component.outputlabel.OutputLabel;
import org.w3c.dom.html.HTMLLabelElement;

import javax.faces.application.FacesMessage;
import javax.faces.bean.ManagedBean;
import javax.faces.bean.ViewScoped;
import javax.faces.component.UIComponent;
import javax.faces.component.UIInput;
import javax.faces.component.UIOutput;
import javax.faces.component.UIViewRoot;
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
public class Player implements Serializable {

    // static because the class is Serializable
    private static final Logger mLogger = Logger.getLogger(Player.class);

    private MediaItem mediaItem;
    private String currentMediaURL;
    private PhysicalPath currentPhysicalPath;
    private List<Mark> markList = new ArrayList<>();
    private boolean editVideo;
    private Long framesPerSecond;
    private String videoCurrentTime;
    private String videoCurrentTimeHidden;
    private String videoMarkIn = "";
    private String videoMarkOut = "";
    private Boolean addMarkButtonDisabled;


    public Player()
    {
        videoCurrentTime = "";
        addMarkButtonDisabled = true;
    }

    private Float smpteTimeCodeToSeconds(String smpteTimecode)
    {
        /*
        String[] smpteTimecodeArray = smpteTimecode.split(":");
        float localHours = Long.parseLong(smpteTimecodeArray[0]);
        float localMinutes = Long.parseLong(smpteTimecodeArray[1]);
        float localSeconds = Long.parseLong(smpteTimecodeArray[2]);
        float localFramesPerSeconds = Long.parseLong(smpteTimecodeArray[3]);
        */

        mLogger.info("smpteTimeCodeToSeconds"
                + ", smpteTimecode: " + smpteTimecode);

        float localHours = Long.parseLong(smpteTimecode.substring(0, 2));
        float localMinutes = Long.parseLong(smpteTimecode.substring(3, 5));
        float localSeconds = Long.parseLong(smpteTimecode.substring(6, 8));
        float localFramesPerSeconds = Long.parseLong(smpteTimecode.substring(9, 11));

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
        // UIInput get the value only in case of inputText with readonly false.
        // Since we needed to use just a label, we set the inputText as hidden
        UIViewRoot view = FacesContext.getCurrentInstance().getViewRoot();
        UIComponent uiComponent = view.findComponent("showVideoForm:timecodeHidden");

        // mLogger.info("uiComponent.toString: " + uiComponent.toString());

        UIInput inputText = (UIInput) uiComponent;
        videoCurrentTime = (String) inputText.getAttributes().get("value");
        // mLogger.info("videoCurrentTime: " + videoCurrentTime);

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
                                "Mark In cannot be greater or equal than Mark Out"));
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
        // UIInput get the value only in case of inputText with readonly false.
        // Since we needed to use just a label, we set the inputText as hidden
        UIViewRoot view = FacesContext.getCurrentInstance().getViewRoot();
        UIComponent uiComponent = view.findComponent("showVideoForm:timecodeHidden");

        UIInput inputText = (UIInput) uiComponent;
        videoCurrentTime = (String) inputText.getAttributes().get("value");

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
                                "Mark In cannot be greater or equal than Mark Out"));
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

        try
        {
            /*
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

            String url = "workflowEditor/workflowEditor.xhtml?predefined=" + predefined
                    + "&data=" + java.net.URLEncoder.encode(joCut.toString(), "UTF-8")
                    ;
            */

            JSONObject joWorkflow = new JSONObject();
            joWorkflow.put("Label", "Edit Player Cut");
            joWorkflow.put("Type", "Workflow");

            if (markList.size() > 1)
            {
                JSONObject joGroupOfTasks = new JSONObject();
                joWorkflow.put("Task", joGroupOfTasks);

                joGroupOfTasks.put("Type", "GroupOfTasks");

                JSONObject joGroupOfTasksParameters = new JSONObject();
                joGroupOfTasks.put("Parameters", joGroupOfTasksParameters);

                joGroupOfTasksParameters.put("ExecutionType", "parallel");

                JSONArray jaCutTasks = new JSONArray();
                joGroupOfTasksParameters.put("Tasks", jaCutTasks);

                int cutIndex = 1;
                for (Mark mark: markList)
                {
                    JSONObject joCutTask = new JSONObject();
                    jaCutTasks.put(joCutTask);

                    joCutTask.put("Label", "Cut Task nr. " + cutIndex++);
                    joCutTask.put("Type", "Cut");

                    JSONObject joCutParameters = new JSONObject();
                    joCutTask.put("Parameters", joCutParameters);

                    joCutParameters.put("StartTimeInSeconds", smpteTimeCodeToSeconds(mark.getIn()));
                    joCutParameters.put("EndTimeInSeconds", smpteTimeCodeToSeconds(mark.getOut()));

                    JSONArray jaReferences = new JSONArray();
                    joCutParameters.put("References", jaReferences);

                    JSONObject joReference = new JSONObject();
                    jaReferences.put(joReference);

                    joReference.put("ReferencePhysicalPathKey", currentPhysicalPath.getPhysicalPathKey());
                }

                JSONObject joGroupOfTasksOnSuccess = new JSONObject();
                joGroupOfTasks.put("OnSuccess", joGroupOfTasksOnSuccess);

                JSONObject joConcatTask = new JSONObject();
                joGroupOfTasksOnSuccess.put("Task", joConcatTask);

                joConcatTask.put("Label", "Concat of all the Cuts (" + markList.size() + ")");
                joConcatTask.put("Type", "Concat-Demuxer");

                JSONObject joConcatParameters = new JSONObject();
                joConcatTask.put("Parameters", joConcatParameters);
            }
            else
            {
                JSONObject joCutTask = new JSONObject();
                joWorkflow.put("Task", joCutTask);

                joCutTask.put("Label", "Cut Task");
                joCutTask.put("Type", "Cut");

                JSONObject joCutParameters = new JSONObject();
                joCutTask.put("Parameters", joCutParameters);

                joCutParameters.put("StartTimeInSeconds", smpteTimeCodeToSeconds(markList.get(0).getIn()));
                joCutParameters.put("EndTimeInSeconds", smpteTimeCodeToSeconds(markList.get(0).getOut()));

                JSONArray jaReferences = new JSONArray();
                joCutParameters.put("References", jaReferences);

                JSONObject joReference = new JSONObject();
                jaReferences.put(joReference);

                joReference.put("ReferencePhysicalPathKey", currentPhysicalPath.getPhysicalPathKey());
            }

            String url = "workflowEditor/workflowEditor.xhtml?loadType=metaDataContent"
                    + "&data=" + java.net.URLEncoder.encode(joWorkflow.toString(), "UTF-8")
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

    public void prepareCurrentMediaURL(MediaItem mediaItem, PhysicalPath physicalPath, boolean editMedia)
    {
        long ttlInSeconds = 60 * 60;
        int maxRetries = 20;

        this.mediaItem = mediaItem;

        PhysicalPath selectedPhysicalPath = null;

        if (physicalPath == null)
        {
            boolean sourceFile = false;

            selectedPhysicalPath = getSelectedPhysicalPath(mediaItem, sourceFile);
        }
        else
            selectedPhysicalPath = physicalPath;

        if (selectedPhysicalPath == null)
        {
            mLogger.info("prepareCurrentMediaURL. No correct profile to play"
                    + ", mediaItem.getMediaItemKey: " + mediaItem.getMediaItemKey()
                    + ", editMedia: " + editMedia
            );

            return;
        }

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

                boolean save = false;
                CatraMMS catraMMS = new CatraMMS();
                currentMediaURL = catraMMS.getDeliveryURL(
                        username, password, selectedPhysicalPath.getPhysicalPathKey(),
                        ttlInSeconds, maxRetries, save);

                setCurrentPhysicalPath(selectedPhysicalPath);
            }

            markList.clear();
        }
        catch (Exception e)
        {
            String errorMessage = "Exception: " + e;
            mLogger.error(errorMessage);
        }

        if (mediaItem.getContentType().equalsIgnoreCase("video"))
        {
            this.editVideo = editMedia;

            long defaultFramesPerSeconds = 25;
            try
            {
                framesPerSecond = null;

                String videoAvgFrameRate = selectedPhysicalPath.getVideoDetails().getVideoAvgFrameRate();
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
    }

    public void downloadCurrentMediaURL(MediaItem mediaItem, PhysicalPath physicalPath, boolean sourceFile)
    {
        long ttlInSeconds = 60 * 60;
        int maxRetries = 20;

        this.mediaItem = mediaItem;

        PhysicalPath selectedPhysicalPath = null;

        if (physicalPath == null)
            selectedPhysicalPath = getSelectedPhysicalPath(mediaItem, sourceFile);
        else
            selectedPhysicalPath = physicalPath;

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

                boolean save = true;
                CatraMMS catraMMS = new CatraMMS();
                currentMediaURL = catraMMS.getDeliveryURL(
                        username, password, selectedPhysicalPath.getPhysicalPathKey(),
                        ttlInSeconds, maxRetries, save);

                mLogger.info("Redirect to " + currentMediaURL);
                FacesContext.getCurrentInstance().getExternalContext().redirect(currentMediaURL);
            }
        }
        catch (Exception e)
        {
            String errorMessage = "Exception: " + e;
            mLogger.error(errorMessage);
        }
    }

    public PhysicalPath getSelectedPhysicalPath(MediaItem mediaItem, boolean sourceFile)
    {
        PhysicalPath selectedPhysicalPath = null;
        try
        {
            if (mediaItem.getContentType().equalsIgnoreCase("video"))
            {
                for (PhysicalPath localPhysicalPath: mediaItem.getPhysicalPathList())
                {
                    if (sourceFile)
                    {
                        if (localPhysicalPath.getEncodingProfileKey() == null)
                        {
                            selectedPhysicalPath = localPhysicalPath;

                            break;
                        }
                    }
                    else
                    {
                        if (localPhysicalPath.getFileFormat().equalsIgnoreCase("mp4")
                                || localPhysicalPath.getFileFormat().equalsIgnoreCase("mov"))
                        {
                            if (selectedPhysicalPath == null)
                                selectedPhysicalPath = localPhysicalPath;
                            else
                            {
                                if (localPhysicalPath.getVideoDetails().getVideoHeight().longValue() <
                                        selectedPhysicalPath.getVideoDetails().getVideoHeight().longValue())
                                    selectedPhysicalPath = localPhysicalPath;
                            }
                        }
                    }
                }
            }
            else if (mediaItem.getContentType().equalsIgnoreCase("audio"))
            {
                for (PhysicalPath localPhysicalPath: mediaItem.getPhysicalPathList())
                {
                    if (localPhysicalPath.getFileFormat().equalsIgnoreCase("mp4"))
                    {
                        if (selectedPhysicalPath == null)
                            selectedPhysicalPath = localPhysicalPath;
                        else
                        {
                            if (localPhysicalPath.getAudioDetails().getBitRate().longValue() >
                                    selectedPhysicalPath.getAudioDetails().getBitRate().longValue())
                                selectedPhysicalPath = localPhysicalPath;
                        }
                    }
                }
            }
            else if (mediaItem.getContentType().equalsIgnoreCase("image"))
            {
                for (PhysicalPath localPhysicalPath: mediaItem.getPhysicalPathList())
                {
                    if (selectedPhysicalPath == null)
                        selectedPhysicalPath = localPhysicalPath;
                    else
                    {
                        if (localPhysicalPath.getImageDetails().getHeight().longValue() <
                                selectedPhysicalPath.getImageDetails().getHeight().longValue())
                            selectedPhysicalPath = localPhysicalPath;
                    }
                }
            }

            if (selectedPhysicalPath == null)
            {
                mLogger.info("No correct profile to play"
                        + ", mediaItem.getMediaItemKey: " + mediaItem.getMediaItemKey()
                        + ", sourceFile: " + sourceFile
                );

                FacesMessage message = new FacesMessage(FacesMessage.SEVERITY_ERROR,
                        "Player", "No correct profile to play");
                FacesContext context = FacesContext.getCurrentInstance();
                context.addMessage(null, message);
            }
        }
        catch (Exception e)
        {
            mLogger.error("Exception: " + e);
        }


        return selectedPhysicalPath;
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

    public List<Mark> getMarkList() {
        return markList;
    }

    public void setMarkList(List<Mark> markList) {
        this.markList = markList;
    }

    public boolean isEditVideo() {
        return editVideo;
    }

    public void setEditVideo(boolean editVideo) {
        this.editVideo = editVideo;
    }

    public Long getFramesPerSecond() {
        return framesPerSecond;
    }

    public void setFramesPerSecond(Long framesPerSecond) {
        this.framesPerSecond = framesPerSecond;
    }

    public String getVideoCurrentTime() {
        return videoCurrentTime;
    }

    public void setVideoCurrentTime(String videoCurrentTime) {
        this.videoCurrentTime = videoCurrentTime;
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

    public Boolean getAddMarkButtonDisabled() {
        return addMarkButtonDisabled;
    }

    public void setAddMarkButtonDisabled(Boolean addMarkButtonDisabled) {
        this.addMarkButtonDisabled = addMarkButtonDisabled;
    }

    public MediaItem getMediaItem() {
        return mediaItem;
    }

    public void setMediaItem(MediaItem mediaItem) {
        this.mediaItem = mediaItem;
    }

    public String getVideoCurrentTimeHidden() {
        return videoCurrentTimeHidden;
    }

    public void setVideoCurrentTimeHidden(String videoCurrentTimeHidden) {
        this.videoCurrentTimeHidden = videoCurrentTimeHidden;
    }
}
