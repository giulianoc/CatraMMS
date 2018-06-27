package com.media.components;

/**
 * Created with IntelliJ IDEA.
 * User: multi
 * Date: 07/11/15
 * Time: 15:13
 * To change this template use File | Settings | File Templates.
 */
public class MediaSource {

    private String source;
    private String type;

    private MediaSource()
    {
        this("", "");
    }

    public MediaSource(String source, String type)
    {
        this.source = source;
        this.type = type;
    }

    public String getSource() {
        return source;
    }

    public void setSource(String source) {
        this.source = source;
    }

    public String getType() {
        return type;
    }

    public void setType(String type) {
        this.type = type;
    }
}
