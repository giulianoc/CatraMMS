package com.catramms.backing;

import com.catramms.backing.common.SessionUtils;
import com.catramms.backing.entity.EncodingProfile;
import com.catramms.backing.entity.MediaItem;
import com.catramms.backing.entity.WorkspaceDetails;
import com.catramms.utility.catramms.CatraMMS;
import org.apache.log4j.Logger;
import org.json.JSONObject;

import javax.annotation.PostConstruct;
import javax.faces.application.FacesMessage;
import javax.faces.bean.ManagedBean;
import javax.faces.bean.ViewScoped;
import javax.faces.context.FacesContext;
import javax.servlet.http.HttpSession;
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
public class EncodingProfiles implements Serializable {

    // static because the class is Serializable
    private static final Logger mLogger = Logger.getLogger(EncodingProfiles.class);

    private String contentType;
    private List<String> contentTypesList;

    private String addEncodingProfileLabel;
    private String addEncodingProfileFileFormat;
    private List<String> videoFileFormatList;
    private List<String> audioFileFormatList;
    private List<String> imageFileFormatList;
    private String addEncodingProfileContentType;

    private String addEncodingProfileVideoCodec;
    private List<String> videoCodecsList;
    private String addEncodingProfileVideoProfile;
    private List<String> videoProfilesList;
    private Long addEncodingProfileVideoWidth;
    private Long addEncodingProfileVideoHeight;
    private boolean addEncodingProfileVideoTwoPasses;
    private Long addEncodingProfileVideoKBitRate;
    private String addEncodingProfileVideoOtherOutputParameters;
    private Long addEncodingProfileVideoFrameRate;
    private Long addEncodingProfileVideoKeyFrameIntervalInSeconds;
    private Long addEncodingProfileVideoMaxRate;
    private Long addEncodingProfileVideoBufferSize;

    private String addEncodingProfileAudioCodec;
    private List<String> audioCodecsList;
    private Long addEncodingProfileAudioKBitRate;
    private String addEncodingProfileAudioOtherOutputParameters;
    private boolean addEncodingProfileAudioStereo;
    private Long addEncodingProfileAudioSampleRate;
    private List<Long> audioSampleRateList;

    private Long addEncodingProfileImageWidth;
    private Long addEncodingProfileImageHeight;
    private boolean addEncodingProfileImageAspectRatio;
    private String addEncodingProfileImageInterlaceType;
    private List<String> imageInterlaceTypeList;

    private List<EncodingProfile> encodingProfileList = new ArrayList<>();

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

