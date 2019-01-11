package com.catramms.backing.newWorkspace2;

import com.catramms.backing.common.SessionUtils;
import com.catramms.backing.common.Workspace;
import com.catramms.backing.entity.*;
import com.catramms.backing.newWorkflow.*;
import com.catramms.utility.catramms.CatraMMS;
import org.apache.commons.io.IOUtils;
import org.apache.log4j.Logger;
import org.json.JSONArray;
import org.json.JSONObject;
import org.primefaces.PrimeFaces;
import org.primefaces.event.FileUploadEvent;
import org.primefaces.event.NodeSelectEvent;
import org.primefaces.event.diagram.ConnectEvent;
import org.primefaces.event.diagram.ConnectionChangeEvent;
import org.primefaces.event.diagram.DisconnectEvent;
import org.primefaces.model.DefaultTreeNode;
import org.primefaces.model.TreeNode;
import org.primefaces.model.diagram.Connection;
import org.primefaces.model.diagram.DefaultDiagramModel;
import org.primefaces.model.diagram.Element;
import org.primefaces.model.diagram.connector.StraightConnector;
import org.primefaces.model.diagram.endpoint.DotEndPoint;
import org.primefaces.model.diagram.endpoint.EndPoint;
import org.primefaces.model.diagram.endpoint.EndPointAnchor;
import org.primefaces.model.diagram.endpoint.RectangleEndPoint;
import org.primefaces.model.diagram.overlay.ArrowOverlay;

import javax.annotation.PostConstruct;
import javax.faces.application.FacesMessage;
import javax.faces.bean.ManagedBean;
import javax.faces.bean.ViewScoped;
import javax.faces.context.FacesContext;
import java.io.*;
import java.text.DateFormat;
import java.text.SimpleDateFormat;
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
public class NewWorkflow2 extends Workspace implements Serializable {

    // static because the class is Serializable
    private static final Logger mLogger = Logger.getLogger(NewWorkflow2.class);

    static public final String configFileName = "catramms.properties";

    private String workflowLabel;

    private String addContentIcon;

    private DefaultDiagramModel model;
    private boolean suspendEvent;

    @PostConstruct
    public void init()
    {
        try
        {
            mLogger.info("loadConfigurationParameters...");
            loadConfigurationParameters();
        }
        catch (Exception e)
        {
            String errorMessage = "Problems to load the configuration file. Exception: " + e + ", ConfigurationFileName: " + configFileName;
            mLogger.error(errorMessage);

            return;
        }

        {
            addContentIcon = "ui-icon-close";
        }

        {
            model = new DefaultDiagramModel();
            model.setMaxConnections(-1);

            model.getDefaultConnectionOverlays().add(new ArrowOverlay(20, 20, 1, 1));
            StraightConnector connector = new StraightConnector();
            connector.setPaintStyle("{strokeStyle:'#98AFC7', lineWidth:3}");
            connector.setHoverPaintStyle("{strokeStyle:'#5C738B'}");
            model.setDefaultConnector(connector);

            Element computerA = new Element(new WorkspaceElement("Computer A", "computer-icon.png"), "10em", "6em");
            EndPoint endPointCA = createRectangleEndPoint(EndPointAnchor.BOTTOM);
            endPointCA.setSource(true);
            computerA.addEndPoint(endPointCA);

            Element computerB = new Element(new WorkspaceElement("Computer B", "computer-icon.png"), "25em", "6em");
            EndPoint endPointCB = createRectangleEndPoint(EndPointAnchor.BOTTOM);
            endPointCB.setSource(true);
            computerB.addEndPoint(endPointCB);

            Element computerC = new Element(new WorkspaceElement("Computer C", "computer-icon.png"), "40em", "6em");
            EndPoint endPointCC = createRectangleEndPoint(EndPointAnchor.BOTTOM);
            endPointCC.setSource(true);
            computerC.addEndPoint(endPointCC);

            Element serverA = new Element(new WorkspaceElement("Server A", "server-icon.png"), "15em", "24em");
            EndPoint endPointSA = createDotEndPoint(EndPointAnchor.AUTO_DEFAULT);
            serverA.setDraggable(false);
            endPointSA.setTarget(true);
            serverA.addEndPoint(endPointSA);

            Element serverB = new Element(new WorkspaceElement("Server B", "server-icon.png"), "35em", "24em");
            EndPoint endPointSB = createDotEndPoint(EndPointAnchor.AUTO_DEFAULT);
            serverB.setDraggable(false);
            endPointSB.setTarget(true);
            serverB.addEndPoint(endPointSB);

            model.addElement(computerA);
            model.addElement(computerB);
            model.addElement(computerC);
            model.addElement(serverA);
            model.addElement(serverB);
        }
    }

