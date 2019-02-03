package com.catramms.webservice.rest;

import com.catramms.backing.entity.MediaItem;
import com.catramms.backing.workflowEditor.utility.IngestionResult;
import com.catramms.utility.catramms.CatraMMS;
import com.catramms.webservice.rest.utility.CutMediaInfo;
import com.catramms.webservice.rest.utility.CutMediaInfo_rsi_v1;
import org.apache.commons.io.IOUtils;
import org.apache.log4j.Logger;
import org.json.JSONArray;
import org.json.JSONObject;

import javax.servlet.http.HttpServletRequest;
import javax.ws.rs.*;
import javax.ws.rs.core.Context;
import javax.ws.rs.core.MediaType;
import javax.ws.rs.core.Response;
import java.io.*;
import java.text.DateFormat;
import java.text.SimpleDateFormat;
import java.util.*;

/**
 * Created by multi on 16.09.18.
 */
@Path("/api_rsi_v1")
public class CatraMMSServices_rsi_v1 {

    private static final Logger mLogger = Logger.getLogger(CatraMMSServices_rsi_v1.class);

    // RSI keys:
    // private Long userKey = new Long(1);
    // private String apiKey = "SU1.8ZO1O2zTg_5SvI12rfN9oQdjRru90XbMRSvACIxfqBXMYGj8k1P9lV4ZcvMRJL";
    // Cloud keys:
    private Long userKey = new Long(1);
    private String apiKey = "SU1.8ZO1O2zTg_5SvI12rfN9oQdjRru90XbMRSvACIxf6iNunYz7nzLF0ZVfaeCChP";


    @GET
    @Produces(MediaType.APPLICATION_JSON)
    @Path("status")
    public Response getStatus(@Context HttpServletRequest pRequest)
    {
        mLogger.info("Received getStatus");

        for (String s: pRequest.getParameterMap().keySet())
        {
            for (String v: pRequest.getParameterMap().get(s))
                mLogger.info("key: " + s
                                + ", value: " + v
                );
        }

        return Response.ok("{ \"status\": \"REST CatraMMS webservice running\" }").build();
    }

