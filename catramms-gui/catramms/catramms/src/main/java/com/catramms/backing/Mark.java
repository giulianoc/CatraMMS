package com.catramms.backing;

import java.io.Serializable;
import java.util.Date;

/**
 * Created by multi on 13.06.18.
 */
public class Mark implements Serializable {

    private String in;
    private String out;

    public String getIn() {
        return in;
    }

    public void setIn(String in) {
        this.in = in;
    }

    public String getOut() {
        return out;
    }

    public void setOut(String out) {
        this.out = out;
    }
}