    public void onConnect(ConnectEvent event)
    {
        if(!suspendEvent) {
            FacesMessage msg = new FacesMessage(FacesMessage.SEVERITY_INFO, "Connected",
                    "From " + event.getSourceElement().getData() + " To " + event.getTargetElement().getData());

            FacesContext.getCurrentInstance().addMessage(null, msg);

            PrimeFaces.current().ajax().update("form:msgs");
        }
        else {
            suspendEvent = false;
        }
    }

    public void onDisconnect(DisconnectEvent event)
    {
        FacesMessage msg = new FacesMessage(FacesMessage.SEVERITY_INFO, "Disconnected",
                "From " + event.getSourceElement().getData() + " To " + event.getTargetElement().getData());

        FacesContext.getCurrentInstance().addMessage(null, msg);

        PrimeFaces.current().ajax().update("form:msgs");
    }

    public void onConnectionChange(ConnectionChangeEvent event)
    {
        FacesMessage msg = new FacesMessage(FacesMessage.SEVERITY_INFO, "Connection Changed",
                "Original Source:" + event.getOriginalSourceElement().getData() +
                        ", New Source: " + event.getNewSourceElement().getData() +
                        ", Original Target: " + event.getOriginalTargetElement().getData() +
                        ", New Target: " + event.getNewTargetElement().getData());

        FacesContext.getCurrentInstance().addMessage(null, msg);

        PrimeFaces.current().ajax().update("form:msgs");
        suspendEvent = true;
    }

    private EndPoint createDotEndPoint(EndPointAnchor anchor)
    {
        DotEndPoint endPoint = new DotEndPoint(anchor);
        endPoint.setScope("network");
        endPoint.setTarget(true);
        endPoint.setStyle("{fillStyle:'#98AFC7'}");
        endPoint.setHoverStyle("{fillStyle:'#5C738B'}");

        return endPoint;
    }

    private EndPoint createRectangleEndPoint(EndPointAnchor anchor)
    {
        RectangleEndPoint endPoint = new RectangleEndPoint(anchor);
        endPoint.setScope("network");
        endPoint.setSource(true);
        endPoint.setStyle("{fillStyle:'#98AFC7'}");
        endPoint.setHoverStyle("{fillStyle:'#5C738B'}");

        return endPoint;
    }

    public void addTask(String taskType)
    {
        mLogger.info("addTask: " + taskType);

        Element serverA = new Element(new WorkspaceElement(taskType, "server-icon.png"), "10em", "6em");
        EndPoint endPointCC = createRectangleEndPoint(EndPointAnchor.BOTTOM);
        endPointCC.setSource(true);
        serverA.addEndPoint(endPointCC);
        model.addElement(serverA);
    }

    private void loadConfigurationParameters()
    {
        try
        {
            Properties properties = getConfigurationParameters();
        }
        catch (Exception e)
        {
            String errorMessage = "Problems to load the configuration file. Exception: " + e + ", ConfigurationFileName: " + configFileName;
            mLogger.error(errorMessage);

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
                        inputStream = NewWorkflow.class.getClassLoader().getResourceAsStream(configFileName);
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

    public String getWorkflowLabel() {
        return workflowLabel;
    }

    public void setWorkflowLabel(String workflowLabel) {
        this.workflowLabel = workflowLabel;
    }

    public DefaultDiagramModel getModel() {
        return model;
    }

    public void setModel(DefaultDiagramModel model) {
        this.model = model;
    }

    public String getAddContentIcon() {
        return addContentIcon;
    }

    public void setAddContentIcon(String addContentIcon) {
        this.addContentIcon = addContentIcon;
    }
}