    @POST
    @Consumes(MediaType.APPLICATION_JSON + ";charset=utf-8")
    @Produces(MediaType.APPLICATION_JSON)
    @Path("cutMedia")
    public Response cutMedia(InputStream json, @Context HttpServletRequest pRequest
    )
    {
        String response = null;
        Long cutIngestionJobKey = null;

        mLogger.info("Received cutMedia");

        try
        {
            JSONArray jaMediaCuts;
            {
                mLogger.info("InputStream to String..." );
                StringWriter writer = new StringWriter();
                IOUtils.copy(json, writer, "UTF-8");
                String sMediaCuts = writer.toString();
                mLogger.info("sMediaCuts (string): " + sMediaCuts);

                jaMediaCuts = new JSONArray(sMediaCuts);
                mLogger.info("jaMediaCuts (json): " + jaMediaCuts);
            }

            DateFormat simpleDateFormat = new SimpleDateFormat("yyyy-MM-dd HH:mm:ss.SSS");

            String cutPlayoutTitle = jaMediaCuts.getJSONObject(0).getString("title");
            String cutPlayoutChannel = jaMediaCuts.getJSONObject(0).getString("channel");
            String ingester = jaMediaCuts.getJSONObject(0).getString("userName");
            String sCutPlayoutStartTimeInMilliSecs;
            if (jaMediaCuts.getJSONObject(0).has("startTime"))
                sCutPlayoutStartTimeInMilliSecs = simpleDateFormat.format(jaMediaCuts.getJSONObject(0).getLong("startTime"));
            else
                sCutPlayoutStartTimeInMilliSecs = jaMediaCuts.getJSONObject(0).getString("sStartTime");
            String sCutPlayoutEndTimeInMilliSecs;
            if (jaMediaCuts.getJSONObject(0).has("endTime"))
                sCutPlayoutEndTimeInMilliSecs = simpleDateFormat.format(jaMediaCuts.getJSONObject(0).getLong("endTime"));
            else
                sCutPlayoutEndTimeInMilliSecs = jaMediaCuts.getJSONObject(0).getString("sEndTime");
            String cutPlayoutId = jaMediaCuts.getJSONObject(0).getString("id");
            String encodingPriority = jaMediaCuts.getJSONObject(0).getString("encodingPriority");

            // fill cutMediaInfoList
            List<CutMediaInfo_rsi_v1> cutMediaInfoList = new ArrayList<>();
            {
                for (int cutIndex = 0; cutIndex < jaMediaCuts.length(); cutIndex++)
                {
                    CutMediaInfo_rsi_v1 cutMediaInfo = new CutMediaInfo_rsi_v1();

                    JSONObject joMediaCut = jaMediaCuts.getJSONObject(cutIndex);
                    cutMediaInfo.setJoMediaCut(joMediaCut);

                    Long lCutPlayoutStartTimeInMilliSecs;
                    if (joMediaCut.has("startTime"))
                        lCutPlayoutStartTimeInMilliSecs = joMediaCut.getLong("startTime");
                    else
                        lCutPlayoutStartTimeInMilliSecs = simpleDateFormat.parse(joMediaCut.getString("sStartTime")).getTime();

                    Long lCutPlayoutEndTimeInMilliSecs;
                    if (joMediaCut.has("endTime"))
                        lCutPlayoutEndTimeInMilliSecs = joMediaCut.getLong("endTime");
                    else
                        lCutPlayoutEndTimeInMilliSecs = simpleDateFormat.parse(joMediaCut.getString("sEndTime")).getTime();

                    // SC: Start Chunk
                    // PS: Playout Start, PE: Playout End
                    // --------------SC--------------SC--------------SC--------------SC (chunk not included in cutMediaInfo.getFileTreeMap()
                    //                        PS-------------------------------PE

                    String jsonCondition = "( ";

                    // first chunk of the cut
                    jsonCondition += (
                            "(JSON_EXTRACT(userData, '$.utcChunkStartTime') * 1000 <= " + lCutPlayoutStartTimeInMilliSecs + " "
                                    + "and " + lCutPlayoutStartTimeInMilliSecs + " < JSON_EXTRACT(userData, '$.utcChunkEndTime') * 1000 ) "
                    );

                    jsonCondition += " or ";

                    // internal chunk of the cut
                    jsonCondition += (
                            "( " + lCutPlayoutStartTimeInMilliSecs + " <= JSON_EXTRACT(userData, '$.utcChunkStartTime') * 1000 "
                                    + "and JSON_EXTRACT(userData, '$.utcChunkEndTime') * 1000 <= " + lCutPlayoutEndTimeInMilliSecs + ") "
                    );

                    jsonCondition += " or ";

                    // last chunk of the cut
                    jsonCondition += (
                            "( JSON_EXTRACT(userData, '$.utcChunkStartTime') * 1000 < " + lCutPlayoutEndTimeInMilliSecs + " "
                                    + "and " + lCutPlayoutEndTimeInMilliSecs + " <= JSON_EXTRACT(userData, '$.utcChunkEndTime') * 1000 ) "
                    );

                    jsonCondition += ") ";

                    String jsonOrderBy = "JSON_EXTRACT(userData, '$.utcChunkStartTime')";

                    // get MediaItemsList...
                    List<MediaItem> mediaItemsList = new ArrayList<>();
                    try
                    {
                        // every MediaItem is 1 minute, let's do 5 hours max
                        Long maxMediaItemsNumber = new Long(60 * 5);
                        String contentType = "video";
                        Date begin = null;
                        Date end = null;
                        String title = null;

                        String ingestionDateAndTitleOrder = "desc";
                        CatraMMS catraMMS = new CatraMMS();
                        catraMMS.getMediaItems(
                                userKey.toString(), apiKey,
                                maxMediaItemsNumber, contentType, begin, end, title,
                                jsonCondition,
                                ingestionDateAndTitleOrder, jsonOrderBy,
                                mediaItemsList);
                    }
                    catch (Exception e)
                    {
                        String errorMessage = "Exception: " + e;
                        mLogger.error(errorMessage);
                    }

                    if (mediaItemsList.size() == 0)
                    {
                        String errorMessage = "No chunks found yet"
                                + ", cutPlayoutTitle: " + cutPlayoutTitle
                                + ", cutPlayoutChannel: " + cutPlayoutChannel
                                + ", sCutPlayoutStartTimeInMilliSecs: " + lCutPlayoutStartTimeInMilliSecs
                                + ", sCutPlayoutEndTimeInMilliSecs: " + lCutPlayoutEndTimeInMilliSecs
                                ;
                        mLogger.warn(errorMessage);

                        throw new Exception(errorMessage);
                    }

                    for (int mediaItemIndex = 0; mediaItemIndex < mediaItemsList.size(); mediaItemIndex++)
                    {
                        MediaItem mediaItem = mediaItemsList.get(mediaItemIndex);
                        JSONObject userData = new JSONObject(mediaItem.getUserData());

                        if (mediaItemIndex == 0)
                        {
                            // check that it is the first chunk

                            if (!(userData.getLong("utcChunkStartTime") * 1000 <= lCutPlayoutStartTimeInMilliSecs
                                    && lCutPlayoutStartTimeInMilliSecs < userData.getLong("utcChunkEndTime") * 1000)
                            )
                            {
                                // it is not the first chunk
                                String errorMessage = "First chunk was not found yet"
                                        + ", cutPlayoutTitle: " + cutPlayoutTitle
                                        + ", cutPlayoutChannel: " + cutPlayoutChannel
                                        + ", sCutMediaStartTimeInMilliSecs: " + lCutPlayoutStartTimeInMilliSecs
                                        + ", sCutMediaEndTimeInMilliSecs: " + lCutPlayoutEndTimeInMilliSecs
                                        ;
                                mLogger.warn(errorMessage);

                                throw new Exception(errorMessage);
                            }
                        }

                        if (mediaItemIndex == mediaItemsList.size() - 1)
                        {
                            // check that it is the last chunk

                            if (!(userData.getLong("utcChunkStartTime") * 1000 < lCutPlayoutEndTimeInMilliSecs
                                    && lCutPlayoutEndTimeInMilliSecs <= userData.getLong("utcChunkEndTime") * 1000)
                            )
                            {
                                // it is not the last chunk
                                String errorMessage = "Last chunk was not found yet"
                                        + ", cutPlayoutTitle: " + cutPlayoutTitle
                                        + ", cutPlayoutChannel: " + cutPlayoutChannel
                                        + ", sCutPlayoutStartTimeInMilliSecs: " + lCutPlayoutStartTimeInMilliSecs
                                        + ", sCutPlayoutEndTimeInMilliSecs: " + lCutPlayoutEndTimeInMilliSecs
                                        ;
                                mLogger.warn(errorMessage);

                                throw new Exception(errorMessage);
                            }
                        }

                        cutMediaInfo.getMediaItems().add(mediaItem);
                    }

                    cutMediaInfoList.add(cutMediaInfo);
                }
            }

            // build json
            {
                try
                {
                    String finalContentTitle = cutPlayoutTitle;

                    JSONObject joWorkflow = null;
                    String keyContentLabel;
                    if (cutPlayoutChannel.equalsIgnoreCase("la1")
                            || cutPlayoutChannel.equalsIgnoreCase("la2"))
                    {
                        String workflowTitle = cutPlayoutTitle;
                        String finalContentRetention = "2g";
                        joWorkflow = buildTVJson(workflowTitle, cutMediaInfoList,
                            ingester, finalContentTitle, finalContentRetention,
                            encodingPriority,
                            cutPlayoutChannel, sCutPlayoutStartTimeInMilliSecs, sCutPlayoutEndTimeInMilliSecs);
                    }
                    /*
                    else
                    {
                        // radio

                        int audioTrackNumber = 0;
                        if (cutMediaChannel.equalsIgnoreCase("RETE UNO"))
                            audioTrackNumber = reteUnoTrackNumber;
                        else if (cutMediaChannel.equalsIgnoreCase("RETE DUE"))
                            audioTrackNumber = reteDueTrackNumber;
                        else if (cutMediaChannel.equalsIgnoreCase("RETE TRE"))
                            audioTrackNumber = reteTreTrackNumber;

                        keyContentLabel = cutMediaTitle;

                        joWorkflow = buildRadioJson_2(cutMediaId, cutMediaChannel, sCutMediaStartTime, sCutMediaEndTime,
                                cutMediaInfoList, cutMediaTitle, keyContentLabel, encodingPriority,
                                ingester, fileExtension,
                                addContentPull, cutMediaRetention, audioTrackNumber);
                    }
                     */

                    {
                        List<IngestionResult> ingestionJobList = new ArrayList<>();

                        CatraMMS catraMMS = new CatraMMS();

                        IngestionResult workflowRoot = catraMMS.ingestWorkflow(userKey.toString(), apiKey,
                                joWorkflow.toString(4), ingestionJobList);

                        for (IngestionResult ingestionResult: ingestionJobList)
                        {
                            if (ingestionResult.getLabel().equals(finalContentTitle))
                            {
                                cutIngestionJobKey = ingestionResult.getKey();

                                break;
                            }
                        }
                    }
                }
                catch (Exception ex)
                {
                    String errorMessage = "Exception: " + ex;
                    mLogger.error(errorMessage);

                    throw ex;
                }
            }

            // here cutIngestionJobKey should be != null, anyway we will do the check
            if (cutIngestionJobKey == null)
            {
                String errorMessage = "cutIngestionJobKey is null!!!";
                mLogger.error(errorMessage);

                throw new Exception(errorMessage);
            }

            response = "{\"status\": \"Success\", "
                    + "\"cutIngestionJobKey\": " + cutIngestionJobKey + ", "
                    + "\"errMsg\": null }";
            mLogger.info("cutMedia response: " + response);

            return Response.ok(response).build();
        }
        catch (Exception e)
        {
            String errorMessage = "cutMedia failed. Exception: " + e;
            mLogger.error(errorMessage);

            response = "{\"status\": \"Failed\", "
                    + "\"errMsg\": \"" + errorMessage + "\" "
                    + "}";

            mLogger.info("cutMedia response: " + response);

            if (e.getMessage().toLowerCase().contains("no media files found"))
                return Response.status(510).entity(response).build();
            else
                return Response.status(Response.Status.INTERNAL_SERVER_ERROR).entity(response).build();
        }
    }

