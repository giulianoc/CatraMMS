package com.catramms.backing.workflowEditor;

import com.catramms.backing.common.SessionUtils;
import com.catramms.backing.common.Workspace;
import com.catramms.backing.workflowEditor.utility.IngestionResult;
import com.catramms.backing.workflowEditor.utility.PushContent;
import com.catramms.backing.workflowEditor.Properties.*;
import com.catramms.backing.workflowEditor.utility.*;
import com.catramms.utility.catramms.CatraMMS;
import org.apache.commons.lang.StringEscapeUtils;
import org.apache.log4j.Logger;
import org.json.JSONArray;
import org.json.JSONObject;
import org.primefaces.event.diagram.ConnectEvent;
import org.primefaces.event.diagram.ConnectionChangeEvent;
import org.primefaces.event.diagram.DisconnectEvent;
import org.primefaces.model.diagram.Connection;
import org.primefaces.model.diagram.DefaultDiagramModel;
import org.primefaces.model.diagram.Element;
import org.primefaces.model.diagram.connector.StraightConnector;
import org.primefaces.model.diagram.endpoint.*;
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

    private String loadType;
    private String data;

    private String storageWorkflowEditorPath;
    private String currentWorkflowEditorName;
    private List<String> workflowEditorNames;

    int workflow_firstX = 1;   // onError
    int workflow_firstY = 1;
    int workflow_stepX = 17;
    int workflow_stepY = 8;

    private IngestionData ingestionData = new IngestionData();

    private MediaItemsReferences mediaItemsReferences = new MediaItemsReferences();

    private String workflowDefaultLabel = "<workflow label>";
    private String groupOfTasksDefaultLabel = "<no label>";
    private String taskDefaultLabel = "<task label __TASKID__>";
    private int creationCurrentElementId;
    private String temporaryPushBinariesPathName;

    private Integer removingCurrentElementId;

    private String workflowLabel;

    private DefaultDiagramModel model;
    private Element workflowElement;

    private String editingCurrentElementId;
    private WorkflowProperties currentWorkflowProperties;
    private AddContentProperties currentAddContentProperties;
    private GroupOfTasksProperties currentGroupOfTasksProperties;
    private RemoveContentProperties currentRemoveContentProperties;
    private ConcatDemuxerProperties currentConcatDemuxerProperties;
    private CutProperties currentCutProperties;
    private ExtractTracksProperties currentExtractTracksProperties;
    private EncodeProperties currentEncodeProperties;
    private OverlayImageOnVideoProperties currentOverlayImageOnVideoProperties;
    private OverlayTextOnVideoProperties currentOverlayTextOnVideoProperties;
    private FrameProperties currentFrameProperties;
    private PeriodicalFramesProperties currentPeriodicalFramesProperties;
    private IFramesProperties currentIFramesProperties;
    private MotionJPEGByPeriodicalFramesProperties currentMotionJPEGByPeriodicalFramesProperties;
    private MotionJPEGByIFramesProperties currentMotionJPEGByIFramesProperties;
    private SlideshowProperties currentSlideshowProperties;
    private FTPDeliveryProperties currentFTPDeliveryProperties;
    private LocalCopyProperties currentLocalCopyProperties;
    private PostOnFacebookProperties currentPostOnFacebookProperties;
    private PostOnYouTubeProperties currentPostOnYouTubeProperties;
    private EmailNotificationProperties currentEmailNotificationProperties;
    private HTTPCallbackProperties currentHttpCallbackProperties;
    private FaceRecognitionProperties currentFaceRecognitionProperties;
    private FaceIdentificationProperties currentFaceIdentificationProperties;
    private LiveRecorderProperties currentLiveRecorderProperties;
    private ChangeFileFormatProperties currentChangeFileFormatProperties;


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

        storageWorkflowEditorPath = "/var/catramms/storage/MMSGUI/WorkflowEditor/";
        workflowEditorNames = new ArrayList<>();

        creationCurrentElementId = 0;
        removingCurrentElementId = new Integer(0);

        {
            model = new DefaultDiagramModel();
            model.setMaxConnections(-1);

            model.getDefaultConnectionOverlays().add(new ArrowOverlay(20, 20, 1, 1));
            StraightConnector connector = new StraightConnector();
            connector.setPaintStyle("{strokeStyle:'#98AFC7', lineWidth:3}");
            connector.setHoverPaintStyle("{strokeStyle:'#5C738B'}");
            model.setDefaultConnector(connector);

            WorkflowProperties workflowProperties = new WorkflowProperties(
                    (workflow_firstX + workflow_stepX) + "em",
                    workflow_firstY + "em",
                    creationCurrentElementId++, workflowDefaultLabel, "Workflow-icon.png",
                    "Root", "Workflow");
            workflowElement = new Element(workflowProperties,
                    workflowProperties.getPositionX(),
                    workflowProperties.getPositionY());
            workflowElement.setId(String.valueOf(workflowProperties.getElementId()));
            workflowElement.setDraggable(true);
            {
                // RectangleEndPoint endPoint = new RectangleEndPoint(EndPointAnchor.BOTTOM);
                ImageEndPoint endPoint = new ImageEndPoint(EndPointAnchor.BOTTOM, "/resources/img/onSuccess.png");
                // endPoint.setScope("network");
                endPoint.setSource(true);
                endPoint.setStyle("{fillStyle:'#00FF00'}");
                endPoint.setHoverStyle("{fillStyle:'#5C738B'}");
                // endPoint.setMaxConnections(1); not working

                workflowElement.addEndPoint(endPoint);
            }

            model.addElement(workflowElement);
        }

        buildWorkflowElementJson();

        mLogger.info("loadType: " + loadType
                        + ", data: " + data
        );
        if (loadType != null)
        {
            if (loadType.equalsIgnoreCase("metaDataContent"))
            {
                buildModelByMetaDataContent(data);
            }
            else if (loadType.equalsIgnoreCase("ingestionRootKey"))
            {
                buildModelByIngestionRootKey(Long.parseLong(data));
            }
            else
            {
                mLogger.error("Unknown loadType Workflow: " + loadType);
            }
        }
    }

    public void addTask(ActionEvent param)
    {
        Map<String, String> params = FacesContext.getCurrentInstance().getExternalContext().getRequestParameterMap();
        String taskType = params.get("taskType");
        String positionX = params.get("positionX");
        String positionY = params.get("positionY");

        addTask(taskType, positionX, positionY);
    }

    public Element addTask(String taskType, String positionX, String positionY)
    {
        mLogger.info("addTask"
                + ", taskType: " + taskType
                + ", positionX: " + positionX
                + ", positionY: " + positionY
        );

        String localPositionX;
        String localPositionY;

        if (positionX == null || positionX.equalsIgnoreCase("")
                || positionY == null || positionY.equalsIgnoreCase(""))
        {
            localPositionX = "5em";
            localPositionY = "5em";
        }
        else
        {
            localPositionX = positionX;
            localPositionY = positionY;
        }

        WorkflowProperties workflowProperties = null;

        if (taskType.equalsIgnoreCase("GroupOfTasks"))
            workflowProperties = new GroupOfTasksProperties(localPositionX, localPositionY,
                    creationCurrentElementId++, groupOfTasksDefaultLabel);
        else if (taskType.equalsIgnoreCase("Add-Content"))
            workflowProperties = new AddContentProperties(localPositionX, localPositionY,
                    creationCurrentElementId++,
                    taskDefaultLabel.replaceAll("__TASKID__", new Long(creationCurrentElementId - 1).toString()),
                    temporaryPushBinariesPathName);
        else if (taskType.equalsIgnoreCase("Remove-Content"))
            workflowProperties = new RemoveContentProperties(localPositionX, localPositionY,
                    creationCurrentElementId++,
                    taskDefaultLabel.replaceAll("__TASKID__", new Long(creationCurrentElementId - 1).toString())
                    );
        else if (taskType.equalsIgnoreCase("Concat-Demuxer"))
            workflowProperties = new ConcatDemuxerProperties(localPositionX, localPositionY,
                    creationCurrentElementId++,
                    taskDefaultLabel.replaceAll("__TASKID__", new Long(creationCurrentElementId - 1).toString())
            );
        else if (taskType.equalsIgnoreCase("Cut"))
            workflowProperties = new CutProperties(localPositionX, localPositionY,
                    creationCurrentElementId++,
                    taskDefaultLabel.replaceAll("__TASKID__", new Long(creationCurrentElementId - 1).toString())
            );
        else if (taskType.equalsIgnoreCase("Extract-Tracks"))
            workflowProperties = new ExtractTracksProperties(localPositionX, localPositionY,
                    creationCurrentElementId++,
                    taskDefaultLabel.replaceAll("__TASKID__", new Long(creationCurrentElementId - 1).toString())
            );
        else if (taskType.equalsIgnoreCase("Encode"))
            workflowProperties = new EncodeProperties(localPositionX, localPositionY,
                    creationCurrentElementId++,
                    taskDefaultLabel.replaceAll("__TASKID__", new Long(creationCurrentElementId - 1).toString())
            );
        else if (taskType.equalsIgnoreCase("Overlay-Image-On-Video"))
            workflowProperties = new OverlayImageOnVideoProperties(localPositionX, localPositionY,
                    creationCurrentElementId++,
                    taskDefaultLabel.replaceAll("__TASKID__", new Long(creationCurrentElementId - 1).toString())
            );
        else if (taskType.equalsIgnoreCase("Overlay-Text-On-Video"))
            workflowProperties = new OverlayTextOnVideoProperties(localPositionX, localPositionY,
                    creationCurrentElementId++,
                    taskDefaultLabel.replaceAll("__TASKID__", new Long(creationCurrentElementId - 1).toString())
            );
        else if (taskType.equalsIgnoreCase("Frame"))
            workflowProperties = new FrameProperties(localPositionX, localPositionY,
                    creationCurrentElementId++,
                    taskDefaultLabel.replaceAll("__TASKID__", new Long(creationCurrentElementId - 1).toString())
            );
        else if (taskType.equalsIgnoreCase("Periodical-Frames"))
            workflowProperties = new PeriodicalFramesProperties(localPositionX, localPositionY,
                    creationCurrentElementId++,
                    taskDefaultLabel.replaceAll("__TASKID__", new Long(creationCurrentElementId - 1).toString())
            );
        else if (taskType.equalsIgnoreCase("I-Frames"))
            workflowProperties = new IFramesProperties(localPositionX, localPositionY,
                    creationCurrentElementId++,
                    taskDefaultLabel.replaceAll("__TASKID__", new Long(creationCurrentElementId - 1).toString())
            );
        else if (taskType.equalsIgnoreCase("Motion-JPEG-by-Periodical-Frames"))
            workflowProperties = new MotionJPEGByPeriodicalFramesProperties(localPositionX, localPositionY,
                    creationCurrentElementId++,
                    taskDefaultLabel.replaceAll("__TASKID__", new Long(creationCurrentElementId - 1).toString())
            );
        else if (taskType.equalsIgnoreCase("Motion-JPEG-by-I-Frames"))
            workflowProperties = new MotionJPEGByIFramesProperties(localPositionX, localPositionY,
                    creationCurrentElementId++,
                    taskDefaultLabel.replaceAll("__TASKID__", new Long(creationCurrentElementId - 1).toString())
            );
        else if (taskType.equalsIgnoreCase("Slideshow"))
            workflowProperties = new SlideshowProperties(localPositionX, localPositionY,
                    creationCurrentElementId++,
                    taskDefaultLabel.replaceAll("__TASKID__", new Long(creationCurrentElementId - 1).toString())
            );
        else if (taskType.equalsIgnoreCase("FTP-Delivery"))
            workflowProperties = new FTPDeliveryProperties(localPositionX, localPositionY,
                    creationCurrentElementId++,
                    taskDefaultLabel.replaceAll("__TASKID__", new Long(creationCurrentElementId - 1).toString())
            );
        else if (taskType.equalsIgnoreCase("Local-Copy"))
            workflowProperties = new LocalCopyProperties(localPositionX, localPositionY,
                    creationCurrentElementId++,
                    taskDefaultLabel.replaceAll("__TASKID__", new Long(creationCurrentElementId - 1).toString())
            );
        else if (taskType.equalsIgnoreCase("Post-On-Facebook"))
            workflowProperties = new PostOnFacebookProperties(localPositionX, localPositionY,
                    creationCurrentElementId++,
                    taskDefaultLabel.replaceAll("__TASKID__", new Long(creationCurrentElementId - 1).toString())
            );
        else if (taskType.equalsIgnoreCase("Post-On-YouTube"))
            workflowProperties = new PostOnYouTubeProperties(localPositionX, localPositionY,
                    creationCurrentElementId++,
                    taskDefaultLabel.replaceAll("__TASKID__", new Long(creationCurrentElementId - 1).toString())
            );
        else if (taskType.equalsIgnoreCase("Email-Notification"))
            workflowProperties = new EmailNotificationProperties(localPositionX, localPositionY,
                    creationCurrentElementId++,
                    taskDefaultLabel.replaceAll("__TASKID__", new Long(creationCurrentElementId - 1).toString())
            );
        else if (taskType.equalsIgnoreCase("HTTP-Callback"))
            workflowProperties = new HTTPCallbackProperties(localPositionX, localPositionY,
                    creationCurrentElementId++,
                    taskDefaultLabel.replaceAll("__TASKID__", new Long(creationCurrentElementId - 1).toString())
            );
        else if (taskType.equalsIgnoreCase("Face-Recognition"))
            workflowProperties = new FaceRecognitionProperties(localPositionX, localPositionY,
                    creationCurrentElementId++,
                    taskDefaultLabel.replaceAll("__TASKID__", new Long(creationCurrentElementId - 1).toString())
            );
        else if (taskType.equalsIgnoreCase("Face-Identification"))
            workflowProperties = new FaceIdentificationProperties(localPositionX, localPositionY,
                    creationCurrentElementId++,
                    taskDefaultLabel.replaceAll("__TASKID__", new Long(creationCurrentElementId - 1).toString())
            );
        else if (taskType.equalsIgnoreCase("Live-Recorder"))
            workflowProperties = new LiveRecorderProperties(localPositionX, localPositionY,
                    creationCurrentElementId++,
                    taskDefaultLabel.replaceAll("__TASKID__", new Long(creationCurrentElementId - 1).toString())
            );
        else if (taskType.equalsIgnoreCase("Change-File-Format"))
            workflowProperties = new ChangeFileFormatProperties(localPositionX, localPositionY,
                    creationCurrentElementId++,
                    taskDefaultLabel.replaceAll("__TASKID__", new Long(creationCurrentElementId - 1).toString())
            );
        else
            mLogger.error("Wrong taskType: " + taskType);

        Element taskElement = new Element(workflowProperties,
                workflowProperties.getPositionX(), workflowProperties.getPositionY());
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

        {

            DotEndPoint endPoint = new DotEndPoint(EndPointAnchor.LEFT);
            // endPoint.setScope("network");
            endPoint.setTarget(true);
            endPoint.setStyle("{fillStyle:'#98AFC7'}");
            endPoint.setHoverStyle("{fillStyle:'#5C738B'}");
            // endPoint.setMaxConnections(1); not working

            taskElement.addEndPoint(endPoint);
        }

        // onSuccess
        {
            // RectangleEndPoint endPoint = new RectangleEndPoint(EndPointAnchor.BOTTOM);
            ImageEndPoint endPoint = new ImageEndPoint(EndPointAnchor.BOTTOM, "/resources/img/onSuccess.png");
            // endPoint.setScope("network");
            endPoint.setSource(true);
            endPoint.setStyle("{fillStyle:'#00FF00'}");
            endPoint.setHoverStyle("{fillStyle:'#5C738B'}");

            taskElement.addEndPoint(endPoint);
        }

        // onError
        {
            // RectangleEndPoint endPoint = new RectangleEndPoint(EndPointAnchor.BOTTOM_LEFT);
            ImageEndPoint endPoint = new ImageEndPoint(EndPointAnchor.BOTTOM_LEFT, "/resources/img/onError.png");
            // endPoint.setScope("network");
            endPoint.setSource(true);
            endPoint.setStyle("{fillStyle:'#FF0000'}");
            endPoint.setHoverStyle("{fillStyle:'#5C738B'}");

            taskElement.addEndPoint(endPoint);
        }

        // onComplete
        {
            // RectangleEndPoint endPoint = new RectangleEndPoint(EndPointAnchor.BOTTOM_RIGHT);
            ImageEndPoint endPoint = new ImageEndPoint(EndPointAnchor.BOTTOM_RIGHT, "/resources/img/onComplete.png");
            // endPoint.setScope("network");
            endPoint.setSource(true);
            endPoint.setStyle("{fillStyle:'#98AFC7'}");
            endPoint.setHoverStyle("{fillStyle:'#5C738B'}");

            taskElement.addEndPoint(endPoint);
        }

        if (taskType.equalsIgnoreCase("GroupOfTasks"))
        {
            // to add the list of Tasks

            // RectangleEndPoint endPoint = new RectangleEndPoint(EndPointAnchor.RIGHT);
            ImageEndPoint endPoint = new ImageEndPoint(EndPointAnchor.RIGHT, "/resources/img/tasks.png");
            // endPoint.setScope("network");
            endPoint.setSource(true);
            endPoint.setStyle("{fillStyle:'#98AFC7'}");
            endPoint.setHoverStyle("{fillStyle:'#5C738B'}");

            taskElement.addEndPoint(endPoint);
        }

        model.addElement(taskElement);

        return taskElement;
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
            editingCurrentElementId = elementId;

            WorkflowProperties workflowProperties = (WorkflowProperties) element.getData();

            mLogger.info("onElementLinkClicked: workflowProperties.getType: " + workflowProperties.getType());

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
            else if (workflowProperties.getType().equalsIgnoreCase("Extract-Tracks"))
            {
                currentExtractTracksProperties = ((ExtractTracksProperties) workflowProperties).clone();

                String currentElementType = workflowProperties.getType();
                String mediaItemsSelectionMode = "multiple";
                boolean videoContentType = true;
                boolean audioContentType = true;
                boolean imageContentType = false;
                StringBuilder taskReferences = currentExtractTracksProperties.getStringBuilderTaskReferences();
                mediaItemsReferences.prepareToSelectMediaItems(currentElementType, mediaItemsSelectionMode,
                        videoContentType, audioContentType, imageContentType, taskReferences);
            }
            else if (workflowProperties.getType().equalsIgnoreCase("Encode"))
            {
                currentEncodeProperties = ((EncodeProperties) workflowProperties).clone();

                String currentElementType = workflowProperties.getType();
                String mediaItemsSelectionMode = "single";
                boolean videoContentType = true;
                boolean audioContentType = true;
                boolean imageContentType = true;
                StringBuilder taskReferences = currentEncodeProperties.getStringBuilderTaskReferences();
                mediaItemsReferences.prepareToSelectMediaItems(currentElementType, mediaItemsSelectionMode,
                        videoContentType, audioContentType, imageContentType, taskReferences);
            }
            else if (workflowProperties.getType().equalsIgnoreCase("Overlay-Image-On-Video"))
            {
                currentOverlayImageOnVideoProperties = ((OverlayImageOnVideoProperties) workflowProperties).clone();

                String currentElementType = workflowProperties.getType();
                String mediaItemsSelectionMode = "multiple";
                boolean videoContentType = true;
                boolean audioContentType = false;
                boolean imageContentType = true;
                StringBuilder taskReferences = currentOverlayImageOnVideoProperties.getStringBuilderTaskReferences();
                mediaItemsReferences.prepareToSelectMediaItems(currentElementType, mediaItemsSelectionMode,
                        videoContentType, audioContentType, imageContentType, taskReferences);
            }
            else if (workflowProperties.getType().equalsIgnoreCase("Overlay-Text-On-Video"))
            {
                currentOverlayTextOnVideoProperties = ((OverlayTextOnVideoProperties) workflowProperties).clone();

                String currentElementType = workflowProperties.getType();
                String mediaItemsSelectionMode = "single";
                boolean videoContentType = true;
                boolean audioContentType = false;
                boolean imageContentType = false;
                StringBuilder taskReferences = currentOverlayTextOnVideoProperties.getStringBuilderTaskReferences();
                mediaItemsReferences.prepareToSelectMediaItems(currentElementType, mediaItemsSelectionMode,
                        videoContentType, audioContentType, imageContentType, taskReferences);
            }
            else if (workflowProperties.getType().equalsIgnoreCase("Frame"))
            {
                currentFrameProperties = ((FrameProperties) workflowProperties).clone();

                String currentElementType = workflowProperties.getType();
                String mediaItemsSelectionMode = "single";
                boolean videoContentType = true;
                boolean audioContentType = false;
                boolean imageContentType = false;
                StringBuilder taskReferences = currentFrameProperties.getStringBuilderTaskReferences();
                mediaItemsReferences.prepareToSelectMediaItems(currentElementType, mediaItemsSelectionMode,
                        videoContentType, audioContentType, imageContentType, taskReferences);
            }
            else if (workflowProperties.getType().equalsIgnoreCase("Periodical-Frames"))
            {
                currentPeriodicalFramesProperties = ((PeriodicalFramesProperties) workflowProperties).clone();

                String currentElementType = workflowProperties.getType();
                String mediaItemsSelectionMode = "single";
                boolean videoContentType = true;
                boolean audioContentType = false;
                boolean imageContentType = false;
                StringBuilder taskReferences = currentPeriodicalFramesProperties.getStringBuilderTaskReferences();
                mediaItemsReferences.prepareToSelectMediaItems(currentElementType, mediaItemsSelectionMode,
                        videoContentType, audioContentType, imageContentType, taskReferences);
            }
            else if (workflowProperties.getType().equalsIgnoreCase("I-Frames"))
            {
                currentIFramesProperties = ((IFramesProperties) workflowProperties).clone();

                String currentElementType = workflowProperties.getType();
                String mediaItemsSelectionMode = "single";
                boolean videoContentType = true;
                boolean audioContentType = false;
                boolean imageContentType = false;
                StringBuilder taskReferences = currentIFramesProperties.getStringBuilderTaskReferences();
                mediaItemsReferences.prepareToSelectMediaItems(currentElementType, mediaItemsSelectionMode,
                        videoContentType, audioContentType, imageContentType, taskReferences);
            }
            else if (workflowProperties.getType().equalsIgnoreCase("Motion-JPEG-by-Periodical-Frames"))
            {
                currentMotionJPEGByPeriodicalFramesProperties = ((MotionJPEGByPeriodicalFramesProperties) workflowProperties).clone();

                String currentElementType = workflowProperties.getType();
                String mediaItemsSelectionMode = "single";
                boolean videoContentType = true;
                boolean audioContentType = false;
                boolean imageContentType = false;
                StringBuilder taskReferences = currentMotionJPEGByPeriodicalFramesProperties.getStringBuilderTaskReferences();
                mediaItemsReferences.prepareToSelectMediaItems(currentElementType, mediaItemsSelectionMode,
                        videoContentType, audioContentType, imageContentType, taskReferences);
            }
            else if (workflowProperties.getType().equalsIgnoreCase("Motion-JPEG-by-I-Frames"))
            {
                currentMotionJPEGByIFramesProperties = ((MotionJPEGByIFramesProperties) workflowProperties).clone();

                String currentElementType = workflowProperties.getType();
                String mediaItemsSelectionMode = "single";
                boolean videoContentType = true;
                boolean audioContentType = false;
                boolean imageContentType = false;
                StringBuilder taskReferences = currentMotionJPEGByIFramesProperties.getStringBuilderTaskReferences();
                mediaItemsReferences.prepareToSelectMediaItems(currentElementType, mediaItemsSelectionMode,
                        videoContentType, audioContentType, imageContentType, taskReferences);
            }
            else if (workflowProperties.getType().equalsIgnoreCase("Slideshow"))
            {
                currentSlideshowProperties = ((SlideshowProperties) workflowProperties).clone();

                String currentElementType = workflowProperties.getType();
                String mediaItemsSelectionMode = "multiple";
                boolean videoContentType = false;
                boolean audioContentType = false;
                boolean imageContentType = true;
                StringBuilder taskReferences = currentSlideshowProperties.getStringBuilderTaskReferences();
                mediaItemsReferences.prepareToSelectMediaItems(currentElementType, mediaItemsSelectionMode,
                        videoContentType, audioContentType, imageContentType, taskReferences);
            }
            else if (workflowProperties.getType().equalsIgnoreCase("FTP-Delivery"))
            {
                currentFTPDeliveryProperties = ((FTPDeliveryProperties) workflowProperties).clone();

                String currentElementType = workflowProperties.getType();
                String mediaItemsSelectionMode = "multiple";
                boolean videoContentType = true;
                boolean audioContentType = true;
                boolean imageContentType = true;
                StringBuilder taskReferences = currentFTPDeliveryProperties.getStringBuilderTaskReferences();
                mediaItemsReferences.prepareToSelectMediaItems(currentElementType, mediaItemsSelectionMode,
                        videoContentType, audioContentType, imageContentType, taskReferences);
            }
            else if (workflowProperties.getType().equalsIgnoreCase("Local-Copy"))
            {
                currentLocalCopyProperties = ((LocalCopyProperties) workflowProperties).clone();

                String currentElementType = workflowProperties.getType();
                String mediaItemsSelectionMode = "multiple";
                boolean videoContentType = true;
                boolean audioContentType = true;
                boolean imageContentType = true;
                StringBuilder taskReferences = currentLocalCopyProperties.getStringBuilderTaskReferences();
                mediaItemsReferences.prepareToSelectMediaItems(currentElementType, mediaItemsSelectionMode,
                        videoContentType, audioContentType, imageContentType, taskReferences);
            }
            else if (workflowProperties.getType().equalsIgnoreCase("Post-On-Facebook"))
            {
                currentPostOnFacebookProperties = ((PostOnFacebookProperties) workflowProperties).clone();

                String currentElementType = workflowProperties.getType();
                String mediaItemsSelectionMode = "multiple";
                boolean videoContentType = true;
                boolean audioContentType = false;
                boolean imageContentType = true;
                StringBuilder taskReferences = currentPostOnFacebookProperties.getStringBuilderTaskReferences();
                mediaItemsReferences.prepareToSelectMediaItems(currentElementType, mediaItemsSelectionMode,
                        videoContentType, audioContentType, imageContentType, taskReferences);
            }
            else if (workflowProperties.getType().equalsIgnoreCase("Post-On-YouTube"))
            {
                currentPostOnYouTubeProperties = ((PostOnYouTubeProperties) workflowProperties).clone();

                String currentElementType = workflowProperties.getType();
                String mediaItemsSelectionMode = "multiple";
                boolean videoContentType = true;
                boolean audioContentType = false;
                boolean imageContentType = false;
                StringBuilder taskReferences = currentPostOnYouTubeProperties.getStringBuilderTaskReferences();
                mediaItemsReferences.prepareToSelectMediaItems(currentElementType, mediaItemsSelectionMode,
                        videoContentType, audioContentType, imageContentType, taskReferences);
            }
            else if (workflowProperties.getType().equalsIgnoreCase("Email-Notification"))
                currentEmailNotificationProperties = ((EmailNotificationProperties) workflowProperties).clone();
            else if (workflowProperties.getType().equalsIgnoreCase("HTTP-Callback"))
            {
                currentHttpCallbackProperties = ((HTTPCallbackProperties) workflowProperties).clone();

                String currentElementType = workflowProperties.getType();
                String mediaItemsSelectionMode = "multiple";
                boolean videoContentType = true;
                boolean audioContentType = true;
                boolean imageContentType = true;
                StringBuilder taskReferences = currentHttpCallbackProperties.getStringBuilderTaskReferences();
                mediaItemsReferences.prepareToSelectMediaItems(currentElementType, mediaItemsSelectionMode,
                        videoContentType, audioContentType, imageContentType, taskReferences);
            }
            else if (workflowProperties.getType().equalsIgnoreCase("Face-Recognition"))
            {
                currentFaceRecognitionProperties = ((FaceRecognitionProperties) workflowProperties).clone();

                String currentElementType = workflowProperties.getType();
                String mediaItemsSelectionMode = "single";
                boolean videoContentType = true;
                boolean audioContentType = false;
                boolean imageContentType = false;
                StringBuilder taskReferences = currentFaceRecognitionProperties.getStringBuilderTaskReferences();
                mediaItemsReferences.prepareToSelectMediaItems(currentElementType, mediaItemsSelectionMode,
                        videoContentType, audioContentType, imageContentType, taskReferences);
            }
            else if (workflowProperties.getType().equalsIgnoreCase("Face-Identification"))
            {
                currentFaceIdentificationProperties = ((FaceIdentificationProperties) workflowProperties).clone();

                String currentElementType = workflowProperties.getType();
                String mediaItemsSelectionMode = "single";
                boolean videoContentType = true;
                boolean audioContentType = false;
                boolean imageContentType = false;
                StringBuilder taskReferences = currentFaceIdentificationProperties.getStringBuilderTaskReferences();
                mediaItemsReferences.prepareToSelectMediaItems(currentElementType, mediaItemsSelectionMode,
                        videoContentType, audioContentType, imageContentType, taskReferences);
            }
            else if (workflowProperties.getType().equalsIgnoreCase("Live-Recorder"))
                currentLiveRecorderProperties = ((LiveRecorderProperties) workflowProperties).clone();
            else if (workflowProperties.getType().equalsIgnoreCase("Change-File-Format"))
            {
                currentChangeFileFormatProperties = ((ChangeFileFormatProperties) workflowProperties).clone();

                String currentElementType = workflowProperties.getType();
                String mediaItemsSelectionMode = "multiple";
                boolean videoContentType = true;
                boolean audioContentType = true;
                boolean imageContentType = false;
                StringBuilder taskReferences = currentChangeFileFormatProperties.getStringBuilderTaskReferences();
                mediaItemsReferences.prepareToSelectMediaItems(currentElementType, mediaItemsSelectionMode,
                        videoContentType, audioContentType, imageContentType, taskReferences);
            }
            else
                mLogger.error("Wrong workflowProperties.getType(): " + workflowProperties.getType());
        }
        else
        {
            mLogger.error("onElementLinkClicked. Didn't find element for elementId " + elementId);
        }
    }

    public void saveTaskProperties()
    {
        Element element = model.findElement(editingCurrentElementId);
        if (element != null)
        {
            WorkflowProperties workflowProperties = (WorkflowProperties) element.getData();

            mLogger.info("saveTaskProperties: workflowProperties.getType: " + workflowProperties.getType());

            if (workflowProperties.getType().equalsIgnoreCase("Workflow"))
                workflowProperties.setData(currentWorkflowProperties);
            else if (workflowProperties.getType().equalsIgnoreCase("Add-Content"))
                ((AddContentProperties) workflowProperties).setData(currentAddContentProperties);
            else if (workflowProperties.getType().equalsIgnoreCase("GroupOfTasks"))
                ((GroupOfTasksProperties) workflowProperties).setData(currentGroupOfTasksProperties);
            else if (workflowProperties.getType().equalsIgnoreCase("Remove-Content"))
                ((RemoveContentProperties) workflowProperties).setData(currentRemoveContentProperties);
            else if (workflowProperties.getType().equalsIgnoreCase("Concat-Demuxer"))
                ((ConcatDemuxerProperties) workflowProperties).setData(currentConcatDemuxerProperties);
            else if (workflowProperties.getType().equalsIgnoreCase("Cut"))
                ((CutProperties) workflowProperties).setData(currentCutProperties);
            else if (workflowProperties.getType().equalsIgnoreCase("Extract-Tracks"))
                ((ExtractTracksProperties) workflowProperties).setData(currentExtractTracksProperties);
            else if (workflowProperties.getType().equalsIgnoreCase("Encode"))
                ((EncodeProperties) workflowProperties).setData(currentEncodeProperties);
            else if (workflowProperties.getType().equalsIgnoreCase("Overlay-Image-On-Video"))
                ((OverlayImageOnVideoProperties) workflowProperties).setData(currentOverlayImageOnVideoProperties);
            else if (workflowProperties.getType().equalsIgnoreCase("Overlay-Text-On-Video"))
                ((OverlayTextOnVideoProperties) workflowProperties).setData(currentOverlayTextOnVideoProperties);
            else if (workflowProperties.getType().equalsIgnoreCase("Frame"))
                ((FrameProperties) workflowProperties).setData(currentFrameProperties);
            else if (workflowProperties.getType().equalsIgnoreCase("Periodical-Frames"))
                ((PeriodicalFramesProperties) workflowProperties).setData(currentPeriodicalFramesProperties);
            else if (workflowProperties.getType().equalsIgnoreCase("I-Frames"))
                ((IFramesProperties) workflowProperties).setData(currentIFramesProperties);
            else if (workflowProperties.getType().equalsIgnoreCase("Motion-JPEG-by-Periodical-Frames"))
                ((MotionJPEGByPeriodicalFramesProperties) workflowProperties).setData(currentMotionJPEGByPeriodicalFramesProperties);
            else if (workflowProperties.getType().equalsIgnoreCase("Motion-JPEG-by-I-Frames"))
                ((MotionJPEGByIFramesProperties) workflowProperties).setData(currentMotionJPEGByIFramesProperties);
            else if (workflowProperties.getType().equalsIgnoreCase("Slideshow"))
                ((SlideshowProperties) workflowProperties).setData(currentSlideshowProperties);
            else if (workflowProperties.getType().equalsIgnoreCase("FTP-Delivery"))
                ((FTPDeliveryProperties) workflowProperties).setData(currentFTPDeliveryProperties);
            else if (workflowProperties.getType().equalsIgnoreCase("Local-Copy"))
                ((LocalCopyProperties) workflowProperties).setData(currentLocalCopyProperties);
            else if (workflowProperties.getType().equalsIgnoreCase("Post-On-Facebook"))
                ((PostOnFacebookProperties) workflowProperties).setData(currentPostOnFacebookProperties);
            else if (workflowProperties.getType().equalsIgnoreCase("Post-On-YouTube"))
                ((PostOnYouTubeProperties) workflowProperties).setData(currentPostOnYouTubeProperties);
            else if (workflowProperties.getType().equalsIgnoreCase("Email-Notification"))
                ((EmailNotificationProperties) workflowProperties).setData(currentEmailNotificationProperties);
            else if (workflowProperties.getType().equalsIgnoreCase("HTTP-Callback"))
                ((HTTPCallbackProperties) workflowProperties).setData(currentHttpCallbackProperties);
            else if (workflowProperties.getType().equalsIgnoreCase("Face-Recognition"))
                ((FaceRecognitionProperties) workflowProperties).setData(currentFaceRecognitionProperties);
            else if (workflowProperties.getType().equalsIgnoreCase("Face-Identification"))
                ((FaceIdentificationProperties) workflowProperties).setData(currentFaceIdentificationProperties);
            else if (workflowProperties.getType().equalsIgnoreCase("Live-Recorder"))
                ((LiveRecorderProperties) workflowProperties).setData(currentLiveRecorderProperties);
            else if (workflowProperties.getType().equalsIgnoreCase("Change-File-Format"))
                ((ChangeFileFormatProperties) workflowProperties).setData(currentChangeFileFormatProperties);
            else
                mLogger.error("Wrong workflowProperties.getType(): " + workflowProperties.getType());

            buildWorkflowElementJson();
        }
        else
        {
            mLogger.error("saveTaskProperties. Didn't find element for editingCurrentElementId " + editingCurrentElementId);
        }
    }

    public void removeDiagramElement()
    {
        try {
            if (removingCurrentElementId != null)
            {
                mLogger.info("Received removeDiagramElement, removingCurrentElementId: " + removingCurrentElementId);

                Element elementToBeRemoved = model.findElement(removingCurrentElementId.toString());
                if (elementToBeRemoved != null)
                {
                    WorkflowProperties workflowPropertiesToBeRemoved = (WorkflowProperties) elementToBeRemoved.getData();

                    if (workflowPropertiesToBeRemoved.getType().equalsIgnoreCase("Workflow"))
                    {
                        mLogger.error("removeDiagramElement. Workflow cannot be removed, removingCurrentElementId " + removingCurrentElementId);

                        FacesMessage msg = new FacesMessage(FacesMessage.SEVERITY_INFO, "Deleting",
                                "Workflow cannot be removed");
                        FacesContext.getCurrentInstance().addMessage(null, msg);

                        return;
                    }

                    // look for the connections of all the other elements and,
                    // for the one's having elementToBeRemoved as target:
                    //  - remove the WorkflowProperties from the list
                    //  - disconnect the connection
                    {
                        for (Element element: model.getElements())
                        {
                            WorkflowProperties sourceWorkflowProperties = (WorkflowProperties) element.getData();

                            if (sourceWorkflowProperties.getElementId() == workflowPropertiesToBeRemoved.getElementId())
                                continue;

                            for (WorkflowProperties onWorkflowProperties: sourceWorkflowProperties.getOnSuccessChildren())
                            {
                                if (onWorkflowProperties.getElementId() == workflowPropertiesToBeRemoved.getElementId())
                                {
                                    mLogger.info("Removing connection from "
                                            + sourceWorkflowProperties.toString()
                                            + " to " + onWorkflowProperties.toString());

                                    sourceWorkflowProperties.getOnSuccessChildren().remove(onWorkflowProperties);

                                    Connection connection = getConnection(
                                            getEndPoint(element, EndPointAnchor.BOTTOM),
                                            getEndPoint(elementToBeRemoved, EndPointAnchor.TOP));
                                    if (connection == null)
                                        connection = getConnection(
                                                getEndPoint(element, EndPointAnchor.BOTTOM),
                                                getEndPoint(elementToBeRemoved, EndPointAnchor.LEFT));
                                    if (connection == null)
                                    {
                                        mLogger.error("It shall never happen");

                                        return;
                                    }

                                    model.disconnect(connection);

                                    break;
                                }
                            }

                            for (WorkflowProperties onWorkflowProperties: sourceWorkflowProperties.getOnErrorChildren())
                            {
                                if (onWorkflowProperties.getElementId() == workflowPropertiesToBeRemoved.getElementId())
                                {
                                    mLogger.info("Removing connection from "
                                            + sourceWorkflowProperties.toString()
                                            + " to " + onWorkflowProperties.toString());

                                    sourceWorkflowProperties.getOnErrorChildren().remove(onWorkflowProperties);

                                    Connection connection = getConnection(
                                            getEndPoint(element, EndPointAnchor.BOTTOM_LEFT),
                                            getEndPoint(elementToBeRemoved, EndPointAnchor.TOP));
                                    if (connection == null)
                                        connection = getConnection(
                                                getEndPoint(element, EndPointAnchor.BOTTOM_LEFT),
                                                getEndPoint(elementToBeRemoved, EndPointAnchor.LEFT));
                                    if (connection == null)
                                    {
                                        mLogger.error("It shall never happen");

                                        return;
                                    }

                                    model.disconnect(connection);

                                    break;
                                }
                            }

                            for (WorkflowProperties onWorkflowProperties: sourceWorkflowProperties.getOnCompleteChildren())
                            {
                                if (onWorkflowProperties.getElementId() == workflowPropertiesToBeRemoved.getElementId())
                                {
                                    mLogger.info("Removing connection from "
                                            + sourceWorkflowProperties.toString()
                                            + " to " + onWorkflowProperties.toString());

                                    sourceWorkflowProperties.getOnCompleteChildren().remove(onWorkflowProperties);

                                    Connection connection = getConnection(
                                            getEndPoint(element, EndPointAnchor.BOTTOM_RIGHT),
                                            getEndPoint(elementToBeRemoved, EndPointAnchor.TOP));
                                    if (connection == null)
                                        connection = getConnection(
                                                getEndPoint(element, EndPointAnchor.BOTTOM_RIGHT),
                                                getEndPoint(elementToBeRemoved, EndPointAnchor.LEFT));
                                    if (connection == null)
                                    {
                                        mLogger.error("It shall never happen");

                                        return;
                                    }

                                    model.disconnect(connection);

                                    break;
                                }
                            }

                            if (sourceWorkflowProperties.getType().equalsIgnoreCase("GroupOfTasks"))
                            {
                                GroupOfTasksProperties sourceGroupOfTasksProperties = (GroupOfTasksProperties) sourceWorkflowProperties;
                                for (WorkflowProperties onWorkflowProperties: sourceGroupOfTasksProperties.getTasks())
                                {
                                    if (onWorkflowProperties.getElementId() == workflowPropertiesToBeRemoved.getElementId())
                                    {
                                        mLogger.info("Removing connection from "
                                                + sourceWorkflowProperties.toString()
                                                + " to " + onWorkflowProperties.toString());

                                        sourceGroupOfTasksProperties.getTasks().remove(onWorkflowProperties);

                                        Connection connection = getConnection(
                                                getEndPoint(element, EndPointAnchor.RIGHT),
                                                getEndPoint(elementToBeRemoved, EndPointAnchor.TOP));
                                        if (connection == null)
                                            connection = getConnection(
                                                    getEndPoint(element, EndPointAnchor.RIGHT),
                                                    getEndPoint(elementToBeRemoved, EndPointAnchor.LEFT));
                                        if (connection == null)
                                        {
                                            mLogger.error("It shall never happen");

                                            return;
                                        }

                                        model.disconnect(connection);

                                        break;
                                    }
                                }
                            }
                        }
                    }

                    mLogger.info("Removing element. removingCurrentElementId: " + removingCurrentElementId);
                    model.removeElement(elementToBeRemoved);
                }
                else
                {
                    mLogger.error("removeDiagramElement. Didn't find element for removingCurrentElementId " + removingCurrentElementId);

                    FacesMessage msg = new FacesMessage(FacesMessage.SEVERITY_INFO, "Deleting",
                            "No Diagram Element found having the " + removingCurrentElementId + " id");
                    FacesContext.getCurrentInstance().addMessage(null, msg);

                    return;
                }
            }
            else
            {
                mLogger.error("removeDiagramElement. removingCurrentElementId is null");
            }
        }
        catch (Exception e)
        {
            mLogger.error("Exception: " + e);
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

            // WorkflowProperties workflowProperties = (WorkflowProperties) element.getData();
            // workflowProperties.setPositionX(elementX);
            // workflowProperties.setPositionY(elementY);
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
            manageNewConnection(event.getSourceElement(), event.getSourceEndPoint(),
                    event.getTargetElement(), event.getTargetEndPoint());
        }
        catch (Exception e)
        {
            mLogger.error("Exception: " + e);
        }
    }

    private void manageNewConnection(Element sourceElement, EndPoint sourceEndPoint,
                                     Element targetElement, EndPoint targetEndPoint)
    {
        try
        {
            mLogger.info("onConnect");

            WorkflowProperties sourceWorkflowProperties = (WorkflowProperties) sourceElement.getData();
            WorkflowProperties targetWorkflowProperties = (WorkflowProperties) targetElement.getData();

            if (!isMainTypeConnectionAllowed(sourceWorkflowProperties, targetWorkflowProperties))
            {
                mLogger.info("!isMainTypeConnectionAllowed");
                Connection connection = getConnection(sourceEndPoint, targetEndPoint);
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
            else if (!isConnectionNumberAllowed(sourceEndPoint, sourceWorkflowProperties))
            {
                mLogger.info("!isConnectionNumberAllowed");
                Connection connection = getConnection(sourceEndPoint, targetEndPoint);
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
                Connection connection = getConnection(sourceEndPoint, targetEndPoint);

                mLogger.info("LabelOverlay, connection: " + connection);
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
                        if (sourceEndPoint.getAnchor() == EndPointAnchor.BOTTOM)
                            connection.getOverlays().add(new LabelOverlay("onSuccess", "connection-label", 0.5));
                        else if (sourceEndPoint.getAnchor() == EndPointAnchor.BOTTOM_RIGHT)
                            connection.getOverlays().add(new LabelOverlay("onComplete", "connection-label", 0.5));
                        else if (sourceEndPoint.getAnchor() == EndPointAnchor.BOTTOM_LEFT)
                            connection.getOverlays().add(new LabelOverlay("onError", "connection-label", 0.5));
                    }
                    else if (sourceWorkflowProperties.getMainType().equalsIgnoreCase("GroupOfTasks")
                            && targetWorkflowProperties.getMainType().equalsIgnoreCase("Task"))
                    {
                        if (sourceEndPoint.getAnchor() == EndPointAnchor.BOTTOM)
                            connection.getOverlays().add(new LabelOverlay("onSuccess", "connection-label", 0.5));
                        else if (sourceEndPoint.getAnchor() == EndPointAnchor.BOTTOM_RIGHT)
                            connection.getOverlays().add(new LabelOverlay("onComplete", "connection-label", 0.5));
                        else if (sourceEndPoint.getAnchor() == EndPointAnchor.BOTTOM_LEFT)
                            connection.getOverlays().add(new LabelOverlay("onError", "connection-label", 0.5));
                        else if (sourceEndPoint.getAnchor() == EndPointAnchor.RIGHT)
                        {
                            GroupOfTasksProperties groupOfTasksProperties = (GroupOfTasksProperties) sourceWorkflowProperties;

                            // The task will be added few statements below
                            connection.getOverlays().add(new LabelOverlay(
                                    "Task nr. " + (groupOfTasksProperties.getTasks().size() + 1),
                                    "connection-label", 0.5));
                        }
                    }
                }
            }

            // update data children
            {
                mLogger.info("update data children. sourceEndPoint.getAnchor(): " + sourceEndPoint.getAnchor());
                if (sourceEndPoint.getAnchor() == EndPointAnchor.BOTTOM)
                    sourceWorkflowProperties.getOnSuccessChildren().add(targetWorkflowProperties);
                else if (sourceEndPoint.getAnchor() == EndPointAnchor.BOTTOM_LEFT)
                    sourceWorkflowProperties.getOnErrorChildren().add(targetWorkflowProperties);
                else if (sourceEndPoint.getAnchor() == EndPointAnchor.BOTTOM_RIGHT)
                    sourceWorkflowProperties.getOnCompleteChildren().add(targetWorkflowProperties);
                else if (sourceEndPoint.getAnchor() == EndPointAnchor.RIGHT)
                {
                    GroupOfTasksProperties groupOfTasksProperties = (GroupOfTasksProperties) sourceWorkflowProperties;

                    groupOfTasksProperties.getTasks().add(targetWorkflowProperties);
                }
            }

            buildWorkflowElementJson();

            FacesMessage msg = new FacesMessage(FacesMessage.SEVERITY_INFO, "Connected",
                    "From " + StringEscapeUtils.escapeHtml(sourceWorkflowProperties.toString())
                            + " To " + StringEscapeUtils.escapeHtml(targetWorkflowProperties.toString()));

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
                sourceWorkflowProperties.getOnErrorChildren().remove(targetWorkflowProperties);
            else if (sourceEndPont.getAnchor() == EndPointAnchor.BOTTOM_RIGHT)
                sourceWorkflowProperties.getOnCompleteChildren().remove(targetWorkflowProperties);
            else if (sourceEndPont.getAnchor() == EndPointAnchor.RIGHT)
            {
                GroupOfTasksProperties groupOfTasksProperties = (GroupOfTasksProperties) sourceWorkflowProperties;

                groupOfTasksProperties.getTasks().remove(targetWorkflowProperties);

                // if the Task nr. 2 is removed, we have to change the label of the connection bigger of the Task nr. 2.
                // For example, the current Task nr. 3 has to be labelled as Task nr. 2 and so on...

                relabelGroupOfTasksConnections(event.getSourceElement(), sourceEndPont);
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
            sourceOriginalWorkflowProperties.getOnErrorChildren().remove(targetOriginalWorkflowProperties);
            // commented because the onConnect is generated too and the add is done there
            // sourceOriginalWorkflowProperties.getOnErrorChildren().add(targetNewWorkflowProperties);
        }
        else if (sourceOriginalEndPont.getAnchor() == EndPointAnchor.BOTTOM_RIGHT)
        {
            sourceOriginalWorkflowProperties.getOnCompleteChildren().remove(targetOriginalWorkflowProperties);
            // commented because the onConnect is generated too and the add is done there
            // sourceOriginalWorkflowProperties.getOnCompleteChildren().add(targetNewWorkflowProperties);
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
                    && sourceWorkflowProperties.getOnErrorChildren().size() == 0)
                isConnectionNumberAllowed = true;
            else if (sourceEndPont.getAnchor() == EndPointAnchor.BOTTOM_RIGHT
                    && sourceWorkflowProperties.getOnCompleteChildren().size() == 0)
                isConnectionNumberAllowed = true;
        }
        else if (sourceWorkflowProperties.getMainType().equalsIgnoreCase("GroupOfTasks")
        )
        {
            if (sourceEndPont.getAnchor() == EndPointAnchor.BOTTOM
                    && sourceWorkflowProperties.getOnSuccessChildren().size() == 0)
                isConnectionNumberAllowed = true;
            else if (sourceEndPont.getAnchor() == EndPointAnchor.BOTTOM_LEFT
                    && sourceWorkflowProperties.getOnErrorChildren().size() == 0)
                isConnectionNumberAllowed = true;
            else if (sourceEndPont.getAnchor() == EndPointAnchor.BOTTOM_RIGHT
                    && sourceWorkflowProperties.getOnCompleteChildren().size() == 0)
                isConnectionNumberAllowed = true;
            else if (sourceEndPont.getAnchor() == EndPointAnchor.RIGHT)
                isConnectionNumberAllowed = true;
        }

        return isConnectionNumberAllowed;
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

    private void relabelGroupOfTasksConnections(Element groupOfTasksElement, EndPoint rightEndPoint)
    {
        try {
            List<Connection> connectionListToBeDisconnected = new ArrayList<>();

            for (Connection connection: model.getConnections())
            {
                if (connection.getSource() == rightEndPoint)
                    connectionListToBeDisconnected.add(connection);
            }

            for (Connection connection: connectionListToBeDisconnected)
                model.disconnect(connection);

            GroupOfTasksProperties groupOfTasksProperties = (GroupOfTasksProperties) groupOfTasksElement.getData();

            for (int taskIndex = 0; taskIndex < groupOfTasksProperties.getTasks().size(); taskIndex++)
            {
                WorkflowProperties taskWorkflowProperties = groupOfTasksProperties.getTasks().get(taskIndex);

                Element taskElement = model.findElement(new Integer(taskWorkflowProperties.getElementId()).toString());
                if (taskElement != null)
                {
                    EndPoint leftEndPoint = getEndPoint(taskElement, EndPointAnchor.LEFT);

                    Connection newConnection = new Connection(rightEndPoint, leftEndPoint);
                    model.connect(newConnection);
                    newConnection.getOverlays().add(new LabelOverlay(
                            "Task nr. " + (taskIndex + 1),
                            "connection-label", 0.5));
                }
            }
        }
        catch (Exception e)
        {
            mLogger.info("Exception: " + e);
        }
    }

    private EndPoint getEndPoint(Element element, EndPointAnchor endPointAnchor)
            throws Exception
    {
        EndPoint endPointToBeReturned = null;

        for (EndPoint endPoint: element.getEndPoints())
        {
            if (endPoint.getAnchor() == endPointAnchor)
            {
                endPointToBeReturned = endPoint;

                break;
            }
        }

        return endPointToBeReturned;
    }

    public void buildWorkflowElementJson()
    {
        try {
            ingestionData.getWorkflowIssueList().clear();
            ingestionData.getPushContentList().clear();

            WorkflowProperties workflowProperties = (WorkflowProperties) workflowElement.getData();
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

                mLogger.info("pushContent.getBinaryPathName: " + pushContent.getBinaryPathName());
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

    public void fillWorkflowNames()
    {
        try
        {
            Long userKey = SessionUtils.getUserProfile().getUserKey();

            File workflowEditorPathDirectory = new File(storageWorkflowEditorPath + userKey);
            if (workflowEditorPathDirectory.exists())
            {
                mLogger.info(workflowEditorPathDirectory + " already exists");
            }
            else if (workflowEditorPathDirectory.mkdirs())
            {
                mLogger.error(workflowEditorPathDirectory + " was created");
            }

            workflowEditorNames.clear();

            File[] filesList = workflowEditorPathDirectory.listFiles();
            for (File file : filesList)
            {
                if (file.isFile() && file.getName().endsWith(".wfm"))
                {
                    workflowEditorNames.add(file.getName().substring(0, file.getName().indexOf(".wfm")));
                }
            }
        }
        catch (Exception e)
        {
            mLogger.error("fillWorkflowNames exception: " + e);
        }
    }

    public void saveModelToFile()
    {
        ObjectOutputStream objectOutputStream = null;
        try
        {
            if (currentWorkflowEditorName == null || currentWorkflowEditorName.equalsIgnoreCase(""))
            {
                mLogger.error("Editor name is not valid. currentWorkflowEditorName: " + currentWorkflowEditorName);

                FacesMessage message = new FacesMessage(FacesMessage.SEVERITY_ERROR, "Workfow Editor",
                        "Failed to save the Workflow");
                FacesContext.getCurrentInstance().addMessage(null, message);

                return;
            }

            Long userKey = SessionUtils.getUserProfile().getUserKey();

            File workflowEditorPathFile = new File(storageWorkflowEditorPath + userKey);
            if (workflowEditorPathFile.exists())
            {
                mLogger.info(workflowEditorPathFile + " already exists");
            }
            else if (workflowEditorPathFile.mkdirs())
            {
                mLogger.error(workflowEditorPathFile + " was created");
            }

            String workflowEditorPathName = storageWorkflowEditorPath + userKey + "/" + currentWorkflowEditorName + ".wfm";
            mLogger.info("Save model to " + workflowEditorPathName);
            FileOutputStream workflowEditorFile = new FileOutputStream(workflowEditorPathName);
            objectOutputStream = new ObjectOutputStream(workflowEditorFile);

            objectOutputStream.writeObject(model);

            mLogger.info("The Model was succesfully written to a file");

            FacesMessage message = new FacesMessage(FacesMessage.SEVERITY_ERROR, "Workfow Editor",
                    "The Workflow was successful saved");
            FacesContext.getCurrentInstance().addMessage(null, message);
        }
        catch (Exception ex)
        {
            mLogger.error("saveModelToFile exception: " + ex);

            FacesMessage message = new FacesMessage(FacesMessage.SEVERITY_ERROR, "Workfow Editor",
                    "Failed to save the Workflow");
            FacesContext.getCurrentInstance().addMessage(null, message);
        }
        finally
        {
            try {
                if(objectOutputStream != null)
                {
                    objectOutputStream.close();
                }
            }
            catch (Exception e)
            {
                mLogger.error("saveModelToFile exception: " + e);
            }
        }
    }

    public void loadModelFromFile()
    {
        ObjectInputStream objectinputstream = null;
        try
        {
            if (currentWorkflowEditorName == null || currentWorkflowEditorName.equalsIgnoreCase(""))
            {
                mLogger.error("Editor name is not valid. currentWorkflowEditorName: " + currentWorkflowEditorName);

                FacesMessage message = new FacesMessage(FacesMessage.SEVERITY_ERROR, "Workfow Editor",
                        "Failed to load the Workflow");
                FacesContext.getCurrentInstance().addMessage(null, message);

                return;
            }

            Long userKey = SessionUtils.getUserProfile().getUserKey();

            File workflowEditorPathFile = new File(storageWorkflowEditorPath + userKey);
            if (workflowEditorPathFile.exists())
            {
                mLogger.info(workflowEditorPathFile + " already exists");
            }
            else if (workflowEditorPathFile.mkdirs())
            {
                mLogger.error(workflowEditorPathFile + " was created");
            }

            String workflowEditorPathName = storageWorkflowEditorPath + userKey + "/" + currentWorkflowEditorName + ".wfm";

            FileInputStream fileInputStream = new FileInputStream(workflowEditorPathName);
            objectinputstream = new ObjectInputStream(fileInputStream);

            // clean current model before to re-assign
            {
                while (model.getConnections().size() > 0)
                    model.disconnect(model.getConnections().get(0));

                while (model.getElements().size() > 0)
                    model.removeElement(model.getElements().get(0));
            }

            model = (DefaultDiagramModel) objectinputstream.readObject();

            mLogger.info("The Model was succesfully read from a file"
                    + ", model.getElements().size: " + model.getElements().size()
            );

            creationCurrentElementId = -1;
            for (Element element: model.getElements())
            {
                int elementId = Integer.parseInt(element.getId());
                if (elementId > creationCurrentElementId)
                    creationCurrentElementId = elementId;

                WorkflowProperties workflowProperties = (WorkflowProperties) element.getData();

                if (workflowProperties.getType().equalsIgnoreCase("Workflow"))
                {
                    workflowElement = element;

                    // mLogger.info("workflowProperties.getOnSuccessChildren().size: " + workflowProperties.getOnSuccessChildren().size());
                }
            }
            creationCurrentElementId++;

            /*
            for (Element element: model.getElements())
            {
                WorkflowProperties workflowProperties = (WorkflowProperties) element.getData();
                mLogger.info("Type: " + workflowProperties.getType());
            }
            */

            buildWorkflowElementJson();
        }
        catch (Exception e)
        {
            mLogger.error("loadModelFromFile exception: " + e);
        }
        finally
        {
            try {
                if(objectinputstream != null)
                {
                    objectinputstream.close();
                }
            }
            catch (Exception e)
            {
                mLogger.error("saveModelToFile exception: " + e);
            }
        }
    }

    public void removeModelFile()
    {
        try
        {
            if (currentWorkflowEditorName == null || currentWorkflowEditorName.equalsIgnoreCase(""))
            {
                mLogger.error("Editor name is not valid. currentWorkflowEditorName: " + currentWorkflowEditorName);

                FacesMessage message = new FacesMessage(FacesMessage.SEVERITY_ERROR, "Workfow Editor",
                        "Failed to remove the Workflow");
                FacesContext.getCurrentInstance().addMessage(null, message);

                return;
            }

            Long userKey = SessionUtils.getUserProfile().getUserKey();

            File workflowEditorPathFile = new File(storageWorkflowEditorPath + userKey);
            if (workflowEditorPathFile.exists())
            {
                mLogger.info(workflowEditorPathFile + " already exists");
            }
            else if (workflowEditorPathFile.mkdirs())
            {
                mLogger.error(workflowEditorPathFile + " was created");
            }

            String workflowEditorPathName = storageWorkflowEditorPath + userKey + "/" + currentWorkflowEditorName + ".wfm";

            File workflowEditorFile = new File(workflowEditorPathName);
            workflowEditorFile.delete();

            mLogger.info("The Model was succesfully removed");

            FacesMessage message = new FacesMessage(FacesMessage.SEVERITY_ERROR, "Workfow Editor",
                    "The Workflow was successful removed");
            FacesContext.getCurrentInstance().addMessage(null, message);

            fillWorkflowNames();
        }
        catch (Exception e)
        {
            mLogger.error("removeModelFile exception: " + e);
        }
    }

    private void buildModelByIngestionRootKey(long ingestionRootKey)
    {
        String metaDataContent;
        try
        {
            Long userKey = SessionUtils.getUserProfile().getUserKey();
            String apiKey = SessionUtils.getCurrentWorkspaceDetails().getApiKey();

            String username = userKey.toString();
            String password = apiKey;

            CatraMMS catraMMS = new CatraMMS();

            metaDataContent = catraMMS.getMetaDataContent(username, password, ingestionRootKey);
        }
        catch (Exception e)
        {
            String errorMessage = "Exception: " + e;
            mLogger.error(errorMessage);

            FacesMessage message = new FacesMessage(FacesMessage.SEVERITY_ERROR, "Workflow Editor",
                    "Retrieve metadata failed: " + errorMessage);
            FacesContext.getCurrentInstance().addMessage(null, message);

            return;
        }

        try
        {
            buildModelByMetaDataContent(metaDataContent);
        }
        catch (Exception e)
        {
            String errorMessage = "buildModelByMetaDataContent. Exception: " + e;
            mLogger.error(errorMessage);

            FacesMessage message = new FacesMessage(FacesMessage.SEVERITY_ERROR, "Workflow Editor",
                    "Parsing metadata failed: " + errorMessage);
            FacesContext.getCurrentInstance().addMessage(null, message);

            return;
        }
    }

    private void buildModelByMetaDataContent(String metaDataContent)
    {
        try
        {
            JSONObject joMetaData = new JSONObject(metaDataContent);

            WorkflowProperties workflowProperties = (WorkflowProperties) workflowElement.getData();
            workflowProperties.setData(joMetaData);

            Vector<Integer> currentRowForAColumn = new Vector<>();
            currentRowForAColumn.add(0);  // for the column 0 (index of the vector), current row is 0
            currentRowForAColumn.add(1);  // for the column 1 (index of the vector), current row is 1 (we already have the Workflow root)
            currentRowForAColumn.add(0);  // for the column 2 (index of the vector), current row is 0
            currentRowForAColumn.add(0);  // for the column 3 (index of the vector), current row is 0
            currentRowForAColumn.add(0);  // for the column 4 (index of the vector), current row is 0
            currentRowForAColumn.add(0);  // for the column 5 (index of the vector), current row is 0

            // on success
            Integer currentColumn = 1;

            JSONObject joTask = joMetaData.getJSONObject("Task");

            if (joTask.getString("Type").equalsIgnoreCase("GroupOfTasks"))
                buildModel_GroupOfTasks(joTask, currentColumn, currentRowForAColumn,
                        workflowElement, EndPointAnchor.BOTTOM);
            else
                buildModel_Task(joTask, currentColumn, currentRowForAColumn,
                        workflowElement, EndPointAnchor.BOTTOM);
        }
        catch (Exception e)
        {
            String errorMessage = "Exception: " + e;
            mLogger.error(errorMessage);

            FacesMessage message = new FacesMessage(FacesMessage.SEVERITY_ERROR, "Workflow Editor",
                    "Parsing metadata failed: " + errorMessage);
            FacesContext.getCurrentInstance().addMessage(null, message);

            return;
        }
    }

    private void buildModel_GroupOfTasks(JSONObject joGroupOfTasks,
                                         Integer currentColumn,
                                         Vector<Integer> currentRowForAColumn,
                                         Element parentElement, EndPointAnchor parentEndPointAnchor)
            throws Exception
    {
        try
        {
            Element groupOfTasksElement = addTask(joGroupOfTasks.getString("Type"),
                    new Integer(workflow_firstX + (currentColumn * workflow_stepX)).toString() + "em",
                    new Integer(workflow_firstY + (currentRowForAColumn.get(currentColumn) * workflow_stepY)) + "em");
            currentRowForAColumn.set(currentColumn, currentRowForAColumn.get(currentColumn) + 1);

            {
                EndPoint sourceEndPoint = getEndPoint(parentElement, parentEndPointAnchor);
                EndPoint targetEndPoint;
                if (parentEndPointAnchor == EndPointAnchor.RIGHT)   // GroupOfTasks
                    targetEndPoint = getEndPoint(groupOfTasksElement, EndPointAnchor.LEFT);
                else    // Task
                    targetEndPoint = getEndPoint(groupOfTasksElement, EndPointAnchor.TOP);
                model.connect(new Connection(sourceEndPoint, targetEndPoint));
                manageNewConnection(parentElement, sourceEndPoint, groupOfTasksElement, targetEndPoint);
            }

            GroupOfTasksProperties workflowProperties = (GroupOfTasksProperties) groupOfTasksElement.getData();
            workflowProperties.setData(joGroupOfTasks);

            mLogger.info("buildModel_GroupOfTasks"
                    + ", workflowProperties.getElementId: " + workflowProperties.getElementId()
                    + ", workflowProperties.getLabel: " + workflowProperties.getLabel()
                    + ", currentColumn: " + currentColumn
                    + ", currentRowForAColumn.toString: " + currentRowForAColumn.toString()
            );

            JSONObject joParameters = joGroupOfTasks.getJSONObject("Parameters");

            JSONArray jaTasks = joParameters.getJSONArray("Tasks");
            for (int taskIndex = 0; taskIndex < jaTasks.length(); taskIndex++)
            {
                JSONObject joTask = jaTasks.getJSONObject(taskIndex);
                mLogger.info("buildModel_GroupOfTasks"
                        + ", Task number: " + taskIndex
                        + ", Task type: " + joTask.getString("Type")
                );
                if (joTask.getString("Type").equalsIgnoreCase("GroupOfTasks"))
                    buildModel_GroupOfTasks(joTask,
                            currentColumn + 2,    // +1: onComplete, +2: tasks
                            currentRowForAColumn,
                            groupOfTasksElement, EndPointAnchor.RIGHT);
                else
                    buildModel_Task(joTask,
                            currentColumn + 2,    // +1: onComplete, +2: tasks
                            currentRowForAColumn,
                            groupOfTasksElement, EndPointAnchor.RIGHT);
            }

            if (joGroupOfTasks.has("OnSuccess"))
            {
                JSONObject joOnSuccess = joGroupOfTasks.getJSONObject("OnSuccess");
                if (joOnSuccess.has("Task"))
                {
                    JSONObject joOnSuccessTask = joOnSuccess.getJSONObject("Task");
                    if (joOnSuccessTask.getString("Type").equalsIgnoreCase("GroupOfTasks"))
                        buildModel_GroupOfTasks(joOnSuccessTask,
                                currentColumn, currentRowForAColumn,
                                groupOfTasksElement, EndPointAnchor.BOTTOM);
                    else
                        buildModel_Task(joOnSuccessTask,
                                currentColumn, currentRowForAColumn,
                                groupOfTasksElement, EndPointAnchor.BOTTOM);
                }
            }

            if (joGroupOfTasks.has("OnError"))
            {
                JSONObject joOnError = joGroupOfTasks.getJSONObject("OnError");
                if (joOnError.has("Task"))
                {
                    JSONObject joOnErrorTask = joOnError.getJSONObject("Task");
                    if (joOnErrorTask.getString("Type").equalsIgnoreCase("GroupOfTasks"))
                        buildModel_GroupOfTasks(joOnErrorTask,
                                currentColumn - 1, currentRowForAColumn,
                                groupOfTasksElement, EndPointAnchor.BOTTOM_LEFT);
                    else
                        buildModel_Task(joOnErrorTask,
                                currentColumn - 1, currentRowForAColumn,
                                groupOfTasksElement, EndPointAnchor.BOTTOM_LEFT);
                }
            }

            if (joGroupOfTasks.has("OnComplete"))
            {
                JSONObject joOnComplete = joGroupOfTasks.getJSONObject("OnComplete");
                if (joOnComplete.has("Task"))
                {
                    JSONObject joOnCompleteTask = joOnComplete.getJSONObject("Task");
                    if (joOnCompleteTask.getString("Type").equalsIgnoreCase("GroupOfTasks"))
                        buildModel_GroupOfTasks(joOnCompleteTask,
                                currentColumn + 1, currentRowForAColumn,
                                groupOfTasksElement, EndPointAnchor.BOTTOM_RIGHT);
                    else
                        buildModel_Task(joOnCompleteTask, currentColumn + 1,
                                currentRowForAColumn,
                                groupOfTasksElement, EndPointAnchor.BOTTOM_RIGHT);
                }
            }
        }
        catch (Exception e)
        {
            String errorMessage = "Exception: " + e;
            mLogger.error(errorMessage);

            throw e;
        }
    }

    private Element buildModel_Task(JSONObject joTask,
                                 Integer currentColumn,
                                    Vector<Integer> currentRowForAColumn,
                                 Element parentElement, EndPointAnchor parentEndPointAnchor)
            throws Exception
    {
        try
        {
            Element taskElement = addTask(joTask.getString("Type"),
                    new Integer(workflow_firstX + (currentColumn * workflow_stepX)).toString() + "em",
                    new Integer(workflow_firstY + (currentRowForAColumn.get(currentColumn) * workflow_stepY)) + "em");
            currentRowForAColumn.set(currentColumn, currentRowForAColumn.get(currentColumn) + 1);

            {
                EndPoint sourceEndPoint = getEndPoint(parentElement, parentEndPointAnchor);
                EndPoint targetEndPoint;
                if (parentEndPointAnchor == EndPointAnchor.RIGHT)   // GroupOfTasks
                    targetEndPoint = getEndPoint(taskElement, EndPointAnchor.LEFT);
                else    // Task
                    targetEndPoint = getEndPoint(taskElement, EndPointAnchor.TOP);
                model.connect(new Connection(sourceEndPoint, targetEndPoint));
                manageNewConnection(parentElement, sourceEndPoint, taskElement, targetEndPoint);
            }

            WorkflowProperties workflowProperties = (WorkflowProperties) taskElement.getData();
            workflowProperties.setData(joTask);

            mLogger.info("buildModel_Task"
                    + ", workflowProperties.getElementId: " + workflowProperties.getElementId()
                    + ", workflowProperties.getLabel: " + workflowProperties.getLabel()
                    + ", currentColumn: " + currentColumn
                    + ", currentRowForAColumn.toString: " + currentRowForAColumn.toString()
            );

            if (joTask.has("OnSuccess"))
            {
                JSONObject joOnSuccess = joTask.getJSONObject("OnSuccess");
                if (joOnSuccess.has("Task"))
                {
                    JSONObject joOnSuccessTask = joOnSuccess.getJSONObject("Task");
                    if (joOnSuccessTask.getString("Type").equalsIgnoreCase("GroupOfTasks"))
                        buildModel_GroupOfTasks(joOnSuccessTask,
                                currentColumn, currentRowForAColumn,
                                taskElement, EndPointAnchor.BOTTOM);
                    else
                        buildModel_Task(joOnSuccessTask,
                                currentColumn, currentRowForAColumn,
                                taskElement, EndPointAnchor.BOTTOM);
                }
            }

            if (joTask.has("OnError"))
            {
                JSONObject joOnError = joTask.getJSONObject("OnError");
                if (joOnError.has("Task"))
                {
                    JSONObject joOnErrorTask = joOnError.getJSONObject("Task");
                    if (joOnErrorTask.getString("Type").equalsIgnoreCase("GroupOfTasks"))
                        buildModel_GroupOfTasks(joOnErrorTask,
                                currentColumn - 1, currentRowForAColumn,
                                taskElement, EndPointAnchor.BOTTOM_LEFT);
                    else
                        buildModel_Task(joOnErrorTask,
                                currentColumn - 1, currentRowForAColumn,
                                taskElement, EndPointAnchor.BOTTOM_LEFT);
                }
            }

            if (joTask.has("OnComplete"))
            {
                JSONObject joOnComplete = joTask.getJSONObject("OnComplete");
                if (joOnComplete.has("Task"))
                {
                    JSONObject joOnCompleteTask = joOnComplete.getJSONObject("Task");
                    if (joOnCompleteTask.getString("Type").equalsIgnoreCase("GroupOfTasks"))
                        buildModel_GroupOfTasks(joOnCompleteTask,
                                currentColumn + 1, currentRowForAColumn,
                                taskElement, EndPointAnchor.BOTTOM_RIGHT);
                    else
                        buildModel_Task(joOnCompleteTask,
                                currentColumn + 1, currentRowForAColumn,
                                taskElement, EndPointAnchor.BOTTOM_RIGHT);
                }
            }

            return taskElement;
        }
        catch (Exception e)
        {
            String errorMessage = "Exception: " + e;
            mLogger.error(errorMessage);

            throw e;
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

    public String getDurationAsString(Long durationInMilliseconds)
    {
        if (durationInMilliseconds == null)
            return "";

        String duration;

        int hours = (int) (durationInMilliseconds / 3600000);
        String sHours = String.format("%02d", hours);

        int minutes = (int) ((durationInMilliseconds - (hours * 3600000)) / 60000);
        String sMinutes = String.format("%02d", minutes);

        int seconds = (int) ((durationInMilliseconds - ((hours * 3600000) + (minutes * 60000))) / 1000);
        String sSeconds = String.format("%02d", seconds);

        int milliSeconds = (int) (durationInMilliseconds - ((hours * 3600000) + (minutes * 60000) + (seconds * 1000)));
        String sMilliSeconds = String.format("%03d", milliSeconds);

        return sHours.concat(":").concat(sMinutes).concat(":").concat(sSeconds).concat(".").concat(sMilliSeconds);
    }

    public String getLoadType() {
        return loadType;
    }

    public void setLoadType(String loadType) {
        this.loadType = loadType;
    }

    public String getData() {
        return data;
    }

    public void setData(String data) {
        this.data = data;
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

    public ExtractTracksProperties getCurrentExtractTracksProperties() {
        return currentExtractTracksProperties;
    }

    public void setCurrentExtractTracksProperties(ExtractTracksProperties currentExtractTracksProperties) {
        this.currentExtractTracksProperties = currentExtractTracksProperties;
    }

    public EncodeProperties getCurrentEncodeProperties() {
        return currentEncodeProperties;
    }

    public void setCurrentEncodeProperties(EncodeProperties currentEncodeProperties) {
        this.currentEncodeProperties = currentEncodeProperties;
    }

    public OverlayImageOnVideoProperties getCurrentOverlayImageOnVideoProperties() {
        return currentOverlayImageOnVideoProperties;
    }

    public void setCurrentOverlayImageOnVideoProperties(OverlayImageOnVideoProperties currentOverlayImageOnVideoProperties) {
        this.currentOverlayImageOnVideoProperties = currentOverlayImageOnVideoProperties;
    }

    public OverlayTextOnVideoProperties getCurrentOverlayTextOnVideoProperties() {
        return currentOverlayTextOnVideoProperties;
    }

    public void setCurrentOverlayTextOnVideoProperties(OverlayTextOnVideoProperties currentOverlayTextOnVideoProperties) {
        this.currentOverlayTextOnVideoProperties = currentOverlayTextOnVideoProperties;
    }

    public FrameProperties getCurrentFrameProperties() {
        return currentFrameProperties;
    }

    public void setCurrentFrameProperties(FrameProperties currentFrameProperties) {
        this.currentFrameProperties = currentFrameProperties;
    }

    public PeriodicalFramesProperties getCurrentPeriodicalFramesProperties() {
        return currentPeriodicalFramesProperties;
    }

    public void setCurrentPeriodicalFramesProperties(PeriodicalFramesProperties currentPeriodicalFramesProperties) {
        this.currentPeriodicalFramesProperties = currentPeriodicalFramesProperties;
    }

    public IFramesProperties getCurrentIFramesProperties() {
        return currentIFramesProperties;
    }

    public void setCurrentIFramesProperties(IFramesProperties currentIFramesProperties) {
        this.currentIFramesProperties = currentIFramesProperties;
    }

    public MotionJPEGByPeriodicalFramesProperties getCurrentMotionJPEGByPeriodicalFramesProperties() {
        return currentMotionJPEGByPeriodicalFramesProperties;
    }

    public void setCurrentMotionJPEGByPeriodicalFramesProperties(MotionJPEGByPeriodicalFramesProperties currentMotionJPEGByPeriodicalFramesProperties) {
        this.currentMotionJPEGByPeriodicalFramesProperties = currentMotionJPEGByPeriodicalFramesProperties;
    }

    public MotionJPEGByIFramesProperties getCurrentMotionJPEGByIFramesProperties() {
        return currentMotionJPEGByIFramesProperties;
    }

    public void setCurrentMotionJPEGByIFramesProperties(MotionJPEGByIFramesProperties currentMotionJPEGByIFramesProperties) {
        this.currentMotionJPEGByIFramesProperties = currentMotionJPEGByIFramesProperties;
    }

    public SlideshowProperties getCurrentSlideshowProperties() {
        return currentSlideshowProperties;
    }

    public void setCurrentSlideshowProperties(SlideshowProperties currentSlideshowProperties) {
        this.currentSlideshowProperties = currentSlideshowProperties;
    }

    public FTPDeliveryProperties getCurrentFTPDeliveryProperties() {
        return currentFTPDeliveryProperties;
    }

    public void setCurrentFTPDeliveryProperties(FTPDeliveryProperties currentFTPDeliveryProperties) {
        this.currentFTPDeliveryProperties = currentFTPDeliveryProperties;
    }

    public LocalCopyProperties getCurrentLocalCopyProperties() {
        return currentLocalCopyProperties;
    }

    public void setCurrentLocalCopyProperties(LocalCopyProperties currentLocalCopyProperties) {
        this.currentLocalCopyProperties = currentLocalCopyProperties;
    }

    public PostOnFacebookProperties getCurrentPostOnFacebookProperties() {
        return currentPostOnFacebookProperties;
    }

    public void setCurrentPostOnFacebookProperties(PostOnFacebookProperties currentPostOnFacebookProperties) {
        this.currentPostOnFacebookProperties = currentPostOnFacebookProperties;
    }

    public PostOnYouTubeProperties getCurrentPostOnYouTubeProperties() {
        return currentPostOnYouTubeProperties;
    }

    public void setCurrentPostOnYouTubeProperties(PostOnYouTubeProperties currentPostOnYouTubeProperties) {
        this.currentPostOnYouTubeProperties = currentPostOnYouTubeProperties;
    }

    public EmailNotificationProperties getCurrentEmailNotificationProperties() {
        return currentEmailNotificationProperties;
    }

    public void setCurrentEmailNotificationProperties(EmailNotificationProperties currentEmailNotificationProperties) {
        this.currentEmailNotificationProperties = currentEmailNotificationProperties;
    }

    public HTTPCallbackProperties getCurrentHttpCallbackProperties() {
        return currentHttpCallbackProperties;
    }

    public void setCurrentHttpCallbackProperties(HTTPCallbackProperties currentHttpCallbackProperties) {
        this.currentHttpCallbackProperties = currentHttpCallbackProperties;
    }

    public FaceRecognitionProperties getCurrentFaceRecognitionProperties() {
        return currentFaceRecognitionProperties;
    }

    public void setCurrentFaceRecognitionProperties(FaceRecognitionProperties currentFaceRecognitionProperties) {
        this.currentFaceRecognitionProperties = currentFaceRecognitionProperties;
    }

    public FaceIdentificationProperties getCurrentFaceIdentificationProperties() {
        return currentFaceIdentificationProperties;
    }

    public void setCurrentFaceIdentificationProperties(FaceIdentificationProperties currentFaceIdentificationProperties) {
        this.currentFaceIdentificationProperties = currentFaceIdentificationProperties;
    }

    public LiveRecorderProperties getCurrentLiveRecorderProperties() {
        return currentLiveRecorderProperties;
    }

    public void setCurrentLiveRecorderProperties(LiveRecorderProperties currentLiveRecorderProperties) {
        this.currentLiveRecorderProperties = currentLiveRecorderProperties;
    }

    public ChangeFileFormatProperties getCurrentChangeFileFormatProperties() {
        return currentChangeFileFormatProperties;
    }

    public void setCurrentChangeFileFormatProperties(ChangeFileFormatProperties currentChangeFileFormatProperties) {
        this.currentChangeFileFormatProperties = currentChangeFileFormatProperties;
    }

    public int getRemovingCurrentElementId() {
        return removingCurrentElementId;
    }

    public void setRemovingCurrentElementId(int removingCurrentElementId) {
        this.removingCurrentElementId = removingCurrentElementId;
    }

    public String getCurrentWorkflowEditorName() {
        return currentWorkflowEditorName;
    }

    public void setCurrentWorkflowEditorName(String currentWorkflowEditorName) {
        this.currentWorkflowEditorName = currentWorkflowEditorName;
    }

    public List<String> getWorkflowEditorNames() {
        return workflowEditorNames;
    }

    public void setWorkflowEditorNames(List<String> workflowEditorNames) {
        this.workflowEditorNames = workflowEditorNames;
    }
}
