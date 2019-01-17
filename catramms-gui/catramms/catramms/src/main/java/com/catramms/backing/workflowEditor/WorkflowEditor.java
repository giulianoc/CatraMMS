package com.catramms.backing.workflowEditor;

import com.catramms.backing.common.SessionUtils;
import com.catramms.backing.common.Workspace;
import com.catramms.backing.newWorkflow.IngestionResult;
import com.catramms.backing.newWorkflow.PushContent;
import com.catramms.backing.workflowEditor.Properties.*;
import com.catramms.backing.workflowEditor.utility.*;
import com.catramms.utility.catramms.CatraMMS;
import org.apache.log4j.Logger;
import org.json.JSONObject;
import org.primefaces.event.diagram.ConnectEvent;
import org.primefaces.event.diagram.ConnectionChangeEvent;
import org.primefaces.event.diagram.DisconnectEvent;
import org.primefaces.model.diagram.Connection;
import org.primefaces.model.diagram.DefaultDiagramModel;
import org.primefaces.model.diagram.Element;
import org.primefaces.model.diagram.connector.StraightConnector;
import org.primefaces.model.diagram.endpoint.DotEndPoint;
import org.primefaces.model.diagram.endpoint.EndPoint;
import org.primefaces.model.diagram.endpoint.EndPointAnchor;
import org.primefaces.model.diagram.endpoint.RectangleEndPoint;
import org.primefaces.model.diagram.overlay.ArrowOverlay;
import org.primefaces.model.diagram.overlay.LabelOverlay;

import javax.annotation.PostConstruct;
import javax.faces.application.FacesMessage;
import javax.faces.bean.ManagedBean;
import javax.faces.bean.ViewScoped;
import javax.faces.context.FacesContext;
import javax.faces.event.ActionEvent;
import java.io.*;
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
public class WorkflowEditor extends Workspace implements Serializable {

    // static because the class is Serializable
    private static final Logger mLogger = Logger.getLogger(WorkflowEditor.class);

    static public final String configFileName = "catramms.properties";

    private IngestionData ingestionData = new IngestionData();

    private MediaItemsReferences mediaItemsReferences = new MediaItemsReferences();

    private String labelTemplatePrefix = "Task id ";
    private int elementId;
    private String temporaryPushBinariesPathName;

    private String workflowLabel;

    private DefaultDiagramModel model;
    private Element rootElement;
    private int currentX;
    private int currentY;
    private int stepY;

    private String currentElementId;
    private WorkflowProperties currentWorkflowProperties;
    private AddContentProperties currentAddContentProperties;
    private GroupOfTasksProperties currentGroupOfTasksProperties;
    private RemoveContentProperties currentRemoveContentProperties;
    private ConcatDemuxerProperties currentConcatDemuxerProperties;
    private CutProperties currentCutProperties;

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

        elementId = 0;

        currentX = 30;
        currentY = 2;
        stepY = 11;

        {
            model = new DefaultDiagramModel();
            model.setMaxConnections(-1);

            model.getDefaultConnectionOverlays().add(new ArrowOverlay(20, 20, 1, 1));
            StraightConnector connector = new StraightConnector();
            connector.setPaintStyle("{strokeStyle:'#98AFC7', lineWidth:3}");
            connector.setHoverPaintStyle("{strokeStyle:'#5C738B'}");
            model.setDefaultConnector(connector);

            WorkflowProperties workflowProperties = new WorkflowProperties(elementId++, "Workflow", "Workflow-icon.png", "Root", "Workflow");
            rootElement = new Element(workflowProperties, currentX + "em", currentY + "em");
            rootElement.setId(String.valueOf(workflowProperties.getElementId()));
            rootElement.setDraggable(true);
            {
                RectangleEndPoint endPoint = new RectangleEndPoint(EndPointAnchor.BOTTOM);
                // endPoint.setScope("network");
                endPoint.setSource(true);
                endPoint.setStyle("{fillStyle:'#98AFC7'}");
                endPoint.setHoverStyle("{fillStyle:'#5C738B'}");
                // endPoint.setMaxConnections(1); not working

                rootElement.addEndPoint(endPoint);
            }

            model.addElement(rootElement);
        }