    private JSONObject buildTVJson(String workflowTitle, List<CutMediaInfo_rsi_v1> cutMediaInfoList,
                                   String ingester, String finalContentTitle, String finalContentRetention,
                                   String encodingPriority,
                                   String cutMediaChannel, String sCutMediaStartTimeInMilliSecs, String sCutMediaEndTimeInMilliSecs
    )
            throws Exception
    {
        try
        {
            JSONObject joWorkflow = new JSONObject();
            joWorkflow.put("Type", "Workflow");
            joWorkflow.put("Label", workflowTitle);

            JSONObject joConcatAndCutGroupOfTasks;
            {
                joConcatAndCutGroupOfTasks = new JSONObject();
                joWorkflow.put("Task", joConcatAndCutGroupOfTasks);

                joConcatAndCutGroupOfTasks.put("Type", "GroupOfTasks");

                JSONArray jaTasks;
                {
                    JSONObject joParameters = new JSONObject();
                    joConcatAndCutGroupOfTasks.put("Parameters", joParameters);

                    joParameters.put("ExecutionType", "parallel");

                    jaTasks = new JSONArray();
                    joParameters.put("Tasks", jaTasks);
                }

                for (int mediaCutIndex = 0; mediaCutIndex < cutMediaInfoList.size(); mediaCutIndex++)
                {
                    CutMediaInfo_rsi_v1 cutMediaInfo = cutMediaInfoList.get(mediaCutIndex);

                    JSONObject joConcatDemux = new JSONObject();
                    {
                        jaTasks.put(joConcatDemux);

                        joConcatDemux.put("Label", mediaCutIndex + ". Concat");
                        joConcatDemux.put("Type", "Concat-Demuxer");

                        JSONObject joParameters = new JSONObject();
                        joConcatDemux.put("Parameters", joParameters);

                        joParameters.put("Ingester", ingester);
                        joParameters.put("Retention", "0");
                        joParameters.put("Title", mediaCutIndex + ". Concat");

                        {
                            JSONArray jaReferences = new JSONArray();
                            joParameters.put("References", jaReferences);

                            for (MediaItem mediaItem : cutMediaInfo.getMediaItems()) {
                                JSONObject joReference = new JSONObject();
                                jaReferences.put(joReference);

                                joReference.put("ReferenceMediaItemKey", mediaItem.getMediaItemKey());
                            }
                        }
                    }

                    // cut
                    {
                        JSONObject joConcatDemuxOnSuccess = new JSONObject();
                        joConcatDemux.put("OnSuccess", joConcatDemuxOnSuccess);

                        JSONObject joCut = new JSONObject();
                        joConcatDemuxOnSuccess.put("Task", joCut);

                        // Same is used below in ReferenceLabel
                        String cutLabel = mediaCutIndex + ". " + "Cut";
                        joCut.put("Label", cutLabel);
                        joCut.put("Type", "Cut");

                        JSONObject joParameters = new JSONObject();
                        joCut.put("Parameters", joParameters);

                        joParameters.put("Ingester", ingester);
                        joParameters.put("Retention", "0");
                        joParameters.put("Title", mediaCutIndex + ". " + "Cut");
                        {
                            joParameters.put("OutputFileFormat", "mp4");

                            // SC: Start Chunk
                            // PS: Playout Start, PE: Playout End
                            // --------------SC--------------SC--------------SC--------------SC
                            //                        PS-------------------------------PE

                            DateFormat simpleDateFormat = new SimpleDateFormat("yyyy-MM-dd HH:mm:ss.SSS");

                            double cutStartTimeInSeconds;
                            {
                                Long firstMediaChunkStartTimeInSeconds;
                                {
                                    JSONObject joFirstChunkUserData = new JSONObject(cutMediaInfo.getMediaItems().get(0).getUserData());
                                    firstMediaChunkStartTimeInSeconds = joFirstChunkUserData.getLong("utcChunkStartTime");
                                }
                                Long lCutPlayoutStartTimeInMilliSecs;
                                if (cutMediaInfo.getJoMediaCut().has("startTime"))
                                    lCutPlayoutStartTimeInMilliSecs = cutMediaInfo.getJoMediaCut().getLong("startTime");
                                else
                                    lCutPlayoutStartTimeInMilliSecs = simpleDateFormat.parse(cutMediaInfo.getJoMediaCut().getString("sStartTime")).getTime();

                                cutStartTimeInSeconds = (double) ((lCutPlayoutStartTimeInMilliSecs - (firstMediaChunkStartTimeInSeconds * 1000)) / 1000);
                            }
                            joParameters.put("StartTimeInSeconds", cutStartTimeInSeconds);

                            double cutEndTimeInSeconds;
                            {
                                Long firstMediaChunkStartTimeInSeconds;
                                {
                                    JSONObject joFirstChunkUserData = new JSONObject(cutMediaInfo.getMediaItems().get(0).getUserData());
                                    firstMediaChunkStartTimeInSeconds = joFirstChunkUserData.getLong("utcChunkStartTime");
                                }
                                Long lCutPlayoutEndTimeInMilliSecs;
                                if (cutMediaInfo.getJoMediaCut().has("endTime"))
                                    lCutPlayoutEndTimeInMilliSecs = cutMediaInfo.getJoMediaCut().getLong("endTime");
                                else
                                    lCutPlayoutEndTimeInMilliSecs = simpleDateFormat.parse(cutMediaInfo.getJoMediaCut().getString("sEndTime")).getTime();

                                cutEndTimeInSeconds = (double) ((lCutPlayoutEndTimeInMilliSecs - (firstMediaChunkStartTimeInSeconds * 1000)) / 1000);
                            }
                            joParameters.put("EndTimeInSeconds", cutEndTimeInSeconds);
                        }
                    }
                }
            }

            // on partial concat/cut success: final concat
            JSONObject joFinalConcatDemux;
            {
                JSONObject jConcatAndCutGroupOfTasksOnSuccess = new JSONObject();
                joConcatAndCutGroupOfTasks.put("OnSuccess", jConcatAndCutGroupOfTasksOnSuccess);

                joFinalConcatDemux = new JSONObject();
                {
                    jConcatAndCutGroupOfTasksOnSuccess.put("Task", joFinalConcatDemux);

                    joFinalConcatDemux.put("Label", finalContentTitle);
                    joFinalConcatDemux.put("Type", "Concat-Demuxer");

                    JSONObject joParameters = new JSONObject();
                    joFinalConcatDemux.put("Parameters", joParameters);

                    joParameters.put("Ingester", ingester);
                    joParameters.put("Retention", finalContentRetention);
                    joParameters.put("Title", finalContentTitle);

                    {
                        JSONObject joUserData = new JSONObject();
                        joParameters.put("UserData", joUserData);

                        joUserData.put("Channel", cutMediaChannel);
                        joUserData.put("StartTimeInMilliSecs", sCutMediaStartTimeInMilliSecs);
                        joUserData.put("EndTimeInMilliSecs", sCutMediaEndTimeInMilliSecs);
                    }

                    {
                        JSONArray jaReferences = new JSONArray();
                        joParameters.put("References", jaReferences);

                        for (int mediaCutIndex = 0; mediaCutIndex < cutMediaInfoList.size(); mediaCutIndex++)
                        {
                            // CutMediaInfo cutMediaInfo = cutMediaInfoList.get(mediaCutIndex);

                            JSONObject joReference = new JSONObject();
                            jaReferences.put(joReference);

                            String cutLabel = mediaCutIndex + ". " + "Cut";
                            joReference.put("ReferenceLabel", cutLabel);
                        }
                    }
                }
            }

            {
                JSONObject joFinalConcatOnSuccess = new JSONObject();
                joFinalConcatDemux.put("OnSuccess", joFinalConcatOnSuccess);

                JSONObject joCallbackAndEncodeGroupOfTasks = new JSONObject();
                joFinalConcatOnSuccess.put("Task", joCallbackAndEncodeGroupOfTasks);

                joCallbackAndEncodeGroupOfTasks.put("Type", "GroupOfTasks");

                JSONObject joParameters = new JSONObject();
                joCallbackAndEncodeGroupOfTasks.put("Parameters", joParameters);

                joParameters.put("ExecutionType", "parallel");

                JSONArray jaTasks = new JSONArray();
                joParameters.put("Tasks", jaTasks);

                {
                    /* change temporary from Callback to Email

                    JSONObject joCallback = new JSONObject();
                    jaTasks.put(joCallback);

                    joCallback.put("Label", "Callback: " + keyTitle);
                    joCallback.put("Type", "HTTP-Callback");

                    JSONObject joCallbackParameters = new JSONObject();
                    joCallback.put("Parameters", joCallbackParameters);

                    joCallbackParameters.put("Protocol", "http");
                    joCallbackParameters.put("HostName", "mp-backend.rsi.ch");
                    joCallbackParameters.put("Port", 80);
                    joCallbackParameters.put("URI",
                            "/metadataProcessorService/rest/veda/playoutMedia/" + cutMediaId + "/mmsFinished");
                    joCallbackParameters.put("Parameters", "");
                    joCallbackParameters.put("Method", "GET");
                    joCallbackParameters.put("Timeout", 60);

                    // JSONArray jaHeaders = new JSONArray();
                    // joCallbackParameters.put("Headers", jaHeaders);

                    // jaHeaders.put("");
                    */
                    JSONObject joCallback = new JSONObject();
                    jaTasks.put(joCallback);

                    joCallback.put("Label", "Email");
                    joCallback.put("Type", "Email-Notification");

                    JSONObject joCallbackParameters = new JSONObject();
                    joCallback.put("Parameters", joCallbackParameters);

                    joCallbackParameters.put("ConfigurationLabel", "giu private");
                }

                {
                    JSONObject joEncode = new JSONObject();
                    jaTasks.put(joEncode);

                    joEncode.put("Label", "Encode: " + finalContentTitle);
                    joEncode.put("Type", "Encode");

                    JSONObject joEncodeParameters = new JSONObject();
                    joEncode.put("Parameters", joEncodeParameters);

                    joEncodeParameters.put("EncodingPriority", encodingPriority);
                    joEncodeParameters.put("EncodingProfileLabel", "MMS_H264_veryslow_360p25_aac_92");
                }
            }

            mLogger.info("Ready for the ingest"
                    + ", sCutMediaStartTimeInMilliSecs: " + sCutMediaStartTimeInMilliSecs
                    + ", sCutMediaEndTimeInMilliSecs: " + sCutMediaEndTimeInMilliSecs
                    + ", json Workflow: " + joWorkflow.toString(4));

            return joWorkflow;
        }
        catch (Exception e)
        {
            String errorMessage = "buildTVJson_2 failed. Exception: " + e;
            mLogger.error(errorMessage);

            throw e;
        }
    }

