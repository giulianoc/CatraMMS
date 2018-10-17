package com.catramms.webservice.rest;

import com.catramms.backing.newWorkflow.IngestionResult;
import com.catramms.utility.catramms.CatraMMS;
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
@Path("/api")
public class CatraMMSServices {

    private static final Logger mLogger = Logger.getLogger(CatraMMSServices.class);

    @GET
    @Produces(MediaType.APPLICATION_JSON)
    @Path("status")
    public Response getStatus()
    {
        mLogger.info("Received getStatus");

        return Response.ok("{ \"status\": \"REST CatraMMS webservice running\" }").build();
    }

    @POST
    @Consumes(MediaType.APPLICATION_JSON + ";charset=utf-8")
    @Produces(MediaType.APPLICATION_JSON)
    @Path("cutVideo")
    public Response cutVideo(InputStream json, @Context HttpServletRequest pRequest
    )
    {
        String response = null;
        Long cutIngestionJobKey = null;

        mLogger.info("Received cutVideo");

        try
        {
            mLogger.info("InputStream to String..." );
            StringWriter writer = new StringWriter();
            IOUtils.copy(json, writer, "UTF-8");
            String sCutVideo = writer.toString();
            mLogger.info("sCutVideo (string): " + sCutVideo);

            JSONObject joCutVideo = new JSONObject(sCutVideo);
            mLogger.info("joCutVideo (json): " + joCutVideo);

            DateFormat simpleDateFormat = new SimpleDateFormat("yyyy-MM-dd HH:mm:ss.SSS");

            String cutVideoId = joCutVideo.getString("id");
            String cutVideoTitle = joCutVideo.getString("title");
            String cutVideoChannel = joCutVideo.getString("channel");
            Long cutVideoStartTime = joCutVideo.getLong("startTime");   // millisecs
            Long cutVideoEndTime = joCutVideo.getLong("endTime");
            String sCutVideoStartTime = simpleDateFormat.format(cutVideoStartTime);
            String sCutVideoEndTime = simpleDateFormat.format(cutVideoEndTime);

            {
                String ingester = "MP";
                Long userKey = new Long(1);
                String apiKey = "SU1.8ZO1O2zTg_5SvI12rfN9oQdjRru90XbMRSvACIxfqBXMYGj8k1P9lV4ZcvMRJL";
                String la1MediaDirectoryPathName = "/mnt/stream_recording/makitoRecording/La1/";
                String la2MediaDirectoryPathName = "/mnt/stream_recording/makitoRecording/La2/";
                String radioMediaDirectoryPathName = "/mnt/stream_recording/makitoRecording/Radio/";
                String cutVideoRetention = "3d";

                int secondsToWaitBeforeStartProcessingAFile = 5;

                try
                {
                    boolean firstOrLastChunkNotFound = false;

                    mLogger.info("cutVideo"
                                    + ", cutVideoTitle: " + cutVideoTitle
                    );

                    TreeMap<Date, File> fileTreeMap = new TreeMap<>();

                    DateFormat fileNameSimpleDateFormat = new SimpleDateFormat("yyyy-MM-dd-H'h'mm'm'ss's'");

                    List<File> mediaFilesToBeManaged = new ArrayList<>();
                    {
                        Calendar calendarStart = Calendar.getInstance();
                        calendarStart.setTime(new Date(cutVideoStartTime));
                        // there is one case where we have to consider the previous dir:
                        //  i.e.: video start: 08:00:15 and chunk start: 08:00:32
                        // in this case we need the last chunk of the previous dir
                        calendarStart.add(Calendar.HOUR_OF_DAY, -1);

                        Calendar calendarEnd = Calendar.getInstance();
                        calendarEnd.setTime(new Date(cutVideoEndTime));

                        DateFormat fileDateFormat = new SimpleDateFormat("yyyy/MM/dd/HH");

                        while (fileDateFormat.format(calendarStart.getTime()).compareTo(
                                fileDateFormat.format(calendarEnd.getTime())) <= 0)
                        {
                            String mediaDirectoryPathName;

                            if (cutVideoChannel.toLowerCase().contains("la1"))
                                mediaDirectoryPathName = la1MediaDirectoryPathName;
                            else if (cutVideoChannel.toLowerCase().contains("la2"))
                                mediaDirectoryPathName = la2MediaDirectoryPathName;
                            else
                                mediaDirectoryPathName = radioMediaDirectoryPathName;

                            mediaDirectoryPathName += fileDateFormat.format(calendarStart.getTime());
                            mLogger.info("Reading directory: " + mediaDirectoryPathName);
                            File mediaDirectoryFile = new File(mediaDirectoryPathName);
                            if (mediaDirectoryFile.exists())
                            {
                                File[] mediaFiles = mediaDirectoryFile.listFiles();

                                mediaFilesToBeManaged.addAll(Arrays.asList(mediaFiles));
                            }

                            calendarStart.add(Calendar.HOUR_OF_DAY, 1);
                        }
                    }

                    mLogger.info("Found " + mediaFilesToBeManaged.size() + " media files (" + "/" + cutVideoChannel + ")");

                    long videoChunkPeriodInSeconds = 60;
                    double cutStartTimeInSeconds = -1;
                    double cutEndTimeInSeconds = 0;
                    boolean firstChunkFound = false;
                    boolean lastChunkFound = false;

                    String fileExtension = null;

                    // fill fileTreeMap
                    for (File mediaFile : mediaFilesToBeManaged)
                    {
                        try
                        {
                            mLogger.info("Processing mediaFile"
                                            + ", cutVideoId: " + cutVideoId
                                            + ", mediaFile.getName: " + mediaFile.getName()
                            );

                            if (mediaFile.isDirectory())
                            {
                                // mLogger.info("Found a directory, ignored. Directory name: " + mediaFile.getName());

                                continue;
                            }
                            else if (new Date().getTime() - mediaFile.lastModified()
                                    < secondsToWaitBeforeStartProcessingAFile * 1000)
                            {
                                mLogger.info("Waiting at least " + secondsToWaitBeforeStartProcessingAFile + " seconds before start processing the file. File name: " + mediaFile.getName());

                                continue;
                            }
                            else if (mediaFile.length() == 0)
                            {
                                mLogger.info("Waiting mediaFile size is greater than 0"
                                                + ", File name: " + mediaFile.getName()
                                                + ", File lastModified: " + simpleDateFormat.format(mediaFile.lastModified())
                                );

                                continue;
                            }
                            else if (!mediaFile.getName().endsWith(".mp4")
                                    && !mediaFile.getName().endsWith(".ts"))
                            {
                                // mLogger.info("Found a NON mp4 file, ignored. File name: " + ftpFile.getName());

                                continue;
                            }

                            {
                                // Channel_1-2018-06-26-10h00m39s.mp4
                                // mLogger.info("###Processing of the " + ftpFile.getName() + " ftp file");

                                if (fileExtension == null)
                                    fileExtension = mediaFile.getName().substring(mediaFile.getName().lastIndexOf('.') + 1);

                                int videoChunkStartIndex = mediaFile.getName().length() - ("2018-06-26-10h00m39s.".length() + fileExtension.length());
                                if (!Character.isDigit(mediaFile.getName().charAt(videoChunkStartIndex)))
                                    videoChunkStartIndex++; // case when hour is just one digit
                                int videoChunkEndIndex = mediaFile.getName().length() - (fileExtension.length() + 1);
                                String sVideoChunkStartTime = mediaFile.getName().substring(videoChunkStartIndex, videoChunkEndIndex);

                                Date videoChunkStartTime = fileNameSimpleDateFormat.parse(sVideoChunkStartTime);

                                // SC: Start Chunk
                                // PS: Playout Start, PE: Playout End
                                // --------------SC--------------SC--------------SC--------------SC--------------
                                //                        PS-------------------------------PE

                                long nextVideoChunkStartInMilliseconds = videoChunkStartTime.getTime() + (videoChunkPeriodInSeconds * 1000);

                                if (videoChunkStartTime.getTime() <= cutVideoStartTime
                                        && cutVideoStartTime <= nextVideoChunkStartInMilliseconds)
                                {
                                    // first chunk

                                    firstChunkFound = true;

                                    fileTreeMap.put(videoChunkStartTime, mediaFile);

                                    double playoutMediaStartTimeInSeconds = ((double) cutVideoStartTime) / 1000;
                                    double videoChunkStartTimeInSeconds = ((double) videoChunkStartTime.getTime()) / 1000;

                                    cutStartTimeInSeconds = playoutMediaStartTimeInSeconds - videoChunkStartTimeInSeconds;

                                    if (videoChunkStartTime.getTime() <= cutVideoEndTime
                                            && cutVideoEndTime <= nextVideoChunkStartInMilliseconds)
                                    {
                                        // playout start-end is within just one chunk

                                        lastChunkFound = true;

                                        double playoutMediaEndTimeInSeconds = ((double) cutVideoEndTime) / 1000;

                                        cutEndTimeInSeconds += (playoutMediaEndTimeInSeconds - videoChunkStartTimeInSeconds);
                                    }
                                    else
                                    {
                                        cutEndTimeInSeconds += videoChunkPeriodInSeconds;
                                    }

                                    mLogger.info("Found first video chunk"
                                                    + ", cutVideoId: " + cutVideoId
                                                    + ", ftpFile.getName: " + mediaFile.getName()
                                                    + ", videoChunkStartTime: " + videoChunkStartTime
                                                    + ", sCutVideoStartTime: " + sCutVideoStartTime
                                                    + ", cutStartTimeInSeconds: " + cutStartTimeInSeconds
                                                    + ", cutEndTimeInSeconds: " + cutEndTimeInSeconds
                                    );
                                }
                                else if (cutVideoStartTime <= videoChunkStartTime.getTime()
                                        && nextVideoChunkStartInMilliseconds <= cutVideoEndTime)
                                {
                                    // internal chunk

                                    fileTreeMap.put(videoChunkStartTime, mediaFile);

                                    cutEndTimeInSeconds += videoChunkPeriodInSeconds;

                                    mLogger.info("Found internal video chunk"
                                                    + ", cutVideoId: " + cutVideoId
                                                    + ", mediaFile.getName: " + mediaFile.getName()
                                                    + ", videoChunkStartTime: " + videoChunkStartTime
                                                    + ", sCutVideoStartTime: " + sCutVideoStartTime
                                                    + ", cutEndTimeInSeconds: " + cutEndTimeInSeconds
                                    );
                                }
                                else if (videoChunkStartTime.getTime() <= cutVideoEndTime
                                        && cutVideoEndTime <= nextVideoChunkStartInMilliseconds)
                                {
                                    // last chunk

                                    lastChunkFound = true;

                                    fileTreeMap.put(videoChunkStartTime, mediaFile);

                                    double playoutMediaEndTimeInSeconds = ((double) cutVideoEndTime) / 1000;
                                    double videoChunkStartTimeInSeconds = ((double) videoChunkStartTime.getTime()) / 1000;

                                    cutEndTimeInSeconds += (playoutMediaEndTimeInSeconds - videoChunkStartTimeInSeconds);

                                    mLogger.info("Found last video chunk"
                                                    + ", cutVideoId: " + cutVideoId
                                                    + ", mediaFile.getName: " + mediaFile.getName()
                                                    + ", videoChunkStartTime: " + videoChunkStartTime
                                                    + ", sCutVideoStartTime: " + sCutVideoStartTime
                                                    + ", cutEndTimeInSeconds: " + cutEndTimeInSeconds
                                                    + ", playoutMediaEndTimeInSeconds: " + playoutMediaEndTimeInSeconds
                                                    + ", videoChunkStartTimeInSeconds: " + videoChunkStartTimeInSeconds
                                    );
                                }
                                else
                                {
                                    // external chunk

                                    /*
                                    mLogger.info("Found external video chunk"
                                                    + ", ftpFile.getName: " + ftpFile.getName()
                                                    + ", videoChunkStartTime: " + videoChunkStartTime
                                                    + ", sCutVideoStartTime: " + sCutVideoStartTime
                                    );
                                    */
                                }
                            }
                        }
                        catch (Exception ex)
                        {
                            String errorMessage = "exception processing the " + mediaFile.getName() + " file. Exception: " + ex
                                    + ", cutVideoId: " + cutVideoId
                                    ;
                            mLogger.warn(errorMessage);

                            continue;
                        }
                    }

                    if (!firstChunkFound || !lastChunkFound)
                    {
                        String errorMessage = "First and/or Last chunk were not generated yet. No media files found"
                                + ", cutVideoId: " + cutVideoId
                                + ", cutVideoTitle: " + cutVideoTitle
                                + ", cutVideoChannel: " + cutVideoChannel
                                + ", sCutVideoStartTime: " + sCutVideoStartTime
                                + ", sCutVideoEndTime: " + sCutVideoEndTime
                                + ", firstChunkFound: " + firstChunkFound
                                + ", lastChunkFound: " + lastChunkFound
                                ;
                        mLogger.warn(errorMessage);

                        firstOrLastChunkNotFound = true;

                        // return firstOrLastChunkNotFound;
                        throw new Exception(errorMessage);
                    }

                    if (fileTreeMap.size() == 0)
                    {
                        String errorMessage = "No media files found"
                                + ", cutVideoId: " + cutVideoId
                                + ", cutVideoTitle: " + cutVideoTitle
                                + ", cutVideoChannel: " + cutVideoChannel
                                + ", sCutVideoStartTime: " + sCutVideoStartTime
                                + ", sCutVideoEndTime: " + sCutVideoEndTime
                                ;
                        mLogger.error(errorMessage);

                        throw new Exception(errorMessage);
                    }

                    if (cutStartTimeInSeconds == -1 || cutEndTimeInSeconds == 0)
                    {
                        String errorMessage = "No media files found"
                                + ", cutVideoId: " + cutVideoId
                                + ", cutVideoTitle: " + cutVideoTitle
                                + ", cutVideoChannel: " + cutVideoChannel
                                + ", sCutVideoStartTime: " + sCutVideoStartTime
                                + ", sCutVideoEndTime: " + sCutVideoEndTime
                                ;
                        mLogger.error(errorMessage);

                        throw new Exception(errorMessage);
                    }

                    // build json
                    {
                        JSONObject joWorkflow = new JSONObject();
                        joWorkflow.put("Type", "Workflow");
                        joWorkflow.put("Label", cutVideoTitle);

                        JSONObject joOnSuccess = new JSONObject();


                        if (fileTreeMap.size() > 1)
                        {
                            JSONObject joGroupOfTasks = new JSONObject();
                            joWorkflow.put("Task", joGroupOfTasks);

                            joGroupOfTasks.put("Type", "GroupOfTasks");

                            JSONObject joParameters = new JSONObject();
                            joGroupOfTasks.put("Parameters", joParameters);

                            joParameters.put("ExecutionType", "parallel");

                            JSONArray jaTasks = new JSONArray();
                            joParameters.put("Tasks", jaTasks);

                            for (Date fileDate : fileTreeMap.keySet())
                            {
                                File mediaFile = fileTreeMap.get(fileDate);

                                JSONObject joAddContent = new JSONObject();
                                jaTasks.put(joAddContent);

                                joAddContent.put("Label", mediaFile.getName());
                                joAddContent.put("Type", "Add-Content");

                                JSONObject joAddContentParameters = new JSONObject();
                                joAddContent.put("Parameters", joAddContentParameters);

                                joAddContentParameters.put("Ingester", ingester);
                                joAddContentParameters.put("FileFormat", fileExtension);
                                joAddContentParameters.put("Retention", "0");
                                joAddContentParameters.put("Title", mediaFile.getName());
                                joAddContentParameters.put("FileSizeInBytes", mediaFile.length());
                            }

                            joGroupOfTasks.put("OnSuccess", joOnSuccess);
                        }
                        else
                        {
                            File mediaFile = fileTreeMap.firstEntry().getValue();

                            JSONObject joAddContent = new JSONObject();
                            joWorkflow.put("Task", joAddContent);

                            joAddContent.put("Label", mediaFile.getName());
                            joAddContent.put("Type", "Add-Content");

                            JSONObject joAddContentParameters = new JSONObject();
                            joAddContent.put("Parameters", joAddContentParameters);

                            joAddContentParameters.put("Ingester", ingester);
                            joAddContentParameters.put("FileFormat", "mp4");
                            joAddContentParameters.put("Retention", "0");
                            joAddContentParameters.put("Title", mediaFile.getName());
                            joAddContentParameters.put("FileSizeInBytes", mediaFile.length());

                            joAddContent.put("OnSuccess", joOnSuccess);
                        }

                        JSONObject joConcatDemux = new JSONObject();
                        {
                            joOnSuccess.put("Task", joConcatDemux);

                            joConcatDemux.put("Label", "Concat: " + cutVideoTitle);
                            joConcatDemux.put("Type", "Concat-Demuxer");

                            JSONObject joConcatDemuxParameters = new JSONObject();
                            joConcatDemux.put("Parameters", joConcatDemuxParameters);

                            joConcatDemuxParameters.put("Ingester", ingester);
                            joConcatDemuxParameters.put("Retention", "0");
                            joConcatDemuxParameters.put("Title", "Concat: " + cutVideoTitle);
                        }

                        JSONObject joCut = new JSONObject();
                        String cutLabel = "Cut: " + cutVideoTitle;
                        {
                            JSONObject joConcatDemuxOnSuccess = new JSONObject();
                            joConcatDemux.put("OnSuccess", joConcatDemuxOnSuccess);

                            joConcatDemuxOnSuccess.put("Task", joCut);

                            joCut.put("Label", cutLabel);
                            joCut.put("Type", "Cut");

                            JSONObject joCutParameters = new JSONObject();
                            joCut.put("Parameters", joCutParameters);

                            joCutParameters.put("Ingester", ingester);
                            joCutParameters.put("Retention", cutVideoRetention);
                            joCutParameters.put("Title", cutVideoTitle);
                            joCutParameters.put("StartTimeInSeconds", cutStartTimeInSeconds);
                            joCutParameters.put("EndTimeInSeconds", cutEndTimeInSeconds);

                            {
                                JSONObject joCutUserData = new JSONObject();
                                joCutParameters.put("UserData", joCutUserData);

                                joCutUserData.put("Channel", cutVideoChannel);
                                joCutUserData.put("StartTime", sCutVideoStartTime);
                                joCutUserData.put("EndTime", sCutVideoEndTime);
                            }
                        }

                        {
                            JSONObject joCutOnSuccess = new JSONObject();
                            joCut.put("OnSuccess", joCutOnSuccess);

                            JSONObject joGroupOfTasks = new JSONObject();
                            joCutOnSuccess.put("Task", joGroupOfTasks);

                            joGroupOfTasks.put("Type", "GroupOfTasks");

                            JSONObject joParameters = new JSONObject();
                            joGroupOfTasks.put("Parameters", joParameters);

                            joParameters.put("ExecutionType", "parallel");

                            JSONArray jaTasks = new JSONArray();
                            joParameters.put("Tasks", jaTasks);

                            {
                                JSONObject joCallback = new JSONObject();
                                jaTasks.put(joCallback);

                                joCallback.put("Label", "Callback: " + cutVideoTitle);
                                joCallback.put("Type", "HTTP-Callback");

                                JSONObject joCallbackParameters = new JSONObject();
                                joCallback.put("Parameters", joCallbackParameters);

                                joCallbackParameters.put("Protocol", "http");
                                joCallbackParameters.put("HostName", "mp-backend.rsi.ch");
                                joCallbackParameters.put("Port", 80);
                                joCallbackParameters.put("URI",
                                        "/metadataProcessorService/rest/veda/playoutMedia/" + cutVideoId + "/mmsFinished");
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

                                joEncode.put("Label", "Encode: " + cutVideoTitle);
                                joEncode.put("Type", "Encode");

                                JSONObject joEncodeParameters = new JSONObject();
                                joEncode.put("Parameters", joEncodeParameters);

                                joEncodeParameters.put("EncodingPriority", "Low");
                                joEncodeParameters.put("EncodingProfileLabel", "MMS_H264_veryslow_360p25_aac_92");
                            }
                        }
                        mLogger.info("Ready for the ingest"
                                + ", sCutVideoStartTime: " + sCutVideoStartTime
                                + ", sCutVideoEndTime: " + sCutVideoEndTime
                                + ", cutStartTimeInSeconds: " + cutStartTimeInSeconds
                                + ", cutEndTimeInSeconds: " + cutEndTimeInSeconds
                                + ", fileTreeMap.size: " + fileTreeMap.size()
                                + ", json Workflow: " + joWorkflow.toString(4));

                        {
                            List<IngestionResult> ingestionJobList = new ArrayList<>();

                            CatraMMS catraMMS = new CatraMMS();

                            IngestionResult workflowRoot = catraMMS.ingestWorkflow(userKey.toString(), apiKey,
                                    joWorkflow.toString(4), ingestionJobList);

                            ingestBinaries(userKey, apiKey, cutVideoChannel, fileTreeMap, ingestionJobList);

                            for (IngestionResult ingestionResult: ingestionJobList)
                            {
                                if (ingestionResult.getLabel().equals(cutLabel))
                                {
                                    cutIngestionJobKey = ingestionResult.getKey();

                                    break;
                                }
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
            mLogger.info("cutVideo response: " + response);

            return Response.ok(response).build();
        }
        catch (Exception e)
        {
            String errorMessage = "cutVideo failed. Exception: " + e;
            mLogger.error(errorMessage);

            response = "{\"status\": \"Failed\", "
                    + "\"errMsg\": \"" + errorMessage + "\" "
                    + "}";

            mLogger.info("cutVideo response: " + response);

            if (e.getMessage().toLowerCase().contains("no media files found"))
                return Response.status(510).entity(response).build();
            else
                return Response.status(Response.Status.INTERNAL_SERVER_ERROR).entity(response).build();
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
