package com.catramms.backing.common;

import com.catramms.backing.entity.WorkspaceDetails;
import com.catramms.utility.catramms.CatraMMS;
import org.apache.log4j.Logger;

import javax.faces.application.FacesMessage;
import javax.faces.context.FacesContext;
import javax.servlet.http.Cookie;
import javax.servlet.http.HttpServletRequest;
import javax.servlet.http.HttpServletResponse;
import javax.servlet.http.HttpSession;
import java.io.File;
import java.io.FileInputStream;
import java.io.InputStream;
import java.io.Serializable;
import java.util.ArrayList;
import java.util.Date;
import java.util.List;
import java.util.Properties;

/**
 * Created by multi on 18.03.17.
 */
public class LoginToBeRemoved implements Serializable {

    private static final Logger mLogger = Logger.getLogger(LoginToBeRemoved.class);

    static public final String configFileName = "catramms.properties";

    // cookie or session
    private String loginType;

    private  String anonymousUserName;
    // cookie data
    private String cookieName;
    private int cookieMaxAgeInSeconds;
    private static final String cookieValuesSeparator = "__SEP__";

    private String username;
    private String password;
    private String clientIPAddress;
    private Long expireTimestamp;
    private String friendlyName;
    private String groupsBelongTo;
    private Boolean alreadyLogged;

