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
import org.primefaces.event.FileUploadEvent;
import org.primefaces.event.NodeSelectEvent;
import org.primefaces.model.DefaultTreeNode;
import org.primefaces.model.TreeNode;
import org.primefaces.model.diagram.Connection;
import org.primefaces.model.diagram.DefaultDiagramModel;
import org.primefaces.model.diagram.Element;
import org.primefaces.model.diagram.endpoint.DotEndPoint;
import org.primefaces.model.diagram.endpoint.EndPointAnchor;

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

    private DefaultDiagramModel model;

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
            model = new DefaultDiagramModel();
            model.setMaxConnections(-1);
            model.setConnectionsDetachable(false);

            Element elementA = new Element("A", "20em", "6em");
            elementA.addEndPoint(new DotEndPoint(EndPointAnchor.BOTTOM));

            Element elementB = new Element("B", "10em", "18em");
            elementB.addEndPoint(new DotEndPoint(EndPointAnchor.TOP));

            Element elementC = new Element("C", "40em", "18em");
            elementC.addEndPoint(new DotEndPoint(EndPointAnchor.TOP));

            model.addElement(elementA);
            model.addElement(elementB);
            model.addElement(elementC);

            model.connect(new Connection(elementA.getEndPoints().get(0), elementB.getEndPoints().get(0)));
            model.connect(new Connection(elementA.getEndPoints().get(0), elementC.getEndPoints().get(0)));
        }

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

    public void onElementClick() {
        String id = FacesContext.getCurrentInstance().getExternalContext().getRequestParameterMap().get("elementId");
        mLogger.info("user clicked on " + id);
    }

    public void onElementRightClick() {
        String id = FacesContext.getCurrentInstance().getExternalContext().getRequestParameterMap().get("elementId");
        mLogger.info("user clicked (right) on " + id);
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
}