    private JSONObject buildRadioJson_2(String cutMediaId, String cutMediaChannel, String sCutMediaStartTime, String sCutMediaEndTime,
                                    List<CutMediaInfo> cutMediaInfoList,
                                    String keyTitle, String keyLabel, String encodingPriority,
                                    String ingester, String fileExtension,
                                    boolean addContentPull, String mediaRetention,
                                        int audioTrackNumber
    )
            throws Exception
    {
        try
        {
            JSONObject joWorkflow = new JSONObject();
            joWorkflow.put("Type", "Workflow");
            joWorkflow.put("Label", keyTitle);

            JSONObject joAddContentsGroupOfTasks;
            {
                joAddContentsGroupOfTasks = new JSONObject();
                joWorkflow.put("Task", joAddContentsGroupOfTasks);

                joAddContentsGroupOfTasks.put("Type", "GroupOfTasks");

                JSONObject joParameters = new JSONObject();
                joAddContentsGroupOfTasks.put("Parameters", joParameters);

                joParameters.put("ExecutionType", "parallel");

                JSONArray jaTasks = new JSONArray();
                joParameters.put("Tasks", jaTasks);

                for (int mediaCutIndex = 0; mediaCutIndex < cutMediaInfoList.size(); mediaCutIndex++)
                {
                    CutMediaInfo cutMediaInfo = cutMediaInfoList.get(mediaCutIndex);

                    for (Date fileDate : cutMediaInfo.getFileTreeMap().keySet())
                    {
                        File mediaFile = cutMediaInfo.getFileTreeMap().get(fileDate);

                        JSONObject joAddContent = new JSONObject();
                        jaTasks.put(joAddContent);

                        String addContentLabel = mediaCutIndex + ". " + mediaFile.getName();
                        joAddContent.put("Label", addContentLabel);
                        joAddContent.put("Type", "Add-Content");

                        JSONObject joAddContentParameters = new JSONObject();
                        joAddContent.put("Parameters", joAddContentParameters);

                        joAddContentParameters.put("Ingester", ingester);
                        joAddContentParameters.put("FileFormat", fileExtension);
                        joAddContentParameters.put("Retention", "0");
                        joAddContentParameters.put("Title", mediaFile.getName());
                        joAddContentParameters.put("FileSizeInBytes", mediaFile.length());
                        if (addContentPull)
                            joAddContentParameters.put("SourceURL", "copy://" + mediaFile.getAbsolutePath());

                        {
                            JSONObject joExtract = new JSONObject();
                            {
                                JSONObject joAddContentOnSuccess = new JSONObject();
                                joAddContent.put("OnSuccess", joAddContentOnSuccess);

                                joAddContentOnSuccess.put("Task", joExtract);

                                String extractLabel = mediaCutIndex + ". Extract " + mediaFile.getName();
                                joExtract.put("Label", extractLabel);
                                joExtract.put("Type", "Extract-Tracks");

                                JSONObject joExtractParameters = new JSONObject();
                                joExtract.put("Parameters", joExtractParameters);

                                joExtractParameters.put("Ingester", ingester);
                                joExtractParameters.put("Retention", "0");
                                joExtractParameters.put("Title", "Extract-Tracks: " + keyTitle);
                                {
                                    joExtractParameters.put("OutputFileFormat", "mp4");

                                    JSONArray jaTracks = new JSONArray();
                                    joExtractParameters.put("Tracks", jaTracks);

                                    JSONObject joTrack = new JSONObject();
                                    jaTracks.put(joTrack);

                                    joTrack.put("TrackType", "audio");
                                    joTrack.put("TrackNumber", audioTrackNumber);
                                }
                            }
                        }
                    }
                }
            }

            // on add content success, for each mediaCut: concat and cut
            JSONObject joPartialConcatAndCutGroupOfTasks;
            {
                JSONObject joAddContentsOnSuccess = new JSONObject();
                joAddContentsGroupOfTasks.put("OnSuccess", joAddContentsOnSuccess);

                joPartialConcatAndCutGroupOfTasks = new JSONObject();
                joAddContentsOnSuccess.put("Task", joPartialConcatAndCutGroupOfTasks);

                joPartialConcatAndCutGroupOfTasks.put("Type", "GroupOfTasks");

                JSONObject joParameters = new JSONObject();
                joPartialConcatAndCutGroupOfTasks.put("Parameters", joParameters);

                joParameters.put("ExecutionType", "parallel");

                JSONArray jaTasks = new JSONArray();
                joParameters.put("Tasks", jaTasks);

                {
                    for (int mediaCutIndex = 0; mediaCutIndex < cutMediaInfoList.size(); mediaCutIndex++)
                    {
                        CutMediaInfo cutMediaInfo = cutMediaInfoList.get(mediaCutIndex);

                        JSONObject joConcatDemux = new JSONObject();
                        {
                            jaTasks.put(joConcatDemux);

                            joConcatDemux.put("Label", mediaCutIndex + ". " + "Concat");
                            joConcatDemux.put("Type", "Concat-Demuxer");

                            JSONObject joConcatDemuxParameters = new JSONObject();
                            joConcatDemux.put("Parameters", joConcatDemuxParameters);

                            joConcatDemuxParameters.put("Ingester", ingester);
                            joConcatDemuxParameters.put("Retention", "0");
                            joConcatDemuxParameters.put("Title", "Concat: " + keyTitle);

                            {
                                JSONArray jaReferences = new JSONArray();
                                joConcatDemuxParameters.put("References", jaReferences);

                                for (Date fileDate : cutMediaInfo.getFileTreeMap().keySet())
                                {
                                    File mediaFile = cutMediaInfo.getFileTreeMap().get(fileDate);

                                    JSONObject joReference = new JSONObject();
                                    jaReferences.put(joReference);

                                    String extractLabel = mediaCutIndex + ". Extract " + mediaFile.getName();
                                    joReference.put("ReferenceLabel", extractLabel);
                                }
                            }
                        }

                        // cut
                        {
                            JSONObject joConcatDemuxOnSuccess = new JSONObject();
                            joConcatDemux.put("OnSuccess", joConcatDemuxOnSuccess);

                            JSONObject joCut = new JSONObject();
                            joConcatDemuxOnSuccess.put("Task", joCut);

                            String cutLabel = mediaCutIndex + ". " + "Cut";
                            joCut.put("Label", cutLabel);
                            joCut.put("Type", "Cut");

                            JSONObject joCutParameters = new JSONObject();
                            joCut.put("Parameters", joCutParameters);

                            joCutParameters.put("Ingester", ingester);
                            joCutParameters.put("Retention", "0");
                            joCutParameters.put("Title", mediaCutIndex + ". " + "Cut");
                            {
                                joCutParameters.put("OutputFileFormat", "mp4");

                                double cutStartTimeInSeconds;
                                {
                                    Date mediaChunkStartTime = cutMediaInfo.getFileTreeMap().firstEntry().getKey();
                                    Long mediaCutStartTimeInMilliSecs = cutMediaInfo.getJoMediaCut().getLong("startTime");

                                    cutStartTimeInSeconds = ((double) (mediaCutStartTimeInMilliSecs - mediaChunkStartTime.getTime())) / 1000;
                                }
                                joCutParameters.put("StartTimeInSeconds", cutStartTimeInSeconds);

                                double cutEndTimeInSeconds;
                                {
                                    Date mediaChunkStartTime = cutMediaInfo.getFileTreeMap().firstEntry().getKey();
                                    Long mediaCutEndTimeInMilliSecs = cutMediaInfo.getJoMediaCut().getLong("endTime");

                                    cutEndTimeInSeconds = ((double) (mediaCutEndTimeInMilliSecs - mediaChunkStartTime.getTime())) / 1000;
                                }
                                joCutParameters.put("EndTimeInSeconds", cutEndTimeInSeconds);
                            }
                        }
                    }
                }
            }

            // on partial concat/cut success: final concat
            JSONObject joFinalConcatDemux;
            {
                JSONObject joPartialConcatAndCutOnSuccess = new JSONObject();
                joPartialConcatAndCutGroupOfTasks.put("OnSuccess", joPartialConcatAndCutOnSuccess);

                joFinalConcatDemux = new JSONObject();
                {
                    joPartialConcatAndCutOnSuccess.put("Task", joFinalConcatDemux);

                    joFinalConcatDemux.put("Label", keyLabel);
                    joFinalConcatDemux.put("Type", "Concat-Demuxer");

                    JSONObject joFinalConcatParameters = new JSONObject();
                    joFinalConcatDemux.put("Parameters", joFinalConcatParameters);

                    joFinalConcatParameters.put("Ingester", ingester);
                    joFinalConcatParameters.put("Retention", mediaRetention);
                    joFinalConcatParameters.put("Title", keyTitle);

                    {
                        JSONObject joUserData = new JSONObject();
                        joFinalConcatParameters.put("UserData", joUserData);

                        joUserData.put("Channel", cutMediaChannel);
                        joUserData.put("StartTime", sCutMediaStartTime);
                        joUserData.put("EndTime", sCutMediaEndTime);
                    }

                    {
                        JSONArray jaReferences = new JSONArray();
                        joFinalConcatParameters.put("References", jaReferences);

                        for (int mediaCutIndex = 0; mediaCutIndex < cutMediaInfoList.size(); mediaCutIndex++)
                        {
                            // CutMediaInfo cutMediaInfo = cutMediaInfoList.get(mediaCutIndex);

                            JSONObject joReference = new JSONObject();
                            jaReferences.put(joReference);

                            String cutLabel = mediaCutIndex + ". " + "Cut";
                            joReference.put("ReferenceLabel", cutLabel);
                        }
                    }
                }
            }

            {
                JSONObject joFinalConcatOnSuccess = new JSONObject();
                joFinalConcatDemux.put("OnSuccess", joFinalConcatOnSuccess);

                JSONObject joCallbackAndEncodeGroupOfTasks = new JSONObject();
                joFinalConcatOnSuccess.put("Task", joCallbackAndEncodeGroupOfTasks);

                joCallbackAndEncodeGroupOfTasks.put("Type", "GroupOfTasks");

                JSONObject joParameters = new JSONObject();
                joCallbackAndEncodeGroupOfTasks.put("Parameters", joParameters);

                joParameters.put("ExecutionType", "parallel");

                JSONArray jaTasks = new JSONArray();
                joParameters.put("Tasks", jaTasks);

                {
                    JSONObject joCallback = new JSONObject();
                    jaTasks.put(joCallback);

                    joCallback.put("Label", "Callback: " + keyTitle);
                    joCallback.put("Type", "HTTP-Callback");

                    JSONObject joCallbackParameters = new JSONObject();
                    joCallback.put("Parameters", joCallbackParameters);

                    joCallbackParameters.put("Protocol", "http");
                    joCallbackParameters.put("HostName", "mp-backend.rsi.ch");
                    joCallbackParameters.put("Port", 80);
                    joCallbackParameters.put("URI",
                            "/metadataProcessorService/rest/veda/playoutMedia/" + cutMediaId + "/mmsFinished");
                    joCallbackParameters.put("Parameters", "");
                    joCallbackParameters.put("Method", "GET");
                    joCallbackParameters.put("Timeout", 60);

                                /*
                                JSONArray jaHeaders = new JSONArray();
                                joCallbackParameters.put("Headers", jaHeaders);

                                jaHeaders.put("");
                                */
                }

                {
                    JSONObject joEncode = new JSONObject();
                    jaTasks.put(joEncode);

                    joEncode.put("Label", "Encode: " + keyTitle);
                    joEncode.put("Type", "Encode");

                    JSONObject joEncodeParameters = new JSONObject();
                    joEncode.put("Parameters", joEncodeParameters);

                    joEncodeParameters.put("EncodingPriority", encodingPriority);
                    joEncodeParameters.put("EncodingProfileLabel", "MMS_AAC_92");
                }
            }

            mLogger.info("Ready for the ingest"
                    + ", sCutMediaStartTime: " + sCutMediaStartTime
                    + ", sCutMediaEndTime: " + sCutMediaEndTime
                    + ", json Workflow: " + joWorkflow.toString(4));

            return joWorkflow;
        }
        catch (Exception e)
        {
            String errorMessage = "buildTVJson_2 failed. Exception: " + e;
            mLogger.error(errorMessage);

            throw e;
        }
    }

