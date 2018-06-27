package com.catramms.backing.newWorkflow;

import java.io.Serializable;

/**
 * Created by multi on 13.06.18.
 */
public class Workflow implements Serializable {
    private String label;

    private boolean childTaskCreated;
    private boolean childEventOnSuccessCreated;
    private boolean childEventOnErrorCreated;
    private boolean childEventOnCompleteCreated;

    @Override
    public String toString() {
        return label + " <Workflow>";
    }

    public String getLabel() {
        return label;
    }

    public void setLabel(String label) {
        this.label = label;
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
