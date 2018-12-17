package com.catramms.backing.common;

import com.catramms.backing.entity.WorkspaceDetails;
import org.apache.log4j.Logger;

import javax.faces.bean.ManagedBean;
import javax.faces.bean.ViewScoped;
import javax.servlet.http.HttpSession;
import java.io.Serializable;
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
public class Workspace implements Serializable {

    // static because the class is Serializable
    private static final Logger mLogger = Logger.getLogger(Workspace.class);

    public String getWorkspaceName()
    {
        WorkspaceDetails currentWorkspaceDetails = SessionUtils.getCurrentWorkspaceDetails();

        return currentWorkspaceDetails.getName();
    }

    public void setWorkspaceName(String workspaceName)
    {
        List<WorkspaceDetails> workspaceDetailsList = SessionUtils.getWorkspaceDetailsList();

        for (WorkspaceDetails workspaceDetails: workspaceDetailsList)
        {
            if (workspaceDetails.getName().equalsIgnoreCase(workspaceName))
            {
                SessionUtils.getSession().setAttribute("currentWorkspaceDetails", workspaceDetails);

                break;
            }
        }
    }

    public List<String> getWorkspaceNames()
    {
        List<String> workspaceNames = new ArrayList<>();

        List<WorkspaceDetails> workspaceDetailsList = SessionUtils.getWorkspaceDetailsList();

        if (workspaceDetailsList != null)
        {
            for (WorkspaceDetails workspaceDetails: workspaceDetailsList)
                workspaceNames.add(workspaceDetails.getName());
        }

        return workspaceNames;
    }
}