        buildWorkflowElementJson();
    }

    /*
    public void onElementClicked()
    {
        String currentId = FacesContext.getCurrentInstance().getExternalContext().getRequestParameterMap().get("currentId");
        String parentCurrentId = FacesContext.getCurrentInstance().getExternalContext().getRequestParameterMap().get("parentCurrentId");

        mLogger.info("onClicked"
                + ", currentId: " + currentId
                + ", parentCurrentId: " + parentCurrentId
        );

        Element element = model.findElement(elementId);
        if (element != null)
        {
            WorkflowProperties workflowElement = (WorkflowProperties) element.getData();

            mLogger.info("onClicked"
                    + ", elementId: " + elementId
                    + ", workflowElement.getLabel: " + workflowElement.getLabel()
            );

            FacesMessage msg = new FacesMessage(FacesMessage.SEVERITY_INFO, "onClicked",
                    "From " + workflowElement.getLabel());

            FacesContext.getCurrentInstance().addMessage(null, msg);

            PrimeFaces.current().ajax().update("form:msgs");
        }
        else
        {
            mLogger.info("onClicked. No Element found"
                    + ", elementId: " + elementId
            );

        }
    }
    */

    public void onElementLinkClicked()
    {
        String elementId = FacesContext.getCurrentInstance().getExternalContext().getRequestParameterMap().get("elementId");

        mLogger.info("onElementLinkClicked"
                + ", elementId: " + elementId
        );

        Element element = model.findElement(elementId);
        if (element != null)
        {
            currentElementId = elementId;

            WorkflowProperties workflowProperties = (WorkflowProperties) element.getData();

            if (workflowProperties.getType().equalsIgnoreCase("Workflow"))
                currentWorkflowProperties = ((WorkflowProperties) workflowProperties).clone();
            else if (workflowProperties.getType().equalsIgnoreCase("Add-Content"))
                currentAddContentProperties = ((AddContentProperties) workflowProperties).clone();
            else if (workflowProperties.getType().equalsIgnoreCase("GroupOfTasks"))
                currentGroupOfTasksProperties = ((GroupOfTasksProperties) workflowProperties).clone();
            else if (workflowProperties.getType().equalsIgnoreCase("Remove-Content"))
            {
                currentRemoveContentProperties = ((RemoveContentProperties) workflowProperties).clone();

                String currentElementType = workflowProperties.getType();
                String mediaItemsSelectionMode = "multiple";
                boolean videoContentType = true;
                boolean audioContentType = true;
                boolean imageContentType = true;
                StringBuilder taskReferences = currentRemoveContentProperties.getStringBuilderTaskReferences();
                mediaItemsReferences.prepareToSelectMediaItems(currentElementType, mediaItemsSelectionMode,
                    videoContentType, audioContentType, imageContentType, taskReferences);
            }
            else if (workflowProperties.getType().equalsIgnoreCase("Concat-Demuxer"))
            {
                currentConcatDemuxerProperties = ((ConcatDemuxerProperties) workflowProperties).clone();

                String currentElementType = workflowProperties.getType();
                String mediaItemsSelectionMode = "multiple";
                boolean videoContentType = true;
                boolean audioContentType = true;
                boolean imageContentType = false;
                StringBuilder taskReferences = currentConcatDemuxerProperties.getStringBuilderTaskReferences();
                mediaItemsReferences.prepareToSelectMediaItems(currentElementType, mediaItemsSelectionMode,
                        videoContentType, audioContentType, imageContentType, taskReferences);
            }
            else if (workflowProperties.getType().equalsIgnoreCase("Cut"))
            {
                currentCutProperties = ((CutProperties) workflowProperties).clone();

                String currentElementType = workflowProperties.getType();
                String mediaItemsSelectionMode = "single";
                boolean videoContentType = true;
                boolean audioContentType = true;
                boolean imageContentType = false;
                StringBuilder taskReferences = currentCutProperties.getStringBuilderTaskReferences();
                mediaItemsReferences.prepareToSelectMediaItems(currentElementType, mediaItemsSelectionMode,
                        videoContentType, audioContentType, imageContentType, taskReferences);
            }
        }
        else
        {
            mLogger.error("onElementLinkClicked. Didn't find element for elementId " + elementId);
        }
    }

    public void saveTaskProperties()
    {
        Element element = model.findElement(currentElementId);
        if (element != null)
        {
            WorkflowProperties workflowProperties = (WorkflowProperties) element.getData();

            if (workflowProperties.getType().equalsIgnoreCase("Workflow"))
                element.setData(currentWorkflowProperties);
            else if (workflowProperties.getType().equalsIgnoreCase("Add-Content"))
                element.setData(currentAddContentProperties);
            else if (workflowProperties.getType().equalsIgnoreCase("GroupOfTasks"))
                element.setData(currentGroupOfTasksProperties);
            else if (workflowProperties.getType().equalsIgnoreCase("Remove-Content"))
                element.setData(currentRemoveContentProperties);
            else if (workflowProperties.getType().equalsIgnoreCase("Concat-Demuxer"))
                element.setData(currentConcatDemuxerProperties);
            else if (workflowProperties.getType().equalsIgnoreCase("Cut"))
                element.setData(currentCutProperties);

            buildWorkflowElementJson();
        }
        else
        {
            mLogger.error("saveTaskProperties. Didn't find element for elementId " + elementId);
        }
    }

    public void onElementMove(ActionEvent param)
    {
        Map<String, String> params = FacesContext.getCurrentInstance().getExternalContext().getRequestParameterMap();
        String elementId = params.get("elementId");
        String elementX = params.get("elementX");
        String elementY = params.get("elementY");

        mLogger.info("onElementMove"
                + ", elementId: " + elementId
                + ", elementX: " + elementX
                + ", elementY: " + elementY
        );

        int pos = elementId.lastIndexOf("-"); // Remove Client ID part
        if (pos != -1)
        {
            elementId = elementId.substring(pos + 1);
        }
        Element element = model.findElement(elementId);
        if (element != null)
        {
            element.setX(elementX);
            element.setY(elementY);
        }
        else
        {
            mLogger.error("onElementMove. Didn't find element for elementId " + elementId);
        }
    }

    public void onConnect(ConnectEvent event)
    {
        try
        {
            WorkflowProperties sourceWorkflowProperties = (WorkflowProperties) event.getSourceElement().getData();
            WorkflowProperties targetWorkflowProperties = (WorkflowProperties) event.getTargetElement().getData();

            mLogger.info("onConnect"
                    + ", sourceWorkflowProperties.getLabel: " + sourceWorkflowProperties.getLabel()
                    + ", targetWorkflowProperties.getLabel: " + targetWorkflowProperties.getLabel()
            );

            EndPoint sourceEndPont = event.getSourceEndPoint();
            EndPoint targetEndPont = event.getTargetEndPoint();

            if (!isMainTypeConnectionAllowed(sourceWorkflowProperties, targetWorkflowProperties))
            {
                Connection connection = getConnection(sourceEndPont, targetEndPont);
                if (connection != null)
                {
                    model.disconnect(connection);

                    FacesMessage msg = new FacesMessage(FacesMessage.SEVERITY_ERROR, "Connection",
                            "It is not possible to connect " + sourceWorkflowProperties.getMainType() + " With " + targetWorkflowProperties.getMainType());

                    FacesContext.getCurrentInstance().addMessage(null, msg);

                    // PrimeFaces.current().ajax().update("form:msgs");

                    return;
                }
            }
            else if (!isConnectionNumberAllowed(sourceEndPont, sourceWorkflowProperties))
            {
                Connection connection = getConnection(sourceEndPont, targetEndPont);
                if (connection != null)
                {
                    model.disconnect(connection);

                    FacesMessage msg = new FacesMessage(FacesMessage.SEVERITY_ERROR, "Connection",
                            "It is not possible to have another connection from " + sourceWorkflowProperties.getMainType() + " To " + targetWorkflowProperties.getMainType());

                    FacesContext.getCurrentInstance().addMessage(null, msg);

                    // PrimeFaces.current().ajax().update("form:msgs");

                    return;
                }
            }

            // label for the connection
            {
                Connection connection = getConnection(sourceEndPont, targetEndPont);

                if (connection != null)
                {
                    if (sourceWorkflowProperties.getMainType().equalsIgnoreCase("Root")
                            && targetWorkflowProperties.getMainType().equalsIgnoreCase("Task"))
                    {
                        connection.getOverlays().add(new LabelOverlay("first task", "connection-label", 0.5));
                    }
                    else if (sourceWorkflowProperties.getMainType().equalsIgnoreCase("Root")
                            && targetWorkflowProperties.getMainType().equalsIgnoreCase("GroupOfTasks"))
                    {
                        connection.getOverlays().add(new LabelOverlay("first task", "connection-label", 0.5));
                    }
                    else if (sourceWorkflowProperties.getMainType().equalsIgnoreCase("Task")
                            && targetWorkflowProperties.getMainType().equalsIgnoreCase("Task"))
                    {
                        if (sourceEndPont.getAnchor() == EndPointAnchor.BOTTOM)
                            connection.getOverlays().add(new LabelOverlay("onSuccess", "connection-label", 0.5));
                        else if (sourceEndPont.getAnchor() == EndPointAnchor.BOTTOM_LEFT)
                            connection.getOverlays().add(new LabelOverlay("onComplete", "connection-label", 0.5));
                        else if (sourceEndPont.getAnchor() == EndPointAnchor.BOTTOM_RIGHT)
                            connection.getOverlays().add(new LabelOverlay("onError", "connection-label", 0.5));
                    }
                    else if (sourceWorkflowProperties.getMainType().equalsIgnoreCase("GroupOfTasks")
                            && targetWorkflowProperties.getMainType().equalsIgnoreCase("Task"))
                    {
                        if (sourceEndPont.getAnchor() == EndPointAnchor.BOTTOM)
                            connection.getOverlays().add(new LabelOverlay("onSuccess", "connection-label", 0.5));
                        else if (sourceEndPont.getAnchor() == EndPointAnchor.BOTTOM_LEFT)
                            connection.getOverlays().add(new LabelOverlay("onComplete", "connection-label", 0.5));
                        else if (sourceEndPont.getAnchor() == EndPointAnchor.BOTTOM_RIGHT)
                            connection.getOverlays().add(new LabelOverlay("onError", "connection-label", 0.5));
                        else if (sourceEndPont.getAnchor() == EndPointAnchor.RIGHT)
                            connection.getOverlays().add(new LabelOverlay("Task of Group", "connection-label", 0.5));
                    }
                }
            }

            // update data children
            {
                if (sourceEndPont.getAnchor() == EndPointAnchor.BOTTOM)
                    sourceWorkflowProperties.getOnSuccessChildren().add(targetWorkflowProperties);
                else if (sourceEndPont.getAnchor() == EndPointAnchor.BOTTOM_LEFT)
                    sourceWorkflowProperties.getOnCompleteChildren().add(targetWorkflowProperties);
                else if (sourceEndPont.getAnchor() == EndPointAnchor.BOTTOM_RIGHT)
                    sourceWorkflowProperties.getOnErrorChildren().add(targetWorkflowProperties);
                else if (sourceEndPont.getAnchor() == EndPointAnchor.RIGHT)
                {
                    GroupOfTasksProperties groupOfTasksProperties = (GroupOfTasksProperties) sourceWorkflowProperties;

                    groupOfTasksProperties.getTasks().add(targetWorkflowProperties);
                }
            }

            buildWorkflowElementJson();

            FacesMessage msg = new FacesMessage(FacesMessage.SEVERITY_INFO, "Connected",
                    "From " + sourceWorkflowProperties + " To " + targetWorkflowProperties);

            FacesContext.getCurrentInstance().addMessage(null, msg);

            // PrimeFaces.current().ajax().update("form:msgs");
        }
        catch (Exception e)
        {
            mLogger.error("Exception: " + e);
        }
    }

    public void onDisconnect(DisconnectEvent event)
    {
        try
        {
            EndPoint sourceEndPont = event.getSourceEndPoint();

            WorkflowProperties sourceWorkflowProperties = (WorkflowProperties) event.getSourceElement().getData();
            WorkflowProperties targetWorkflowProperties = (WorkflowProperties) event.getTargetElement().getData();

            mLogger.info("onDisconnect"
                    + ", sourceWorkflowProperties.getLabel: " + sourceWorkflowProperties.getLabel()
                    + ", targetWorkflowProperties.getLabel: " + targetWorkflowProperties.getLabel()
            );

            if (sourceEndPont.getAnchor() == EndPointAnchor.BOTTOM)
                sourceWorkflowProperties.getOnSuccessChildren().remove(targetWorkflowProperties);
            else if (sourceEndPont.getAnchor() == EndPointAnchor.BOTTOM_LEFT)
                sourceWorkflowProperties.getOnCompleteChildren().remove(targetWorkflowProperties);
            else if (sourceEndPont.getAnchor() == EndPointAnchor.BOTTOM_RIGHT)
                sourceWorkflowProperties.getOnErrorChildren().remove(targetWorkflowProperties);
            else if (sourceEndPont.getAnchor() == EndPointAnchor.RIGHT)
            {
                GroupOfTasksProperties groupOfTasksProperties = (GroupOfTasksProperties) sourceWorkflowProperties;

                groupOfTasksProperties.getTasks().remove(targetWorkflowProperties);
            }

            buildWorkflowElementJson();

            FacesMessage msg = new FacesMessage(FacesMessage.SEVERITY_INFO, "Disconnected",
                    "From " + event.getSourceElement().getData() + " To " + event.getTargetElement().getData());

            FacesContext.getCurrentInstance().addMessage(null, msg);

            // PrimeFaces.current().ajax().update("form:msgs");
        }
        catch (Exception e)
        {
            mLogger.error("Exception: " + e);
        }
    }

    public void onConnectionChange(ConnectionChangeEvent event)
    {
        EndPoint sourceOriginalEndPont = event.getOriginalSourceEndPoint();

        WorkflowProperties sourceOriginalWorkflowProperties = (WorkflowProperties) event.getOriginalSourceElement().getData();
        WorkflowProperties targetOriginalWorkflowProperties = (WorkflowProperties) event.getOriginalTargetElement().getData();
        WorkflowProperties sourceNewWorkflowProperties = (WorkflowProperties) event.getNewSourceElement().getData();
        WorkflowProperties targetNewWorkflowProperties = (WorkflowProperties) event.getNewTargetElement().getData();

        mLogger.info("onConnectionChange"
                + ", sourceOriginalWorkflowProperties.getLabel: " + sourceOriginalWorkflowProperties.getLabel()
                + ", targetOriginalWorkflowProperties.getLabel: " + targetOriginalWorkflowProperties.getLabel()
                + ", sourceNewWorkflowProperties.getLabel: " + sourceNewWorkflowProperties.getLabel()
                + ", targetNewWorkflowProperties.getLabel: " + targetNewWorkflowProperties.getLabel()
        );

        if (sourceOriginalEndPont.getAnchor() == EndPointAnchor.BOTTOM)
        {
            sourceOriginalWorkflowProperties.getOnSuccessChildren().remove(targetOriginalWorkflowProperties);
            // commented because the onConnect is generated too and the add is done there
            // sourceOriginalWorkflowProperties.getOnSuccessChildren().add(targetNewWorkflowProperties);
        }
        else if (sourceOriginalEndPont.getAnchor() == EndPointAnchor.BOTTOM_LEFT)
        {
            sourceOriginalWorkflowProperties.getOnCompleteChildren().remove(targetOriginalWorkflowProperties);
            // commented because the onConnect is generated too and the add is done there
            // sourceOriginalWorkflowProperties.getOnCompleteChildren().add(targetNewWorkflowProperties);
        }
        else if (sourceOriginalEndPont.getAnchor() == EndPointAnchor.BOTTOM_RIGHT)
        {
            sourceOriginalWorkflowProperties.getOnErrorChildren().remove(targetOriginalWorkflowProperties);
            // commented because the onConnect is generated too and the add is done there
            // sourceOriginalWorkflowProperties.getOnErrorChildren().add(targetNewWorkflowProperties);
        }
        else if (sourceOriginalEndPont.getAnchor() == EndPointAnchor.RIGHT)
        {
            GroupOfTasksProperties groupOfTasksProperties = (GroupOfTasksProperties) sourceOriginalWorkflowProperties;

            groupOfTasksProperties.getTasks().remove(targetOriginalWorkflowProperties);
            // commented because the onConnect is generated too and the add is done there
            // sourceOriginalWorkflowProperties.getOnErrorChildren().add(targetNewWorkflowProperties);
        }

        buildWorkflowElementJson();

        FacesMessage msg = new FacesMessage(FacesMessage.SEVERITY_INFO, "Connection Changed",
                "Original Source:" + event.getOriginalSourceElement().getData() +
                        ", New Source: " + event.getNewSourceElement().getData() +
                        ", Original Target: " + event.getOriginalTargetElement().getData() +
                        ", New Target: " + event.getNewTargetElement().getData());

        FacesContext.getCurrentInstance().addMessage(null, msg);

        // PrimeFaces.current().ajax().update("form:msgs");
        // suspendEvent = true;
    }

    private boolean isMainTypeConnectionAllowed(WorkflowProperties sourceWorkflowProperties, WorkflowProperties targetWorkflowProperties)
    {
        boolean isConnectionAllowed = false;

        if (sourceWorkflowProperties.getMainType().equalsIgnoreCase("Root"))
        {
            if (targetWorkflowProperties.getMainType().equalsIgnoreCase("Task")
                    || targetWorkflowProperties.getMainType().equalsIgnoreCase("GroupOfTasks")
            )
                isConnectionAllowed = true;
        }
        else if (sourceWorkflowProperties.getMainType().equalsIgnoreCase("Task"))
        {
            if (targetWorkflowProperties.getMainType().equalsIgnoreCase("Task")
                    || targetWorkflowProperties.getMainType().equalsIgnoreCase("GroupOfTasks")
            )
                isConnectionAllowed = true;
        }
        else if (sourceWorkflowProperties.getMainType().equalsIgnoreCase("GroupOfTasks"))
        {
            if (targetWorkflowProperties.getMainType().equalsIgnoreCase("Task")
            )
                isConnectionAllowed = true;
        }

        return isConnectionAllowed;
    }

    private boolean isConnectionNumberAllowed(EndPoint sourceEndPont, WorkflowProperties sourceWorkflowProperties)
    {
        boolean isConnectionNumberAllowed = false;

        if (sourceWorkflowProperties.getMainType().equalsIgnoreCase("Root"))
        {
            if (sourceWorkflowProperties.getOnSuccessChildren().size() == 0)
                isConnectionNumberAllowed = true;
        }
        else if (sourceWorkflowProperties.getMainType().equalsIgnoreCase("Task"))
        {
            if (sourceEndPont.getAnchor() == EndPointAnchor.BOTTOM
                    && sourceWorkflowProperties.getOnSuccessChildren().size() == 0)
                isConnectionNumberAllowed = true;
            else if (sourceEndPont.getAnchor() == EndPointAnchor.BOTTOM_LEFT
                    && sourceWorkflowProperties.getOnCompleteChildren().size() == 0)
                isConnectionNumberAllowed = true;
            else if (sourceEndPont.getAnchor() == EndPointAnchor.BOTTOM_RIGHT
                    && sourceWorkflowProperties.getOnErrorChildren().size() == 0)
                isConnectionNumberAllowed = true;
        }
        else if (sourceWorkflowProperties.getMainType().equalsIgnoreCase("GroupOfTasks")
        )
        {
            if (sourceEndPont.getAnchor() == EndPointAnchor.BOTTOM
                    && sourceWorkflowProperties.getOnSuccessChildren().size() == 0)
                isConnectionNumberAllowed = true;
            else if (sourceEndPont.getAnchor() == EndPointAnchor.BOTTOM_LEFT
                    && sourceWorkflowProperties.getOnCompleteChildren().size() == 0)
                isConnectionNumberAllowed = true;
            else if (sourceEndPont.getAnchor() == EndPointAnchor.BOTTOM_RIGHT
                    && sourceWorkflowProperties.getOnErrorChildren().size() == 0)
                isConnectionNumberAllowed = true;
            else if (sourceEndPont.getAnchor() == EndPointAnchor.RIGHT)
                isConnectionNumberAllowed = true;
        }

        return isConnectionNumberAllowed;
    }