    private void ingestBinaries(Long userKey, String apiKey,
                                String channel,
                                TreeMap<Date, File> fileTreeMap,
                                List<IngestionResult> ingestionJobList)
            throws Exception
    {
        try
        {
            for (Date fileDate : fileTreeMap.keySet())
            {
                File mediaFile = fileTreeMap.get(fileDate);

                IngestionResult fileIngestionTask = null;
                try
                {
                    for (IngestionResult ingestionTaskResult: ingestionJobList)
                    {
                        if (ingestionTaskResult.getLabel().equalsIgnoreCase(mediaFile.getName()))
                        {
                            fileIngestionTask = ingestionTaskResult;

                            break;
                        }
                    }

                    if (fileIngestionTask == null)
                    {
                        String errorMessage = "Content to be pushed was not found among the IngestionResults"
                                + ", mediaFile.getName: " + mediaFile.getName()
                                ;
                        mLogger.error(errorMessage);

                        continue;
                    }

                    mLogger.info("ftpClient.retrieveFileStream. Channel: " + channel
                            + ", Name: " + mediaFile.getName() + ", size (bytes): " + mediaFile.length());
                    InputStream inputStream = new DataInputStream(new FileInputStream(mediaFile));

                    CatraMMS catraMMS = new CatraMMS();
                    catraMMS.ingestBinaryContent(userKey.toString(), apiKey,
                            inputStream, mediaFile.length(),
                            fileIngestionTask.getKey());
                }
                catch (Exception e)
                {
                    String errorMessage = "Upload Push Content failed"
                            + ", fileIngestionTask.getLabel: " + fileIngestionTask.getLabel()
                            + ", Exception: " + e
                            ;
                    mLogger.error(errorMessage);
                }
            }
        }
        catch (Exception e)
        {
            String errorMessage = "Exception: " + e;
            mLogger.error(errorMessage);

            throw e;
        }
    }
}
