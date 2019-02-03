package com.catramms.webservice.rest.utility;

import com.catramms.backing.entity.MediaItem;
import org.json.JSONObject;

import java.io.File;
import java.util.ArrayList;
import java.util.Date;
import java.util.List;
import java.util.TreeMap;

public class CutMediaInfo_rsi_v1 {
    private List<MediaItem> mediaItems;
    private JSONObject joMediaCut;

    public CutMediaInfo_rsi_v1()
    {
        mediaItems = new ArrayList<>();

        joMediaCut = null;
    }

    public List<MediaItem> getMediaItems() {
        return mediaItems;
    }

    public void setMediaItems(List<MediaItem> mediaItems) {
        this.mediaItems = mediaItems;
    }

    public JSONObject getJoMediaCut() {
        return joMediaCut;
    }

    public void setJoMediaCut(JSONObject joMediaCut) {
        this.joMediaCut = joMediaCut;
    }

}