//    public void addTask(String taskType)
//    {
    public void addTask(ActionEvent param)
    {
        Map<String, String> params = FacesContext.getCurrentInstance().getExternalContext().getRequestParameterMap();
        String taskType = params.get("taskType");
        String positionX = params.get("positionX");
        String positionY = params.get("positionY");

        mLogger.info("addTask"
                        + ", taskType: " + taskType
                        + ", positionX: " + positionX
                        + ", positionY: " + positionY
        );

        WorkflowProperties workflowProperties = null;

        if (taskType.equalsIgnoreCase("Add-Content"))
            workflowProperties = new AddContentProperties(elementId++, labelTemplatePrefix + (elementId - 1),
                    temporaryPushBinariesPathName);
        else if (taskType.equalsIgnoreCase("GroupOfTasks"))
            workflowProperties = new GroupOfTasksProperties(elementId++, labelTemplatePrefix + (elementId - 1));
        else if (taskType.equalsIgnoreCase("Remove-Content"))
            workflowProperties = new RemoveContentProperties(elementId++, labelTemplatePrefix + (elementId - 1));
        else if (taskType.equalsIgnoreCase("Concat-Demuxer"))
            workflowProperties = new ConcatDemuxerProperties(elementId++, labelTemplatePrefix + (elementId - 1));
        else if (taskType.equalsIgnoreCase("Cut"))
            workflowProperties = new CutProperties(elementId++, labelTemplatePrefix + (elementId - 1));

        /*
        // some initialization here because otherwise the buildWorkflow (json), will fail
        {
            if (task.getType().equalsIgnoreCase("Encode"))
            {
                task.setEncodingProfileType("profilesSet");
            }
        }
        */

        currentY += stepY;

        // Element taskElement = new Element(workflowProperties, currentX + "em", currentY + "em");
        Element taskElement = new Element(workflowProperties, positionX, positionY);
        taskElement.setId(String.valueOf(workflowProperties.getElementId()));
        taskElement.setDraggable(true);
        {

            DotEndPoint endPoint = new DotEndPoint(EndPointAnchor.TOP);
            // endPoint.setScope("network");
            endPoint.setTarget(true);
            endPoint.setStyle("{fillStyle:'#98AFC7'}");
            endPoint.setHoverStyle("{fillStyle:'#5C738B'}");
            // endPoint.setMaxConnections(1); not working

            taskElement.addEndPoint(endPoint);
        }

        // onSuccess
        {
            RectangleEndPoint endPoint = new RectangleEndPoint(EndPointAnchor.BOTTOM);
            // endPoint.setScope("network");
            endPoint.setSource(true);
            endPoint.setStyle("{fillStyle:'#98AFC7'}");
            endPoint.setHoverStyle("{fillStyle:'#5C738B'}");

            taskElement.addEndPoint(endPoint);
        }

        // onError
        {
            RectangleEndPoint endPoint = new RectangleEndPoint(EndPointAnchor.BOTTOM_LEFT);
            // endPoint.setScope("network");
            endPoint.setSource(true);
            endPoint.setStyle("{fillStyle:'#98AFC7'}");
            endPoint.setHoverStyle("{fillStyle:'#5C738B'}");

            taskElement.addEndPoint(endPoint);
        }

        // onComplete
        {
            RectangleEndPoint endPoint = new RectangleEndPoint(EndPointAnchor.BOTTOM_RIGHT);
            // endPoint.setScope("network");
            endPoint.setSource(true);
            endPoint.setStyle("{fillStyle:'#98AFC7'}");
            endPoint.setHoverStyle("{fillStyle:'#5C738B'}");

            taskElement.addEndPoint(endPoint);
        }

        if (taskType.equalsIgnoreCase("GroupOfTasks"))
        {
            // to add the list of Tasks

            RectangleEndPoint endPoint = new RectangleEndPoint(EndPointAnchor.RIGHT);
            // endPoint.setScope("network");
            endPoint.setSource(true);
            endPoint.setStyle("{fillStyle:'#98AFC7'}");
            endPoint.setHoverStyle("{fillStyle:'#5C738B'}");

            taskElement.addEndPoint(endPoint);
        }

        model.addElement(taskElement);
    }

    private Connection getConnection(EndPoint sourceEndPont, EndPoint targetEndPont)
            throws Exception
    {
        // mLogger.info("sourceEndPont: " + sourceEndPont);
        // mLogger.info("targetEndPont: " + targetEndPont);

        Connection connectionToBeReturned = null;

        for (Connection connection: model.getConnections())
        {
            // mLogger.info("connection.getSource: " + connection.getSource());
            // mLogger.info("connection.getTarget: " + connection.getTarget());

            if (connection.getSource() == sourceEndPont && connection.getTarget() == targetEndPont)
            {
                // mLogger.info("Connection found");

                connectionToBeReturned = connection;

                break;
            }
        }

        return connectionToBeReturned;
    }

    public void buildWorkflowElementJson()
    {
        try {
            ingestionData.getWorkflowIssueList().clear();
            ingestionData.getPushContentList().clear();

            WorkflowProperties workflowProperties = (WorkflowProperties) rootElement.getData();
            JSONObject joWorkflow = workflowProperties.buildWorkflowElementJson(ingestionData);

            ingestionData.setJsonWorkflow(joWorkflow.toString(8));
        }
        catch (Exception e)
        {
            mLogger.error("buildWorkflow failed. Exception: " + e);
        }
    }

    public void ingestWorkflow()
    {
        String username;
        String password;

        ingestionData.setIngestWorkflowErrorMessage(null);

        try
        {
            Long userKey = SessionUtils.getUserProfile().getUserKey();
            String apiKey = SessionUtils.getCurrentWorkspaceDetails().getApiKey();

            username = userKey.toString();
            password = apiKey;

            CatraMMS catraMMS = new CatraMMS();

            ingestionData.getIngestionJobList().clear();

            ingestionData.setWorkflowRoot(catraMMS.ingestWorkflow(username, password,
                    ingestionData.getJsonWorkflow(), ingestionData.getIngestionJobList()));
        }
        catch (Exception e)
        {
            String errorMessage = "Exception: " + e;
            mLogger.error(errorMessage);

            ingestionData.setIngestWorkflowErrorMessage(errorMessage);

            FacesMessage message = new FacesMessage(FacesMessage.SEVERITY_ERROR, "Ingestion",
                    "Ingestion Workflow failed: " + errorMessage);
            FacesContext.getCurrentInstance().addMessage(null, message);

            return;
        }

        for (PushContent pushContent: ingestionData.getPushContentList())
        {
            try
            {
                IngestionResult pushContentIngestionTask = null;
                for (IngestionResult ingestionTaskResult: ingestionData.getIngestionJobList())
                {
                    if (ingestionTaskResult.getLabel().equalsIgnoreCase(pushContent.getLabel()))
                    {
                        pushContentIngestionTask = ingestionTaskResult;

                        break;
                    }
                }

                if (pushContentIngestionTask == null)
                {
                    String errorMessage = "Content to be pushed was not found among the IngestionResults"
                            + ", pushContent.getLabel: " + pushContent.getLabel()
                            ;
                    mLogger.error(errorMessage);

                    FacesMessage message = new FacesMessage(FacesMessage.SEVERITY_ERROR, "Ingestion",
                            errorMessage);
                    FacesContext.getCurrentInstance().addMessage(null, message);

                    continue;
                }

                File mediaFile = new File(pushContent.getBinaryPathName());
                InputStream binaryFileInputStream = new DataInputStream(new FileInputStream(mediaFile));

                CatraMMS catraMMS = new CatraMMS();
                catraMMS.ingestBinaryContent(username, password,
                        binaryFileInputStream, mediaFile.length(),
                        pushContentIngestionTask.getKey());

                // this is the server (tomcat) copy of the file that has to be removed once
                // it was uploaded into MMS
                // His pathname will be somthing like /var/catramms/storage/MMSGUI/temporaryPushUploads/<userKey>-<filename>
                mediaFile.delete();
            }
            catch (Exception e)
            {
                String errorMessage = "Upload Push Content failed"
                        + ", pushContent.getLabel: " + pushContent.getLabel()
                        + ", Exception: " + e
                        ;
                mLogger.error(errorMessage);

                FacesMessage message = new FacesMessage(FacesMessage.SEVERITY_ERROR, "Ingestion",
                        errorMessage);
                FacesContext.getCurrentInstance().addMessage(null, message);
            }
        }
    }

    private void loadConfigurationParameters()
    {
        try
        {
            Properties properties = getConfigurationParameters();

            {
                {
                    temporaryPushBinariesPathName = properties.getProperty("catramms.push.temporaryBinariesPathName");
                    if (temporaryPushBinariesPathName == null)
                    {
                        String errorMessage = "No catramms.push.temporaryBinariesPathName found. ConfigurationFileName: " + configFileName;
                        mLogger.error(errorMessage);

                        return;
                    }

                    File temporaryPushBinariesFile = new File(temporaryPushBinariesPathName);
                    if (!temporaryPushBinariesFile.exists())
                        temporaryPushBinariesFile.mkdirs();
                }
            }
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
                        inputStream = WorkflowEditor.class.getClassLoader().getResourceAsStream(configFileName);
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

    public IngestionData getIngestionData() {
        return ingestionData;
    }

    public void setIngestionData(IngestionData ingestionData) {
        this.ingestionData = ingestionData;
    }

    public MediaItemsReferences getMediaItemsReferences() {
        return mediaItemsReferences;
    }

    public void setMediaItemsReferences(MediaItemsReferences mediaItemsReferences) {
        this.mediaItemsReferences = mediaItemsReferences;
    }

    public AddContentProperties getCurrentAddContentProperties() {
        return currentAddContentProperties;
    }

    public void setCurrentAddContentProperties(AddContentProperties currentAddContentProperties) {
        this.currentAddContentProperties = currentAddContentProperties;
    }

    public WorkflowProperties getCurrentWorkflowProperties() {
        return currentWorkflowProperties;
    }

    public void setCurrentWorkflowProperties(WorkflowProperties currentWorkflowProperties) {
        this.currentWorkflowProperties = currentWorkflowProperties;
    }

    public GroupOfTasksProperties getCurrentGroupOfTasksProperties() {
        return currentGroupOfTasksProperties;
    }

    public void setCurrentGroupOfTasksProperties(GroupOfTasksProperties currentGroupOfTasksProperties) {
        this.currentGroupOfTasksProperties = currentGroupOfTasksProperties;
    }

    public RemoveContentProperties getCurrentRemoveContentProperties() {
        return currentRemoveContentProperties;
    }

    public void setCurrentRemoveContentProperties(RemoveContentProperties currentRemoveContentProperties) {
        this.currentRemoveContentProperties = currentRemoveContentProperties;
    }

    public ConcatDemuxerProperties getCurrentConcatDemuxerProperties() {
        return currentConcatDemuxerProperties;
    }

    public void setCurrentConcatDemuxerProperties(ConcatDemuxerProperties currentConcatDemuxerProperties) {
        this.currentConcatDemuxerProperties = currentConcatDemuxerProperties;
    }

    public CutProperties getCurrentCutProperties() {
        return currentCutProperties;
    }

    public void setCurrentCutProperties(CutProperties currentCutProperties) {
        this.currentCutProperties = currentCutProperties;
    }
}
