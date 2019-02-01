package com.catramms.backing.conf;

import com.catramms.backing.common.SessionUtils;
import com.catramms.backing.common.Workspace;
import com.catramms.backing.entity.FTPConf;
import com.catramms.backing.entity.LiveURLConf;
import com.catramms.backing.entity.WorkspaceDetails;
import com.catramms.utility.catramms.CatraMMS;
import org.apache.log4j.Logger;

import javax.annotation.PostConstruct;
import javax.faces.application.FacesMessage;
import javax.faces.bean.ManagedBean;
import javax.faces.bean.ViewScoped;
import javax.faces.context.FacesContext;
import java.io.Serializable;
import java.text.SimpleDateFormat;
import java.util.ArrayList;
import java.util.List;

/**
 * Created with IntelliJ IDEA.
 * User: multi
 * Date: 27/09/15
 * Time: 20:28
 * To change this template use File | Settings | File Templates.
 */
@ManagedBean
@ViewScoped
public class FtpView extends Workspace implements Serializable {

    // static because the class is Serializable
    private static final Logger mLogger = Logger.getLogger(FtpView.class);

    private Long editFTPConfKey;
    private String editFTPLabel;
    private String editFTPServer;
    private Long editFTPPort;
    private String editFTPUserName;
    private String editFTPPassword;
    private String editFTPRemoteDirectory;

    private List<FTPConf> ftpConfList = new ArrayList<>();

    @PostConstruct
    public void init()
    {
        mLogger.debug("init");

    }

    public void prepareEditFTPConf(int rowId)
    {
        if (rowId != -1)
        {
            editFTPConfKey = ftpConfList.get(rowId).getConfKey();
            editFTPLabel = ftpConfList.get(rowId).getLabel();
            editFTPServer = ftpConfList.get(rowId).getServer();
            editFTPPort = ftpConfList.get(rowId).getPort();
            editFTPUserName = ftpConfList.get(rowId).getUserName();
            editFTPPassword = ftpConfList.get(rowId).getPassword();
            editFTPRemoteDirectory = ftpConfList.get(rowId).getRemoteDirectory();
        }
        else
        {
            editFTPConfKey = new Long(-1);
            editFTPLabel = "";
            editFTPServer = "";
            editFTPPort = new Long(21);
            editFTPUserName = "";
            editFTPPassword = "";
            editFTPRemoteDirectory = "";
        }
    }

    public void addModifyFTPConf()
    {
        try
        {
            Long userKey = SessionUtils.getUserProfile().getUserKey();
            String apiKey = SessionUtils.getCurrentWorkspaceDetails().getApiKey();

            if (userKey == null || apiKey == null || apiKey.equalsIgnoreCase(""))
            {
                mLogger.warn("no input to require mediaItemsKey"
                                + ", userKey: " + userKey
                                + ", apiKey: " + apiKey
                );
            }
            else
            {
                String username = userKey.toString();
                String password = apiKey;
                WorkspaceDetails currentWorkspaceDetails = SessionUtils.getCurrentWorkspaceDetails();

                CatraMMS catraMMS = new CatraMMS();
                if (editFTPConfKey == -1)
                    catraMMS.addFTPConf(
                            username, password,
                            editFTPLabel, editFTPServer, editFTPPort,
                            editFTPUserName, editFTPPassword, editFTPRemoteDirectory);
                else
                    catraMMS.modifyFTPConf(
                            username, password,
                            editFTPConfKey, editFTPLabel, editFTPServer, editFTPPort,
                            editFTPUserName, editFTPPassword, editFTPRemoteDirectory);

                fillList(false);

                FacesMessage message = new FacesMessage(FacesMessage.SEVERITY_ERROR, "FTP",
                        "Added successful");
                FacesContext.getCurrentInstance().addMessage(null, message);
            }
        }
        catch (Exception e)
        {
            String errorMessage = "Exception: " + e;
            mLogger.error(errorMessage);

            FacesMessage message = new FacesMessage(FacesMessage.SEVERITY_ERROR, "FTP",
                    "Add failed");
            FacesContext.getCurrentInstance().addMessage(null, message);
        }
    }