    public LoginToBeRemoved()
    {
        try
        {
            loadConfigurationParameters();
            mLogger.info("loadConfigurationParameters. loginType: " + loginType + ", cookieMaxAgeInSeconds: " + cookieMaxAgeInSeconds);
        }
        catch (Exception e)
        {
            String errorMessage = "Problems to load the configuration file. Exception: " + e + ", ConfigurationFileName: " + configFileName;
            mLogger.error(errorMessage);
//            MpCommonFacade.saveErrorLog(LogLevel.Error, Component.LiveLink, errorMessage,
//                    Thread.currentThread().getStackTrace()[1].getFileName(),
//                    Thread.currentThread().getStackTrace()[1].getLineNumber());

            return;
        }

        FacesContext facesContext = FacesContext.getCurrentInstance();
        // if not exist the session is created
        HttpSession session = (HttpSession) facesContext.getExternalContext().getSession(true);

        if (loginType.equalsIgnoreCase("cookie"))
        {
            HttpServletRequest httpServletRequest = (HttpServletRequest) facesContext.getExternalContext().getRequest();

            /*
            Enumeration headerNames = httpServletRequest.getHeaderNames();
            while (headerNames.hasMoreElements()) {
                String key = (String) headerNames.nextElement();
                String value = httpServletRequest.getHeader(key);
                mLogger.info("Header Name: " + key + ", value: " + value);
            }
            */

            Cookie cookie = getCookie(httpServletRequest);

            boolean cookieFieldsFound = false;

            if (cookie != null)
            {
                String cookieValue = new EncryptionToBeRemoved().decrypt(cookie.getValue());
                mLogger.info("Cookie encrypted: " + cookie.getValue() + ", cookie value: " + cookieValue);

                if (cookieValue != null)
                {
                    String[] cookieValues = cookieValue.split(cookieValuesSeparator);

                    if (cookieValues.length == 6)
                    {
                        if (cookieValues[0].equalsIgnoreCase("true"))
                            alreadyLogged = true;
                        else
                            alreadyLogged = false;
                        mLogger.info("alreadyLogged setting"
                                        + ", alreadyLogged: " + alreadyLogged
                        );
                        username = cookieValues[1];
                        friendlyName = cookieValues[2];
                        clientIPAddress = cookieValues[3];
                        if (clientIPAddress.equalsIgnoreCase("<n.a.>"))
                            clientIPAddress = "";
                        expireTimestamp = new Long(cookieValues[4]);
                        groupsBelongTo = cookieValues[5];
                        if (groupsBelongTo.equalsIgnoreCase("<n.a.>"))
                            groupsBelongTo = "";

                        cookieFieldsFound = true;

                        mLogger.info("Cookie found"
                                        + ", alreadyLogged: " + alreadyLogged
                                        + ", username: " + username
                                        + ", friendlyName: " + friendlyName
                                        + ", clientIPAddress: " + clientIPAddress
                                        + ", expireTimestamp: " + expireTimestamp
                                        + ", groupsBelongTo: " + groupsBelongTo
                        );
                    }
                    else
                    {
                        mLogger.info("Cookie found but it is not well formed. cookieValues.length: " + cookieValues.length);
                    }
                }
                else
                {
                    mLogger.info("Cookie found but it is not well formed");
                }
            }
            else
            {
                mLogger.info("Cookie was not found");
            }

            if (!cookieFieldsFound)
            {
                HttpServletResponse httpServletResponse = (HttpServletResponse) facesContext.getExternalContext().getResponse();

                alreadyLogged = false;
                mLogger.info("alreadyLogged setting"
                                + ", alreadyLogged: " + alreadyLogged
                );
                username = anonymousUserName;
                friendlyName = anonymousUserName;
                clientIPAddress = getClientIPAddress(httpServletRequest);
                expireTimestamp = new Date().getTime() + (cookieMaxAgeInSeconds * 1000);
                groupsBelongTo = "<n.a.>";

                try {
                    setCookie(httpServletRequest, httpServletResponse, cookieMaxAgeInSeconds);

                    mLogger.info("Cookie was set"
                                    + ", alreadyLogged: " + alreadyLogged
                                    + ", username: " + username
                                    + ", friendlyName: " + friendlyName
                                    + ", clientIPAddress: " + clientIPAddress
                                    + ", expireTimestamp: " + expireTimestamp
                                    + ", groupsBelongTo: " + groupsBelongTo
                    );
                }
                catch (Exception ex)
                {
                    mLogger.error("setCookie failed");
                }
            }
        }
        else
        {

            HttpServletRequest httpServletRequest = (HttpServletRequest) facesContext.getExternalContext().getRequest();

            alreadyLogged = (Boolean) session.getAttribute("alreadyLogged");

            boolean sessionFieldsFound = false;

            if (alreadyLogged != null)
            {
                friendlyName = (String) session.getAttribute("friendlyName");
                username = (String) session.getAttribute("username");
                clientIPAddress = (String) session.getAttribute("clientIPAddress");
                expireTimestamp = (Long) session.getAttribute("expireTimestamp");
                groupsBelongTo = (String) session.getAttribute("groupsBelongTo");

                if (friendlyName != null && !friendlyName.equalsIgnoreCase("") &&
                        username != null && !username.equalsIgnoreCase("") &&
                    groupsBelongTo != null
                )
                {
                    sessionFieldsFound = true;
                }
            }

            if (!sessionFieldsFound)
            {
                alreadyLogged = false;
                mLogger.info("alreadyLogged setting"
                                + ", alreadyLogged: " + alreadyLogged
                );
                username = anonymousUserName;
                friendlyName = anonymousUserName;
                clientIPAddress = getClientIPAddress(httpServletRequest);
                expireTimestamp = new Date().getTime() + (cookieMaxAgeInSeconds * 1000);
                groupsBelongTo = "";

                session.setAttribute("alreadyLogged", alreadyLogged);
                session.setAttribute("friendlyName", friendlyName);
                session.setAttribute("username", username);
                session.setAttribute("clientIPAddress", clientIPAddress);
                session.setAttribute("expireTimestamp", expireTimestamp);
                session.setAttribute("groupsBelongTo", groupsBelongTo);
            }
        }
    }

