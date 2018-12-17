package com.catramms.webservice.rest;

import com.catramms.backing.newWorkflow.IngestionResult;
import com.catramms.utility.catramms.CatraMMS;
import com.catramms.utility.httpFetcher.HttpFeedFetcher;
import com.catramms.webservice.rest.utility.CutMediaInfo;
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

    @GET
    @Produces(MediaType.APPLICATION_JSON)
    @Path("youTubeCallback")
    public Response youTubeCallback(@Context HttpServletRequest pRequest)
    {
        try {
            mLogger.info("Received youTubeCallback");

            for (String s: pRequest.getParameterMap().keySet())
            {
                for (String v: pRequest.getParameterMap().get(s))
                    mLogger.info("key: " + s
                                    + ", value: " + v
                    );
            }

            String[] code = pRequest.getParameterMap().get("code");
            if (code == null || code.length == 0)
            {
                mLogger.error("'code' is not present");

                String[] error = pRequest.getParameterMap().get("error");
                if (error == null || error.length == 0)
                {
                    mLogger.error("'code/error' are not present");

                    return Response.ok("{ \"error\": \"<access token not available>\" }").build();
                }

                return Response.ok("{ \"error\": \"" + error + "\" }").build();
            }

            String authorizationToken = code[0];

            // clientId is retrieved by the credentials
            String clientId = "700586767360-96om12ccsf16m41qijrdagkk0oqf2o7m.apps.googleusercontent.com";

            // clientSecret is retrieved by the credentials
            String clientSecret = "Uabf92wFTF80vOL3z_zzRUtT";

            // this URL is configured inside the YouTube credentials
            String mmsYouTubeCallbak = "https://mms-gui.catrasoft.cloud/rest/api/youTubeCallback";

            String url = "https://www.googleapis.com/oauth2/v4/token";

            String body = "code=" + java.net.URLEncoder.encode(authorizationToken, "UTF-8")
                    + "&client_id=" + clientId
                    + "&client_secret=" + clientSecret
                    + "&redirect_uri=" + java.net.URLEncoder.encode(mmsYouTubeCallbak, "UTF-8")
                    + "&grant_type=authorization_code"
                    ;

            mLogger.info("url: " + url);

            Date now = new Date();
            int timeoutInSeconds = 120;
            int maxRetriesNumber = 1;
            String username = null;
            String password = null;
            String contentType = "application/x-www-form-urlencoded";
            String youTubeResponse = HttpFeedFetcher.fetchPostHttpsJson(url, contentType, timeoutInSeconds, maxRetriesNumber,
                    username, password, body);
            mLogger.info("Elapsed time login (@" + url + "@): @" + (new Date().getTime() - now.getTime()) + "@ millisecs."
                    + ", youTubeResponse: " + youTubeResponse
            );

            /*
            {
              "access_token": "ya29.GlxvBv2JUSUGmxHncG7KK118PHh4IY3ce6hbSRBoBjeXMiZjD53y3ZoeGchIkyJMb2rwQHlp-tQUZcIJ5zrt6CL2iWj-fV_2ArlAOCTy8y2B0_3KeZrbbJYgoFXCYA",
              "expires_in": 3600,
              "scope": "https://www.googleapis.com/auth/youtube https://www.googleapis.com/auth/youtube.upload",
              "token_type": "Bearer"
            }
             */
            JSONObject joYouTubeResponse = new JSONObject(youTubeResponse);

            JSONObject joCallbackResponse = new JSONObject();
            joCallbackResponse.put("comment", "Please, copy the 'refresh_token' value into the appropriate MMS configuration field");
            joCallbackResponse.put("refresh_token", joYouTubeResponse.getString("refresh_token"));

            return Response.ok(joCallbackResponse.toString(4)).build();
        }
        catch (Exception e)
        {
            mLogger.error("Exception: " + e);

            return Response.ok("{ \"error\": \"" + e + "\" }").build();
        }
    }

    @GET
    @Produces(MediaType.APPLICATION_JSON)
    @Path("facebookCallback")
    public Response facebookCallback(@Context HttpServletRequest pRequest)
    {
        try {
            mLogger.info("Received facebookCallback");

            /*
            In case of error:
                - error_code (i.e.: 100)
                - error_message (i.e.: Invalid Scopes: manage_pages, publish_pages. This message is only shown to developers. Users of your app will ignore these permissions if present. Please read the documentation for valid permissions at: https://developers.facebook.com/docs/facebook-login/permissions)
                - state (i.e: {"{st=state123abc,ds=123456789}"})

             */
            for (String s: pRequest.getParameterMap().keySet())
            {
                for (String v: pRequest.getParameterMap().get(s))
                    mLogger.info("key: " + s
                            + ", value: " + v
                    );
            }

            String[] code = pRequest.getParameterMap().get("code");
            if (code == null || code.length == 0)
            {
                mLogger.error("'code' is not present");

                String[] error = pRequest.getParameterMap().get("error_message");
                if (error == null || error.length == 0)
                {
                    mLogger.error("'code/error' are not present");

                    return Response.ok("{ \"error\": \"<access token not available>\" }").build();
                }

                return Response.ok("{ \"error\": \"" + error + "\" }").build();
            }

            String authorizationToken = code[0];

            // clientId is retrieved by the credentials
            String clientId = "1862418063793547";

            // clientSecret is retrieved by the credentials
            String clientSecret = "04a76f8e11e9dc70ea5975649a91574c";

            // this URL is configured inside the YouTube credentials
            String mmsFacebookCallbak = "https://mms-gui.catrasoft.cloud/rest/api/facebookCallback";

            String url = "https://graph.facebook.com/v3.2/oauth/access_token?";

            url += "code=" + java.net.URLEncoder.encode(authorizationToken, "UTF-8")
                    + "&client_id=" + clientId
                    + "&client_secret=" + clientSecret
                    + "&redirect_uri=" + java.net.URLEncoder.encode(mmsFacebookCallbak, "UTF-8")
                    ;

            mLogger.info("url: " + url);

            Date now = new Date();
            int timeoutInSeconds = 120;
            int maxRetriesNumber = 1;
            String username = null;
            String password = null;
            String facebookResponse = HttpFeedFetcher.fetchGetHttpsJson(url, timeoutInSeconds, maxRetriesNumber,
                    username, password);
            mLogger.info("Elapsed time login (@" + url + "@): @" + (new Date().getTime() - now.getTime()) + "@ millisecs."
                    + ", facebookResponse: " + facebookResponse
            );

            /*
            {
                "access_token":"EAAad2ZC8dnYsBAE0m0tfAcdNj6T7i6TdZAm5DeP81UNDNcHzp6z8z5W2n43DsQZBqMOyqwllkazwhdc2P1HsCDcWZBNXXmrX1GtgQOiL82MZACkoZAg3Q63pw6ZA72ZAxUD990sxjjIaRfiZAMJqonAdoVpv1ZAZAfzDy3GQG9xZCQF3jwZDZD",
                "token_type":"bearer",
                "expires_in":5099202
            }

            // expires_in: seconds-til-expiration

            A questo punto hai l'access token, puoi:
                1. verificare le proprietà del token (vedi https://developers.facebook.com/docs/facebook-login/manually-build-a-login-flow, sezione: Esame dei token d'accesso)
                    E' possibile che l'utente non accetta lo scope 'publish_pages'? In questo caso cosa faccio?
                2. Devo estendere la durata dell'access token dell'utente? (vedi https://developers.facebook.com/docs/facebook-login/access-tokens/refreshing)
                    Considera che se si utilizza un token dell'utente di lunga durata anche il token della pagina sarà di lunga durata (nel caso del punto 3 sotto)
                3. Devo ottenere un token d'accesso alla pagina? (in questo caso vedi: https://developers.facebook.com/docs/facebook-login/access-tokens#pagetokens, sezione: Token d'accesso della Pagina)
             */
            JSONObject joFacebookResponse = new JSONObject(facebookResponse);

            JSONObject joCallbackResponse = new JSONObject();
            joCallbackResponse.put("comment", "Please, copy the 'access_token' value into the appropriate MMS configuration field");
            joCallbackResponse.put("access_token", joFacebookResponse.getString("access_token"));

            return Response.ok(joCallbackResponse.toString(4)).build();
        }
        catch (Exception e)
        {
            mLogger.error("Exception: " + e);

            return Response.ok("{ \"error\": \"" + e + "\" }").build();
        }
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
            mLogger.info("InputStream to String..." );
            StringWriter writer = new StringWriter();
            IOUtils.copy(json, writer, "UTF-8");
            String sCutMedia = writer.toString();
            mLogger.info("sCutMedia (string): " + sCutMedia);

            JSONObject joCutMedia = new JSONObject(sCutMedia);
            mLogger.info("joCutMedia (json): " + joCutMedia);

            DateFormat simpleDateFormat = new SimpleDateFormat("yyyy-MM-dd HH:mm:ss.SSS");

            String cutMediaId = joCutMedia.getString("id");
            String cutMediaTitle = joCutMedia.getString("title");
            String cutMediaChannel = joCutMedia.getString("channel");
            String ingester = joCutMedia.getString("userName");
            Long cutMediaStartTimeInMilliseconds = joCutMedia.getLong("startTime");   // millisecs
            Long cutMediaEndTimeInMilliseconds = joCutMedia.getLong("endTime");
            String sCutMediaStartTime = simpleDateFormat.format(cutMediaStartTimeInMilliseconds);
            String sCutMediaEndTime = simpleDateFormat.format(cutMediaEndTimeInMilliseconds);

            {
                Long userKey = new Long(1);
                String apiKey = "SU1.8ZO1O2zTg_5SvI12rfN9oQdjRru90XbMRSvACIxfqBXMYGj8k1P9lV4ZcvMRJL";
                String la1MediaDirectoryPathName = "/mnt/stream_recording/makitoRecording/La1/";
                String la2MediaDirectoryPathName = "/mnt/stream_recording/makitoRecording/La2/";
                String radioMediaDirectoryPathName = "/mnt/stream_recording/makitoRecording/Radio/";
                int reteUnoTrackNumber = 0;
                int reteDueTrackNumber = 1;
                int reteTreTrackNumber = 2;
                String cutMediaRetention = "3d";
                boolean addContentPull = true;

                int secondsToWaitBeforeStartProcessingAFile = 5;

                try
                {
                    boolean firstOrLastChunkNotFound = false;

                    mLogger.info("cutMedia"
                                    + ", cutMediaTitle: " + cutMediaTitle
                    );

                    TreeMap<Date, File> fileTreeMap = new TreeMap<>();

                    // List<File> mediaFilesToBeManaged = new ArrayList<>();
                    {
                        Calendar calendarStart = Calendar.getInstance();
                        calendarStart.setTime(new Date(cutMediaStartTimeInMilliseconds));
                        // there is one case where we have to consider the previous dir:
                        //  i.e.: Media start: 08:00:15 and chunk start: 08:00:32
                        // in this case we need the last chunk of the previous dir
                        calendarStart.add(Calendar.HOUR_OF_DAY, -1);

                        Calendar calendarEnd = Calendar.getInstance();
                        calendarEnd.setTime(new Date(cutMediaEndTimeInMilliseconds));
                        // In case the video to cut finishes at the end of the hour, we need
                        // the next hour to get the next file
                        calendarEnd.add(Calendar.HOUR_OF_DAY, 1);

                        DateFormat fileDateFormat = new SimpleDateFormat("yyyy/MM/dd/HH");

                        while (fileDateFormat.format(calendarStart.getTime()).compareTo(
                                fileDateFormat.format(calendarEnd.getTime())) <= 0)
                        {
                            String mediaDirectoryPathName;

                            if (cutMediaChannel.toLowerCase().contains("la1"))
                                mediaDirectoryPathName = la1MediaDirectoryPathName;
                            else if (cutMediaChannel.toLowerCase().contains("la2"))
                                mediaDirectoryPathName = la2MediaDirectoryPathName;
                            else
                                mediaDirectoryPathName = radioMediaDirectoryPathName;

                            mediaDirectoryPathName += fileDateFormat.format(calendarStart.getTime());
                            mLogger.info("Reading directory: " + mediaDirectoryPathName);
                            File mediaDirectoryFile = new File(mediaDirectoryPathName);
                            if (mediaDirectoryFile.exists())
                            {
                                File[] mediaFiles = mediaDirectoryFile.listFiles();

                                // mediaFilesToBeManaged.addAll(Arrays.asList(mediaFiles));
                                // fill fileTreeMap
                                for (File mediaFile: mediaFiles)
                                {
                                    try {
                                        // File mediaFile = mediaFilesToBeManaged.get(mediaFileIndex);

                                        mLogger.debug("Processing mediaFile"
                                                        + ", cutMediaId: " + cutMediaId
                                                        + ", mediaFile.getName: " + mediaFile.getName()
                                        );

                                        if (mediaFile.isDirectory()) {
                                            // mLogger.info("Found a directory, ignored. Directory name: " + mediaFile.getName());

                                            continue;
                                        } else if (new Date().getTime() - mediaFile.lastModified()
                                                < secondsToWaitBeforeStartProcessingAFile * 1000) {
                                            mLogger.info("Waiting at least " + secondsToWaitBeforeStartProcessingAFile + " seconds before start processing the file. File name: " + mediaFile.getName());

                                            continue;
                                        } else if (mediaFile.length() == 0) {
                                            mLogger.debug("Waiting mediaFile size is greater than 0"
                                                            + ", File name: " + mediaFile.getName()
                                                            + ", File lastModified: " + simpleDateFormat.format(mediaFile.lastModified())
                                            );

                                            continue;
                                        } else if (!mediaFile.getName().endsWith(".mp4")
                                                && !mediaFile.getName().endsWith(".ts")) {
                                            // mLogger.info("Found a NON mp4 file, ignored. File name: " + ftpFile.getName());

                                            continue;
                                        }

                                        fileTreeMap.put(getMediaChunkStartTime(mediaFile.getName()), mediaFile);
                                    }
                                    catch (Exception ex)
                                    {
                                        String errorMessage = "exception processing the " + mediaFile.getName() + " file. Exception: " + ex
                                                + ", cutMediaId: " + cutMediaId
                                                ;
                                        mLogger.warn(errorMessage);

                                        continue;
                                    }
                                }
                            }

                            calendarStart.add(Calendar.HOUR_OF_DAY, 1);
                        }
                    }

                    mLogger.info("Found " + fileTreeMap.size() + " media files (" + "/" + cutMediaChannel + ")");

                    long cutStartTimeInMilliSeconds = -1;
                    boolean firstChunkFound = false;
                    boolean lastChunkFound = false;

                    ArrayList<Map.Entry<Date, File>> filesArray = new ArrayList(fileTreeMap.entrySet());

                    for (int mediaFileIndex = 0; mediaFileIndex < filesArray.size(); mediaFileIndex++)
                    {
                        try
                        {
                            Map.Entry<Date, File> dateFileEntry = filesArray.get(mediaFileIndex);

                            File mediaFile = dateFileEntry.getValue();
                            Date mediaChunkStartTime = dateFileEntry.getKey();

                            Date nextMediaChunkStart = null;
                            if (mediaFileIndex + 1 < filesArray.size())
                                nextMediaChunkStart = filesArray.get(mediaFileIndex + 1).getKey();

                            mLogger.info("Processing mediaFile"
                                            + ", cutMediaId: " + cutMediaId
                                            + ", mediaFile.getName: " + mediaFile.getName()
                            );

                            {
                                // Channel_1-2018-06-26-10h00m39s.mp4
                                // mLogger.info("###Processing of the " + ftpFile.getName() + " ftp file");

                                // Date mediaChunkStartTime = getMediaChunkStartTime(mediaFile.getName());

                                // SC: Start Chunk
                                // PS: Playout Start, PE: Playout End
                                // --------------SC--------------SC--------------SC--------------SC--------------
                                //                        PS-------------------------------PE


                                if (mediaChunkStartTime.getTime() <= cutMediaStartTimeInMilliseconds
                                        && (nextMediaChunkStart != null && cutMediaStartTimeInMilliseconds <= nextMediaChunkStart.getTime()))
                                {
                                    // first chunk

                                    firstChunkFound = true;

                                    // fileTreeMap.put(mediaChunkStartTime, mediaFile);

                                    /*
                                    double playoutMediaStartTimeInSeconds = ((double) cutMediaStartTimeInMilliseconds) / 1000;
                                    double mediaChunkStartTimeInSeconds = ((double) mediaChunkStartTime.getTime()) / 1000;

                                    cutStartTimeInSeconds = playoutMediaStartTimeInSeconds - mediaChunkStartTimeInSeconds;
                                    */
                                    cutStartTimeInMilliSeconds = cutMediaStartTimeInMilliseconds - mediaChunkStartTime.getTime();

                                    if (mediaChunkStartTime.getTime() <= cutMediaEndTimeInMilliseconds
                                            && (nextMediaChunkStart != null && cutMediaEndTimeInMilliseconds <= nextMediaChunkStart.getTime()))
                                    {
                                        // playout start-end is within just one chunk

                                        lastChunkFound = true;

                                        // double playoutMediaEndTimeInSeconds = ((double) cutMediaEndTimeInMilliseconds) / 1000;

                                        // cutEndTimeInSeconds += (playoutMediaEndTimeInSeconds - mediaChunkStartTimeInSeconds);
                                    }
                                    /*
                                    else
                                    {
                                        cutEndTimeInSeconds += mediaChunkPeriodInSeconds;
                                    }
                                    */

                                    mLogger.info("Found first media chunk"
                                                    + ", cutMediaId: " + cutMediaId
                                                    + ", ftpFile.getName: " + mediaFile.getName()
                                                    + ", mediaChunkStartTime: " + mediaChunkStartTime
                                                    + ", sCutMediaStartTime: " + sCutMediaStartTime
                                                    + ", cutStartTimeInMilliSeconds: " + cutStartTimeInMilliSeconds
                                                    + ", lastChunkFound: " + lastChunkFound
                                    );
                                }
                                else if (cutMediaStartTimeInMilliseconds <= mediaChunkStartTime.getTime()
                                        && (nextMediaChunkStart != null && nextMediaChunkStart.getTime() <= cutMediaEndTimeInMilliseconds))
                                {
                                    // internal chunk

                                    // fileTreeMap.put(mediaChunkStartTime, mediaFile);

                                    // cutEndTimeInSeconds += mediaChunkPeriodInSeconds;

                                    mLogger.info("Found internal media chunk"
                                                    + ", cutMediaId: " + cutMediaId
                                                    + ", mediaFile.getName: " + mediaFile.getName()
                                                    + ", mediaChunkStartTime: " + mediaChunkStartTime
                                                    + ", sCutMediaStartTime: " + sCutMediaStartTime
                                                    // + ", cutEndTimeInSeconds: " + cutEndTimeInSeconds
                                    );
                                }
                                else if (mediaChunkStartTime.getTime() <= cutMediaEndTimeInMilliseconds
                                        && (nextMediaChunkStart != null && cutMediaEndTimeInMilliseconds <= nextMediaChunkStart.getTime()))
                                {
                                    // last chunk

                                    lastChunkFound = true;

                                    // fileTreeMap.put(mediaChunkStartTime, mediaFile);

                                    // double playoutMediaEndTimeInSeconds = ((double) cutMediaEndTimeInMilliseconds) / 1000;
                                    // double mediaChunkStartTimeInSeconds = ((double) mediaChunkStartTime.getTime()) / 1000;

                                    // cutEndTimeInSeconds += (playoutMediaEndTimeInSeconds - mediaChunkStartTimeInSeconds);

                                    mLogger.info("Found last media chunk"
                                                    + ", cutMediaId: " + cutMediaId
                                                    + ", mediaFile.getName: " + mediaFile.getName()
                                                    + ", mediaChunkStartTime: " + mediaChunkStartTime
                                                    + ", sCutMediaStartTime: " + sCutMediaStartTime
                                                    // + ", cutEndTimeInSeconds: " + cutEndTimeInSeconds
                                                    // + ", playoutMediaEndTimeInSeconds: " + playoutMediaEndTimeInSeconds
                                                    // + ", mediaChunkStartTimeInSeconds: " + mediaChunkStartTimeInSeconds
                                    );
                                }
                                else
                                {
                                    // external chunk

                                    fileTreeMap.remove(mediaChunkStartTime);
                                    /*
                                    mLogger.info("Found external media chunk"
                                                    + ", ftpFile.getName: " + ftpFile.getName()
                                                    + ", mediaChunkStartTime: " + mediaChunkStartTime
                                                    + ", sCutMediaStartTime: " + sCutMediaStartTime
                                    );
                                    */
                                }
                            }
                        }
                        catch (Exception ex)
                        {
                            String errorMessage = "exception processing the " + filesArray.get(mediaFileIndex).getValue().getName() + " file. Exception: " + ex
                                    + ", cutMediaId: " + cutMediaId
                                    ;
                            mLogger.warn(errorMessage);

                            continue;
                        }
                    }

                    if (!firstChunkFound || !lastChunkFound)
                    {
                        String errorMessage = "First and/or Last chunk were not generated yet. No media files found"
                                + ", cutMediaId: " + cutMediaId
                                + ", cutMediaTitle: " + cutMediaTitle
                                + ", cutMediaChannel: " + cutMediaChannel
                                + ", sCutMediaStartTime: " + sCutMediaStartTime
                                + ", sCutMediaEndTime: " + sCutMediaEndTime
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
                                + ", cutMediaId: " + cutMediaId
                                + ", cutMediaTitle: " + cutMediaTitle
                                + ", cutMediaChannel: " + cutMediaChannel
                                + ", sCutMediaStartTime: " + sCutMediaStartTime
                                + ", sCutMediaEndTime: " + sCutMediaEndTime
                                ;
                        mLogger.error(errorMessage);

                        throw new Exception(errorMessage);
                    }

                    if (cutStartTimeInMilliSeconds == -1) // || cutEndTimeInSeconds == 0)
                    {
                        String errorMessage = "No media files found"
                                + ", cutMediaId: " + cutMediaId
                                + ", cutMediaTitle: " + cutMediaTitle
                                + ", cutMediaChannel: " + cutMediaChannel
                                + ", sCutMediaStartTime: " + sCutMediaStartTime
                                + ", sCutMediaEndTime: " + sCutMediaEndTime
                                ;
                        mLogger.error(errorMessage);

                        throw new Exception(errorMessage);
                    }

                    String firstFileName = fileTreeMap.firstEntry().getValue().getName();
                    String fileExtension = firstFileName.substring(firstFileName.lastIndexOf('.') + 1);

                    // build json
                    JSONObject joWorkflow = null;
                    String keyContentLabel;
                    if (cutMediaChannel.equalsIgnoreCase("la1")
                            || cutMediaChannel.equalsIgnoreCase("la2"))
                    {
                        keyContentLabel = cutMediaTitle;

                        joWorkflow = buildTVJson(cutMediaTitle, keyContentLabel, ingester, fileExtension,
                            addContentPull, cutMediaRetention,
                            cutStartTimeInMilliSeconds, cutMediaEndTimeInMilliseconds - cutMediaStartTimeInMilliseconds,
                            cutMediaId, cutMediaChannel, sCutMediaStartTime, sCutMediaEndTime,
                            fileTreeMap);
                    }
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

                        joWorkflow = buildRadioJson(cutMediaTitle, keyContentLabel, ingester, fileExtension,
                                addContentPull, cutMediaRetention,
                                cutStartTimeInMilliSeconds, cutMediaEndTimeInMilliseconds - cutMediaStartTimeInMilliseconds,
                                cutMediaId, cutMediaChannel, sCutMediaStartTime, sCutMediaEndTime,
                                audioTrackNumber, fileTreeMap);
                    }

                    {
                        List<IngestionResult> ingestionJobList = new ArrayList<>();

                        CatraMMS catraMMS = new CatraMMS();

                        IngestionResult workflowRoot = catraMMS.ingestWorkflow(userKey.toString(), apiKey,
                                joWorkflow.toString(4), ingestionJobList);

                        if (!addContentPull)
                            ingestBinaries(userKey, apiKey, cutMediaChannel, fileTreeMap, ingestionJobList);

                        for (IngestionResult ingestionResult: ingestionJobList)
                        {
                            if (ingestionResult.getLabel().equals(keyContentLabel))
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

    @POST
    @Consumes(MediaType.APPLICATION_JSON + ";charset=utf-8")
    @Produces(MediaType.APPLICATION_JSON)
    @Path("cutMedia2")
    public Response cutMedia2(InputStream json, @Context HttpServletRequest pRequest
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

            String cutMediaTitle = jaMediaCuts.getJSONObject(0).getString("title");
            String cutMediaChannel = jaMediaCuts.getJSONObject(0).getString("channel");
            String ingester = jaMediaCuts.getJSONObject(0).getString("userName");
            String sCutMediaStartTime = simpleDateFormat.format(jaMediaCuts.getJSONObject(0).getLong("startTime"));
            String sCutMediaEndTime = simpleDateFormat.format(jaMediaCuts.getJSONObject(0).getLong("endTime"));
            String cutMediaId = jaMediaCuts.getJSONObject(0).getString("id");
            String encodingPriority = jaMediaCuts.getJSONObject(0).getString("encodingPriority");

            {
                Long userKey = new Long(1);
                String apiKey = "SU1.8ZO1O2zTg_5SvI12rfN9oQdjRru90XbMRSvACIxfqBXMYGj8k1P9lV4ZcvMRJL";
                String la1MediaDirectoryPathName = "/mnt/stream_recording/makitoRecording/La1/";
                String la2MediaDirectoryPathName = "/mnt/stream_recording/makitoRecording/La2/";
                String radioMediaDirectoryPathName = "/mnt/stream_recording/makitoRecording/Radio/";
                int reteUnoTrackNumber = 0;
                int reteDueTrackNumber = 1;
                int reteTreTrackNumber = 2;
                String cutMediaRetention = "3d";
                boolean addContentPull = true;

                try
                {
                    mLogger.info("cutMedia"
                            + ", cutMediaTitle: " + cutMediaTitle
                                    + ", sCutMediaStartTime: " + sCutMediaStartTime
                                    + ", sCutMediaEndTime: " + sCutMediaEndTime
                    );

                    // fill fileTreeMap with all the files within the directories
                    TreeMap<Date, File> fileTreeMap = new TreeMap<>();
                    Date mediaChunkStartTimeTooEarly = null;
                    {
                        int secondsToWaitBeforeStartProcessingAFile = 5;

                        JSONObject joFirstMediaCut = jaMediaCuts.getJSONObject(0);
                        JSONObject joLastMediaCut = jaMediaCuts.getJSONObject(jaMediaCuts.length() - 1);

                        Calendar calendarStart = Calendar.getInstance();
                        calendarStart.setTime(new Date(joFirstMediaCut.getLong("startTime")));
                        // there is one case where we have to consider the previous dir:
                        //  i.e.: Media start: 08:00:15 and chunk start: 08:00:32
                        // in this case we need the last chunk of the previous dir
                        calendarStart.add(Calendar.HOUR_OF_DAY, -1);

                        Calendar calendarEnd = Calendar.getInstance();
                        calendarEnd.setTime(new Date(joLastMediaCut.getLong("endTime")));
                        // In case the video to cut finishes at the end of the hour, we need
                        // the next hour to get the next file
                        calendarEnd.add(Calendar.HOUR_OF_DAY, 1);

                        DateFormat fileDateFormat = new SimpleDateFormat("yyyy/MM/dd/HH");

                        while (fileDateFormat.format(calendarStart.getTime()).compareTo(
                                fileDateFormat.format(calendarEnd.getTime())) <= 0)
                        {
                            String mediaDirectoryPathName;

                            if (cutMediaChannel.toLowerCase().contains("la1"))
                                mediaDirectoryPathName = la1MediaDirectoryPathName;
                            else if (cutMediaChannel.toLowerCase().contains("la2"))
                                mediaDirectoryPathName = la2MediaDirectoryPathName;
                            else
                                mediaDirectoryPathName = radioMediaDirectoryPathName;

                            mediaDirectoryPathName += fileDateFormat.format(calendarStart.getTime());
                            mLogger.info("Reading directory: " + mediaDirectoryPathName);

                            File mediaDirectoryFile = new File(mediaDirectoryPathName);
                            if (mediaDirectoryFile != null && mediaDirectoryFile.exists())
                            {
                                File[] mediaFiles = mediaDirectoryFile.listFiles();

                                // mediaFilesToBeManaged.addAll(Arrays.asList(mediaFiles));
                                // fill fileTreeMap
                                for (File mediaFile : mediaFiles)
                                {
                                    try {
                                        // File mediaFile = mediaFilesToBeManaged.get(mediaFileIndex);

                                        mLogger.debug("Processing mediaFile"
                                                        + ", cutMediaTitle: " + cutMediaTitle
                                                        + ", mediaFile.getName: " + mediaFile.getName()
                                        );

                                        Date mediaChunkStartTime = getMediaChunkStartTime(mediaFile.getName());

                                        if (mediaFile.isDirectory())
                                        {
                                            // mLogger.info("Found a directory, ignored. Directory name: " + mediaFile.getName());

                                            continue;
                                        }
                                        else if (new Date().getTime() - mediaFile.lastModified()
                                                < secondsToWaitBeforeStartProcessingAFile * 1000)
                                        {
                                            mLogger.info("Waiting at least " + secondsToWaitBeforeStartProcessingAFile + " seconds before start processing the file"
                                                    + ", File name: " + mediaFile.getName()
                                                    + ", mediaChunkStartTimeTooEarly: " + mediaChunkStartTimeTooEarly
                                            );

                                            // since we are skipping the media file that does not have at least secondsToWaitBeforeStartProcessingAFile,
                                            // we have to skip also all the media files next to this one.
                                            // This because, in case we will accept the next one we may have problems since we expect to have
                                            // all the minutes available but this is not the case
                                            // This scenario should not happen since we the current files does not have secondsToWaitBeforeStartProcessingAFile
                                            // the next one should not have secondsToWaitBeforeStartProcessingAFile as well but it seems happen once.

                                            if (mediaChunkStartTimeTooEarly != null
                                                    && mediaChunkStartTime.getTime() < mediaChunkStartTimeTooEarly.getTime())
                                                mediaChunkStartTimeTooEarly = mediaChunkStartTime;

                                            continue;
                                        }
                                        else if (mediaFile.length() == 0)
                                        {
                                            mLogger.debug("Waiting mediaFile size is greater than 0"
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

                                        fileTreeMap.put(mediaChunkStartTime, mediaFile);
                                    }
                                    catch (Exception ex)
                                    {
                                        String errorMessage = "exception processing the " + mediaFile.getName() + " file. Exception: " + ex
                                                + ", cutMediaTitle: " + cutMediaTitle;
                                        mLogger.warn(errorMessage);

                                        continue;
                                    }
                                }
                            }

                            calendarStart.add(Calendar.HOUR_OF_DAY, 1);
                        }
                    }

                    mLogger.info("Found " + fileTreeMap.size() + " media files (" + "/" + cutMediaChannel + ")"
                            + ", mediaChunkStartTimeTooEarly: " + (mediaChunkStartTimeTooEarly == null ? "null" : simpleDateFormat.format(mediaChunkStartTimeTooEarly))
                    );

                    // fill cutMediaInfoList
                    List<CutMediaInfo> cutMediaInfoList = new ArrayList<>();
                    {
                        for (int mediaCutIndex = 0; mediaCutIndex < jaMediaCuts.length(); mediaCutIndex++)
                        {
                            CutMediaInfo cutMediaInfo = new CutMediaInfo();

                            cutMediaInfo.setJoMediaCut(jaMediaCuts.getJSONObject(mediaCutIndex));
                            cutMediaInfoList.add(cutMediaInfo);
                        }

                        ArrayList<Map.Entry<Date, File>> filesArray = new ArrayList(fileTreeMap.entrySet());

                        for (int mediaFileIndex = 0; mediaFileIndex < filesArray.size(); mediaFileIndex++)
                        {
                            try
                            {
                                Map.Entry<Date, File> dateFileEntry = filesArray.get(mediaFileIndex);

                                File mediaFile = dateFileEntry.getValue();
                                Date mediaChunkStartTime = dateFileEntry.getKey();

                                if (mediaChunkStartTimeTooEarly != null
                                        // && mediaFile.lastModified() >= mediaFileLastModifiedTooEarly.getTime())
                                        && mediaChunkStartTime.getTime() >= mediaChunkStartTimeTooEarly.getTime())
                                {
                                    // see the comment where mediaFileLastModifiedTooEarly is set

                                    mLogger.info("Found a file that cannot be used yet, so also the next files cannot be used as well. The scanning (sorted) of files is then interrupted"
                                            + ", mediaFile.getName: " + mediaFile.getName()
                                            + ", mediaFile.lastModified: " + simpleDateFormat.format(mediaFile.lastModified())
                                            + ", mediaChunkStartTime: " + simpleDateFormat.format(mediaChunkStartTime)
                                            + ", mediaChunkStartTimeTooEarly: " + simpleDateFormat.format(mediaChunkStartTimeTooEarly)
                                    );

                                    break;
                                }

                                Date nextMediaChunkStart = null;
                                if (mediaFileIndex + 1 < filesArray.size())
                                    nextMediaChunkStart = filesArray.get(mediaFileIndex + 1).getKey();

                                mLogger.info("Processing mediaFile"
                                        + ", cutMediaTitle: " + cutMediaTitle
                                        + ", mediaFile.getName: " + mediaFile.getName()
                                        + ", mediaChunkStartTime: " + simpleDateFormat.format(mediaChunkStartTime)
                                        + ", nextMediaChunkStart: " + nextMediaChunkStart == null ? "null" : simpleDateFormat.format(nextMediaChunkStart)
                                );

                                for (int mediaCutIndex = 0; mediaCutIndex < cutMediaInfoList.size(); mediaCutIndex++)
                                {
                                    CutMediaInfo cutMediaInfo = cutMediaInfoList.get(mediaCutIndex);

                                    // Channel_1-2018-06-26-10h00m39s.mp4
                                    // mLogger.info("###Processing of the " + ftpFile.getName() + " ftp file");

                                    // Date mediaChunkStartTime = getMediaChunkStartTime(mediaFile.getName());

                                    // SC: Start Chunk
                                    // PS: Playout Start, PE: Playout End
                                    // --------------SC--------------SC--------------SC--------------SC (chunk not included in cutMediaInfo.getFileTreeMap()
                                    //                        PS-------------------------------PE


                                    if (mediaChunkStartTime.getTime() <= cutMediaInfo.getJoMediaCut().getLong("startTime")
                                            && (nextMediaChunkStart != null && cutMediaInfo.getJoMediaCut().getLong("startTime") <= nextMediaChunkStart.getTime()))
                                    {
                                        // first chunk

                                        cutMediaInfo.setFirstChunkFound(true);
                                        cutMediaInfo.getFileTreeMap().put(mediaChunkStartTime, mediaFile);
                                        // cutMediaInfo.setChunksDurationInMilliSeconds(cutMediaInfo.getChunksDurationInMilliSeconds()
                                        //        + (nextMediaChunkStart.getTime() - mediaChunkStartTime.getTime()));

                                        if (mediaChunkStartTime.getTime() <= cutMediaInfo.getJoMediaCut().getLong("endTime")
                                                && (nextMediaChunkStart != null && cutMediaInfo.getJoMediaCut().getLong("endTime") <= nextMediaChunkStart.getTime()))
                                        {
                                            mLogger.info("playout start-end is within just one chunk");

                                            cutMediaInfo.setLastChunkFound(true);
                                        }

                                        mLogger.info("Found first media chunk"
                                                + ", cutMediaTitle: " + cutMediaTitle
                                                + ", ftpFile.getName: " + mediaFile.getName()
                                                + ", mediaChunkStartTime: " + mediaChunkStartTime
                                                + ", sCutMediaStartTime: " + cutMediaInfo.getJoMediaCut().getLong("startTime")
                                                // + ", cutStartTimeInMilliSeconds: " + cutMediaInfo.getCutStartTimeInMilliSeconds()
                                                + ", lastChunkFound: " + cutMediaInfo.isLastChunkFound()
                                        );
                                    }
                                    else if (cutMediaInfo.getJoMediaCut().getLong("startTime") <= mediaChunkStartTime.getTime()
                                            && (nextMediaChunkStart != null && nextMediaChunkStart.getTime() <= cutMediaInfo.getJoMediaCut().getLong("endTime")))
                                    {
                                        // internal chunk

                                        cutMediaInfo.getFileTreeMap().put(mediaChunkStartTime, mediaFile);
                                        // cutMediaInfo.setChunksDurationInMilliSeconds(cutMediaInfo.getChunksDurationInMilliSeconds()
                                        //        + (nextMediaChunkStart.getTime() - mediaChunkStartTime.getTime()));

                                        mLogger.info("Found internal media chunk"
                                                        + ", cutMediaTitle: " + cutMediaTitle
                                                        + ", mediaFile.getName: " + mediaFile.getName()
                                                        + ", mediaChunkStartTime: " + mediaChunkStartTime
                                                        + ", sCutMediaStartTime: " + cutMediaInfo.getJoMediaCut().getLong("startTime")
                                        );
                                    }
                                    else if (mediaChunkStartTime.getTime() <= cutMediaInfo.getJoMediaCut().getLong("endTime")
                                            && (nextMediaChunkStart != null && cutMediaInfo.getJoMediaCut().getLong("endTime") <= nextMediaChunkStart.getTime()))
                                    {
                                        // last chunk

                                        // SC: Start Chunk
                                        // PS: Playout Start, PE: Playout End
                                        // --------------SC--------------SC--------------SC--------------SC (chunk not included in cutMediaInfo.getFileTreeMap()
                                        //                        PS-------------------------------PE
                                        cutMediaInfo.getFileTreeMap().put(mediaChunkStartTime, mediaFile);
                                        // cutMediaInfo.setChunksDurationInMilliSeconds(cutMediaInfo.getChunksDurationInMilliSeconds()
                                        //        + (nextMediaChunkStart.getTime() - mediaChunkStartTime.getTime()));
                                        cutMediaInfo.setLastChunkFound(true);

                                        mLogger.info("Found last media chunk"
                                                + ", cutMediaTitle: " + cutMediaTitle
                                                + ", mediaFile.getName: " + mediaFile.getName()
                                                + ", mediaChunkStartTime: " + mediaChunkStartTime
                                                + ", sCutMediaEndTime: " + new Date(cutMediaInfo.getJoMediaCut().getLong("endTime"))
                                                + ", nextMediaChunkStart: " + nextMediaChunkStart
                                        );
                                    }
                                }
                            }
                            catch (Exception ex)
                            {
                                String errorMessage = "exception processing the " + filesArray.get(mediaFileIndex).getValue().getName() + " file. Exception: " + ex
                                        + ", cutMediaTitle: " + cutMediaTitle;
                                mLogger.warn(errorMessage);

                                continue;
                            }
                        }

                        for (int mediaCutIndex = 0; mediaCutIndex < cutMediaInfoList.size(); mediaCutIndex++)
                        {
                            CutMediaInfo cutMediaInfo = cutMediaInfoList.get(mediaCutIndex);

                            if (!cutMediaInfo.isFirstChunkFound() || !cutMediaInfo.isLastChunkFound())
                            {
                                String errorMessage = "First and/or Last chunk were not generated yet. No media files found"
                                        + ", cutMediaTitle: " + cutMediaTitle
                                        + ", cutMediaChannel: " + cutMediaChannel
                                        + ", sCutMediaStartTime: " + cutMediaInfo.getJoMediaCut().getLong("startTime")
                                        + ", sCutMediaEndTime: " + cutMediaInfo.getJoMediaCut().getLong("endTime")
                                        + ", firstChunkFound: " + cutMediaInfo.isFirstChunkFound()
                                        + ", lastChunkFound: " + cutMediaInfo.isLastChunkFound();
                                mLogger.warn(errorMessage);

                                throw new Exception(errorMessage);
                            }

                            if (cutMediaInfo.getFileTreeMap().size() == 0)
                            {
                                String errorMessage = "No media files found"
                                        + ", cutMediaTitle: " + cutMediaTitle
                                        + ", cutMediaChannel: " + cutMediaChannel
                                        + ", sCutMediaStartTime: " + cutMediaInfo.getJoMediaCut().getLong("startTime")
                                        + ", sCutMediaEndTime: " + cutMediaInfo.getJoMediaCut().getLong("endTime");
                                mLogger.error(errorMessage);

                                throw new Exception(errorMessage);
                            }
                        }
                    }

                    String fileExtension;
                    {
                        String firstFileName = fileTreeMap.firstEntry().getValue().getName();
                        fileExtension = firstFileName.substring(firstFileName.lastIndexOf('.') + 1);
                    }

                    // build json
                    JSONObject joWorkflow = null;
                    String keyContentLabel;
                    if (cutMediaChannel.equalsIgnoreCase("la1")
                            || cutMediaChannel.equalsIgnoreCase("la2"))
                    {
                        keyContentLabel = cutMediaTitle;

                        joWorkflow = buildTVJson_2(cutMediaId, cutMediaChannel, sCutMediaStartTime, sCutMediaEndTime,
                                cutMediaInfoList, cutMediaTitle, keyContentLabel, encodingPriority,
                                ingester, fileExtension,
                                addContentPull, cutMediaRetention);
                    }
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

                    {
                        List<IngestionResult> ingestionJobList = new ArrayList<>();

                        CatraMMS catraMMS = new CatraMMS();

                        IngestionResult workflowRoot = catraMMS.ingestWorkflow(userKey.toString(), apiKey,
                                joWorkflow.toString(4), ingestionJobList);

                        if (!addContentPull)
                            ingestBinaries(userKey, apiKey, cutMediaChannel, fileTreeMap, ingestionJobList);

                        for (IngestionResult ingestionResult: ingestionJobList)
                        {
                            if (ingestionResult.getLabel().equals(keyContentLabel))
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

    private Date getMediaChunkStartTime(String mediaFileName)
            throws Exception
    {
        // Channel_1-2018-06-26-10h00m39s.mp4
        // mLogger.info("###Processing of the " + ftpFile.getName() + " ftp file");

        String fileExtension = mediaFileName.substring(mediaFileName.lastIndexOf('.') + 1);

        int mediaChunkStartIndex = mediaFileName.length() - ("2018-06-26-10h00m39s.".length() + fileExtension.length());
        if (!Character.isDigit(mediaFileName.charAt(mediaChunkStartIndex)))
            mediaChunkStartIndex++; // case when hour is just one digit
        int mediaChunkEndIndex = mediaFileName.length() - (fileExtension.length() + 1);
        String sMediaChunkStartTime = mediaFileName.substring(mediaChunkStartIndex, mediaChunkEndIndex);

        DateFormat fileNameSimpleDateFormat = new SimpleDateFormat("yyyy-MM-dd-H'h'mm'm'ss's'");

        return fileNameSimpleDateFormat.parse(sMediaChunkStartTime);
    }

    private JSONObject buildTVJson(String keyTitle, String keyLabel, String ingester, String fileExtension,
                                   boolean addContentPull, String cutMediaRetention,
                                   Long cutStartTimeInMilliSeconds, Long cutMediaDurationInMilliSeconds,
                                   String cutMediaId, String cutMediaChannel, String sCutMediaStartTime, String sCutMediaEndTime,
                                   TreeMap<Date, File> fileTreeMap)
            throws Exception
    {
        try {
            JSONObject joWorkflow = new JSONObject();
            joWorkflow.put("Type", "Workflow");
            joWorkflow.put("Label", keyTitle);

            JSONObject joCut = null;

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
                    if (addContentPull)
                        joAddContentParameters.put("SourceURL", "copy://" + mediaFile.getAbsolutePath());
                }

                JSONObject joConcatDemux = new JSONObject();
                {
                    JSONObject joGroupOfTasksOnSuccess = new JSONObject();
                    joGroupOfTasks.put("OnSuccess", joGroupOfTasksOnSuccess);

                    joGroupOfTasksOnSuccess.put("Task", joConcatDemux);

                    joConcatDemux.put("Label", "Concat: " + keyTitle);
                    joConcatDemux.put("Type", "Concat-Demuxer");

                    JSONObject joConcatDemuxParameters = new JSONObject();
                    joConcatDemux.put("Parameters", joConcatDemuxParameters);

                    joConcatDemuxParameters.put("Ingester", ingester);
                    joConcatDemuxParameters.put("Retention", "0");
                    joConcatDemuxParameters.put("Title", "Concat: " + keyTitle);
                }

                joCut = new JSONObject();
                {
                    JSONObject joConcatDemuxOnSuccess = new JSONObject();
                    joConcatDemux.put("OnSuccess", joConcatDemuxOnSuccess);

                    joConcatDemuxOnSuccess.put("Task", joCut);

                    joCut.put("Label", keyLabel);
                    joCut.put("Type", "Cut");

                    JSONObject joCutParameters = new JSONObject();
                    joCut.put("Parameters", joCutParameters);

                    joCutParameters.put("Ingester", ingester);
                    joCutParameters.put("Retention", cutMediaRetention);
                    joCutParameters.put("Title", keyTitle);
                    {
                        joCutParameters.put("OutputFileFormat", "mp4");

                        double cutStartTimeInSeconds = ((double) cutStartTimeInMilliSeconds) / 1000;
                        joCutParameters.put("StartTimeInSeconds", cutStartTimeInSeconds);

                        Calendar calendar = Calendar.getInstance();
                        calendar.setTime(new Date(cutStartTimeInMilliSeconds));
                        calendar.add(Calendar.MILLISECOND, (int) (cutMediaDurationInMilliSeconds.longValue()));

                        double cutEndTimeInSeconds = ((double) calendar.getTime().getTime()) / 1000;
                        joCutParameters.put("EndTimeInSeconds", cutEndTimeInSeconds);
                    }

                    {
                        JSONObject joCutUserData = new JSONObject();
                        joCutParameters.put("UserData", joCutUserData);

                        joCutUserData.put("Channel", cutMediaChannel);
                        joCutUserData.put("StartTime", sCutMediaStartTime);
                        joCutUserData.put("EndTime", sCutMediaEndTime);
                    }
                }
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
                joAddContentParameters.put("FileFormat", fileExtension);
                joAddContentParameters.put("Retention", "0");
                joAddContentParameters.put("Title", mediaFile.getName());
                joAddContentParameters.put("FileSizeInBytes", mediaFile.length());
                if (addContentPull)
                    joAddContentParameters.put("SourceURL", "copy://" + mediaFile.getAbsolutePath());

                joCut = new JSONObject();
                {
                    JSONObject joAddContentOnSuccess = new JSONObject();
                    joAddContent.put("OnSuccess", joAddContentOnSuccess);

                    joAddContentOnSuccess.put("Task", joCut);

                    joCut.put("Label", keyLabel);
                    joCut.put("Type", "Cut");

                    JSONObject joCutParameters = new JSONObject();
                    joCut.put("Parameters", joCutParameters);

                    joCutParameters.put("Ingester", ingester);
                    joCutParameters.put("Retention", cutMediaRetention);
                    joCutParameters.put("Title", keyTitle);
                    {
                        joCutParameters.put("OutputFileFormat", "mp4");

                        double cutStartTimeInSeconds = ((double) cutStartTimeInMilliSeconds) / 1000;
                        joCutParameters.put("StartTimeInSeconds", cutStartTimeInSeconds);

                        Calendar calendar = Calendar.getInstance();
                        calendar.setTime(new Date(cutStartTimeInMilliSeconds));
                        calendar.add(Calendar.MILLISECOND, (int) (cutMediaDurationInMilliSeconds.longValue()));

                        double cutEndTimeInSeconds = ((double) calendar.getTime().getTime()) / 1000;
                        joCutParameters.put("EndTimeInSeconds", cutEndTimeInSeconds);
                    }

                    {
                        JSONObject joCutUserData = new JSONObject();
                        joCutParameters.put("UserData", joCutUserData);

                        joCutUserData.put("Channel", cutMediaChannel);
                        joCutUserData.put("StartTime", sCutMediaStartTime);
                        joCutUserData.put("EndTime", sCutMediaEndTime);
                    }
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

                    joEncodeParameters.put("EncodingPriority", "Low");
                    joEncodeParameters.put("EncodingProfileLabel", "MMS_H264_veryslow_360p25_aac_92");
                }
            }
            mLogger.info("Ready for the ingest"
                    + ", sCutMediaStartTime: " + sCutMediaStartTime
                    + ", sCutMediaEndTime: " + sCutMediaEndTime
                    // + ", cutStartTimeInSeconds: " + cutStartTimeInSeconds
                    // + ", cutEndTimeInSeconds: " + cutEndTimeInSeconds
                    + ", fileTreeMap.size: " + fileTreeMap.size()
                    + ", json Workflow: " + joWorkflow.toString(4));

            return joWorkflow;
        }
        catch (Exception e)
        {
            String errorMessage = "buildTVJson failed. Exception: " + e;
            mLogger.error(errorMessage);

            throw e;
        }
    }

    private JSONObject buildTVJson_2(String cutMediaId, String cutMediaChannel, String sCutMediaStartTime, String sCutMediaEndTime,
                                    List<CutMediaInfo> cutMediaInfoList,
                                    String keyTitle, String keyLabel, String encodingPriority,
                                    String ingester, String fileExtension,
                                    boolean addContentPull, String mediaRetention
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

                                    String addContentLabel = mediaCutIndex + ". " + mediaFile.getName();
                                    joReference.put("ReferenceLabel", addContentLabel);
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

                                // SC: Start Chunk
                                // PS: Playout Start, PE: Playout End
                                // --------------SC--------------SC--------------SC--------------SC (chunk not included in cutMediaInfo.getFileTreeMap()
                                //                        PS-------------------------------PE

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
                    joEncodeParameters.put("EncodingProfileLabel", "MMS_H264_veryslow_360p25_aac_92");
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

    // Extract-Tracks at the end for all the Add-Content
    private JSONObject buildRadioJson(String keyTitle, String keyLabel, String ingester, String fileExtension,
                                        boolean addContentPull, String cutMediaRetention,
                                        Long cutStartTimeInMilliSeconds, Long cutMediaDurationInMilliSeconds,
                                        String cutMediaId, String cutMediaChannel, String sCutMediaStartTime, String sCutMediaEndTime,
                                        int audioTrackNumber, TreeMap<Date, File> fileTreeMap)
            throws Exception
    {
        try
        {
            JSONObject joWorkflow = new JSONObject();
            joWorkflow.put("Type", "Workflow");
            joWorkflow.put("Label", keyTitle);

            JSONObject joCut = null;

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
                    if (addContentPull)
                        joAddContentParameters.put("SourceURL", "copy://" + mediaFile.getAbsolutePath());
                }

                JSONObject joExtract = new JSONObject();
                {
                    JSONObject joGroupOfTaskOnSuccess = new JSONObject();
                    joGroupOfTasks.put("OnSuccess", joGroupOfTaskOnSuccess);

                    joGroupOfTaskOnSuccess.put("Task", joExtract);

                    joExtract.put("Label", "Extract-Tracks: " + keyTitle);
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

                JSONObject joConcatDemux = new JSONObject();
                {
                    JSONObject joExtractTracksOnSuccess = new JSONObject();
                    joExtract.put("OnSuccess", joExtractTracksOnSuccess);

                    joExtractTracksOnSuccess.put("Task", joConcatDemux);

                    joConcatDemux.put("Label", "Concat: " + keyTitle);
                    joConcatDemux.put("Type", "Concat-Demuxer");

                    JSONObject joConcatDemuxParameters = new JSONObject();
                    joConcatDemux.put("Parameters", joConcatDemuxParameters);

                    joConcatDemuxParameters.put("Ingester", ingester);
                    joConcatDemuxParameters.put("Retention", "0");
                    joConcatDemuxParameters.put("Title", "Concat: " + keyTitle);
                }

                joCut = new JSONObject();
                {
                    JSONObject joConcatDemuxOnSuccess = new JSONObject();
                    joConcatDemux.put("OnSuccess", joConcatDemuxOnSuccess);

                    joConcatDemuxOnSuccess.put("Task", joCut);

                    joCut.put("Label", keyLabel);
                    joCut.put("Type", "Cut");

                    JSONObject joCutParameters = new JSONObject();
                    joCut.put("Parameters", joCutParameters);

                    joCutParameters.put("Ingester", ingester);
                    joCutParameters.put("Retention", cutMediaRetention);
                    joCutParameters.put("Title", keyTitle);
                    {
                        double cutStartTimeInSeconds = ((double) cutStartTimeInMilliSeconds) / 1000;
                        joCutParameters.put("StartTimeInSeconds", cutStartTimeInSeconds);

                        Calendar calendar = Calendar.getInstance();
                        calendar.setTime(new Date(cutStartTimeInMilliSeconds));
                        calendar.add(Calendar.MILLISECOND, (int) (cutMediaDurationInMilliSeconds.longValue()));

                        double cutEndTimeInSeconds = ((double) calendar.getTime().getTime()) / 1000;
                        joCutParameters.put("EndTimeInSeconds", cutEndTimeInSeconds);
                    }

                    {
                        JSONObject joCutUserData = new JSONObject();
                        joCutParameters.put("UserData", joCutUserData);

                        joCutUserData.put("Channel", cutMediaChannel);
                        joCutUserData.put("StartTime", sCutMediaStartTime);
                        joCutUserData.put("EndTime", sCutMediaEndTime);
                    }
                }
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
                joAddContentParameters.put("FileFormat", fileExtension);
                joAddContentParameters.put("Retention", "0");
                joAddContentParameters.put("Title", mediaFile.getName());
                joAddContentParameters.put("FileSizeInBytes", mediaFile.length());
                if (addContentPull)
                    joAddContentParameters.put("SourceURL", "copy://" + mediaFile.getAbsolutePath());

                JSONObject joExtract = new JSONObject();
                {
                    JSONObject joAddContentOnSuccess = new JSONObject();
                    joAddContent.put("OnSuccess", joAddContentOnSuccess);

                    joAddContentOnSuccess.put("Task", joExtract);

                    joExtract.put("Label", "Extract from " + mediaFile.getName());
                    joExtract.put("Type", "Extract-Tracks");

                    JSONObject joExtractParameters = new JSONObject();
                    joExtract.put("Parameters", joExtractParameters);

                    joExtractParameters.put("Ingester", ingester);
                    joExtractParameters.put("Retention", "0");
                    joExtractParameters.put("Title", "Extract from " + mediaFile.getName());
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

                joCut = new JSONObject();
                {
                    JSONObject joExtractOnSuccess = new JSONObject();
                    joExtract.put("OnSuccess", joExtractOnSuccess);

                    joExtractOnSuccess.put("Task", joCut);

                    joCut.put("Label", keyLabel);
                    joCut.put("Type", "Cut");

                    JSONObject joCutParameters = new JSONObject();
                    joCut.put("Parameters", joCutParameters);

                    joCutParameters.put("Ingester", ingester);
                    joCutParameters.put("Retention", cutMediaRetention);
                    joCutParameters.put("Title", keyTitle);
                    {
                        double cutStartTimeInSeconds = ((double) cutStartTimeInMilliSeconds) / 1000;
                        joCutParameters.put("StartTimeInSeconds", cutStartTimeInSeconds);

                        Calendar calendar = Calendar.getInstance();
                        calendar.setTime(new Date(cutStartTimeInMilliSeconds));
                        calendar.add(Calendar.MILLISECOND, (int) (cutMediaDurationInMilliSeconds.longValue()));

                        double cutEndTimeInSeconds = ((double) calendar.getTime().getTime()) / 1000;
                        joCutParameters.put("EndTimeInSeconds", cutEndTimeInSeconds);
                    }

                    {
                        JSONObject joCutUserData = new JSONObject();
                        joCutParameters.put("UserData", joCutUserData);

                        joCutUserData.put("Channel", cutMediaChannel);
                        joCutUserData.put("StartTime", sCutMediaStartTime);
                        joCutUserData.put("EndTime", sCutMediaEndTime);
                    }
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

                    joEncodeParameters.put("EncodingPriority", "Low");
                    joEncodeParameters.put("EncodingProfileLabel", "MMS_aac_92");
                }
            }
            mLogger.info("Ready for the ingest"
                    + ", sCutMediaStartTime: " + sCutMediaStartTime
                    + ", sCutMediaEndTime: " + sCutMediaEndTime
                    // + ", cutStartTimeInSeconds: " + cutStartTimeInSeconds
                    // + ", cutEndTimeInSeconds: " + cutEndTimeInSeconds
                    + ", fileTreeMap.size: " + fileTreeMap.size()
                    + ", json Workflow: " + joWorkflow.toString(4));

            return joWorkflow;
        }
        catch (Exception e)
        {
            String errorMessage = "buildTVJson failed. Exception: " + e;
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
