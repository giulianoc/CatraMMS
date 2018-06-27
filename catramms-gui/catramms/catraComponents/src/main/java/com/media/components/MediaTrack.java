package com.media.components;

/**
 * Created with IntelliJ IDEA.
 * User: multi
 * Date: 07/11/15
 * Time: 15:15
 * To change this template use File | Settings | File Templates.
 */
public class MediaTrack {
    private String source;
    private MediaTrackKind kind;
    private boolean defaultTrack;
    private String label;
    private String locale;

    public MediaTrack()
    {
        this("", null, "", null, false);
    }

    public MediaTrack(String source, MediaTrackKind kind)
    {
        this(source, kind, "", null, false);
    }

    public MediaTrack(String source, MediaTrackKind kind, String label)
    {
        this(source, kind, label, null, false);
    }

    public MediaTrack(String source, MediaTrackKind kind, String label, String locale)
    {
        this(source, kind, label, locale, false);
    }

    public MediaTrack(String source, MediaTrackKind kind, String label, String locale, boolean defaultTrack)
    {
        this.source = source;
        this.kind = kind;
        this.defaultTrack = defaultTrack;
        this.label = label;
        this.locale = locale;
    }

    public String getSource() {
        return source;
    }

    public void setSource(String source) {
        this.source = source;
    }

    public MediaTrackKind getKind() {
        return kind;
    }

    public void setKind(MediaTrackKind kind) {
        this.kind = kind;
    }

    public boolean isDefaultTrack() {
        return defaultTrack;
    }

    public void setDefaultTrack(boolean defaultTrack) {
        this.defaultTrack = defaultTrack;
    }

    public String getLabel() {
        return label;
    }

    public void setLabel(String label) {
        this.label = label;
    }

    public String getLocale() {
        return locale;
    }

    public void setLocale(String locale) {
        this.locale = locale;
    }
}