    public boolean login()
    {
        FacesContext facesContext = FacesContext.getCurrentInstance();

        HttpServletRequest httpServletRequest = (HttpServletRequest) facesContext.getExternalContext().getRequest();
        HttpServletResponse httpServletResponse = (HttpServletResponse) facesContext.getExternalContext().getResponse();

        if (alreadyLogged)
        {
            mLogger.info("User is already logged");

            return true;
        }

        if (username == null || username.equalsIgnoreCase("") ||
                password == null || password.equalsIgnoreCase(""))
        {
            String errorMessage = "Login failed. username: " + username;
            mLogger.error(errorMessage);

            FacesMessage message = new FacesMessage(FacesMessage.SEVERITY_ERROR,
                    "Login", errorMessage);
            FacesContext context = FacesContext.getCurrentInstance();
            context.addMessage(null, message);

            return false;
        }

        if (httpServletRequest == null || httpServletResponse == null)
        {
            String errorMessage = "Login failed"
                    + ", httpServletRequest: " + httpServletRequest
                    + ", httpServletResponse: " + httpServletResponse
                    ;
            mLogger.error(errorMessage);

            FacesMessage message = new FacesMessage(FacesMessage.SEVERITY_ERROR,
                    "Login", errorMessage);
            FacesContext context = FacesContext.getCurrentInstance();
            context.addMessage(null, message);

            return false;
        }

        // if not exist the session is created
        HttpSession session = (HttpSession) facesContext.getExternalContext().getSession(true);

        try
        {
            List<WorkspaceDetails> workspaceDetailsList = new ArrayList<>();

            CatraMMS catraMMS = new CatraMMS();
            Long userKey = catraMMS.login(username, password, workspaceDetailsList);

            mLogger.info("workspaceDetailsList added in session"
                            + ", userKey: " + userKey
                            + ", workspaceDetailsList: " + workspaceDetailsList
                    + ", workspaceDetailsList.size: " + (workspaceDetailsList != null ? workspaceDetailsList.size() : "null")
            );
            session.setAttribute("userKey", userKey);
            session.setAttribute("workspaceDetailsList", workspaceDetailsList);
            if (workspaceDetailsList != null && workspaceDetailsList.size() > 0)
                session.setAttribute("workspaceName", workspaceDetailsList.get(0).getName());
        }
        catch (Exception e)
        {
            String errorMessage = "Login MMS failed. Exception: " + e;
            mLogger.error(errorMessage);

            mLogger.info("workspaceDetailsList removed from session");
            session.removeAttribute("workspaceDetailsList");
            mLogger.info("userKey removed from session");
            session.removeAttribute("userKey");
            mLogger.info("workspaceName removed from session");
            session.removeAttribute("workspaceName");

            FacesMessage message = new FacesMessage(FacesMessage.SEVERITY_ERROR,
                    "Login", errorMessage);
            FacesContext context = FacesContext.getCurrentInstance();
            context.addMessage(null, message);

            return false;
        }

        try
        {
            alreadyLogged = true;
            mLogger.info("alreadyLogged setting"
                + ", alreadyLogged: " + alreadyLogged
            );

            friendlyName = username;

            expireTimestamp = new Date().getTime() + (cookieMaxAgeInSeconds * 1000);

            groupsBelongTo = "";
            if (groupsBelongTo.equalsIgnoreCase(""))
                groupsBelongTo = "<n.a.>";

            if (loginType.equalsIgnoreCase("cookie"))
            {
                setCookie(httpServletRequest, httpServletResponse, cookieMaxAgeInSeconds);
            }
            else
            {
                session.setAttribute("alreadyLogged", alreadyLogged);
                session.setAttribute("friendlyName", friendlyName);
                session.setAttribute("username", username);
                session.setAttribute("clientIPAddress", clientIPAddress);
                session.setAttribute("expireTimestamp", expireTimestamp);
                session.setAttribute("groupsBelongTo", groupsBelongTo);
            }
        }
        catch (Exception e)
        {
            logout();

            String errorMessage = "setCookie failed. Exception: " + e;
            mLogger.error(errorMessage);

            FacesMessage message = new FacesMessage(FacesMessage.SEVERITY_ERROR,
                    "Login", errorMessage);
            FacesContext context = FacesContext.getCurrentInstance();
            context.addMessage(null, message);

            return false;
        }

        FacesMessage message = new FacesMessage(FacesMessage.SEVERITY_INFO,
                "Login", "Login success");
        FacesContext context = FacesContext.getCurrentInstance();
        context.addMessage(null, message);


        return true;
    }

