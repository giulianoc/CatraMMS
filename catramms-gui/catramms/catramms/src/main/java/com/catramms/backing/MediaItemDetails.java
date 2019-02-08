package com.catramms.backing;

import com.catramms.backing.common.Player;
import com.catramms.backing.common.SessionUtils;
import com.catramms.backing.common.Workspace;
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
import javax.faces.context.ExternalContext;
import javax.faces.context.FacesContext;
import javax.servlet.http.HttpServletResponse;
import java.io.Serializable;
import java.text.DateFormat;
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
public class MediaItemDetails extends Workspace implements Serializable {

    // static because the class is Serializable
    private static final Logger mLogger = Logger.getLogger(MediaItemDetails.class);

    private Player player;

    private MediaItem mediaItem = null;
    private Long mediaItemKey;
    private Long physicalPathKey;

    public void init()
    {
        player = new Player();


        if (mediaItemKey == null && physicalPathKey == null)
        {
            String errorMessage = "mediaItemKey/physicalPathKey is null";
            mLogger.error(errorMessage);

            return;
        }

        try {
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

                CatraMMS catraMMS = new CatraMMS();
                mLogger.info("catraMMS.getMediaItem"
                                + ", mediaItemKey: " + mediaItemKey
                                + ", physicalPathKey: " + physicalPathKey
                );
                mediaItem = catraMMS.getMediaItem(
                        username, password, mediaItemKey, physicalPathKey);
            }
        }
        catch (Exception e)
        {
            String errorMessage = "Exception: " + e;
            mLogger.error(errorMessage);

            mediaItem = null;
        }
    }

    public String getDurationAsString(Long durationInMilliseconds)
    {
        if (durationInMilliseconds == null)
            return "";

        DateFormat dateFormat = new SimpleDateFormat("HH:mm:ss.SSS");
        dateFormat.setTimeZone(TimeZone.getTimeZone("GMT"));

        Date durationDate = new Date(durationInMilliseconds);

        return dateFormat.format(durationDate);
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

    public Long getPhysicalPathKey() {
        return physicalPathKey;
    }

    public void setPhysicalPathKey(Long physicalPathKey) {
        this.physicalPathKey = physicalPathKey;
    }

    public Player getPlayer() {
        return player;
    }

    public void setPlayer(Player player) {
        this.player = player;
    }
}
