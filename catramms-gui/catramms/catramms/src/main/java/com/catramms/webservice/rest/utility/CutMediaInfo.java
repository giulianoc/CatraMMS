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
    private long cutStartTimeInMilliSeconds;

    public CutMediaInfo()
    {
        fileTreeMap = new TreeMap<>();
        firstChunkFound = false;
        lastChunkFound = false;
        cutStartTimeInMilliSeconds = -1;
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

    public long getCutStartTimeInMilliSeconds() {
        return cutStartTimeInMilliSeconds;
    }

    public void setCutStartTimeInMilliSeconds(long cutStartTimeInMilliSeconds) {
        this.cutStartTimeInMilliSeconds = cutStartTimeInMilliSeconds;
    }
}