    public boolean logout()
    {
        FacesContext facesContext = FacesContext.getCurrentInstance();

        HttpServletRequest httpServletRequest = (HttpServletRequest) facesContext.getExternalContext().getRequest();
        HttpServletResponse httpServletResponse = (HttpServletResponse) facesContext.getExternalContext().getResponse();

        if (!alreadyLogged)
        {
            String errorMessage = "User is already not logged";
            mLogger.info(errorMessage);

            FacesMessage message = new FacesMessage(FacesMessage.SEVERITY_ERROR,
                    "Login", errorMessage);
            FacesContext context = FacesContext.getCurrentInstance();
            context.addMessage(null, message);

            return false;
        }

        if (httpServletRequest == null || httpServletResponse == null)
        {
            String errorMessage = "Logout failed"
                    + ", httpServletRequest: " + httpServletRequest
                    + ", httpServletResponse: " + httpServletResponse
                    ;
            mLogger.error(errorMessage);

            FacesMessage message = new FacesMessage(FacesMessage.SEVERITY_ERROR,
                    "Login", errorMessage);
            FacesContext context = FacesContext.getCurrentInstance();
            context.addMessage(null, message);

            return false;
        }

        // if not exist the session is created
        HttpSession session = (HttpSession) facesContext.getExternalContext().getSession(true);

        alreadyLogged = false;
        mLogger.info("alreadyLogged setting"
                        + ", alreadyLogged: " + alreadyLogged
        );
        username = anonymousUserName;
        friendlyName = anonymousUserName;
        clientIPAddress = "<n.a.>";
        expireTimestamp = new Date().getTime(); // expired
        groupsBelongTo = "<n.a.>";

        mLogger.info("workspaceDetailsList removed from session");
        session.removeAttribute("workspaceDetailsList");
        mLogger.info("userKey removed from session");
        session.removeAttribute("userKey");
        mLogger.info("workspaceName removed from session");
        session.removeAttribute("workspaceName");

        if (loginType.equalsIgnoreCase("cookie"))
        {
            try
            {
                setCookie(httpServletRequest, httpServletResponse, -1);
            }
            catch (Exception e)
            {
                String errorMessage = "setCookie failed. Exception: " + e;
                mLogger.error(errorMessage);

                FacesMessage message = new FacesMessage(FacesMessage.SEVERITY_ERROR,
                        "Login", errorMessage);
                FacesContext context = FacesContext.getCurrentInstance();
                context.addMessage(null, message);

                return false;
            }
        }
        else
        {
            session.setAttribute("alreadyLogged", alreadyLogged);
            session.setAttribute("friendlyName", friendlyName);
            session.setAttribute("username", username);
            session.setAttribute("clientIPAddress", clientIPAddress);
            session.setAttribute("expireTimestamp", expireTimestamp);
            session.setAttribute("groupsBelongTo", groupsBelongTo);
        }

        FacesMessage message = new FacesMessage(FacesMessage.SEVERITY_INFO,
                "Logout", "Logout success");
        FacesContext context = FacesContext.getCurrentInstance();
        context.addMessage(null, message);


        return true;
    }

    public boolean belongToGroup (String groupName)
    {
        if (!alreadyLogged)
        {
            /*
            String errorMessage = "User is already not logged";
            mLogger.info(errorMessage);

            FacesMessage message = new FacesMessage(FacesMessage.SEVERITY_ERROR,
                    "Login", errorMessage);
            FacesContext context = FacesContext.getCurrentInstance();
            context.addMessage(null, message);
            */

            return false;
        }

        return (groupsBelongTo.indexOf(groupName) != -1);
    }

    private void setCookie(HttpServletRequest httpServletRequest, HttpServletResponse httpServletResponse,
                           int maxAge)
            throws Exception
    {
        Cookie cookie = null;
        String cookieValue;
        String path;

        if (httpServletRequest == null || httpServletResponse == null)
        {
            String errorMessage = "httpServletRequest/httpServletResponse is null"
                    + ", httpServletRequest: " + httpServletRequest
                    + ", httpServletResponse: " + httpServletResponse;
            mLogger.error(errorMessage);

            throw new Exception(errorMessage);
        }

        cookieValue = (alreadyLogged ? "true" : "false") + cookieValuesSeparator
                + username + cookieValuesSeparator
                + friendlyName + cookieValuesSeparator
                + clientIPAddress + cookieValuesSeparator
                + expireTimestamp + cookieValuesSeparator
                + groupsBelongTo;

        path = httpServletRequest.getContextPath(); // thi is always the base URL/path

        Cookie[] userCookies = httpServletRequest.getCookies();
        if (userCookies != null && userCookies.length > 0)
        {
            for (int userCookieIndex = 0; userCookieIndex < userCookies.length; userCookieIndex++)
            {
                if (userCookies[userCookieIndex].getName() != null &&
                        userCookies[userCookieIndex].getName().equals(cookieName))
                {
                    cookie = userCookies[userCookieIndex];

                    break;
                }
            }
        }

        String cookieValueEncrypted = new EncryptionToBeRemoved().encrypt(cookieValue);
        mLogger.info("Cookie value: " + cookieValue + ", cookie encrypted: " + cookieValueEncrypted);

        if (cookie != null)
        {
            cookie.setValue(cookieValueEncrypted);
            cookie.setPath(path);
        }
        else
        {
            cookie = new Cookie(cookieName, cookieValueEncrypted);
            cookie.setPath(path);
        }

        cookie.setMaxAge(maxAge);

        mLogger.info("setCookie"
                        + ", cookieName: " + cookieName
                        + ", cookieValue: " + cookieValue
                        + ", path: " + path
                        + ", maxAge (secs): " + maxAge
        );

        httpServletResponse.addCookie(cookie);
    }

