package com.catramms.backing.newWorkflow;

import java.io.Serializable;

/**
 * Created by multi on 13.06.18.
 */
public class Event implements Serializable {
    private String type;

    private boolean childTaskCreated;
    private boolean childEventOnSuccessCreated;
    private boolean childEventOnErrorCreated;
    private boolean childEventOnCompleteCreated;

    @Override
    public String toString() {
        return type + " <Event>";
    }

    public String getType() {
        return type;
    }

    public void setType(String type) {
        this.type = type;
    }

    public boolean isChildTaskCreated() {
        return childTaskCreated;
    }

    public void setChildTaskCreated(boolean childTaskCreated) {
        this.childTaskCreated = childTaskCreated;
    }

    public boolean isChildEventOnSuccessCreated() {
        return childEventOnSuccessCreated;
    }

    public void setChildEventOnSuccessCreated(boolean childEventOnSuccessCreated) {
        this.childEventOnSuccessCreated = childEventOnSuccessCreated;
    }

    public boolean isChildEventOnErrorCreated() {
        return childEventOnErrorCreated;
    }

    public void setChildEventOnErrorCreated(boolean childEventOnErrorCreated) {
        this.childEventOnErrorCreated = childEventOnErrorCreated;
    }

    public boolean isChildEventOnCompleteCreated() {
        return childEventOnCompleteCreated;
    }

    public void setChildEventOnCompleteCreated(boolean childEventOnCompleteCreated) {
        this.childEventOnCompleteCreated = childEventOnCompleteCreated;
    }
}
