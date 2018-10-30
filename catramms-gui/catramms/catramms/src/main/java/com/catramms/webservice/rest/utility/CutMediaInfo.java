package com.catramms.webservice.rest.utility;

import org.json.JSONObject;

import java.io.File;
import java.util.Date;
import java.util.TreeMap;

public class CutMediaInfo {
    private TreeMap<Date, File> fileTreeMap;
    private JSONObject joMediaCut;
    private boolean firstChunkFound;
    private boolean lastChunkFound;
    private long chunksDurationInMilliSeconds;

    public CutMediaInfo()
    {
        fileTreeMap = new TreeMap<>();

        firstChunkFound = false;
        lastChunkFound = false;
        chunksDurationInMilliSeconds = 0;
    }

    public TreeMap<Date, File> getFileTreeMap() {
        return fileTreeMap;
    }

    public void setFileTreeMap(TreeMap<Date, File> fileTreeMap) {
        this.fileTreeMap = fileTreeMap;
    }

    public JSONObject getJoMediaCut() {
        return joMediaCut;
    }

    public void setJoMediaCut(JSONObject joMediaCut) {
        this.joMediaCut = joMediaCut;
    }

    public boolean isFirstChunkFound() {
        return firstChunkFound;
    }

    public void setFirstChunkFound(boolean firstChunkFound) {
        this.firstChunkFound = firstChunkFound;
    }

    public boolean isLastChunkFound() {
        return lastChunkFound;
    }

    public void setLastChunkFound(boolean lastChunkFound) {
        this.lastChunkFound = lastChunkFound;
    }

    public long getChunksDurationInMilliSeconds() {
        return chunksDurationInMilliSeconds;
    }

    public void setChunksDurationInMilliSeconds(long chunksDurationInMilliSeconds) {
        this.chunksDurationInMilliSeconds = chunksDurationInMilliSeconds;
    }
}