    private Cookie getCookie(HttpServletRequest httpServletRequest)
    {
        Cookie cookie = null;


        if (httpServletRequest == null)
        {
            mLogger.error("httpServletRequest is null!!!!!!!");

            return cookie;
        }

        Cookie[] userCookies = httpServletRequest.getCookies();
        if (userCookies != null && userCookies.length > 0)
        {
            for (int userCookieIndex = 0; userCookieIndex < userCookies.length; userCookieIndex++)
            {
                if (userCookies[userCookieIndex].getName() != null &&
                        userCookies[userCookieIndex].getName().equals(cookieName))
                {
                    cookie = userCookies[userCookieIndex];

                    break;
                }
            }
        }

        if (cookie != null)
        {
            // check the expiration and client IP address
            {
                String cookieValue = new EncryptionToBeRemoved().decrypt(cookie.getValue());
                mLogger.info("Cookie encrypted: " + cookie.getValue() + ", cookie value: " + cookieValue);

                if (cookieValue != null)
                {
                    String[] cookieValues = cookieValue.split(cookieValuesSeparator);

                    if (cookieValues.length == 6)
                    {
                        // check expiration
                        {
                            expireTimestamp = new Long(cookieValues[4]);

                            if (expireTimestamp > new Date().getTime())
                            {
                                mLogger.info("getCookie"
                                                + ", cookieName: " + cookie.getName()
                                                + ", cookieValue: " + cookie.getValue()
                                                + ", path: " + cookie.getPath()
                                                + ", maximumCookieAgeInSeconds: " + cookie.getMaxAge()
                                );
                            }
                            else
                            {
                                mLogger.info("getCookie. Cookie expired (becuse of a previous logout)");

                                cookie = null;
                            }
                        }

                        // check client ip is the same
                        if (cookie != null)
                        {
                            String cookieClientIPAddress = cookieValues[3];
                            if (cookieClientIPAddress.equalsIgnoreCase("<n.a.>"))
                                cookieClientIPAddress = "";


                            String localClientIPAddress = getClientIPAddress(httpServletRequest);

                            if (localClientIPAddress.equalsIgnoreCase(cookieClientIPAddress))
                            {
                                mLogger.info("Client IP address check successful. clientIPAddress: " + cookieClientIPAddress);
                            }
                            else
                            {
                                mLogger.error("Client IP address failed"
                                                + ", cookieClientIPAddress: " + cookieClientIPAddress
                                                + ", localClientIPAddress: " + localClientIPAddress
                                );

                                cookie = null;
                            }
                        }
                    }
                }
            }

            /*
            if (cookie.getMaxAge() != -1)
            {
                mLogger.info("getCookie"
                                + ", cookieName: " + cookie.getName()
                                + ", cookieValue: " + cookie.getValue()
                                + ", path: " + cookie.getPath()
                                + ", maximumCookieAgeInSeconds: " + cookie.getMaxAge()
                );
            }
            else
            {
                mLogger.info("getCookie. Cookie expired (becuse of a previous logout)");

                cookie = null;
            }
            */
        }
        else
            mLogger.info("getCookie. It is null");

        return cookie;
    }

    private String getClientIPAddress(HttpServletRequest httpServletRequest)
    {
        String localClientIPAddress = httpServletRequest.getHeader("X-FORWARDED-FOR"); // client, proxy1, proxy2, ...
        if (localClientIPAddress == null)
        {
            localClientIPAddress = httpServletRequest.getRemoteAddr();
        }

        return localClientIPAddress;
    }