    public void removeFTPConf(Long confKey)
    {
        try
        {
            Long userKey = SessionUtils.getUserProfile().getUserKey();
            String apiKey = SessionUtils.getCurrentWorkspaceDetails().getApiKey();

            if (userKey == null || apiKey == null || apiKey.equalsIgnoreCase(""))
            {
                mLogger.warn("no input to require mediaItemsKey"
                                + ", userKey: " + userKey
                                + ", apiKey: " + apiKey
                );
            }
            else
            {
                String username = userKey.toString();
                String password = apiKey;

                CatraMMS catraMMS = new CatraMMS();
                catraMMS.removeFTPConf(
                        username, password, confKey);

                fillList(false);

                FacesMessage message = new FacesMessage(FacesMessage.SEVERITY_ERROR, "FTP",
                        "Removed successful");
                FacesContext.getCurrentInstance().addMessage(null, message);
            }
        }
        catch (Exception e)
        {
            String errorMessage = "Exception: " + e;
            mLogger.error(errorMessage);

            FacesMessage message = new FacesMessage(FacesMessage.SEVERITY_ERROR, "FTP",
                    "Remove failed");
            FacesContext.getCurrentInstance().addMessage(null, message);
        }
    }

    public void fillList(boolean toBeRedirected)
    {
        SimpleDateFormat simpleDateFormat = new SimpleDateFormat("yyyy-MM-dd HH:mm:ss");

        mLogger.info("fillList"
                + ", toBeRedirected: " + toBeRedirected);

        if (toBeRedirected)
        {
            try
            {
                // SimpleDateFormat simpleDateFormat_1 = new SimpleDateFormat("yyyy-MM-dd'T'HH:mm:ss");

                String url = "ftp.xhtml"
                        ;
                mLogger.info("Redirect to " + url);
                FacesContext.getCurrentInstance().getExternalContext().redirect(url);
            }
            catch (Exception e)
            {
                String errorMessage = "Exception: " + e;
                mLogger.error(errorMessage);
            }
        }
        else
        {
            try
            {
                Long userKey = SessionUtils.getUserProfile().getUserKey();
                String apiKey = SessionUtils.getCurrentWorkspaceDetails().getApiKey();

                if (userKey == null || apiKey == null || apiKey.equalsIgnoreCase(""))
                {
                    mLogger.warn("no input to require mediaItemsKey"
                            + ", userKey: " + userKey
                            + ", apiKey: " + apiKey
                    );
                }
                else
                {
                    String username = userKey.toString();
                    String password = apiKey;

                    ftpConfList.clear();

                    CatraMMS catraMMS = new CatraMMS();
                    ftpConfList = catraMMS.getFTPConf(
                            username, password);
                }
            }
            catch (Exception e)
            {
                String errorMessage = "Exception: " + e;
                mLogger.error(errorMessage);
            }
        }
    }

    public Long getEditFTPConfKey() {
        return editFTPConfKey;
    }

    public void setEditFTPConfKey(Long editFTPConfKey) {
        this.editFTPConfKey = editFTPConfKey;
    }

    public String getEditFTPLabel() {
        return editFTPLabel;
    }

    public void setEditFTPLabel(String editFTPLabel) {
        this.editFTPLabel = editFTPLabel;
    }

    public String getEditFTPServer() {
        return editFTPServer;
    }

    public void setEditFTPServer(String editFTPServer) {
        this.editFTPServer = editFTPServer;
    }

    public Long getEditFTPPort() {
        return editFTPPort;
    }

    public void setEditFTPPort(Long editFTPPort) {
        this.editFTPPort = editFTPPort;
    }

    public String getEditFTPUserName() {
        return editFTPUserName;
    }

    public void setEditFTPUserName(String editFTPUserName) {
        this.editFTPUserName = editFTPUserName;
    }

    public String getEditFTPPassword() {
        return editFTPPassword;
    }

    public void setEditFTPPassword(String editFTPPassword) {
        this.editFTPPassword = editFTPPassword;
    }

    public String getEditFTPRemoteDirectory() {
        return editFTPRemoteDirectory;
    }

    public void setEditFTPRemoteDirectory(String editFTPRemoteDirectory) {
        this.editFTPRemoteDirectory = editFTPRemoteDirectory;
    }

    public List<FTPConf> getFtpConfList() {
        return ftpConfList;
    }

    public void setFtpConfList(List<FTPConf> ftpConfList) {
        this.ftpConfList = ftpConfList;
    }
}
