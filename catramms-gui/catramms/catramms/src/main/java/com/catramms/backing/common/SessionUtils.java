package com.catramms.backing.common;

import com.catramms.backing.entity.WorkspaceDetails;
import org.apache.log4j.Logger;

import javax.faces.context.FacesContext;
import javax.servlet.http.HttpServletRequest;
import javax.servlet.http.HttpSession;
import java.util.List;

public class SessionUtils {

    private static final Logger mLogger = Logger.getLogger(SessionUtils.class);

    public static HttpSession getSession() {
        return (HttpSession) FacesContext.getCurrentInstance()
                .getExternalContext().getSession(false);
    }

    public static HttpServletRequest getRequest() {
        return (HttpServletRequest) FacesContext.getCurrentInstance()
                .getExternalContext().getRequest();
    }

    public static String getUserName() {
        HttpSession session = (HttpSession) FacesContext.getCurrentInstance()
                .getExternalContext().getSession(false);
        return session.getAttribute("username").toString();
    }

    public static String getUserId() {
        HttpSession session = getSession();
        if (session != null)
            return (String) session.getAttribute("userid");
        else
            return null;
    }

    public static Long getUserKey()
    {
        HttpSession session = getSession();
        if (session != null)
        {
            return (Long) session.getAttribute("userKey");
        }
        else
            return null;
    }

    public static String getAPIKey()
    {
        HttpSession session = getSession();
        if (session != null)
        {
            String apiKey = null;

            WorkspaceDetails currentWorkspaceDetails = (WorkspaceDetails) session.getAttribute("currentWorkspaceDetails");
            List<WorkspaceDetails> workspaceDetailsList =
                    (List<WorkspaceDetails>) session.getAttribute("workspaceDetailsList");

            if (workspaceDetailsList != null)
            {
                for (WorkspaceDetails workspaceDetails: workspaceDetailsList)
                {
                    if (workspaceDetails.getName().equalsIgnoreCase(currentWorkspaceDetails.getName()))
                    {
                        apiKey = workspaceDetails.getApiKey();

                        break;
                    }
                }
            }

            if (apiKey == null || apiKey.equalsIgnoreCase(""))
            {
                mLogger.warn("apiKey is not valid"
                                + ", apiKey: " + apiKey
                );
            }

            return apiKey;
        }
        else
            return null;
    }

    public static List<WorkspaceDetails> getWorkspaceDetailsList()
    {
        HttpSession session = getSession();
        if (session != null)
        {
            return (List<WorkspaceDetails>) session.getAttribute("workspaceDetailsList");
        }
        else
            return null;
    }

    public static WorkspaceDetails getCurrentWorkspaceDetails()
    {
        HttpSession session = getSession();
        if (session != null)
        {
            return (WorkspaceDetails) session.getAttribute("currentWorkspaceDetails");
        }
        else
            return null;
    }
}