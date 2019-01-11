package com.catramms.backing.newWorkspace2;

import java.io.Serializable;

/**
 * Created by multi on 11.01.19.
 */
public class WorkspaceElement implements Serializable {
    private String name;
    private String image;

    public WorkspaceElement() {
    }

    public WorkspaceElement(String name, String image) {
        this.name = name;
        this.image = image;
    }

    public String getName() {
        return name;
    }

    public void setName(String name) {
        this.name = name;
    }

    public String getImage() {
        return image;
    }

    public void setImage(String image) {
        this.image = image;
    }

    @Override
    public String toString() {
        return name;
    }
}