            addEncodingProfileContentType = contentTypesList.get(0);
        }

        {
            videoFileFormatList = new ArrayList<>();
            videoFileFormatList.add("MP4");

            addEncodingProfileFileFormat = videoFileFormatList.get(0);

            audioFileFormatList = new ArrayList<>();
            audioFileFormatList.add("MP4");

            imageFileFormatList = new ArrayList<>();
            imageFileFormatList.add("JPG");
            imageFileFormatList.add("PNG");
            imageFileFormatList.add("GIF");
        }

        {
            videoCodecsList = new ArrayList<>();
            videoCodecsList.add("libx264");
            videoCodecsList.add("libvpx");
            addEncodingProfileVideoCodec = videoCodecsList.get(0);

            videoProfilesList = new ArrayList<>();
            videoCodecChanged();

            audioCodecsList = new ArrayList<>();
            audioCodecsList.add("aac");

            addEncodingProfileAudioCodec = audioCodecsList.get(0);
        }

        {
            audioSampleRateList = new ArrayList<>();
            audioSampleRateList.add(new Long(8000));
            audioSampleRateList.add(new Long(11025));
            audioSampleRateList.add(new Long(12000));
            audioSampleRateList.add(new Long(16000));
            audioSampleRateList.add(new Long(22050));
            audioSampleRateList.add(new Long(24000));
            audioSampleRateList.add(new Long(32000));
            audioSampleRateList.add(new Long(44100));
            audioSampleRateList.add(new Long(48000));
            audioSampleRateList.add(new Long(64000));
            audioSampleRateList.add(new Long(88200));
            audioSampleRateList.add(new Long(96000));

            addEncodingProfileAudioSampleRate = null;
        }

        {
            imageInterlaceTypeList = new ArrayList<>();
            imageInterlaceTypeList.add("NoInterlace");
            imageInterlaceTypeList.add("LineInterlace");
            imageInterlaceTypeList.add("PlaneInterlace");
            imageInterlaceTypeList.add("PartitionInterlace");

            addEncodingProfileImageInterlaceType = imageInterlaceTypeList.get(0);
        }
    }

    public void videoCodecChanged()
    {
        if (addEncodingProfileVideoCodec.equalsIgnoreCase("libx264"))
        {
            videoProfilesList.add("high");
            videoProfilesList.add("baseline");
            videoProfilesList.add("main");
        }
        else if (addEncodingProfileVideoCodec.equalsIgnoreCase("libvpx"))
        {
            videoProfilesList.add("best");
            videoProfilesList.add("good");
        }

        addEncodingProfileVideoProfile = videoProfilesList.get(0);
    }

    public void addEncodingProfile()
    {
        String jsonEncodingProfile;

        try
        {
            JSONObject joEncodingProfile = new JSONObject();

            if (addEncodingProfileContentType.equalsIgnoreCase("video"))
            {
                joEncodingProfile.put("Label", addEncodingProfileLabel);
                joEncodingProfile.put("FileFormat", addEncodingProfileFileFormat);

                JSONObject joVideoEncodingProfile = new JSONObject();
                joEncodingProfile.put("Video", joVideoEncodingProfile);

                joVideoEncodingProfile.put("Codec", addEncodingProfileVideoCodec);
                if (addEncodingProfileVideoProfile != null && addEncodingProfileVideoProfile.equalsIgnoreCase(""))
                    joVideoEncodingProfile.put("Profile", addEncodingProfileVideoProfile);
                joVideoEncodingProfile.put("Width", addEncodingProfileVideoWidth);
                joVideoEncodingProfile.put("Height", addEncodingProfileVideoHeight);
                joVideoEncodingProfile.put("TwoPasses", addEncodingProfileVideoTwoPasses);
                joVideoEncodingProfile.put("TwoPasses", addEncodingProfileVideoTwoPasses);
                if (addEncodingProfileVideoKBitRate != null)
                    joVideoEncodingProfile.put("KBitRate", addEncodingProfileVideoKBitRate);
                if (addEncodingProfileVideoOtherOutputParameters != null && addEncodingProfileVideoOtherOutputParameters.equalsIgnoreCase(""))
                    joVideoEncodingProfile.put("OtherOutputParameters", addEncodingProfileVideoOtherOutputParameters);
                if (addEncodingProfileVideoMaxRate != null)
                    joVideoEncodingProfile.put("KMaxRate", addEncodingProfileVideoMaxRate);
                if (addEncodingProfileVideoBufferSize != null)
                    joVideoEncodingProfile.put("KBufSize", addEncodingProfileVideoBufferSize);
                if (addEncodingProfileVideoFrameRate != null)
                    joVideoEncodingProfile.put("FrameRate", addEncodingProfileVideoFrameRate);
                if (addEncodingProfileVideoKeyFrameIntervalInSeconds != null)
                    joVideoEncodingProfile.put("KeyFrameIntervalInSeconds", addEncodingProfileVideoKeyFrameIntervalInSeconds);

                JSONObject joAudioEncodingProfile = new JSONObject();
                joEncodingProfile.put("Audio", joAudioEncodingProfile);

                joAudioEncodingProfile.put("Codec", addEncodingProfileAudioCodec);
                if (addEncodingProfileAudioKBitRate != null)
                    joAudioEncodingProfile.put("KBitRate", addEncodingProfileAudioKBitRate);
                if (addEncodingProfileAudioOtherOutputParameters != null && addEncodingProfileAudioOtherOutputParameters.equalsIgnoreCase(""))
                    joAudioEncodingProfile.put("OtherOutputParameters", addEncodingProfileAudioOtherOutputParameters);
                joAudioEncodingProfile.put("ChannelsNumber", addEncodingProfileAudioStereo ? 2 : 1);
                if (addEncodingProfileAudioSampleRate != null)
                    joAudioEncodingProfile.put("SampleRate", addEncodingProfileAudioSampleRate);
            }
            else if (addEncodingProfileContentType.equalsIgnoreCase("audio"))
            {
                joEncodingProfile.put("Label", addEncodingProfileLabel);
                joEncodingProfile.put("FileFormat", addEncodingProfileFileFormat);

                JSONObject joAudioEncodingProfile = new JSONObject();
                joEncodingProfile.put("Audio", joAudioEncodingProfile);

                joAudioEncodingProfile.put("Codec", addEncodingProfileAudioCodec);
                if (addEncodingProfileAudioKBitRate != null)
                    joAudioEncodingProfile.put("KBitRate", addEncodingProfileAudioKBitRate);
                if (addEncodingProfileAudioOtherOutputParameters != null && addEncodingProfileAudioOtherOutputParameters.equalsIgnoreCase(""))
                    joAudioEncodingProfile.put("OtherOutputParameters", addEncodingProfileAudioOtherOutputParameters);
                joAudioEncodingProfile.put("ChannelsNumber", addEncodingProfileAudioStereo ? 2 : 1);
                if (addEncodingProfileAudioSampleRate != null)
                    joAudioEncodingProfile.put("SampleRate", addEncodingProfileAudioSampleRate);
            }
            else // if (addEncodingProfileContentType.equalsIgnoreCase("image"))
            {
                joEncodingProfile.put("Label", addEncodingProfileLabel);
                joEncodingProfile.put("FileFormat", addEncodingProfileFileFormat);

                JSONObject joImageEncodingProfile = new JSONObject();
                joEncodingProfile.put("Image", joImageEncodingProfile);

                joImageEncodingProfile.put("Width", addEncodingProfileImageWidth);
                joImageEncodingProfile.put("Height", addEncodingProfileImageHeight);
                joImageEncodingProfile.put("AspectRatio", addEncodingProfileImageAspectRatio);
                joImageEncodingProfile.put("InterlaceType", addEncodingProfileImageInterlaceType);
            }

            jsonEncodingProfile = joEncodingProfile.toString(4);
        }
        catch (Exception e)
        {
            String errorMessage = "Exception: " + e;
            mLogger.error(errorMessage);

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
                catraMMS.addEncodingProfile(
                        username, password,
                        contentType, jsonEncodingProfile);

                fillList(false);
            }
        }
        catch (Exception e)
        {
            String errorMessage = "Exception: " + e;
            mLogger.error(errorMessage);
        }
    }

    public void contentTypeChanged()
    {
        fillList(true);
    }

    public void fillList(boolean toBeRedirected)
    {
        SimpleDateFormat simpleDateFormat = new SimpleDateFormat("yyyy-MM-dd HH:mm:ss");

        mLogger.info("fillList"
                + ", toBeRedirected: " + toBeRedirected
                + ", contentType: " + contentType);

        if (toBeRedirected)
        {
            try
            {
                SimpleDateFormat simpleDateFormat_1 = new SimpleDateFormat("yyyy-MM-dd'T'HH:mm:ss");

                String url = "encodingProfiles.xhtml?contentType=" + contentType
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

                        encodingProfileList.clear();

                        CatraMMS catraMMS = new CatraMMS();
                        catraMMS.getEncodingProfiles(
                                username, password,
                                contentType, encodingProfileList);
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

    public List<String> getFileFormatList()
    {
        if (addEncodingProfileContentType.equalsIgnoreCase("video"))
            return videoFileFormatList;
        else if (addEncodingProfileContentType.equalsIgnoreCase("audio"))
            return audioFileFormatList;
        else // if (addEncodingProfileContentType.equalsIgnoreCase("video"))
            return imageFileFormatList;
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

    public List<EncodingProfile> getEncodingProfileList() {
        return encodingProfileList;
    }

    public void setEncodingProfileList(List<EncodingProfile> encodingProfileList) {
        this.encodingProfileList = encodingProfileList;
    }

    public String getAddEncodingProfileLabel() {
        return addEncodingProfileLabel;
    }

    public void setAddEncodingProfileLabel(String addEncodingProfileLabel) {
        this.addEncodingProfileLabel = addEncodingProfileLabel;
    }

    public String getAddEncodingProfileFileFormat() {
        return addEncodingProfileFileFormat;
    }

    public void setAddEncodingProfileFileFormat(String addEncodingProfileFileFormat) {
        this.addEncodingProfileFileFormat = addEncodingProfileFileFormat;
    }

    public String getAddEncodingProfileContentType() {
        return addEncodingProfileContentType;
    }

    public void setAddEncodingProfileContentType(String addEncodingProfileContentType) {
        this.addEncodingProfileContentType = addEncodingProfileContentType;
    }

    public String getAddEncodingProfileVideoCodec() {
        return addEncodingProfileVideoCodec;
    }

    public void setAddEncodingProfileVideoCodec(String addEncodingProfileVideoCodec) {
        this.addEncodingProfileVideoCodec = addEncodingProfileVideoCodec;
    }

    public List<String> getVideoCodecsList() {
        return videoCodecsList;
    }

    public void setVideoCodecsList(List<String> videoCodecsList) {
        this.videoCodecsList = videoCodecsList;
    }

    public String getAddEncodingProfileAudioCodec() {
        return addEncodingProfileAudioCodec;
    }

    public void setAddEncodingProfileAudioCodec(String addEncodingProfileAudioCodec) {
        this.addEncodingProfileAudioCodec = addEncodingProfileAudioCodec;
    }

    public List<String> getAudioCodecsList() {
        return audioCodecsList;
    }

    public void setAudioCodecsList(List<String> audioCodecsList) {
        this.audioCodecsList = audioCodecsList;
    }

    public String getAddEncodingProfileVideoProfile() {
        return addEncodingProfileVideoProfile;
    }

    public void setAddEncodingProfileVideoProfile(String addEncodingProfileVideoProfile) {
        this.addEncodingProfileVideoProfile = addEncodingProfileVideoProfile;
    }

    public List<String> getVideoProfilesList() {
        return videoProfilesList;
    }

    public void setVideoProfilesList(List<String> videoProfilesList) {
        this.videoProfilesList = videoProfilesList;
    }

    public Long getAddEncodingProfileVideoWidth() {
        return addEncodingProfileVideoWidth;
    }

    public void setAddEncodingProfileVideoWidth(Long addEncodingProfileVideoWidth) {
        this.addEncodingProfileVideoWidth = addEncodingProfileVideoWidth;
    }

    public Long getAddEncodingProfileVideoHeight() {
        return addEncodingProfileVideoHeight;
    }

    public void setAddEncodingProfileVideoHeight(Long addEncodingProfileVideoHeight) {
        this.addEncodingProfileVideoHeight = addEncodingProfileVideoHeight;
    }

    public boolean isAddEncodingProfileVideoTwoPasses() {
        return addEncodingProfileVideoTwoPasses;
    }

    public void setAddEncodingProfileVideoTwoPasses(boolean addEncodingProfileVideoTwoPasses) {
        this.addEncodingProfileVideoTwoPasses = addEncodingProfileVideoTwoPasses;
    }

    public Long getAddEncodingProfileVideoKBitRate() {
        return addEncodingProfileVideoKBitRate;
    }

    public void setAddEncodingProfileVideoKBitRate(Long addEncodingProfileVideoKBitRate) {
        this.addEncodingProfileVideoKBitRate = addEncodingProfileVideoKBitRate;
    }

    public String getAddEncodingProfileVideoOtherOutputParameters() {
        return addEncodingProfileVideoOtherOutputParameters;
    }

    public void setAddEncodingProfileVideoOtherOutputParameters(String addEncodingProfileVideoOtherOutputParameters) {
        this.addEncodingProfileVideoOtherOutputParameters = addEncodingProfileVideoOtherOutputParameters;
    }

    public Long getAddEncodingProfileVideoFrameRate() {
        return addEncodingProfileVideoFrameRate;
    }

    public void setAddEncodingProfileVideoFrameRate(Long addEncodingProfileVideoFrameRate) {
        this.addEncodingProfileVideoFrameRate = addEncodingProfileVideoFrameRate;
    }

    public Long getAddEncodingProfileVideoKeyFrameIntervalInSeconds() {
        return addEncodingProfileVideoKeyFrameIntervalInSeconds;
    }

    public void setAddEncodingProfileVideoKeyFrameIntervalInSeconds(Long addEncodingProfileVideoKeyFrameIntervalInSeconds) {
        this.addEncodingProfileVideoKeyFrameIntervalInSeconds = addEncodingProfileVideoKeyFrameIntervalInSeconds;
    }

    public Long getAddEncodingProfileVideoMaxRate() {
        return addEncodingProfileVideoMaxRate;
    }

    public void setAddEncodingProfileVideoMaxRate(Long addEncodingProfileVideoMaxRate) {
        this.addEncodingProfileVideoMaxRate = addEncodingProfileVideoMaxRate;
    }

    public Long getAddEncodingProfileVideoBufferSize() {
        return addEncodingProfileVideoBufferSize;
    }

    public void setAddEncodingProfileVideoBufferSize(Long addEncodingProfileVideoBufferSize) {
        this.addEncodingProfileVideoBufferSize = addEncodingProfileVideoBufferSize;
    }

    public Long getAddEncodingProfileAudioKBitRate() {
        return addEncodingProfileAudioKBitRate;
    }

    public void setAddEncodingProfileAudioKBitRate(Long addEncodingProfileAudioKBitRate) {
        this.addEncodingProfileAudioKBitRate = addEncodingProfileAudioKBitRate;
    }

    public String getAddEncodingProfileAudioOtherOutputParameters() {
        return addEncodingProfileAudioOtherOutputParameters;
    }

    public void setAddEncodingProfileAudioOtherOutputParameters(String addEncodingProfileAudioOtherOutputParameters) {
        this.addEncodingProfileAudioOtherOutputParameters = addEncodingProfileAudioOtherOutputParameters;
    }

    public boolean isAddEncodingProfileAudioStereo() {
        return addEncodingProfileAudioStereo;
    }

    public void setAddEncodingProfileAudioStereo(boolean addEncodingProfileAudioStereo) {
        this.addEncodingProfileAudioStereo = addEncodingProfileAudioStereo;
    }

    public Long getAddEncodingProfileAudioSampleRate() {
        return addEncodingProfileAudioSampleRate;
    }

    public void setAddEncodingProfileAudioSampleRate(Long addEncodingProfileAudioSampleRate) {
        this.addEncodingProfileAudioSampleRate = addEncodingProfileAudioSampleRate;
    }

    public List<Long> getAudioSampleRateList() {
        return audioSampleRateList;
    }

    public void setAudioSampleRateList(List<Long> audioSampleRateList) {
        this.audioSampleRateList = audioSampleRateList;
    }

    public Long getAddEncodingProfileImageWidth() {
        return addEncodingProfileImageWidth;
    }

    public void setAddEncodingProfileImageWidth(Long addEncodingProfileImageWidth) {
        this.addEncodingProfileImageWidth = addEncodingProfileImageWidth;
    }

    public Long getAddEncodingProfileImageHeight() {
        return addEncodingProfileImageHeight;
    }

    public void setAddEncodingProfileImageHeight(Long addEncodingProfileImageHeight) {
        this.addEncodingProfileImageHeight = addEncodingProfileImageHeight;
    }

    public boolean isAddEncodingProfileImageAspectRatio() {
        return addEncodingProfileImageAspectRatio;
    }

    public void setAddEncodingProfileImageAspectRatio(boolean addEncodingProfileImageAspectRatio) {
        this.addEncodingProfileImageAspectRatio = addEncodingProfileImageAspectRatio;
    }

    public String getAddEncodingProfileImageInterlaceType() {
        return addEncodingProfileImageInterlaceType;
    }

    public void setAddEncodingProfileImageInterlaceType(String addEncodingProfileImageInterlaceType) {
        this.addEncodingProfileImageInterlaceType = addEncodingProfileImageInterlaceType;
    }

    public List<String> getImageInterlaceTypeList() {
        return imageInterlaceTypeList;
    }

    public void setImageInterlaceTypeList(List<String> imageInterlaceTypeList) {
        this.imageInterlaceTypeList = imageInterlaceTypeList;
    }
}