    private void loadConfigurationParameters()
    {
        try
        {
            Properties properties = getConfigurationParameters();

            {
                {
                    anonymousUserName = properties.getProperty("login.anonymous.username");
                    if (anonymousUserName == null)
                    {
                        String errorMessage = "No login.anonymous.username found. ConfigurationFileName: " + configFileName;
                        mLogger.error(errorMessage);
//                        MpCommonFacade.saveErrorLog(LogLevel.Error, Component.OpenMedia, errorMessage,
//                                Thread.currentThread().getStackTrace()[1].getFileName(),
//                                Thread.currentThread().getStackTrace()[1].getLineNumber());

                        return;
                    }

                    loginType = properties.getProperty("login.type");
                    if (loginType == null)
                    {
                        String errorMessage = "No login.type.name found. ConfigurationFileName: " + configFileName;
                        mLogger.error(errorMessage);
//                        MpCommonFacade.saveErrorLog(LogLevel.Error, Component.OpenMedia, errorMessage,
//                                Thread.currentThread().getStackTrace()[1].getFileName(),
//                                Thread.currentThread().getStackTrace()[1].getLineNumber());

                        return;
                    }

                    cookieName = properties.getProperty("login.cookie.name");
                    if (cookieName == null)
                    {
                        String errorMessage = "No login.cookie.name found. ConfigurationFileName: " + configFileName;
                        mLogger.error(errorMessage);
//                        MpCommonFacade.saveErrorLog(LogLevel.Error, Component.OpenMedia, errorMessage,
//                                Thread.currentThread().getStackTrace()[1].getFileName(),
//                                Thread.currentThread().getStackTrace()[1].getLineNumber());

                        return;
                    }

                    String tmpCookieMaxAgeInSeconds = properties.getProperty("login.cookie.maxAgeInSeconds");
                    if (tmpCookieMaxAgeInSeconds == null)
                    {
                        String errorMessage = "No login.cookie.maxAgeInSeconds found. ConfigurationFileName: " + configFileName;
                        mLogger.error(errorMessage);
//                        MpCommonFacade.saveErrorLog(LogLevel.Error, Component.LiveLink, errorMessage,
//                                Thread.currentThread().getStackTrace()[1].getFileName(),
//                                Thread.currentThread().getStackTrace()[1].getLineNumber());

                        return;
                    }
                    cookieMaxAgeInSeconds = Long.valueOf(tmpCookieMaxAgeInSeconds).intValue();
                }
            }
        }
        catch (Exception e)
        {
            String errorMessage = "Problems to load the configuration file. Exception: " + e + ", ConfigurationFileName: " + configFileName;
            mLogger.error(errorMessage);
//            MpCommonFacade.saveErrorLog(LogLevel.Error, Component.LiveLink, errorMessage,
//                    Thread.currentThread().getStackTrace()[1].getFileName(),
//                    Thread.currentThread().getStackTrace()[1].getLineNumber());

            return;
        }
    }

    public Properties getConfigurationParameters()
    {
        Properties properties = new Properties();

        try
        {
            {
                InputStream inputStream;
                String commonPath = "/mnt/common/mp";
                String tomcatPath = System.getProperty("catalina.base");

                File configFile = new File(commonPath + "/conf/" + configFileName);
                if (configFile.exists())
                {
                    mLogger.info("Configuration file: " + configFile.getAbsolutePath());
                    inputStream = new FileInputStream(configFile);
                }
                else
                {
                    configFile = new File(tomcatPath + "/conf/" + configFileName);
                    if (configFile.exists())
                    {
                        mLogger.info("Configuration file: " + configFile.getAbsolutePath());
                        inputStream = new FileInputStream(configFile);
                    }
                    else
                    {
                        mLogger.info("Using the internal configuration file");
                        inputStream = LoginToBeRemoved.class.getClassLoader().getResourceAsStream(configFileName);
                    }
                }

                if (inputStream == null)
                {
                    String errorMessage = "Login configuration file not found. ConfigurationFileName: " + configFileName;
                    mLogger.error(errorMessage);

                    return properties;
                }
                properties.load(inputStream);
            }
        }
        catch (Exception e)
        {
            String errorMessage = "Problems to load the configuration file. Exception: " + e + ", ConfigurationFileName: " + configFileName;
            mLogger.error(errorMessage);

            return properties;
        }

        return properties;
    }

    public String getUsername() {
        return username;
    }

    public void setUsername(String username) {
        this.username = username;
    }

    public String getFriendlyName() {
        return friendlyName;
    }

    public void setFriendlyName(String friendlyName) {
        this.friendlyName = friendlyName;
    }

    public String getPassword() {
        return password;
    }

    public void setPassword(String password) {
        this.password = password;
    }

    public Boolean getAlreadyLogged() {
        return alreadyLogged;
    }

    public void setAlreadyLogged(Boolean alreadyLogged) {
        this.alreadyLogged = alreadyLogged;
    }
}
