package com.catramms.backing.newWorkflow;

import com.catramms.backing.common.SessionUtils;
import com.catramms.backing.entity.EncodingProfile;
import com.catramms.backing.entity.EncodingProfilesSet;
import com.catramms.backing.entity.MediaItem;
import com.catramms.backing.entity.PhysicalPath;
import com.catramms.utility.catramms.CatraMMS;
import org.apache.commons.io.IOUtils;
import org.apache.log4j.Logger;
import org.json.JSONArray;
import org.json.JSONObject;
import org.primefaces.event.FileUploadEvent;
import org.primefaces.event.NodeSelectEvent;
import org.primefaces.model.DefaultTreeNode;
import org.primefaces.model.TreeNode;
import org.primefaces.model.UploadedFile;

import javax.annotation.PostConstruct;
import javax.faces.application.FacesMessage;
import javax.faces.bean.ManagedBean;
import javax.faces.bean.SessionScoped;
import javax.faces.bean.ViewScoped;
import javax.faces.context.FacesContext;
import javax.servlet.http.HttpSession;
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
public class NewWorkflow implements Serializable {

    // static because the class is Serializable
    private static final Logger mLogger = Logger.getLogger(NewWorkflow.class);

    static public final String configFileName = "catramms.properties";
    private String temporaryPushBinariesPathName;

    private String predefined;
    private String data;

    private Long timeInSecondsDecimalsPrecision;

    private TreeNode tnRoot = null;
    private TreeNode tnWorkflow;
    private TreeNode tnSelectedNode;
    private TreeNode tnWorkingNode;

    private String jsonWorkflow;
    private List<WorkflowIssue> workflowIssueList = new ArrayList<>();
    private List<PushContent> pushContentList = new ArrayList<>();
    private IngestionResult workflowRoot = new IngestionResult();
    private List<IngestionResult> ingestionJobList = new ArrayList<>();
    private int taskNumber;
    private String ingestWorkflowErrorMessage;

    private Long mediaItemsNumber = new Long(0);
    private List<MediaItem> mediaItemsList = new ArrayList<>();
    private List<MediaItem> mediaItemsSelectedList = new ArrayList<>();
    private String mediaItemsSelectionMode;
    private Date mediaItemsBegin;
    private Date mediaItemsEnd;
    private String mediaItemsContentType;
    private List<String> mediaItemsContentTypesList = new ArrayList<>();
    private Long mediaItemsMaxMediaItemsNumber = new Long(100);
    private String mediaItemsToBeAddedOrReplaced;
    private String taskReferences;


    // Workflow properties
    private String workflowLabel;

    // Task (GroupOfTasks) properties
    private String groupOfTaskExecutionType;

    // Task: properties
    private String nodeType;
    private String taskLabel;
    private String taskSourceDownloadType;
    private String taskPullSourceURL;
    private String taskPushBinaryPathName;
    private String taskFileFormat;
    List<String> taskFileFormatsList = new ArrayList<>();
    private String taskUserData;
    private String taskRetention;
    private String taskTitle;
    private String taskUniqueName;
    private String taskIngester;
    private String taskKeywords;
    private String taskMD5FileChecksum;
    private Long taskFileSizeInBytes;
    private String taskContentProviderName;
    private String taskDeliveryFileName;
    private Date taskStartPublishing;
    private Date taskEndPublishing;
    private Float taskStartTimeInSeconds;
    private Float taskEndTimeInSeconds;
    private Long taskFramesNumber;
    private String taskCutEndType;
    private String taskEncodingPriority;
    List<String> taskEncodingPrioritiesList = new ArrayList<>();
    private String taskEncodingProfileType;
    private String taskEncodingProfilesSetLabel;
    private String taskEncodingProfileLabel;
    private String taskEmailAddress;
    private String taskSubject;
    private String taskMessage;
    private String taskOverlayPositionXInPixel;
    private String taskOverlayPositionYInPixel;
    private String taskOverlayText;
    private String taskOverlayFontType;
    private Long taskOverlayFontSize;
    private String taskOverlayFontColor;
    private Long taskOverlayTextPercentageOpacity;
    private Boolean taskOverlayBoxEnable;
    private String taskOverlayBoxColor;
    private Long taskOverlayBoxPercentageOpacity;
    private Float taskFrameInstantInSeconds;
    private Long taskFrameWidth;
    private Long taskFrameHeight;
    private Long taskFramePeriodInSeconds;
    private Long taskFrameMaxFramesNumber;
    private Float taskFrameDurationOfEachSlideInSeconds;

    private String taskContentType;
    List<String> taskContentTypesList = new ArrayList<>();

    List<String> taskEncodingProfilesLabelSetList = new ArrayList<>();
    List<EncodingProfilesSet> taskVideoEncodingProfilesSetList = new ArrayList<>();
    List<EncodingProfilesSet> taskAudioEncodingProfilesSetList = new ArrayList<>();
    List<EncodingProfilesSet> taskImageEncodingProfilesSetList = new ArrayList<>();

    List<String> taskEncodingProfilesLabelList = new ArrayList<>();
    List<EncodingProfile> taskVideoEncodingProfilesList = new ArrayList<>();
    List<EncodingProfile> taskAudioEncodingProfilesList = new ArrayList<>();
    List<EncodingProfile> taskImageEncodingProfilesList = new ArrayList<>();

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

        taskNumber = 1;

        timeInSecondsDecimalsPrecision = new Long(6);

        // tree initialization
        {
            tnRoot = new DefaultTreeNode("Root", null);

            Workflow workflow = new Workflow();
            workflow.setLabel("Workflow label");
            tnWorkflow = new DefaultTreeNode(workflow, tnRoot);
            tnWorkflow.setType("Workflow");
            tnWorkflow.setExpanded(true);

            tnSelectedNode = tnWorkflow;

            setNodeProperties();
        }

        {
            taskFileFormatsList.clear();
            taskFileFormatsList.add("mp4");
            taskFileFormatsList.add("wmv");
            taskFileFormatsList.add("mpeg");
            taskFileFormatsList.add("avi");
            taskFileFormatsList.add("webm");
            taskFileFormatsList.add("mp3");
            taskFileFormatsList.add("aac");
            taskFileFormatsList.add("png");
            taskFileFormatsList.add("jpg");
        }

        {
            taskEncodingPrioritiesList.add("Low");
            taskEncodingPrioritiesList.add("Medium");
            taskEncodingPrioritiesList.add("High");
        }

        {
            taskContentTypesList.add("video");
            taskContentTypesList.add("audio");
            taskContentTypesList.add("image");
        }

        mediaItemsToBeAddedOrReplaced = "toBeReplaced";

        // needed otherwise when the ingestionWorkflowDetails is built at the beginning will generate
        // the excetion ...at java.net.URLEncoder.encode(URLEncoder.java:204)
        workflowRoot.setKey(new Long(0));

        try
        {
            Long userKey = SessionUtils.getUserKey();
            String apiKey = SessionUtils.getAPIKey();

            if (userKey == null || apiKey == null || apiKey.equalsIgnoreCase(""))
            {
                mLogger.warn("no input to require encodingProfilesSetKey"
                                + ", userKey: " + userKey
                                + ", apiKey: " + apiKey
                );
            }
            else
            {
                String username = userKey.toString();
                String password = apiKey;

                CatraMMS catraMMS = new CatraMMS();

                catraMMS.getEncodingProfilesSets(username, password,
                        "video", taskVideoEncodingProfilesSetList);
                catraMMS.getEncodingProfilesSets(username, password,
                        "audio", taskAudioEncodingProfilesSetList);
                catraMMS.getEncodingProfilesSets(username, password,
                        "image", taskImageEncodingProfilesSetList);

                catraMMS.getEncodingProfiles(username, password,
                        "video", taskVideoEncodingProfilesList);
                catraMMS.getEncodingProfiles(username, password,
                        "audio", taskAudioEncodingProfilesList);
                catraMMS.getEncodingProfiles(username, password,
                        "image", taskImageEncodingProfilesList);
            }
        }
        catch (Exception e)
        {
            String errorMessage = "Exception: " + e;
            mLogger.error(errorMessage);
        }

        {
            Calendar calendar = Calendar.getInstance();

            calendar.add(Calendar.DAY_OF_MONTH, -1);

            calendar.set(Calendar.HOUR_OF_DAY, 0);
            calendar.set(Calendar.MINUTE, 0);
            calendar.set(Calendar.SECOND, 0);
            calendar.set(Calendar.MILLISECOND, 0);

            mediaItemsBegin = calendar.getTime();
        }

        {
            Calendar calendar = Calendar.getInstance();

            calendar.add(Calendar.HOUR_OF_DAY, 5);

            calendar.set(Calendar.MINUTE, 0);
            calendar.set(Calendar.SECOND, 0);
            calendar.set(Calendar.MILLISECOND, 0);

            mediaItemsEnd = calendar.getTime();
        }

        buildWorkflow();

        mLogger.info("predefined: " + predefined
                + ", data: " + data
        );
        if (predefined != null)
        {
            if (predefined.equalsIgnoreCase("cut"))
            {
                if (data != null && !data.equalsIgnoreCase(""))
                {
                    try {
                        JSONObject joCut = new JSONObject(data);

                        Long physicalPathKey = joCut.getLong("key");

                        JSONArray jaMarks = joCut.getJSONArray("marks");

                        if (jaMarks.length() > 1)
                        {
                            tnSelectedNode = tnWorkflow;

                            tnSelectedNode = addGroupOfTasks();
                        }

                        for (int markIndex = 0; markIndex < jaMarks.length(); markIndex++)
                        {
                            JSONObject joMark = jaMarks.getJSONObject(markIndex);

                            TreeNode tnTreeNode = addTask("Cut");
                            Task cutTask = (Task) tnTreeNode.getData();

                            cutTask.setReferences(physicalPathKey.toString());

                            cutTask.setStartTimeInSeconds(Float.parseFloat(joMark.getString("s")));
                            cutTask.setCutEndType("endTime");
                            cutTask.setEndTimeInSeconds(Float.parseFloat(joMark.getString("e")));

                            if (jaMarks.length() > 1)
                                cutTask.setRetention("0");
                        }

                        if (jaMarks.length() > 1)
                        {
                            tnSelectedNode = addOnSuccessEvent();

                            addTask("Concat-Demuxer");
                        }
                    }
                    catch (Exception e)
                    {
                        mLogger.error("predefined workflow failed"
                                        + ", predefined: " + predefined
                                        + ", data: " + data
                        );
                    }
                }
                else
                {
                    mLogger.error("No data for the predefined Workflow " + predefined);
                }
            }
            else
            {
                mLogger.error("Unknown predefined Workflow: " + predefined);
            }
        }
    }

    public void selectionChanged(NodeSelectEvent event)
    {
        mLogger.info("selectionChanged. Clicked: " + event.getTreeNode());
        setNodeProperties();
        // selectedNode = event.getTreeNode();
    }

    public void buildWorkflow()
    {
        try {
            workflowIssueList.clear();
            pushContentList.clear();

            JSONObject joWorkflow = new JSONObject();

            joWorkflow.put("Type", "Workflow");

            Workflow workflow = (Workflow) tnWorkflow.getData();
            if (workflow.getLabel() != null && !workflow.getLabel().equalsIgnoreCase(""))
                joWorkflow.put("Label", workflow.getLabel());
            else
            {
                WorkflowIssue workflowIssue = new WorkflowIssue();
                workflowIssue.setLabel("");
                workflowIssue.setFieldName("Label");
                workflowIssue.setTaskType("Workflow");
                workflowIssue.setIssue("The field is not initialized");

                workflowIssueList.add(workflowIssue);
            }

            // mLogger.info("tnWorkflow.getChildren().size: " + tnWorkflow.getChildren().size());

            if (tnWorkflow.getChildren().size() == 1)
                joWorkflow.put("Task", buildTask(tnWorkflow.getChildren().get(0)));

            jsonWorkflow = joWorkflow.toString(4);
        }
        catch (Exception e)
        {
            mLogger.error("buildWorkflow failed. Exception: " + e);
        }
    }

    public JSONObject buildTask(TreeNode tnTreeNode)
            throws Exception
    {
        try
        {
            DateFormat dateFormat = new SimpleDateFormat("yyyy-MM-dd'T'HH:mm:ss'Z'");
            dateFormat.setTimeZone(TimeZone.getTimeZone("GMT"));

            JSONObject jsonObject = new JSONObject();

            Task task = (Task) tnTreeNode.getData();

            jsonObject.put("Type", task.getType());

            JSONObject joParameters = new JSONObject();
            jsonObject.put("Parameters", joParameters);

            mLogger.info("task.getType: " + task.getType());

            if (task.getType().equalsIgnoreCase("GroupOfTasks"))
            {
                joParameters.put("ExecutionType", task.getGroupOfTaskExecutionType());

                JSONArray jsonArray = new JSONArray();
                joParameters.put("Tasks", jsonArray);

                for (TreeNode tnTaskOfGroup: tnTreeNode.getChildren())
                {
                    if (!tnTaskOfGroup.getType().equalsIgnoreCase("Event"))
                        jsonArray.put(buildTask(tnTaskOfGroup));
                }
            }
            else if (task.getType().equalsIgnoreCase("Add-Content"))
            {
                if (task.getLabel() != null && !task.getLabel().equalsIgnoreCase(""))
                    jsonObject.put("Label", task.getLabel());
                else
                {
                    WorkflowIssue workflowIssue = new WorkflowIssue();
                    workflowIssue.setLabel("");
                    workflowIssue.setFieldName("Label");
                    workflowIssue.setTaskType(task.getType());
                    workflowIssue.setIssue("The field is not initialized");

                    workflowIssueList.add(workflowIssue);
                }

                if (task.getSourceDownloadType().equalsIgnoreCase("pull"))
                {
                    if (task.getPullSourceURL() != null && !task.getPullSourceURL().equalsIgnoreCase(""))
                        joParameters.put("SourceURL", task.getPullSourceURL());
                    else
                    {
                        WorkflowIssue workflowIssue = new WorkflowIssue();
                        workflowIssue.setLabel(task.getLabel());
                        workflowIssue.setFieldName("SourceURL");
                        workflowIssue.setTaskType(task.getType());
                        workflowIssue.setIssue("The field is not initialized");

                        workflowIssueList.add(workflowIssue);
                    }
                }
                else
                {
                    if (task.getPushBinaryPathName() == null || task.getPushBinaryPathName().equalsIgnoreCase(""))
                    {
                        WorkflowIssue workflowIssue = new WorkflowIssue();
                        workflowIssue.setLabel(task.getLabel());
                        workflowIssue.setFieldName("File");
                        workflowIssue.setTaskType(task.getType());
                        workflowIssue.setIssue("The field is not initialized");

                        workflowIssueList.add(workflowIssue);
                    }
                    else
                    {
                        PushContent pushContent = new PushContent();
                        pushContent.setLabel(task.getLabel());
                        pushContent.setBinaryPathName(task.getPushBinaryPathName());

                        pushContentList.add(pushContent);
                    }
                }
                joParameters.put("FileFormat", task.getFileFormat());
                if (task.getUserData() != null && !task.getUserData().equalsIgnoreCase(""))
                    joParameters.put("UniqueData", task.getUserData());
                if (task.getRetention() != null && !task.getRetention().equalsIgnoreCase(""))
                    joParameters.put("Retention", task.getRetention());
                if (task.getTitle() != null && !task.getTitle().equalsIgnoreCase(""))
                    joParameters.put("Title", task.getTitle());
                if (task.getUniqueName() != null && !task.getUniqueName().equalsIgnoreCase(""))
                    joParameters.put("UniqueName", task.getUniqueName());
                if (task.getIngester() != null && !task.getIngester().equalsIgnoreCase(""))
                    joParameters.put("Ingester", task.getIngester());
                if (task.getKeywords() != null && !task.getKeywords().equalsIgnoreCase(""))
                    joParameters.put("Keywords", task.getKeywords());
                if (task.getMd5FileCheckSum() != null && !task.getMd5FileCheckSum().equalsIgnoreCase(""))
                    joParameters.put("MD5FileChecksum", task.getMd5FileCheckSum());
                if (task.getFileSizeInBytes() != null)
                    joParameters.put("FileSizeInBytes", task.getFileSizeInBytes());
                if (task.getContentProviderName() != null && !task.getContentProviderName().equalsIgnoreCase(""))
                    joParameters.put("ContentProviderName", task.getContentProviderName());
                if (task.getDeliveryFileName() != null && !task.getDeliveryFileName().equalsIgnoreCase(""))
                    joParameters.put("DeliveryFileName", task.getDeliveryFileName());
                if (task.getStartPublishing() != null || task.getEndPublishing() != null)
                {
                    JSONObject joPublishing = new JSONObject();
                    joParameters.put("Publishing", joPublishing);

                    if (task.getStartPublishing() != null)
                        joPublishing.put("StartPublishing", dateFormat.format(task.getStartPublishing()));
                    else
                        joPublishing.put("StartPublishing", "NOW");
                    if (task.getEndPublishing() != null)
                        joPublishing.put("EndPublishing", dateFormat.format(task.getEndPublishing()));
                    else
                        joPublishing.put("EndPublishing", "FOREVER");
                }
            }
            else if (task.getType().equalsIgnoreCase("Remove-Content"))
            {
                if (task.getLabel() != null && !task.getLabel().equalsIgnoreCase(""))
                    jsonObject.put("Label", task.getLabel());
                else {
                    WorkflowIssue workflowIssue = new WorkflowIssue();
                    workflowIssue.setLabel("");
                    workflowIssue.setFieldName("Label");
                    workflowIssue.setTaskType(task.getType());
                    workflowIssue.setIssue("The field is not initialized");

                    workflowIssueList.add(workflowIssue);
                }

                if (task.getReferences() != null && !task.getReferences().equalsIgnoreCase(""))
                {
                    JSONArray jaReferences = new JSONArray();
                    joParameters.put("References", jaReferences);

                    String [] mediaItemKeyReferences = task.getReferences().split(",");
                    for (String mediaItemKeyReference: mediaItemKeyReferences)
                    {
                        JSONObject joReference = new JSONObject();
                        joReference.put("ReferenceMediaItemKey", Long.parseLong(mediaItemKeyReference));

                        jaReferences.put(joReference);
                    }
                }
            }
            else if (task.getType().equalsIgnoreCase("Concat-Demuxer"))
            {
                if (task.getLabel() != null && !task.getLabel().equalsIgnoreCase(""))
                    jsonObject.put("Label", task.getLabel());
                else
                {
                    WorkflowIssue workflowIssue = new WorkflowIssue();
                    workflowIssue.setLabel("");
                    workflowIssue.setFieldName("Label");
                    workflowIssue.setTaskType(task.getType());
                    workflowIssue.setIssue("The field is not initialized");

                    workflowIssueList.add(workflowIssue);
                }

                if (task.getUserData() != null && !task.getUserData().equalsIgnoreCase(""))
                    joParameters.put("UserData", task.getUserData());
                if (task.getRetention() != null && !task.getRetention().equalsIgnoreCase(""))
                    joParameters.put("Retention", task.getRetention());
                if (task.getTitle() != null && !task.getTitle().equalsIgnoreCase(""))
                    joParameters.put("Title", task.getTitle());
                if (task.getUniqueName() != null && !task.getUniqueName().equalsIgnoreCase(""))
                    joParameters.put("UniqueName", task.getUniqueName());
                if (task.getIngester() != null && !task.getIngester().equalsIgnoreCase(""))
                    joParameters.put("Ingester", task.getIngester());
                if (task.getKeywords() != null && !task.getKeywords().equalsIgnoreCase(""))
                    joParameters.put("Keywords", task.getKeywords());
                if (task.getContentProviderName() != null && !task.getContentProviderName().equalsIgnoreCase(""))
                    joParameters.put("ContentProviderName", task.getContentProviderName());
                if (task.getDeliveryFileName() != null && !task.getDeliveryFileName().equalsIgnoreCase(""))
                    joParameters.put("DeliveryFileName", task.getDeliveryFileName());
                if (task.getStartPublishing() != null || task.getEndPublishing() != null)
                {
                    JSONObject joPublishing = new JSONObject();
                    joParameters.put("Publishing", joPublishing);

                    if (task.getStartPublishing() != null)
                        joPublishing.put("StartPublishing", dateFormat.format(task.getStartPublishing()));
                    else
                        joPublishing.put("StartPublishing", "NOW");
                    if (task.getEndPublishing() != null)
                        joPublishing.put("EndPublishing", dateFormat.format(task.getEndPublishing()));
                    else
                        joPublishing.put("EndPublishing", "FOREVER");
                }

                if (task.getReferences() != null && !task.getReferences().equalsIgnoreCase(""))
                {
                    JSONArray jaReferences = new JSONArray();
                    joParameters.put("References", jaReferences);

                    String [] physicalPathKeyReferences = task.getReferences().split(",");
                    for (String physicalPathKeyReference: physicalPathKeyReferences)
                    {
                        JSONObject joReference = new JSONObject();
                        joReference.put("ReferencePhysicalPathKey", Long.parseLong(physicalPathKeyReference));

                        jaReferences.put(joReference);
                    }
                }
            }
            else if (task.getType().equalsIgnoreCase("Cut"))
            {
                if (task.getLabel() != null && !task.getLabel().equalsIgnoreCase(""))
                    jsonObject.put("Label", task.getLabel());
                else
                {
                    WorkflowIssue workflowIssue = new WorkflowIssue();
                    workflowIssue.setLabel("");
                    workflowIssue.setFieldName("Label");
                    workflowIssue.setTaskType(task.getType());
                    workflowIssue.setIssue("The field is not initialized");

                    workflowIssueList.add(workflowIssue);
                }

                if (task.getStartTimeInSeconds() != null)
                    joParameters.put("StartTimeInSeconds", task.getStartTimeInSeconds());
                            // String.format("%." + timeInSecondsDecimalsPrecision + "g",
                            //        task.getStartTimeInSeconds().floatValue()));
                else
                {
                    WorkflowIssue workflowIssue = new WorkflowIssue();
                    workflowIssue.setLabel(task.getLabel());
                    workflowIssue.setFieldName("StartTimeInSeconds");
                    workflowIssue.setTaskType(task.getType());
                    workflowIssue.setIssue("The field is not initialized");

                    workflowIssueList.add(workflowIssue);
                }
                if (task.getCutEndType().equalsIgnoreCase("endTime"))
                {
                    if (task.getEndTimeInSeconds() != null)
                        joParameters.put("EndTimeInSeconds", task.getEndTimeInSeconds());
                                // String.format("%." + timeInSecondsDecimalsPrecision + "g",
                                //        task.getEndTimeInSeconds().floatValue()));
                    else
                    {
                        WorkflowIssue workflowIssue = new WorkflowIssue();
                        workflowIssue.setLabel(task.getLabel());
                        workflowIssue.setFieldName("EndTimeInSeconds");
                        workflowIssue.setTaskType(task.getType());
                        workflowIssue.setIssue("The field is not initialized");

                        workflowIssueList.add(workflowIssue);
                    }
                }
                else
                {
                    if (task.getFramesNumber() != null)
                        joParameters.put("FramesNumber", task.getFramesNumber());
                    else
                    {
                        WorkflowIssue workflowIssue = new WorkflowIssue();
                        workflowIssue.setLabel(task.getLabel());
                        workflowIssue.setFieldName("FramesNumber");
                        workflowIssue.setTaskType(task.getType());
                        workflowIssue.setIssue("The field is not initialized");

                        workflowIssueList.add(workflowIssue);
                    }
                }
                if (task.getUserData() != null && !task.getUserData().equalsIgnoreCase(""))
                    joParameters.put("UserData", task.getUserData());
                if (task.getRetention() != null && !task.getRetention().equalsIgnoreCase(""))
                    joParameters.put("Retention", task.getRetention());
                if (task.getTitle() != null && !task.getTitle().equalsIgnoreCase(""))
                    joParameters.put("Title", task.getTitle());
                if (task.getUniqueName() != null && !task.getUniqueName().equalsIgnoreCase(""))
                    joParameters.put("UniqueName", task.getUniqueName());
                if (task.getIngester() != null && !task.getIngester().equalsIgnoreCase(""))
                    joParameters.put("Ingester", task.getIngester());
                if (task.getKeywords() != null && !task.getKeywords().equalsIgnoreCase(""))
                    joParameters.put("Keywords", task.getKeywords());
                if (task.getContentProviderName() != null && !task.getContentProviderName().equalsIgnoreCase(""))
                    joParameters.put("ContentProviderName", task.getContentProviderName());
                if (task.getDeliveryFileName() != null && !task.getDeliveryFileName().equalsIgnoreCase(""))
                    joParameters.put("DeliveryFileName", task.getDeliveryFileName());
                if (task.getStartPublishing() != null || task.getEndPublishing() != null)
                {
                    JSONObject joPublishing = new JSONObject();
                    joParameters.put("Publishing", joPublishing);

                    if (task.getStartPublishing() != null)
                        joPublishing.put("StartPublishing", dateFormat.format(task.getStartPublishing()));
                    else
                        joPublishing.put("StartPublishing", "NOW");
                    if (task.getEndPublishing() != null)
                        joPublishing.put("EndPublishing", dateFormat.format(task.getEndPublishing()));
                    else
                        joPublishing.put("EndPublishing", "FOREVER");
                }

                if (task.getReferences() != null && !task.getReferences().equalsIgnoreCase(""))
                {
                    JSONArray jaReferences = new JSONArray();
                    joParameters.put("References", jaReferences);

                    String [] physicalPathKeyReferences = task.getReferences().split(",");
                    for (String physicalPathKeyReference: physicalPathKeyReferences)
                    {
                        JSONObject joReference = new JSONObject();
                        joReference.put("ReferencePhysicalPathKey", Long.parseLong(physicalPathKeyReference));

                        jaReferences.put(joReference);
                    }
                }
            }
            else if (task.getType().equalsIgnoreCase("Overlay-Image-On-Video"))
            {
                if (task.getLabel() != null && !task.getLabel().equalsIgnoreCase(""))
                    jsonObject.put("Label", task.getLabel());
                else
                {
                    WorkflowIssue workflowIssue = new WorkflowIssue();
                    workflowIssue.setLabel("");
                    workflowIssue.setFieldName("Label");
                    workflowIssue.setTaskType(task.getType());
                    workflowIssue.setIssue("The field is not initialized");

                    workflowIssueList.add(workflowIssue);
                }

                if (task.getOverlayPositionXInPixel() != null && !task.getOverlayPositionXInPixel().equalsIgnoreCase(""))
                    joParameters.put("ImagePosition_X_InPixel", task.getOverlayPositionXInPixel());
                else
                {
                    WorkflowIssue workflowIssue = new WorkflowIssue();
                    workflowIssue.setLabel(task.getLabel());
                    workflowIssue.setFieldName("ImagePosition_X_InPixel");
                    workflowIssue.setTaskType(task.getType());
                    workflowIssue.setIssue("The field is not initialized");

                    workflowIssueList.add(workflowIssue);
                }
                if (task.getOverlayPositionYInPixel() != null && !task.getOverlayPositionYInPixel().equalsIgnoreCase(""))
                    joParameters.put("ImagePosition_Y_InPixel", task.getOverlayPositionYInPixel());
                else
                {
                    WorkflowIssue workflowIssue = new WorkflowIssue();
                    workflowIssue.setLabel(task.getLabel());
                    workflowIssue.setFieldName("ImagePosition_Y_InPixel");
                    workflowIssue.setTaskType(task.getType());
                    workflowIssue.setIssue("The field is not initialized");

                    workflowIssueList.add(workflowIssue);
                }
                if (task.getUserData() != null && !task.getUserData().equalsIgnoreCase(""))
                    joParameters.put("UserData", task.getUserData());
                if (task.getRetention() != null && !task.getRetention().equalsIgnoreCase(""))
                    joParameters.put("Retention", task.getRetention());
                if (task.getTitle() != null && !task.getTitle().equalsIgnoreCase(""))
                    joParameters.put("Title", task.getTitle());
                if (task.getUniqueName() != null && !task.getUniqueName().equalsIgnoreCase(""))
                    joParameters.put("UniqueName", task.getUniqueName());
                if (task.getIngester() != null && !task.getIngester().equalsIgnoreCase(""))
                    joParameters.put("Ingester", task.getIngester());
                if (task.getKeywords() != null && !task.getKeywords().equalsIgnoreCase(""))
                    joParameters.put("Keywords", task.getKeywords());
                if (task.getContentProviderName() != null && !task.getContentProviderName().equalsIgnoreCase(""))
                    joParameters.put("ContentProviderName", task.getContentProviderName());
                if (task.getDeliveryFileName() != null && !task.getDeliveryFileName().equalsIgnoreCase(""))
                    joParameters.put("DeliveryFileName", task.getDeliveryFileName());
                if (task.getStartPublishing() != null || task.getEndPublishing() != null)
                {
                    JSONObject joPublishing = new JSONObject();
                    joParameters.put("Publishing", joPublishing);

                    if (task.getStartPublishing() != null)
                        joPublishing.put("StartPublishing", dateFormat.format(task.getStartPublishing()));
                    else
                        joPublishing.put("StartPublishing", "NOW");
                    if (task.getEndPublishing() != null)
                        joPublishing.put("EndPublishing", dateFormat.format(task.getEndPublishing()));
                    else
                        joPublishing.put("EndPublishing", "FOREVER");
                }

                if (task.getReferences() != null && !task.getReferences().equalsIgnoreCase(""))
                {
                    JSONArray jaReferences = new JSONArray();
                    joParameters.put("References", jaReferences);

                    String [] physicalPathKeyReferences = task.getReferences().split(",");
                    for (String physicalPathKeyReference: physicalPathKeyReferences)
                    {
                        JSONObject joReference = new JSONObject();
                        joReference.put("ReferencePhysicalPathKey", Long.parseLong(physicalPathKeyReference));

                        jaReferences.put(joReference);
                    }
                }
            }
            else if (task.getType().equalsIgnoreCase("Overlay-Text-On-Video"))
            {
                if (task.getLabel() != null && !task.getLabel().equalsIgnoreCase(""))
                    jsonObject.put("Label", task.getLabel());
                else
                {
                    WorkflowIssue workflowIssue = new WorkflowIssue();
                    workflowIssue.setLabel("");
                    workflowIssue.setFieldName("Label");
                    workflowIssue.setTaskType(task.getType());
                    workflowIssue.setIssue("The field is not initialized");

                    workflowIssueList.add(workflowIssue);
                }

                if (task.getOverlayText() != null && !task.getOverlayText().equalsIgnoreCase(""))
                    joParameters.put("Text", task.getOverlayText());
                else
                {
                    WorkflowIssue workflowIssue = new WorkflowIssue();
                    workflowIssue.setLabel(task.getLabel());
                    workflowIssue.setFieldName("Text");
                    workflowIssue.setTaskType(task.getType());
                    workflowIssue.setIssue("The field is not initialized");

                    workflowIssueList.add(workflowIssue);
                }

                if (task.getOverlayPositionXInPixel() != null && !task.getOverlayPositionXInPixel().equalsIgnoreCase(""))
                    joParameters.put("TextPosition_X_InPixel", task.getOverlayPositionXInPixel());
                if (task.getOverlayPositionYInPixel() != null && !task.getOverlayPositionYInPixel().equalsIgnoreCase(""))
                    joParameters.put("TextPosition_Y_InPixel", task.getOverlayPositionYInPixel());
                if (task.getOverlayFontType() != null && !task.getOverlayFontType().equalsIgnoreCase(""))
                    joParameters.put("FontType", task.getOverlayFontType());
                if (task.getOverlayFontSize() != null)
                    joParameters.put("FontSize", task.getOverlayFontSize());
                if (task.getOverlayFontColor() != null && !task.getOverlayFontColor().equalsIgnoreCase(""))
                    joParameters.put("FontColor", task.getOverlayFontColor());
                if (task.getOverlayTextPercentageOpacity() != null)
                    joParameters.put("TextPercentageOpacity", task.getOverlayTextPercentageOpacity());
                if (task.getOverlayBoxColor() != null)
                    joParameters.put("BoxEnable", task.getOverlayBoxEnable());
                if (task.getOverlayBoxColor() != null && !task.getOverlayBoxColor().equalsIgnoreCase(""))
                    joParameters.put("BoxColor", task.getOverlayBoxColor());
                if (task.getOverlayBoxPercentageOpacity() != null)
                    joParameters.put("BoxPercentageOpacity", task.getOverlayBoxPercentageOpacity());
                if (task.getEncodingPriority() != null && !task.getEncodingPriority().equalsIgnoreCase(""))
                    joParameters.put("EncodingPriority", task.getEncodingPriority());
                if (task.getUserData() != null && !task.getUserData().equalsIgnoreCase(""))
                    joParameters.put("UserData", task.getUserData());
                if (task.getRetention() != null && !task.getRetention().equalsIgnoreCase(""))
                    joParameters.put("Retention", task.getRetention());
                if (task.getTitle() != null && !task.getTitle().equalsIgnoreCase(""))
                    joParameters.put("Title", task.getTitle());
                if (task.getUniqueName() != null && !task.getUniqueName().equalsIgnoreCase(""))
                    joParameters.put("UniqueName", task.getUniqueName());
                if (task.getIngester() != null && !task.getIngester().equalsIgnoreCase(""))
                    joParameters.put("Ingester", task.getIngester());
                if (task.getKeywords() != null && !task.getKeywords().equalsIgnoreCase(""))
                    joParameters.put("Keywords", task.getKeywords());
                if (task.getContentProviderName() != null && !task.getContentProviderName().equalsIgnoreCase(""))
                    joParameters.put("ContentProviderName", task.getContentProviderName());
                if (task.getDeliveryFileName() != null && !task.getDeliveryFileName().equalsIgnoreCase(""))
                    joParameters.put("DeliveryFileName", task.getDeliveryFileName());
                if (task.getStartPublishing() != null || task.getEndPublishing() != null)
                {
                    JSONObject joPublishing = new JSONObject();
                    joParameters.put("Publishing", joPublishing);

                    if (task.getStartPublishing() != null)
                        joPublishing.put("StartPublishing", dateFormat.format(task.getStartPublishing()));
                    else
                        joPublishing.put("StartPublishing", "NOW");
                    if (task.getEndPublishing() != null)
                        joPublishing.put("EndPublishing", dateFormat.format(task.getEndPublishing()));
                    else
                        joPublishing.put("EndPublishing", "FOREVER");
                }

                if (task.getReferences() != null && !task.getReferences().equalsIgnoreCase(""))
                {
                    JSONArray jaReferences = new JSONArray();
                    joParameters.put("References", jaReferences);

                    String [] physicalPathKeyReferences = task.getReferences().split(",");
                    for (String physicalPathKeyReference: physicalPathKeyReferences)
                    {
                        JSONObject joReference = new JSONObject();
                        joReference.put("ReferencePhysicalPathKey", Long.parseLong(physicalPathKeyReference));

                        jaReferences.put(joReference);
                    }
                }
            }
            else if (task.getType().equalsIgnoreCase("Frame"))
            {
                if (task.getLabel() != null && !task.getLabel().equalsIgnoreCase(""))
                    jsonObject.put("Label", task.getLabel());
                else
                {
                    WorkflowIssue workflowIssue = new WorkflowIssue();
                    workflowIssue.setLabel("");
                    workflowIssue.setFieldName("Label");
                    workflowIssue.setTaskType(task.getType());
                    workflowIssue.setIssue("The field is not initialized");

                    workflowIssueList.add(workflowIssue);
                }

                if (task.getFrameInstantInSeconds() != null)
                    joParameters.put("InstantInSeconds", task.getFrameInstantInSeconds());
                if (task.getFrameWidth() != null)
                    joParameters.put("Width", task.getFrameWidth());
                if (task.getFrameHeight() != null)
                    joParameters.put("Height", task.getFrameHeight());
                if (task.getUserData() != null && !task.getUserData().equalsIgnoreCase(""))
                    joParameters.put("UserData", task.getUserData());
                if (task.getRetention() != null && !task.getRetention().equalsIgnoreCase(""))
                    joParameters.put("Retention", task.getRetention());
                if (task.getTitle() != null && !task.getTitle().equalsIgnoreCase(""))
                    joParameters.put("Title", task.getTitle());
                if (task.getUniqueName() != null && !task.getUniqueName().equalsIgnoreCase(""))
                    joParameters.put("UniqueName", task.getUniqueName());
                if (task.getIngester() != null && !task.getIngester().equalsIgnoreCase(""))
                    joParameters.put("Ingester", task.getIngester());
                if (task.getKeywords() != null && !task.getKeywords().equalsIgnoreCase(""))
                    joParameters.put("Keywords", task.getKeywords());
                if (task.getContentProviderName() != null && !task.getContentProviderName().equalsIgnoreCase(""))
                    joParameters.put("ContentProviderName", task.getContentProviderName());
                if (task.getDeliveryFileName() != null && !task.getDeliveryFileName().equalsIgnoreCase(""))
                    joParameters.put("DeliveryFileName", task.getDeliveryFileName());
                if (task.getStartPublishing() != null || task.getEndPublishing() != null)
                {
                    JSONObject joPublishing = new JSONObject();
                    joParameters.put("Publishing", joPublishing);

                    if (task.getStartPublishing() != null)
                        joPublishing.put("StartPublishing", dateFormat.format(task.getStartPublishing()));
                    else
                        joPublishing.put("StartPublishing", "NOW");
                    if (task.getEndPublishing() != null)
                        joPublishing.put("EndPublishing", dateFormat.format(task.getEndPublishing()));
                    else
                        joPublishing.put("EndPublishing", "FOREVER");
                }

                if (task.getReferences() != null && !task.getReferences().equalsIgnoreCase(""))
                {
                    JSONArray jaReferences = new JSONArray();
                    joParameters.put("References", jaReferences);

                    String [] physicalPathKeyReferences = task.getReferences().split(",");
                    for (String physicalPathKeyReference: physicalPathKeyReferences)
                    {
                        JSONObject joReference = new JSONObject();
                        joReference.put("ReferencePhysicalPathKey", Long.parseLong(physicalPathKeyReference));

                        jaReferences.put(joReference);
                    }
                }
            }
            else if (task.getType().equalsIgnoreCase("Periodical-Frames"))
            {
                if (task.getLabel() != null && !task.getLabel().equalsIgnoreCase(""))
                    jsonObject.put("Label", task.getLabel());
                else
                {
                    WorkflowIssue workflowIssue = new WorkflowIssue();
                    workflowIssue.setLabel("");
                    workflowIssue.setFieldName("Label");
                    workflowIssue.setTaskType(task.getType());
                    workflowIssue.setIssue("The field is not initialized");

                    workflowIssueList.add(workflowIssue);
                }

                if (task.getFramePeriodInSeconds() != null)
                    joParameters.put("PeriodInSeconds", task.getFramePeriodInSeconds());
                else
                {
                    WorkflowIssue workflowIssue = new WorkflowIssue();
                    workflowIssue.setLabel(task.getLabel());
                    workflowIssue.setFieldName("Label");
                    workflowIssue.setTaskType(task.getType());
                    workflowIssue.setIssue("The field is not initialized");

                    workflowIssueList.add(workflowIssue);
                }
                if (task.getStartTimeInSeconds() != null)
                    joParameters.put("StartTimeInSeconds", task.getStartTimeInSeconds());
                if (task.getFrameMaxFramesNumber() != null)
                    joParameters.put("MaxFramesNumber", task.getFrameMaxFramesNumber());
                if (task.getFrameWidth() != null)
                    joParameters.put("Width", task.getFrameWidth());
                if (task.getFrameHeight() != null)
                    joParameters.put("Height", task.getFrameHeight());
                if (task.getUserData() != null && !task.getUserData().equalsIgnoreCase(""))
                    joParameters.put("UserData", task.getUserData());
                if (task.getRetention() != null && !task.getRetention().equalsIgnoreCase(""))
                    joParameters.put("Retention", task.getRetention());
                if (task.getTitle() != null && !task.getTitle().equalsIgnoreCase(""))
                    joParameters.put("Title", task.getTitle());
                if (task.getUniqueName() != null && !task.getUniqueName().equalsIgnoreCase(""))
                    joParameters.put("UniqueName", task.getUniqueName());
                if (task.getIngester() != null && !task.getIngester().equalsIgnoreCase(""))
                    joParameters.put("Ingester", task.getIngester());
                if (task.getKeywords() != null && !task.getKeywords().equalsIgnoreCase(""))
                    joParameters.put("Keywords", task.getKeywords());
                if (task.getContentProviderName() != null && !task.getContentProviderName().equalsIgnoreCase(""))
                    joParameters.put("ContentProviderName", task.getContentProviderName());
                if (task.getDeliveryFileName() != null && !task.getDeliveryFileName().equalsIgnoreCase(""))
                    joParameters.put("DeliveryFileName", task.getDeliveryFileName());
                if (task.getStartPublishing() != null || task.getEndPublishing() != null)
                {
                    JSONObject joPublishing = new JSONObject();
                    joParameters.put("Publishing", joPublishing);

                    if (task.getStartPublishing() != null)
                        joPublishing.put("StartPublishing", dateFormat.format(task.getStartPublishing()));
                    else
                        joPublishing.put("StartPublishing", "NOW");
                    if (task.getEndPublishing() != null)
                        joPublishing.put("EndPublishing", dateFormat.format(task.getEndPublishing()));
                    else
                        joPublishing.put("EndPublishing", "FOREVER");
                }

                if (task.getReferences() != null && !task.getReferences().equalsIgnoreCase(""))
                {
                    JSONArray jaReferences = new JSONArray();
                    joParameters.put("References", jaReferences);

                    String [] physicalPathKeyReferences = task.getReferences().split(",");
                    for (String physicalPathKeyReference: physicalPathKeyReferences)
                    {
                        JSONObject joReference = new JSONObject();
                        joReference.put("ReferencePhysicalPathKey", Long.parseLong(physicalPathKeyReference));

                        jaReferences.put(joReference);
                    }
                }
            }
            else if (task.getType().equalsIgnoreCase("I-Frames"))
            {
                if (task.getLabel() != null && !task.getLabel().equalsIgnoreCase(""))
                    jsonObject.put("Label", task.getLabel());
                else
                {
                    WorkflowIssue workflowIssue = new WorkflowIssue();
                    workflowIssue.setLabel("");
                    workflowIssue.setFieldName("Label");
                    workflowIssue.setTaskType(task.getType());
                    workflowIssue.setIssue("The field is not initialized");

                    workflowIssueList.add(workflowIssue);
                }

                if (task.getStartTimeInSeconds() != null)
                    joParameters.put("StartTimeInSeconds", task.getStartTimeInSeconds());
                if (task.getFrameMaxFramesNumber() != null)
                    joParameters.put("MaxFramesNumber", task.getFrameMaxFramesNumber());
                if (task.getFrameWidth() != null)
                    joParameters.put("Width", task.getFrameWidth());
                if (task.getFrameHeight() != null)
                    joParameters.put("Height", task.getFrameHeight());
                if (task.getUserData() != null && !task.getUserData().equalsIgnoreCase(""))
                    joParameters.put("UserData", task.getUserData());
                if (task.getRetention() != null && !task.getRetention().equalsIgnoreCase(""))
                    joParameters.put("Retention", task.getRetention());
                if (task.getTitle() != null && !task.getTitle().equalsIgnoreCase(""))
                    joParameters.put("Title", task.getTitle());
                if (task.getUniqueName() != null && !task.getUniqueName().equalsIgnoreCase(""))
                    joParameters.put("UniqueName", task.getUniqueName());
                if (task.getIngester() != null && !task.getIngester().equalsIgnoreCase(""))
                    joParameters.put("Ingester", task.getIngester());
                if (task.getKeywords() != null && !task.getKeywords().equalsIgnoreCase(""))
                    joParameters.put("Keywords", task.getKeywords());
                if (task.getContentProviderName() != null && !task.getContentProviderName().equalsIgnoreCase(""))
                    joParameters.put("ContentProviderName", task.getContentProviderName());
                if (task.getDeliveryFileName() != null && !task.getDeliveryFileName().equalsIgnoreCase(""))
                    joParameters.put("DeliveryFileName", task.getDeliveryFileName());
                if (task.getStartPublishing() != null || task.getEndPublishing() != null)
                {
                    JSONObject joPublishing = new JSONObject();
                    joParameters.put("Publishing", joPublishing);

                    if (task.getStartPublishing() != null)
                        joPublishing.put("StartPublishing", dateFormat.format(task.getStartPublishing()));
                    else
                        joPublishing.put("StartPublishing", "NOW");
                    if (task.getEndPublishing() != null)
                        joPublishing.put("EndPublishing", dateFormat.format(task.getEndPublishing()));
                    else
                        joPublishing.put("EndPublishing", "FOREVER");
                }

                if (task.getReferences() != null && !task.getReferences().equalsIgnoreCase(""))
                {
                    JSONArray jaReferences = new JSONArray();
                    joParameters.put("References", jaReferences);

                    String [] physicalPathKeyReferences = task.getReferences().split(",");
                    for (String physicalPathKeyReference: physicalPathKeyReferences)
                    {
                        JSONObject joReference = new JSONObject();
                        joReference.put("ReferencePhysicalPathKey", Long.parseLong(physicalPathKeyReference));

                        jaReferences.put(joReference);
                    }
                }
            }
            else if (task.getType().equalsIgnoreCase("Motion-JPEG-by-Periodical-Frames"))
            {
                if (task.getLabel() != null && !task.getLabel().equalsIgnoreCase(""))
                    jsonObject.put("Label", task.getLabel());
                else
                {
                    WorkflowIssue workflowIssue = new WorkflowIssue();
                    workflowIssue.setLabel("");
                    workflowIssue.setFieldName("Label");
                    workflowIssue.setTaskType(task.getType());
                    workflowIssue.setIssue("The field is not initialized");

                    workflowIssueList.add(workflowIssue);
                }

                if (task.getFramePeriodInSeconds() != null)
                    joParameters.put("PeriodInSeconds", task.getFramePeriodInSeconds());
                else
                {
                    WorkflowIssue workflowIssue = new WorkflowIssue();
                    workflowIssue.setLabel(task.getLabel());
                    workflowIssue.setFieldName("Label");
                    workflowIssue.setTaskType(task.getType());
                    workflowIssue.setIssue("The field is not initialized");

                    workflowIssueList.add(workflowIssue);
                }
                if (task.getStartTimeInSeconds() != null)
                    joParameters.put("StartTimeInSeconds", task.getStartTimeInSeconds());
                if (task.getFrameMaxFramesNumber() != null)
                    joParameters.put("MaxFramesNumber", task.getFrameMaxFramesNumber());
                if (task.getFrameWidth() != null)
                    joParameters.put("Width", task.getFrameWidth());
                if (task.getFrameHeight() != null)
                    joParameters.put("Height", task.getFrameHeight());
                if (task.getUserData() != null && !task.getUserData().equalsIgnoreCase(""))
                    joParameters.put("UserData", task.getUserData());
                if (task.getRetention() != null && !task.getRetention().equalsIgnoreCase(""))
                    joParameters.put("Retention", task.getRetention());
                if (task.getTitle() != null && !task.getTitle().equalsIgnoreCase(""))
                    joParameters.put("Title", task.getTitle());
                if (task.getUniqueName() != null && !task.getUniqueName().equalsIgnoreCase(""))
                    joParameters.put("UniqueName", task.getUniqueName());
                if (task.getIngester() != null && !task.getIngester().equalsIgnoreCase(""))
                    joParameters.put("Ingester", task.getIngester());
                if (task.getKeywords() != null && !task.getKeywords().equalsIgnoreCase(""))
                    joParameters.put("Keywords", task.getKeywords());
                if (task.getContentProviderName() != null && !task.getContentProviderName().equalsIgnoreCase(""))
                    joParameters.put("ContentProviderName", task.getContentProviderName());
                if (task.getDeliveryFileName() != null && !task.getDeliveryFileName().equalsIgnoreCase(""))
                    joParameters.put("DeliveryFileName", task.getDeliveryFileName());
                if (task.getStartPublishing() != null || task.getEndPublishing() != null)
                {
                    JSONObject joPublishing = new JSONObject();
                    joParameters.put("Publishing", joPublishing);

                    if (task.getStartPublishing() != null)
                        joPublishing.put("StartPublishing", dateFormat.format(task.getStartPublishing()));
                    else
                        joPublishing.put("StartPublishing", "NOW");
                    if (task.getEndPublishing() != null)
                        joPublishing.put("EndPublishing", dateFormat.format(task.getEndPublishing()));
                    else
                        joPublishing.put("EndPublishing", "FOREVER");
                }

                if (task.getReferences() != null && !task.getReferences().equalsIgnoreCase(""))
                {
                    JSONArray jaReferences = new JSONArray();
                    joParameters.put("References", jaReferences);

                    String [] physicalPathKeyReferences = task.getReferences().split(",");
                    for (String physicalPathKeyReference: physicalPathKeyReferences)
                    {
                        JSONObject joReference = new JSONObject();
                        joReference.put("ReferencePhysicalPathKey", Long.parseLong(physicalPathKeyReference));

                        jaReferences.put(joReference);
                    }
                }
            }
            else if (task.getType().equalsIgnoreCase("Motion-JPEG-by-I-Frames"))
            {
                if (task.getLabel() != null && !task.getLabel().equalsIgnoreCase(""))
                    jsonObject.put("Label", task.getLabel());
                else
                {
                    WorkflowIssue workflowIssue = new WorkflowIssue();
                    workflowIssue.setLabel("");
                    workflowIssue.setFieldName("Label");
                    workflowIssue.setTaskType(task.getType());
                    workflowIssue.setIssue("The field is not initialized");

                    workflowIssueList.add(workflowIssue);
                }

                if (task.getStartTimeInSeconds() != null)
                    joParameters.put("StartTimeInSeconds", task.getStartTimeInSeconds());
                if (task.getFrameMaxFramesNumber() != null)
                    joParameters.put("MaxFramesNumber", task.getFrameMaxFramesNumber());
                if (task.getFrameWidth() != null)
                    joParameters.put("Width", task.getFrameWidth());
                if (task.getFrameHeight() != null)
                    joParameters.put("Height", task.getFrameHeight());
                if (task.getUserData() != null && !task.getUserData().equalsIgnoreCase(""))
                    joParameters.put("UserData", task.getUserData());
                if (task.getRetention() != null && !task.getRetention().equalsIgnoreCase(""))
                    joParameters.put("Retention", task.getRetention());
                if (task.getTitle() != null && !task.getTitle().equalsIgnoreCase(""))
                    joParameters.put("Title", task.getTitle());
                if (task.getUniqueName() != null && !task.getUniqueName().equalsIgnoreCase(""))
                    joParameters.put("UniqueName", task.getUniqueName());
                if (task.getIngester() != null && !task.getIngester().equalsIgnoreCase(""))
                    joParameters.put("Ingester", task.getIngester());
                if (task.getKeywords() != null && !task.getKeywords().equalsIgnoreCase(""))
                    joParameters.put("Keywords", task.getKeywords());
                if (task.getContentProviderName() != null && !task.getContentProviderName().equalsIgnoreCase(""))
                    joParameters.put("ContentProviderName", task.getContentProviderName());
                if (task.getDeliveryFileName() != null && !task.getDeliveryFileName().equalsIgnoreCase(""))
                    joParameters.put("DeliveryFileName", task.getDeliveryFileName());
                if (task.getStartPublishing() != null || task.getEndPublishing() != null)
                {
                    JSONObject joPublishing = new JSONObject();
                    joParameters.put("Publishing", joPublishing);

                    if (task.getStartPublishing() != null)
                        joPublishing.put("StartPublishing", dateFormat.format(task.getStartPublishing()));
                    else
                        joPublishing.put("StartPublishing", "NOW");
                    if (task.getEndPublishing() != null)
                        joPublishing.put("EndPublishing", dateFormat.format(task.getEndPublishing()));
                    else
                        joPublishing.put("EndPublishing", "FOREVER");
                }

                if (task.getReferences() != null && !task.getReferences().equalsIgnoreCase(""))
                {
                    JSONArray jaReferences = new JSONArray();
                    joParameters.put("References", jaReferences);

                    String [] physicalPathKeyReferences = task.getReferences().split(",");
                    for (String physicalPathKeyReference: physicalPathKeyReferences)
                    {
                        JSONObject joReference = new JSONObject();
                        joReference.put("ReferencePhysicalPathKey", Long.parseLong(physicalPathKeyReference));

                        jaReferences.put(joReference);
                    }
                }
            }
            else if (task.getType().equalsIgnoreCase("Slideshow"))
            {
                if (task.getLabel() != null && !task.getLabel().equalsIgnoreCase(""))
                    jsonObject.put("Label", task.getLabel());
                else
                {
                    WorkflowIssue workflowIssue = new WorkflowIssue();
                    workflowIssue.setLabel("");
                    workflowIssue.setFieldName("Label");
                    workflowIssue.setTaskType(task.getType());
                    workflowIssue.setIssue("The field is not initialized");

                    workflowIssueList.add(workflowIssue);
                }

                if (task.getFrameDurationOfEachSlideInSeconds() != null)
                    joParameters.put("DurationOfEachSlideInSeconds", task.getFrameDurationOfEachSlideInSeconds());
                if (task.getFrameOutputFrameRate() != null)
                    joParameters.put("OutputFrameRate", task.getFrameOutputFrameRate());
                if (task.getUserData() != null && !task.getUserData().equalsIgnoreCase(""))
                    joParameters.put("UserData", task.getUserData());
                if (task.getRetention() != null && !task.getRetention().equalsIgnoreCase(""))
                    joParameters.put("Retention", task.getRetention());
                if (task.getTitle() != null && !task.getTitle().equalsIgnoreCase(""))
                    joParameters.put("Title", task.getTitle());
                if (task.getUniqueName() != null && !task.getUniqueName().equalsIgnoreCase(""))
                    joParameters.put("UniqueName", task.getUniqueName());
                if (task.getIngester() != null && !task.getIngester().equalsIgnoreCase(""))
                    joParameters.put("Ingester", task.getIngester());
                if (task.getKeywords() != null && !task.getKeywords().equalsIgnoreCase(""))
                    joParameters.put("Keywords", task.getKeywords());
                if (task.getContentProviderName() != null && !task.getContentProviderName().equalsIgnoreCase(""))
                    joParameters.put("ContentProviderName", task.getContentProviderName());
                if (task.getDeliveryFileName() != null && !task.getDeliveryFileName().equalsIgnoreCase(""))
                    joParameters.put("DeliveryFileName", task.getDeliveryFileName());
                if (task.getStartPublishing() != null || task.getEndPublishing() != null)
                {
                    JSONObject joPublishing = new JSONObject();
                    joParameters.put("Publishing", joPublishing);

                    if (task.getStartPublishing() != null)
                        joPublishing.put("StartPublishing", dateFormat.format(task.getStartPublishing()));
                    else
                        joPublishing.put("StartPublishing", "NOW");
                    if (task.getEndPublishing() != null)
                        joPublishing.put("EndPublishing", dateFormat.format(task.getEndPublishing()));
                    else
                        joPublishing.put("EndPublishing", "FOREVER");
                }

                if (task.getReferences() != null && !task.getReferences().equalsIgnoreCase(""))
                {
                    JSONArray jaReferences = new JSONArray();
                    joParameters.put("References", jaReferences);

                    String [] physicalPathKeyReferences = task.getReferences().split(",");
                    for (String physicalPathKeyReference: physicalPathKeyReferences)
                    {
                        JSONObject joReference = new JSONObject();
                        joReference.put("ReferencePhysicalPathKey", Long.parseLong(physicalPathKeyReference));

                        jaReferences.put(joReference);
                    }
                }
            }
            else if (task.getType().equalsIgnoreCase("Encode"))
            {
                jsonObject.put("Label", task.getLabel());

                if (task.getEncodingPriority() != null && !task.getEncodingPriority().equalsIgnoreCase(""))
                    joParameters.put("EncodingPriority", task.getEncodingPriority());
                if (task.getEncodingProfileType().equalsIgnoreCase("profilesSet"))
                {
                    if (task.getEncodingProfilesSetLabel() != null && !task.getEncodingProfilesSetLabel().equalsIgnoreCase(""))
                        joParameters.put("EncodingProfilesSetLabel", task.getEncodingProfilesSetLabel());
                    else
                    {
                        WorkflowIssue workflowIssue = new WorkflowIssue();
                        workflowIssue.setLabel(task.getLabel());
                        workflowIssue.setFieldName("EncodingProfilesSetLabel");
                        workflowIssue.setTaskType(task.getType());
                        workflowIssue.setIssue("The field is not initialized");

                        workflowIssueList.add(workflowIssue);
                    }
                }
                else if (task.getEncodingProfileType().equalsIgnoreCase("singleProfile"))
                {
                    if (task.getEncodingProfileLabel() != null && !task.getEncodingProfileLabel().equalsIgnoreCase(""))
                        joParameters.put("EncodingProfileLabel", task.getEncodingProfileLabel());
                    else
                    {
                        WorkflowIssue workflowIssue = new WorkflowIssue();
                        workflowIssue.setLabel(task.getLabel());
                        workflowIssue.setFieldName("EncodingProfileLabel");
                        workflowIssue.setTaskType(task.getType());
                        workflowIssue.setIssue("The field is not initialized");

                        workflowIssueList.add(workflowIssue);
                    }
                }
                else
                {
                    mLogger.error("Unknown task.getEncodingProfileType: " + task.getEncodingProfileType());
                }

                if (task.getReferences() != null && !task.getReferences().equalsIgnoreCase(""))
                {
                    JSONArray jaReferences = new JSONArray();
                    joParameters.put("References", jaReferences);

                    String [] physicalPathKeyReferences = task.getReferences().split(",");
                    for (String physicalPathKeyReference: physicalPathKeyReferences)
                    {
                        JSONObject joReference = new JSONObject();
                        joReference.put("ReferencePhysicalPathKey", Long.parseLong(physicalPathKeyReference));

                        jaReferences.put(joReference);
                    }
                }
            }
            else if (task.getType().equalsIgnoreCase("Email-Notification"))
            {
                jsonObject.put("Label", task.getLabel());

                if (task.getEmailAddress() != null && !task.getEmailAddress().equalsIgnoreCase(""))
                    joParameters.put("EmailAddress", task.getEmailAddress());
                else
                {
                    WorkflowIssue workflowIssue = new WorkflowIssue();
                    workflowIssue.setLabel(task.getLabel());
                    workflowIssue.setFieldName("EmailAddress");
                    workflowIssue.setTaskType(task.getType());
                    workflowIssue.setIssue("The field is not initialized");

                    workflowIssueList.add(workflowIssue);
                }
                if (task.getSubject() != null && !task.getSubject().equalsIgnoreCase(""))
                    joParameters.put("Subject", task.getSubject());
                else
                {
                    WorkflowIssue workflowIssue = new WorkflowIssue();
                    workflowIssue.setLabel(task.getLabel());
                    workflowIssue.setFieldName("Subject");
                    workflowIssue.setTaskType(task.getType());
                    workflowIssue.setIssue("The field is not initialized");

                    workflowIssueList.add(workflowIssue);
                }
                if (task.getMessage() != null && !task.getMessage().equalsIgnoreCase(""))
                    joParameters.put("Message", task.getMessage());
                else
                {
                    WorkflowIssue workflowIssue = new WorkflowIssue();
                    workflowIssue.setLabel(task.getLabel());
                    workflowIssue.setFieldName("Message");
                    workflowIssue.setTaskType(task.getType());
                    workflowIssue.setIssue("The field is not initialized");

                    workflowIssueList.add(workflowIssue);
                }
            }
            else
            {
                mLogger.error("Unknonw task.getType(): " + task.getType());
            }

            for (TreeNode tnEvent: tnTreeNode.getChildren())
            {
                if (tnEvent.getType().equalsIgnoreCase("Event"))
                {
                    Event event = (Event) tnEvent.getData();

                    JSONObject joEvent = new JSONObject();;
                    if (event.getType().equalsIgnoreCase("OnSuccess"))
                    {
                        jsonObject.put("OnSuccess", joEvent);
                    }
                    else if (event.getType().equalsIgnoreCase("OnError"))
                    {
                        jsonObject.put("OnError", joEvent);
                    }
                    else if (event.getType().equalsIgnoreCase("OnComplete"))
                    {
                        jsonObject.put("OnComplete", joEvent);
                    }
                    else
                    {
                        mLogger.error("Unknonw task.getType(): " + task.getType());
                    }

                    if (tnEvent.getChildren().size() > 0)
                        joEvent.put("Task", buildTask(tnEvent.getChildren().get(0)));
                }
            }

            return jsonObject;
        }
        catch (Exception e)
        {
            mLogger.error("buildTask failed. Exception: " + e);

            throw e;
        }
    }

    public void ingestWorkflow()
    {
        String username;
        String password;

        ingestWorkflowErrorMessage = null;

        try
        {
            Long userKey = SessionUtils.getUserKey();
            String apiKey = SessionUtils.getAPIKey();

            username = userKey.toString();
            password = apiKey;

            CatraMMS catraMMS = new CatraMMS();

            ingestionJobList.clear();

            workflowRoot = catraMMS.ingestWorkflow(username, password,
                    jsonWorkflow, ingestionJobList);
        }
        catch (Exception e)
        {
            String errorMessage = "Exception: " + e;
            mLogger.error(errorMessage);

            ingestWorkflowErrorMessage = errorMessage;

            FacesMessage message = new FacesMessage(FacesMessage.SEVERITY_ERROR, "Ingestion",
                    "Ingestion Workflow failed: " + errorMessage);
            FacesContext.getCurrentInstance().addMessage(null, message);

            return;
        }

        for (PushContent pushContent: pushContentList)
        {
            try
            {
                IngestionResult pushContentIngestionTask = null;
                for (IngestionResult ingestionTaskResult: ingestionJobList)
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

                File binaryPathName = new File(pushContent.getBinaryPathName());

                CatraMMS catraMMS = new CatraMMS();
                catraMMS.ingestBinaryContent(username, password,
                        binaryPathName, pushContentIngestionTask.getKey());

                binaryPathName.delete();
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

    /*
    public void setWorkflowProperties()
    {
        if (tnSelectedNode != null)
        {
            if (tnSelectedNode.getData() instanceof Workflow)
            {
                tnWorkingNode = tnSelectedNode;

                Workflow workflow = (Workflow) tnWorkingNode.getData();

                workflowLabel = workflow.getLabel();
            }
            else
            {
                FacesMessage message = new FacesMessage(FacesMessage.SEVERITY_ERROR, "Workflow",
                        "A Workflow node is not selected");
                FacesContext.getCurrentInstance().addMessage(null, message);
            }
        }
        else
        {
            FacesMessage message = new FacesMessage(FacesMessage.SEVERITY_ERROR, "Workflow",
                    "A Workflow node is not selected");
            FacesContext.getCurrentInstance().addMessage(null, message);
        }
    }
    */

    public void saveWorkflowProperties()
    {
        if (tnWorkingNode != null)
        {
            Workflow workflow = (Workflow) tnWorkingNode.getData();

            workflow.setLabel(workflowLabel);

            buildWorkflow();

            // tnWorkingNode = null;

            FacesMessage message = new FacesMessage(FacesMessage.SEVERITY_INFO, "Workflow",
                    "The Workflow properties was saved");
            FacesContext.getCurrentInstance().addMessage(null, message);
        }
        else
        {
            FacesMessage message = new FacesMessage(FacesMessage.SEVERITY_ERROR, "Workflow",
                    "A Workflow node is not selected");
            FacesContext.getCurrentInstance().addMessage(null, message);
        }
    }

    public TreeNode addGroupOfTasks()
    {
        mLogger.info("addGroupOfTasks");

        TreeNode tnTaskNode = null;

        if(tnSelectedNode != null)
        {
            if (tnSelectedNode.getType().equalsIgnoreCase("Workflow"))
            {
                Workflow workflow = (Workflow) tnSelectedNode.getData();
                if (workflow.isChildTaskCreated())
                {
                    mLogger.error("No more Tasks are allowed here");

                    FacesMessage message = new FacesMessage(FacesMessage.SEVERITY_ERROR, "Workflow",
                            "No more Tasks are allowed here");
                    FacesContext.getCurrentInstance().addMessage(null, message);

                    return null;
                }
                else
                {
                    workflow.setChildTaskCreated(true);
                }
            }
            else if (tnSelectedNode.getType().equalsIgnoreCase("Event"))
            {
                Event event = (Event) tnSelectedNode.getData();
                if (event.isChildTaskCreated())
                {
                    mLogger.error("No more Tasks are allowed here");

                    FacesMessage message = new FacesMessage(FacesMessage.SEVERITY_ERROR, "Event",
                            "No more Tasks are allowed here");
                    FacesContext.getCurrentInstance().addMessage(null, message);

                    return null;
                }
                else
                {
                    event.setChildTaskCreated(true);
                }
            }
            else // it is a task
            {
                mLogger.error("A Task cannot be a child of a Task, create before an Event");

                FacesMessage message = new FacesMessage(FacesMessage.SEVERITY_ERROR, "Task",
                        "A Task cannot be a child of a Task, create before an Event");
                FacesContext.getCurrentInstance().addMessage(null, message);

                return null;
            }

            Task task = new Task();
            task.setGroupOfTaskExecutionType("parallel");
            task.setType("GroupOfTasks");

            tnTaskNode = new DefaultTreeNode(task, tnSelectedNode);
            tnTaskNode.setType("GroupOfTasks");
            tnTaskNode.setExpanded(true);

            // Next because it will be opened the Task Dialog properties
            /* 2018-06-24: Commented because it is not opened anymore the dialog
            {
                TreeNode tnCurrentSelectedNode = tnSelectedNode;
                tnSelectedNode = tnTaskNode;
                setNodeProperties();
                tnSelectedNode = tnCurrentSelectedNode;
            }
            */

            buildWorkflow();

            return tnTaskNode;
        }
        else
        {
            mLogger.error("tnSelectedNode is null");

            return null;
        }
    }

    public TreeNode addTask(String taskType)
    {
        mLogger.info("addTask: " + taskType);

        TreeNode tnTaskNode = null;

        if(tnSelectedNode != null)
        {
            if (tnSelectedNode.getType().equalsIgnoreCase("Workflow"))
            {
                Workflow workflow = (Workflow) tnSelectedNode.getData();
                if (workflow.isChildTaskCreated())
                {
                    FacesMessage message = new FacesMessage(FacesMessage.SEVERITY_ERROR, "Workflow",
                            "No more Tasks are allowed here");
                    FacesContext.getCurrentInstance().addMessage(null, message);

                    return null;
                }
                else
                {
                    workflow.setChildTaskCreated(true);
                }
            }
            else if (tnSelectedNode.getType().equalsIgnoreCase("Event"))
            {
                Event event = (Event) tnSelectedNode.getData();
                if (event.isChildTaskCreated())
                {
                    FacesMessage message = new FacesMessage(FacesMessage.SEVERITY_ERROR, "Event",
                            "No more Tasks are allowed here");
                    FacesContext.getCurrentInstance().addMessage(null, message);

                    return null;
                }
                else
                {
                    event.setChildTaskCreated(true);
                }
            }
            else if (tnSelectedNode.getType().equalsIgnoreCase("GroupOfTasks"))
            {
                // nothing to do
            }
            else // it is a task
            {
                FacesMessage message = new FacesMessage(FacesMessage.SEVERITY_ERROR, "Task",
                        "A Task cannot be a child of a Task, create before an Event");
                FacesContext.getCurrentInstance().addMessage(null, message);

                return null;
            }

            Task task = new Task();
            task.setLabel("Task nr. " + taskNumber++);
            task.setType(taskType);

            // some initialization here because otherwise the buildWorkflow (json), will fail
            {
                if (task.getType().equalsIgnoreCase("Add-Content"))
                {
                    task.setSourceDownloadType("pull");
                }
                else if (task.getType().equalsIgnoreCase("Encode"))
                {
                    task.setEncodingProfileType("profilesSet");
                }
                else if (task.getType().equalsIgnoreCase("Cut"))
                {
                    task.setCutEndType("endTime");
                }
            }

            tnTaskNode = new DefaultTreeNode(task, tnSelectedNode);
            tnTaskNode.setType("Task");
            tnTaskNode.setExpanded(true);

            // Next because it will be opened the Task Dialog properties
            /* 2018-06-24: Commented because it is not opened anymore the dialog
            {
                TreeNode tnCurrentSelectedNode = tnSelectedNode;
                tnSelectedNode = tnTaskNode;
                setNodeProperties();
                tnSelectedNode = tnCurrentSelectedNode;
            }
            */

            buildWorkflow();

            return tnTaskNode;
        }
        else
        {
            mLogger.error("tnSelectedNode is null");

            return null;
        }
    }

    public TreeNode addOnSuccessEvent()
    {
        mLogger.info("addOnSuccessEvent");

        if(tnSelectedNode != null)
        {
            if (tnSelectedNode.getType().equalsIgnoreCase("Workflow"))
            {
                Workflow workflow = (Workflow) tnSelectedNode.getData();
                if (workflow.isChildEventOnSuccessCreated())
                {
                    FacesMessage message = new FacesMessage(FacesMessage.SEVERITY_ERROR, "Workflow",
                            "No more OnSuccess Event are allowed here");
                    FacesContext.getCurrentInstance().addMessage(null, message);

                    return null;
                }
                else
                {
                    workflow.setChildEventOnSuccessCreated(true);
                }
            }
            else if (tnSelectedNode.getType().equalsIgnoreCase("Event"))
            {
                FacesMessage message = new FacesMessage(FacesMessage.SEVERITY_ERROR, "Event",
                        "No more Event are allowed here");
                FacesContext.getCurrentInstance().addMessage(null, message);

                return null;
            }
            else // it is a task
            {
                Task task = (Task) tnSelectedNode.getData();
                if (task.isChildEventOnSuccessCreated())
                {
                    FacesMessage message = new FacesMessage(FacesMessage.SEVERITY_ERROR, "Event",
                            "No more OnSuccess Event are allowed here");
                    FacesContext.getCurrentInstance().addMessage(null, message);

                    return null;
                }
                else
                {
                    task.setChildEventOnSuccessCreated(true);
                }
            }

            Event event = new Event();
            event.setType("OnSuccess");

            TreeNode tnEventNode = new DefaultTreeNode(event, tnSelectedNode);
            tnEventNode.setType("Event");
            tnEventNode.setExpanded(true);

            buildWorkflow();

            return tnEventNode;
        }
        else
        {
            mLogger.error("tnSelectedNode is null");

            return null;
        }
    }

    public TreeNode addOnErrorEvent()
    {
        mLogger.info("addOnErrorEvent");

        if(tnSelectedNode != null)
        {
            if (tnSelectedNode.getType().equalsIgnoreCase("Workflow"))
            {
                Workflow workflow = (Workflow) tnSelectedNode.getData();
                if (workflow.isChildEventOnErrorCreated())
                {
                    FacesMessage message = new FacesMessage(FacesMessage.SEVERITY_ERROR, "Workflow",
                            "No more OnError Event are allowed here");
                    FacesContext.getCurrentInstance().addMessage(null, message);

                    return null;
                }
                else
                {
                    workflow.setChildEventOnErrorCreated(true);
                }
            }
            else if (tnSelectedNode.getType().equalsIgnoreCase("Event"))
            {
                FacesMessage message = new FacesMessage(FacesMessage.SEVERITY_ERROR, "Event",
                        "No more Event are allowed here");
                FacesContext.getCurrentInstance().addMessage(null, message);

                return null;
            }
            else // it is a task
            {
                Task task = (Task) tnSelectedNode.getData();
                if (task.isChildEventOnErrorCreated())
                {
                    FacesMessage message = new FacesMessage(FacesMessage.SEVERITY_ERROR, "Event",
                            "No more OnError Event are allowed here");
                    FacesContext.getCurrentInstance().addMessage(null, message);

                    return null;
                }
                else
                {
                    task.setChildEventOnErrorCreated(true);
                }
            }

            Event event = new Event();
            event.setType("OnError");

            TreeNode tnEventNode = new DefaultTreeNode(event, tnSelectedNode);
            tnEventNode.setType("Event");
            tnEventNode.setExpanded(true);

            buildWorkflow();

            return tnEventNode;
        }
        else
        {
            mLogger.error("tnSelectedNode is null");

            return null;
        }
    }

    public TreeNode addOnCompleteEvent()
    {
        mLogger.info("addOnCompleteEvent");

        if(tnSelectedNode != null)
        {
            if (tnSelectedNode.getType().equalsIgnoreCase("Workflow"))
            {
                Workflow workflow = (Workflow) tnSelectedNode.getData();
                if (workflow.isChildEventOnCompleteCreated())
                {
                    FacesMessage message = new FacesMessage(FacesMessage.SEVERITY_ERROR, "Workflow",
                            "No more OnComplete Event are allowed here");
                    FacesContext.getCurrentInstance().addMessage(null, message);

                    return null;
                }
                else
                {
                    workflow.setChildEventOnCompleteCreated(true);
                }
            }
            else if (tnSelectedNode.getType().equalsIgnoreCase("Event"))
            {
                FacesMessage message = new FacesMessage(FacesMessage.SEVERITY_ERROR, "Event",
                        "No more Event are allowed here");
                FacesContext.getCurrentInstance().addMessage(null, message);

                return null;
            }
            else // it is a task
            {
                Task task = (Task) tnSelectedNode.getData();
                if (task.isChildEventOnCompleteCreated())
                {
                    FacesMessage message = new FacesMessage(FacesMessage.SEVERITY_ERROR, "Event",
                            "No more OnComplete Event are allowed here");
                    FacesContext.getCurrentInstance().addMessage(null, message);

                    return null;
                }
                else
                {
                    task.setChildEventOnCompleteCreated(true);
                }
            }

            Event event = new Event();
            event.setType("OnComplete");

            TreeNode tnEventNode = new DefaultTreeNode(event, tnSelectedNode);
            tnEventNode.setType("Event");
            tnEventNode.setExpanded(true);

            buildWorkflow();

            return tnEventNode;
        }
        else
        {
            mLogger.error("tnSelectedNode is null");

            return null;
        }
    }

    public void setNodeProperties()
    {
        mLogger.info("setNodeProperties");
        if (tnSelectedNode != null)
        {
            if (tnSelectedNode.getData() instanceof Event)
            {
                nodeType = "Event";
            }
            else if (tnSelectedNode.getData() instanceof Workflow)
            {
                tnWorkingNode = tnSelectedNode;

                Workflow workflow = (Workflow) tnWorkingNode.getData();

                nodeType = "Workflow";

                workflowLabel = workflow.getLabel();
            }
            else if (tnSelectedNode.getData() instanceof Task)
            {
                String userName = SessionUtils.getUserName();

                tnWorkingNode = tnSelectedNode;

                Task task = (Task) tnWorkingNode.getData();

                nodeType = task.getType();

                if (task.getType().equalsIgnoreCase("GroupOfTasks"))
                {
                    groupOfTaskExecutionType = task.getGroupOfTaskExecutionType();
                }
                else if (task.getType().equalsIgnoreCase("Add-Content"))
                {
                    taskLabel = task.getLabel();
                    if (task.getSourceDownloadType() != null
                            && !task.getSourceDownloadType().equalsIgnoreCase(""))
                        taskSourceDownloadType = task.getSourceDownloadType();
                    else
                        taskSourceDownloadType = "pull";
                    taskPullSourceURL = task.getPullSourceURL();
                    taskPushBinaryPathName = task.getPushBinaryPathName();
                    taskFileFormat = task.getFileFormat();
                    taskUserData = task.getUserData();
                    taskRetention = task.getRetention();
                    taskTitle = task.getTitle();
                    taskUniqueName = task.getUniqueName();
                    if (task.getIngester() != null && !task.getIngester().equalsIgnoreCase(""))
                        taskIngester = task.getIngester();
                    else
                        taskIngester = userName;
                    taskKeywords = task.getKeywords();
                    taskMD5FileChecksum = task.getMd5FileCheckSum();
                    taskFileSizeInBytes = task.getFileSizeInBytes();
                    taskContentProviderName = task.getContentProviderName();
                    taskDeliveryFileName = task.getDeliveryFileName();
                    taskStartPublishing = task.getStartPublishing();
                    taskEndPublishing = task.getEndPublishing();
                }
                else if (task.getType().equalsIgnoreCase("Remove-Content"))
                {
                    taskReferences = task.getReferences() == null ? "" : task.getReferences();
                    taskLabel = task.getLabel();

                    {
                        mLogger.info("Initializing mediaItems...");

                        mediaItemsList.clear();
                        mediaItemsSelectedList.clear();
                        mediaItemsSelectionMode = "multiple";
                        mediaItemsMaxMediaItemsNumber = new Long(100);
                        {
                            mediaItemsContentTypesList.clear();
                            mediaItemsContentTypesList.add("video");
                            mediaItemsContentTypesList.add("audio");
                            mediaItemsContentTypesList.add("image");

                            mediaItemsContentType = mediaItemsContentTypesList.get(0);
                        }

                        fillMediaItems();
                    }
                }
                else if (task.getType().equalsIgnoreCase("Concat-Demuxer"))
                {
                    taskReferences = task.getReferences() == null ? "" : task.getReferences();
                    taskLabel = task.getLabel();
                    taskUserData = task.getUserData();
                    taskRetention = task.getRetention();
                    taskTitle = task.getTitle();
                    taskUniqueName = task.getUniqueName();
                    if (task.getIngester() != null && !task.getIngester().equalsIgnoreCase(""))
                        taskIngester = task.getIngester();
                    else
                        taskIngester = userName;
                    taskKeywords = task.getKeywords();
                    taskContentProviderName= task.getContentProviderName();
                    taskDeliveryFileName= task.getDeliveryFileName();
                    taskStartPublishing= task.getStartPublishing();
                    taskEndPublishing= task.getEndPublishing();

                    {
                        mLogger.info("Initializing mediaItems...");

                        mediaItemsList.clear();
                        mediaItemsSelectedList.clear();
                        mediaItemsSelectionMode = "multiple";
                        mediaItemsMaxMediaItemsNumber = new Long(100);
                        {
                            mediaItemsContentTypesList.clear();
                            mediaItemsContentTypesList.add("video");
                            mediaItemsContentTypesList.add("audio");

                            mediaItemsContentType = mediaItemsContentTypesList.get(0);
                        }

                        fillMediaItems();
                    }
                }
                else if (task.getType().equalsIgnoreCase("Cut"))
                {
                    taskReferences = task.getReferences() == null ? "" : task.getReferences();
                    taskLabel = task.getLabel();
                    if (task.getCutEndType() != null
                            && !task.getCutEndType().equalsIgnoreCase(""))
                        taskCutEndType = task.getCutEndType();
                    else
                        taskCutEndType = "endTime";
                    taskStartTimeInSeconds = task.getStartTimeInSeconds();
                    taskEndTimeInSeconds = task.getEndTimeInSeconds();
                    taskFramesNumber = task.getFramesNumber();
                    taskUserData = task.getUserData();
                    taskRetention = task.getRetention();
                    taskTitle = task.getTitle();
                    taskUniqueName = task.getUniqueName();
                    if (task.getIngester() != null && !task.getIngester().equalsIgnoreCase(""))
                        taskIngester = task.getIngester();
                    else
                        taskIngester = userName;
                    taskKeywords = task.getKeywords();
                    taskContentProviderName= task.getContentProviderName();
                    taskDeliveryFileName= task.getDeliveryFileName();
                    taskStartPublishing= task.getStartPublishing();
                    taskEndPublishing= task.getEndPublishing();

                    {
                        mLogger.info("Initializing mediaItems...");

                        mediaItemsList.clear();
                        mediaItemsSelectedList.clear();
                        mediaItemsSelectionMode = "single";
                        mediaItemsMaxMediaItemsNumber = new Long(100);
                        {
                            mediaItemsContentTypesList.clear();
                            mediaItemsContentTypesList.add("video");
                            mediaItemsContentTypesList.add("audio");

                            mediaItemsContentType = mediaItemsContentTypesList.get(0);
                        }

                        fillMediaItems();
                    }
                }
                else if (task.getType().equalsIgnoreCase("Overlay-Image-On-Video"))
                {
                    taskReferences = task.getReferences() == null ? "" : task.getReferences();
                    taskLabel = task.getLabel();
                    taskOverlayPositionXInPixel = task.getOverlayPositionXInPixel();
                    taskOverlayPositionYInPixel = task.getOverlayPositionYInPixel();
                    taskUserData = task.getUserData();
                    taskRetention = task.getRetention();
                    taskTitle = task.getTitle();
                    taskUniqueName = task.getUniqueName();
                    if (task.getIngester() != null && !task.getIngester().equalsIgnoreCase(""))
                        taskIngester = task.getIngester();
                    else
                        taskIngester = userName;
                    taskKeywords = task.getKeywords();
                    taskContentProviderName= task.getContentProviderName();
                    taskDeliveryFileName= task.getDeliveryFileName();
                    taskStartPublishing= task.getStartPublishing();
                    taskEndPublishing= task.getEndPublishing();

                    {
                        mLogger.info("Initializing mediaItems...");

                        mediaItemsList.clear();
                        mediaItemsSelectedList.clear();
                        mediaItemsSelectionMode = "multiple";
                        mediaItemsMaxMediaItemsNumber = new Long(100);
                        {
                            mediaItemsContentTypesList.clear();
                            mediaItemsContentTypesList.add("video");
                            mediaItemsContentTypesList.add("image");

                            mediaItemsContentType = mediaItemsContentTypesList.get(0);
                        }

                        fillMediaItems();
                    }
                }
                else if (task.getType().equalsIgnoreCase("Overlay-Text-On-Video"))
                {
                    taskReferences = task.getReferences() == null ? "" : task.getReferences();
                    taskLabel = task.getLabel();
                    taskOverlayText = task.getOverlayText();
                    taskOverlayPositionXInPixel = task.getOverlayPositionXInPixel();
                    taskOverlayPositionYInPixel = task.getOverlayPositionYInPixel();
                    taskOverlayFontType = task.getOverlayFontType();
                    taskOverlayFontSize = task.getOverlayFontSize();
                    taskOverlayFontColor = task.getOverlayFontColor();
                    taskOverlayTextPercentageOpacity = task.getOverlayTextPercentageOpacity();
                    taskOverlayBoxEnable = task.getOverlayBoxEnable();
                    taskOverlayBoxColor = task.getOverlayBoxColor();
                    taskOverlayBoxPercentageOpacity = task.getOverlayBoxPercentageOpacity();
                    taskEncodingPriority = task.getEncodingPriority();
                    taskUserData = task.getUserData();
                    taskRetention = task.getRetention();
                    taskTitle = task.getTitle();
                    taskUniqueName = task.getUniqueName();
                    if (task.getIngester() != null && !task.getIngester().equalsIgnoreCase(""))
                        taskIngester = task.getIngester();
                    else
                        taskIngester = userName;
                    taskKeywords = task.getKeywords();
                    taskContentProviderName= task.getContentProviderName();
                    taskDeliveryFileName= task.getDeliveryFileName();
                    taskStartPublishing= task.getStartPublishing();
                    taskEndPublishing= task.getEndPublishing();

                    {
                        mLogger.info("Initializing mediaItems...");

                        mediaItemsList.clear();
                        mediaItemsSelectedList.clear();
                        mediaItemsSelectionMode = "single";
                        mediaItemsMaxMediaItemsNumber = new Long(100);
                        {
                            mediaItemsContentTypesList.clear();
                            mediaItemsContentTypesList.add("video");

                            mediaItemsContentType = mediaItemsContentTypesList.get(0);
                        }

                        fillMediaItems();
                    }
                }
                else if (task.getType().equalsIgnoreCase("Frame"))
                {
                    taskReferences = task.getReferences() == null ? "" : task.getReferences();
                    taskLabel = task.getLabel();
                    taskFrameInstantInSeconds = task.getFrameInstantInSeconds();
                    taskFrameWidth = task.getFrameWidth();
                    taskFrameHeight = task.getFrameHeight();
                    taskUserData = task.getUserData();
                    taskRetention = task.getRetention();
                    taskTitle = task.getTitle();
                    taskUniqueName = task.getUniqueName();
                    if (task.getIngester() != null && !task.getIngester().equalsIgnoreCase(""))
                        taskIngester = task.getIngester();
                    else
                        taskIngester = userName;
                    taskKeywords = task.getKeywords();
                    taskContentProviderName= task.getContentProviderName();
                    taskDeliveryFileName= task.getDeliveryFileName();
                    taskStartPublishing= task.getStartPublishing();
                    taskEndPublishing= task.getEndPublishing();

                    {
                        mLogger.info("Initializing mediaItems...");

                        mediaItemsList.clear();
                        mediaItemsSelectedList.clear();
                        mediaItemsSelectionMode = "single";
                        mediaItemsMaxMediaItemsNumber = new Long(100);
                        {
                            mediaItemsContentTypesList.clear();
                            mediaItemsContentTypesList.add("video");

                            mediaItemsContentType = mediaItemsContentTypesList.get(0);
                        }

                        fillMediaItems();
                    }
                }
                else if (task.getType().equalsIgnoreCase("Periodical-Frames"))
                {
                    taskReferences = task.getReferences() == null ? "" : task.getReferences();
                    taskLabel = task.getLabel();
                    taskFramePeriodInSeconds = task.getFramePeriodInSeconds();
                    taskStartTimeInSeconds = task.getStartTimeInSeconds();
                    taskFrameMaxFramesNumber = task.getFrameMaxFramesNumber();
                    taskFrameWidth = task.getFrameWidth();
                    taskFrameHeight = task.getFrameHeight();
                    taskUserData = task.getUserData();
                    taskRetention = task.getRetention();
                    taskTitle = task.getTitle();
                    taskUniqueName = task.getUniqueName();
                    if (task.getIngester() != null && !task.getIngester().equalsIgnoreCase(""))
                        taskIngester = task.getIngester();
                    else
                        taskIngester = userName;
                    taskKeywords = task.getKeywords();
                    taskContentProviderName= task.getContentProviderName();
                    taskDeliveryFileName= task.getDeliveryFileName();
                    taskStartPublishing= task.getStartPublishing();
                    taskEndPublishing= task.getEndPublishing();

                    {
                        mLogger.info("Initializing mediaItems...");

                        mediaItemsList.clear();
                        mediaItemsSelectedList.clear();
                        mediaItemsSelectionMode = "single";
                        mediaItemsMaxMediaItemsNumber = new Long(100);
                        {
                            mediaItemsContentTypesList.clear();
                            mediaItemsContentTypesList.add("video");

                            mediaItemsContentType = mediaItemsContentTypesList.get(0);
                        }

                        fillMediaItems();
                    }
                }
                else if (task.getType().equalsIgnoreCase("I-Frames"))
                {
                    taskReferences = task.getReferences() == null ? "" : task.getReferences();
                    taskLabel = task.getLabel();
                    taskStartTimeInSeconds = task.getStartTimeInSeconds();
                    taskFrameMaxFramesNumber = task.getFrameMaxFramesNumber();
                    taskFrameWidth = task.getFrameWidth();
                    taskFrameHeight = task.getFrameHeight();
                    taskUserData = task.getUserData();
                    taskRetention = task.getRetention();
                    taskTitle = task.getTitle();
                    taskUniqueName = task.getUniqueName();
                    if (task.getIngester() != null && !task.getIngester().equalsIgnoreCase(""))
                        taskIngester = task.getIngester();
                    else
                        taskIngester = userName;
                    taskKeywords = task.getKeywords();
                    taskContentProviderName= task.getContentProviderName();
                    taskDeliveryFileName= task.getDeliveryFileName();
                    taskStartPublishing= task.getStartPublishing();
                    taskEndPublishing= task.getEndPublishing();

                    {
                        mLogger.info("Initializing mediaItems...");

                        mediaItemsList.clear();
                        mediaItemsSelectedList.clear();
                        mediaItemsSelectionMode = "single";
                        mediaItemsMaxMediaItemsNumber = new Long(100);
                        {
                            mediaItemsContentTypesList.clear();
                            mediaItemsContentTypesList.add("video");

                            mediaItemsContentType = mediaItemsContentTypesList.get(0);
                        }

                        fillMediaItems();
                    }
                }
                else if (task.getType().equalsIgnoreCase("Motion-JPEG-by-Periodical-Frames"))
                {
                    taskReferences = task.getReferences() == null ? "" : task.getReferences();
                    taskLabel = task.getLabel();
                    taskFramePeriodInSeconds = task.getFramePeriodInSeconds();
                    taskStartTimeInSeconds = task.getStartTimeInSeconds();
                    taskFrameMaxFramesNumber = task.getFrameMaxFramesNumber();
                    taskFrameWidth = task.getFrameWidth();
                    taskFrameHeight = task.getFrameHeight();
                    taskUserData = task.getUserData();
                    taskRetention = task.getRetention();
                    taskTitle = task.getTitle();
                    taskUniqueName = task.getUniqueName();
                    if (task.getIngester() != null && !task.getIngester().equalsIgnoreCase(""))
                        taskIngester = task.getIngester();
                    else
                        taskIngester = userName;
                    taskKeywords = task.getKeywords();
                    taskContentProviderName= task.getContentProviderName();
                    taskDeliveryFileName= task.getDeliveryFileName();
                    taskStartPublishing= task.getStartPublishing();
                    taskEndPublishing= task.getEndPublishing();

                    {
                        mLogger.info("Initializing mediaItems...");

                        mediaItemsList.clear();
                        mediaItemsSelectedList.clear();
                        mediaItemsSelectionMode = "single";
                        mediaItemsMaxMediaItemsNumber = new Long(100);
                        {
                            mediaItemsContentTypesList.clear();
                            mediaItemsContentTypesList.add("video");

                            mediaItemsContentType = mediaItemsContentTypesList.get(0);
                        }

                        fillMediaItems();
                    }
                }
                else if (task.getType().equalsIgnoreCase("Motion-JPEG-by-I-Frames"))
                {
                    taskReferences = task.getReferences() == null ? "" : task.getReferences();
                    taskLabel = task.getLabel();
                    taskStartTimeInSeconds = task.getStartTimeInSeconds();
                    taskFrameMaxFramesNumber = task.getFrameMaxFramesNumber();
                    taskFrameWidth = task.getFrameWidth();
                    taskFrameHeight = task.getFrameHeight();
                    taskUserData = task.getUserData();
                    taskRetention = task.getRetention();
                    taskTitle = task.getTitle();
                    taskUniqueName = task.getUniqueName();
                    if (task.getIngester() != null && !task.getIngester().equalsIgnoreCase(""))
                        taskIngester = task.getIngester();
                    else
                        taskIngester = userName;
                    taskKeywords = task.getKeywords();
                    taskContentProviderName= task.getContentProviderName();
                    taskDeliveryFileName= task.getDeliveryFileName();
                    taskStartPublishing= task.getStartPublishing();
                    taskEndPublishing= task.getEndPublishing();

                    {
                        mLogger.info("Initializing mediaItems...");

                        mediaItemsList.clear();
                        mediaItemsSelectedList.clear();
                        mediaItemsSelectionMode = "single";
                        mediaItemsMaxMediaItemsNumber = new Long(100);
                        {
                            mediaItemsContentTypesList.clear();
                            mediaItemsContentTypesList.add("video");

                            mediaItemsContentType = mediaItemsContentTypesList.get(0);
                        }

                        fillMediaItems();
                    }
                }
                else if (task.getType().equalsIgnoreCase("Slideshow"))
                {
                    taskReferences = task.getReferences() == null ? "" : task.getReferences();
                    taskLabel = task.getLabel();
                    taskFrameDurationOfEachSlideInSeconds= task.getFrameDurationOfEachSlideInSeconds();
                    taskUserData = task.getUserData();
                    taskRetention = task.getRetention();
                    taskTitle = task.getTitle();
                    taskUniqueName = task.getUniqueName();
                    if (task.getIngester() != null && !task.getIngester().equalsIgnoreCase(""))
                        taskIngester = task.getIngester();
                    else
                        taskIngester = userName;
                    taskKeywords = task.getKeywords();
                    taskContentProviderName= task.getContentProviderName();
                    taskDeliveryFileName= task.getDeliveryFileName();
                    taskStartPublishing= task.getStartPublishing();
                    taskEndPublishing= task.getEndPublishing();

                    {
                        mLogger.info("Initializing mediaItems...");

                        mediaItemsList.clear();
                        mediaItemsSelectedList.clear();
                        mediaItemsSelectionMode = "multiple";
                        mediaItemsMaxMediaItemsNumber = new Long(100);
                        {
                            mediaItemsContentTypesList.clear();
                            mediaItemsContentTypesList.add("image");

                            mediaItemsContentType = mediaItemsContentTypesList.get(0);
                        }

                        fillMediaItems();
                    }
                }
                else if (task.getType().equalsIgnoreCase("Encode"))
                {
                    taskReferences = task.getReferences() == null ? "" : task.getReferences();
                    taskLabel = task.getLabel();
                    taskEncodingPriority = task.getEncodingPriority();
                    taskContentType = taskContentTypesList.get(0);
                    if (task.getEncodingProfileType() != null
                            && !task.getEncodingProfileType().equalsIgnoreCase(""))
                        taskEncodingProfileType = task.getEncodingProfileType();
                    else
                        taskEncodingProfileType = "profilesSet";
                    taskEncodingProfilesSetLabel = task.getEncodingProfilesSetLabel();
                    taskEncodingProfileLabel = task.getEncodingProfileLabel();

                    {
                        mLogger.info("Initializing mediaItems...");

                        mediaItemsList.clear();
                        mediaItemsSelectedList.clear();
                        mediaItemsSelectionMode = "single";
                        mediaItemsMaxMediaItemsNumber = new Long(100);
                        {
                            mediaItemsContentTypesList.clear();
                            mediaItemsContentTypesList.add("video");
                            mediaItemsContentTypesList.add("audio");

                            mediaItemsContentType = mediaItemsContentTypesList.get(0);
                        }

                        fillMediaItems();
                    }
                }
                else if (task.getType().equalsIgnoreCase("Email-Notification"))
                {
                    taskLabel = task.getLabel();
                    taskEmailAddress = task.getEmailAddress();
                    taskSubject = task.getSubject();
                    taskMessage = task.getMessage();
                }
                else
                {
                    mLogger.error("Unknown task.getType(): " + task.getType());
                }
            }
            else
            {
                String errorMessage = "A Task node is not selected";
                mLogger.error(errorMessage);

                FacesMessage message = new FacesMessage(FacesMessage.SEVERITY_ERROR, "Workflow",
                        errorMessage);
                FacesContext.getCurrentInstance().addMessage(null, message);
            }
        }
        else
        {
            nodeType = "NoSelectedType";

            String errorMessage = "A Workflow/Task/Event node is not selected";
            mLogger.error(errorMessage);

            /*
            FacesMessage message = new FacesMessage(FacesMessage.SEVERITY_ERROR, "Workflow",
                    errorMessage);
            FacesContext.getCurrentInstance().addMessage(null, message);
            */
        }
    }

    public void saveTaskProperties()
    {
        if (tnWorkingNode != null)
        {
            Task task = (Task) tnWorkingNode.getData();

            if (task.getType().equalsIgnoreCase("GroupOfTasks"))
            {
                task.setGroupOfTaskExecutionType(groupOfTaskExecutionType);
            }
            else if (task.getType().equalsIgnoreCase("Add-Content"))
            {
                task.setLabel(taskLabel);
                task.setSourceDownloadType(taskSourceDownloadType);
                task.setPullSourceURL(taskPullSourceURL);
                task.setPushBinaryPathName(taskPushBinaryPathName);
                task.setFileFormat(taskFileFormat);
                task.setUserData(taskUserData);
                task.setRetention(taskRetention);
                task.setTitle(taskTitle);
                task.setUniqueName(taskUniqueName);
                task.setIngester(taskIngester);
                task.setKeywords(taskKeywords);
                task.setMd5FileCheckSum(taskMD5FileChecksum);
                task.setFileSizeInBytes(taskFileSizeInBytes);
                task.setContentProviderName(taskContentProviderName);
                task.setDeliveryFileName(taskDeliveryFileName);
                task.setStartPublishing(taskStartPublishing);
                task.setEndPublishing(taskEndPublishing);
            }
            else if (task.getType().equalsIgnoreCase("Remove-Content"))
            {
                task.setReferences(taskReferences);
                task.setLabel(taskLabel);
            }
            else if (task.getType().equalsIgnoreCase("Concat-Demuxer"))
            {
                task.setReferences(taskReferences);
                task.setLabel(taskLabel);
                task.setUserData(taskUserData);
                task.setRetention(taskRetention);
                task.setTitle(taskTitle);
                task.setUniqueName(taskUniqueName);
                task.setIngester(taskIngester);
                task.setKeywords(taskKeywords);
                task.setContentProviderName(taskContentProviderName);
                task.setDeliveryFileName(taskDeliveryFileName);
                task.setStartPublishing(taskStartPublishing);
                task.setEndPublishing(taskEndPublishing);
            }
            else if (task.getType().equalsIgnoreCase("Cut"))
            {
                task.setReferences(taskReferences);
                task.setLabel(taskLabel);
                task.setCutEndType(taskCutEndType);
                task.setStartTimeInSeconds(taskStartTimeInSeconds);
                task.setEndTimeInSeconds(taskEndTimeInSeconds);
                task.setFramesNumber(taskFramesNumber);
                task.setUserData(taskUserData);
                task.setRetention(taskRetention);
                task.setTitle(taskTitle);
                task.setUniqueName(taskUniqueName);
                task.setIngester(taskIngester);
                task.setKeywords(taskKeywords);
                task.setContentProviderName(taskContentProviderName);
                task.setDeliveryFileName(taskDeliveryFileName);
                task.setStartPublishing(taskStartPublishing);
                task.setEndPublishing(taskEndPublishing);
            }
            else if (task.getType().equalsIgnoreCase("Overlay-Image-On-Video"))
            {
                task.setReferences(taskReferences);
                task.setLabel(taskLabel);
                task.setOverlayPositionXInPixel(taskOverlayPositionXInPixel);
                task.setOverlayPositionYInPixel(taskOverlayPositionYInPixel);
                task.setUserData(taskUserData);
                task.setRetention(taskRetention);
                task.setTitle(taskTitle);
                task.setUniqueName(taskUniqueName);
                task.setIngester(taskIngester);
                task.setKeywords(taskKeywords);
                task.setContentProviderName(taskContentProviderName);
                task.setDeliveryFileName(taskDeliveryFileName);
                task.setStartPublishing(taskStartPublishing);
                task.setEndPublishing(taskEndPublishing);
            }
            else if (task.getType().equalsIgnoreCase("Overlay-Text-On-Video"))
            {
                task.setReferences(taskReferences);
                task.setLabel(taskLabel);
                task.setOverlayText(taskOverlayText);
                task.setOverlayPositionXInPixel(taskOverlayPositionXInPixel);
                task.setOverlayPositionYInPixel(taskOverlayPositionYInPixel);
                task.setOverlayFontType(taskOverlayFontType);
                task.setOverlayFontSize(taskOverlayFontSize);
                task.setOverlayFontColor(taskOverlayFontColor);
                task.setOverlayTextPercentageOpacity(taskOverlayTextPercentageOpacity);
                task.setOverlayBoxEnable(taskOverlayBoxEnable);
                task.setOverlayBoxColor(taskOverlayBoxColor);
                task.setOverlayBoxPercentageOpacity(taskOverlayBoxPercentageOpacity);
                task.setEncodingPriority(taskEncodingPriority);
                task.setUserData(taskUserData);
                task.setRetention(taskRetention);
                task.setTitle(taskTitle);
                task.setUniqueName(taskUniqueName);
                task.setIngester(taskIngester);
                task.setKeywords(taskKeywords);
                task.setContentProviderName(taskContentProviderName);
                task.setDeliveryFileName(taskDeliveryFileName);
                task.setStartPublishing(taskStartPublishing);
                task.setEndPublishing(taskEndPublishing);
            }
            else if (task.getType().equalsIgnoreCase("Frame"))
            {
                task.setReferences(taskReferences);
                task.setLabel(taskLabel);
                task.setFrameInstantInSeconds(taskFrameInstantInSeconds);
                task.setFrameWidth(taskFrameWidth);
                task.setFrameHeight(taskFrameHeight);
                task.setUserData(taskUserData);
                task.setRetention(taskRetention);
                task.setTitle(taskTitle);
                task.setUniqueName(taskUniqueName);
                task.setIngester(taskIngester);
                task.setKeywords(taskKeywords);
                task.setContentProviderName(taskContentProviderName);
                task.setDeliveryFileName(taskDeliveryFileName);
                task.setStartPublishing(taskStartPublishing);
                task.setEndPublishing(taskEndPublishing);
            }
            else if (task.getType().equalsIgnoreCase("Periodical-Frames"))
            {
                task.setReferences(taskReferences);
                task.setLabel(taskLabel);
                task.setFramePeriodInSeconds(taskFramePeriodInSeconds);
                task.setStartTimeInSeconds(taskStartTimeInSeconds);
                task.setFrameMaxFramesNumber(taskFrameMaxFramesNumber);
                task.setFrameWidth(taskFrameWidth);
                task.setFrameHeight(taskFrameHeight);
                task.setUserData(taskUserData);
                task.setRetention(taskRetention);
                task.setTitle(taskTitle);
                task.setUniqueName(taskUniqueName);
                task.setIngester(taskIngester);
                task.setKeywords(taskKeywords);
                task.setContentProviderName(taskContentProviderName);
                task.setDeliveryFileName(taskDeliveryFileName);
                task.setStartPublishing(taskStartPublishing);
                task.setEndPublishing(taskEndPublishing);
            }
            else if (task.getType().equalsIgnoreCase("I-Frames"))
            {
                task.setReferences(taskReferences);
                task.setLabel(taskLabel);
                task.setStartTimeInSeconds(taskStartTimeInSeconds);
                task.setFrameMaxFramesNumber(taskFrameMaxFramesNumber);
                task.setFrameWidth(taskFrameWidth);
                task.setFrameHeight(taskFrameHeight);
                task.setUserData(taskUserData);
                task.setRetention(taskRetention);
                task.setTitle(taskTitle);
                task.setUniqueName(taskUniqueName);
                task.setIngester(taskIngester);
                task.setKeywords(taskKeywords);
                task.setContentProviderName(taskContentProviderName);
                task.setDeliveryFileName(taskDeliveryFileName);
                task.setStartPublishing(taskStartPublishing);
                task.setEndPublishing(taskEndPublishing);
            }
            else if (task.getType().equalsIgnoreCase("Motion-JPEG-by-Periodical-Frames"))
            {
                task.setReferences(taskReferences);
                task.setLabel(taskLabel);
                task.setFramePeriodInSeconds(taskFramePeriodInSeconds);
                task.setStartTimeInSeconds(taskStartTimeInSeconds);
                task.setFrameMaxFramesNumber(taskFrameMaxFramesNumber);
                task.setFrameWidth(taskFrameWidth);
                task.setFrameHeight(taskFrameHeight);
                task.setUserData(taskUserData);
                task.setRetention(taskRetention);
                task.setTitle(taskTitle);
                task.setUniqueName(taskUniqueName);
                task.setIngester(taskIngester);
                task.setKeywords(taskKeywords);
                task.setContentProviderName(taskContentProviderName);
                task.setDeliveryFileName(taskDeliveryFileName);
                task.setStartPublishing(taskStartPublishing);
                task.setEndPublishing(taskEndPublishing);
            }
            else if (task.getType().equalsIgnoreCase("Motion-JPEG-by-I-Frames"))
            {
                task.setReferences(taskReferences);
                task.setLabel(taskLabel);
                task.setStartTimeInSeconds(taskStartTimeInSeconds);
                task.setFrameMaxFramesNumber(taskFrameMaxFramesNumber);
                task.setFrameWidth(taskFrameWidth);
                task.setFrameHeight(taskFrameHeight);
                task.setUserData(taskUserData);
                task.setRetention(taskRetention);
                task.setTitle(taskTitle);
                task.setUniqueName(taskUniqueName);
                task.setIngester(taskIngester);
                task.setKeywords(taskKeywords);
                task.setContentProviderName(taskContentProviderName);
                task.setDeliveryFileName(taskDeliveryFileName);
                task.setStartPublishing(taskStartPublishing);
                task.setEndPublishing(taskEndPublishing);
            }
            else if (task.getType().equalsIgnoreCase("Slideshow"))
            {
                task.setReferences(taskReferences);
                task.setLabel(taskLabel);
                task.setFrameDurationOfEachSlideInSeconds(taskFrameDurationOfEachSlideInSeconds);
                task.setUserData(taskUserData);
                task.setRetention(taskRetention);
                task.setTitle(taskTitle);
                task.setUniqueName(taskUniqueName);
                task.setIngester(taskIngester);
                task.setKeywords(taskKeywords);
                task.setContentProviderName(taskContentProviderName);
                task.setDeliveryFileName(taskDeliveryFileName);
                task.setStartPublishing(taskStartPublishing);
                task.setEndPublishing(taskEndPublishing);
            }
            else if (task.getType().equalsIgnoreCase("Encode"))
            {
                task.setReferences(taskReferences);
                task.setLabel(taskLabel);
                task.setEncodingPriority(taskEncodingPriority);
                task.setEncodingProfileType(taskEncodingProfileType);
                task.setEncodingProfilesSetLabel(taskEncodingProfilesSetLabel);
                task.setEncodingProfileLabel(taskEncodingProfileLabel);
            }
            else if (task.getType().equalsIgnoreCase("Email-Notification"))
            {
                task.setLabel(taskLabel);
                task.setEmailAddress(taskEmailAddress);
                task.setSubject(taskSubject);
                task.setMessage(taskMessage);
            }
            else
            {
                mLogger.error("Unknown task.getType(): " + task.getType());
            }

            buildWorkflow();

            // tnWorkingNode = null;

            FacesMessage message = new FacesMessage(FacesMessage.SEVERITY_INFO, "Workflow",
                    "The " + task.getType() + " properties are saved");
            FacesContext.getCurrentInstance().addMessage(null, message);
        }
        else
        {
            FacesMessage message = new FacesMessage(FacesMessage.SEVERITY_ERROR, "Workflow",
                    "A Task node is not selected");
            FacesContext.getCurrentInstance().addMessage(null, message);
        }
    }

    public void handleFileUpload(FileUploadEvent event)
    {
        mLogger.info("Uploaded binary file name: " + event.getFile().getFileName());

        // save
        File binaryFile = null;
        {
            InputStream input = null;
            OutputStream output = null;

            try
            {
                taskPushBinaryPathName = getLocalBinaryPathName(event.getFile().getFileName());
                binaryFile = new File(taskPushBinaryPathName);

                input = event.getFile().getInputstream();
                output = new FileOutputStream(binaryFile);

                IOUtils.copy(input, output);
            }
            catch (Exception e)
            {
                mLogger.error("Exception: " + e.getMessage());

                return;
            }
            finally
            {
                if (input != null)
                    IOUtils.closeQuietly(input);
                if (output != null)
                    IOUtils.closeQuietly(output);
            }
        }
    }

    public String getVideoAudioAllowTypes()
    {
        String fileExtension = "wmv|mp4|mpeg|avi|webm|mp3|aac|png|jpg";

        return ("/(\\.|\\/)(" + fileExtension + ")$/");
    }

    private String getLocalBinaryPathName(String fileName)
    {
        try
        {
            Long userKey = SessionUtils.getUserKey();

            String localBinaryPathName = temporaryPushBinariesPathName + "/" + userKey + "-" + fileName;

            return localBinaryPathName;
        }
        catch (Exception e)
        {
            String errorMessage = "Exception: " + e;
            mLogger.error(errorMessage);

            return null;
        }
    }

    public void fillMediaItems()
    {
        try
        {
            Long userKey = SessionUtils.getUserKey();
            String apiKey = SessionUtils.getAPIKey();

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

                mLogger.info("Calling catraMMS.getMediaItems"
                                + ", mediaItemsMaxMediaItemsNumber: " + mediaItemsMaxMediaItemsNumber
                                + ", mediaItemsContentType: " + mediaItemsContentType
                                + ", mediaItemsBegin: " + mediaItemsBegin
                                + ", mediaItemsEnd: " + mediaItemsEnd
                );

                CatraMMS catraMMS = new CatraMMS();
                Vector<Long> mediaItemsData = catraMMS.getMediaItems(
                        username, password, mediaItemsMaxMediaItemsNumber,
                        mediaItemsContentType, mediaItemsBegin, mediaItemsEnd, mediaItemsList);
                mediaItemsNumber = mediaItemsData.get(0);
            }
        }
        catch (Exception e)
        {
            String errorMessage = "Exception: " + e;
            mLogger.error(errorMessage);
        }
    }

    public void deleteNode()
    {
        mLogger.info("deleteNode, tnWorkingNode: " + tnWorkingNode);

        if (tnSelectedNode != null)
        {
            String typeToBeRemoved = tnSelectedNode.getType();

            if (!typeToBeRemoved.equalsIgnoreCase("Workflow"))
            {
                {
                    // tnSelectedNode could be a Task (including a GroupOfTasks) or an Event
                    boolean isTaskToBeRemoved = tnSelectedNode.getData() instanceof Task;
                    String parentType = tnSelectedNode.getParent().getType();

                    if (isTaskToBeRemoved)
                    {
                        // parentType could be: Workflow or an Event
                        if (parentType.equalsIgnoreCase("Workflow"))
                        {
                            Workflow workflow = (Workflow) tnSelectedNode.getParent().getData();
                            workflow.setChildTaskCreated(false);
                        }
                        else if (parentType.equalsIgnoreCase("Event"))
                        {
                            Event event = (Event) tnSelectedNode.getParent().getData();

                            event.setChildTaskCreated(false);
                        }
                        else if (parentType.equalsIgnoreCase("GroupOfTasks"))
                        {
                            // nothing to do
                        }
                    }
                    else // it is an even to be removed
                    {
                        Event eventToBeRemoved = (Event) tnSelectedNode.getData();

                        // parentType could be: Workflow or a Task (including a GroupOfTasks)
                        if (parentType.equalsIgnoreCase("Workflow"))
                        {
                            Workflow workflow = (Workflow) tnSelectedNode.getParent().getData();

                            if (eventToBeRemoved.getType().equalsIgnoreCase("OnSuccess"))
                                workflow.setChildEventOnSuccessCreated(false);
                            else if (eventToBeRemoved.getType().equalsIgnoreCase("OnError"))
                                workflow.setChildEventOnErrorCreated(false);
                            else if (eventToBeRemoved.getType().equalsIgnoreCase("OnComplete"))
                                workflow.setChildEventOnCompleteCreated(false);
                        }
                        else if (parentType.equalsIgnoreCase("Task"))
                        {
                            Task task = (Task) tnSelectedNode.getParent().getData();

                            if (eventToBeRemoved.getType().equalsIgnoreCase("OnSuccess"))
                                task.setChildEventOnSuccessCreated(false);
                            else if (eventToBeRemoved.getType().equalsIgnoreCase("OnError"))
                                task.setChildEventOnErrorCreated(false);
                            else if (eventToBeRemoved.getType().equalsIgnoreCase("OnComplete"))
                                task.setChildEventOnCompleteCreated(false);
                        }
                    }
                }

                tnSelectedNode.getChildren().clear();
                tnSelectedNode.getParent().getChildren().remove(tnSelectedNode);
                tnSelectedNode.setParent(null);

                tnSelectedNode = null;

                setNodeProperties();

                buildWorkflow();

                FacesMessage message = new FacesMessage(FacesMessage.SEVERITY_INFO, "Workflow",
                        "The " + typeToBeRemoved + " node was removed");
                FacesContext.getCurrentInstance().addMessage(null, message);
            }
            else
            {
                FacesMessage message = new FacesMessage(FacesMessage.SEVERITY_INFO, "Workflow",
                        "The Workflow node cannot be removed");
                FacesContext.getCurrentInstance().addMessage(null, message);
            }
        }
        else
        {
            FacesMessage message = new FacesMessage(FacesMessage.SEVERITY_INFO, "Workflow",
                    "No node selected to be removed");
            FacesContext.getCurrentInstance().addMessage(null, message);
        }
    }

    public List<String> getTaskEncodingProfilesLabelList()
    {
        taskEncodingProfilesLabelList.clear();

        if (taskContentType.equalsIgnoreCase("video"))
        {
            for (EncodingProfile encodingProfile: taskVideoEncodingProfilesList)
                taskEncodingProfilesLabelList.add(encodingProfile.getLabel());

        }
        else if (taskContentType.equalsIgnoreCase("audio"))
        {
            for (EncodingProfile encodingProfile: taskAudioEncodingProfilesList)
                taskEncodingProfilesLabelList.add(encodingProfile.getLabel());

        }
        else if (taskContentType.equalsIgnoreCase("image"))
        {
            for (EncodingProfile encodingProfile: taskImageEncodingProfilesList)
                taskEncodingProfilesLabelList.add(encodingProfile.getLabel());

        }
        else
            mLogger.error("Unknown taskContentType: " + taskContentType);

        return taskEncodingProfilesLabelList;
    }

    public void setTaskEncodingProfilesLabelList(List<String> taskEncodingProfilesLabelList) {
        this.taskEncodingProfilesLabelList = taskEncodingProfilesLabelList;
    }

    public List<String> getTaskEncodingProfilesLabelSetList()
    {
        taskEncodingProfilesLabelSetList.clear();

        if (taskContentType.equalsIgnoreCase("video"))
        {
            for (EncodingProfilesSet encodingProfilesSet: taskVideoEncodingProfilesSetList)
                taskEncodingProfilesLabelSetList.add(encodingProfilesSet.getLabel());

        }
        else if (taskContentType.equalsIgnoreCase("audio"))
        {
            for (EncodingProfilesSet encodingProfilesSet: taskAudioEncodingProfilesSetList)
                taskEncodingProfilesLabelSetList.add(encodingProfilesSet.getLabel());

        }
        else if (taskContentType.equalsIgnoreCase("image"))
        {
            for (EncodingProfilesSet encodingProfilesSet: taskImageEncodingProfilesSetList)
                taskEncodingProfilesLabelSetList.add(encodingProfilesSet.getLabel());

        }
        else
            mLogger.error("Unknown taskContentType: " + taskContentType);

        return taskEncodingProfilesLabelSetList;
    }

    public void setTaskEncodingProfilesLabelSetList(List<String> taskEncodingProfilesLabelSetList) {
        this.taskEncodingProfilesLabelSetList = taskEncodingProfilesLabelSetList;
    }

    public List<MediaItem> getMediaItemsSelectedList() {
        return mediaItemsSelectedList;
    }

    public void setMediaItemsSelectedList(List<MediaItem> mediaItemsSelectedList)
    {
        this.mediaItemsSelectedList = mediaItemsSelectedList;

        mLogger.info("taskReferences initialization"
                        + ", mediaItemsSelectedList.size: " + mediaItemsSelectedList.size()
        );

        if (mediaItemsToBeAddedOrReplaced.equalsIgnoreCase("toBeReplaced"))
            taskReferences = "";

        for (MediaItem mediaItem: mediaItemsSelectedList)
        {
            if (nodeType.equalsIgnoreCase("Remove-Content"))
            {
                if (taskReferences == "")
                    taskReferences = mediaItem.getMediaItemKey().toString();
                else
                    taskReferences += ("," + mediaItem.getMediaItemKey().toString());
            }
            else
            {
                mLogger.info("Looking for the SourcePhysicalPath. MediaItemKey: " + mediaItem.getMediaItemKey());
                PhysicalPath sourcePhysicalPath = mediaItem.getSourcePhysicalPath();

                if (sourcePhysicalPath != null)
                {
                    if (taskReferences == "")
                        taskReferences = sourcePhysicalPath.getPhysicalPathKey().toString();
                    else
                        taskReferences += ("," + sourcePhysicalPath.getPhysicalPathKey().toString());
                }
                else
                {
                    String errorMessage = "No sourcePhysicalPath found"
                            + ", mediaItemKey: " + mediaItem.getMediaItemKey();
                    mLogger.error(errorMessage);

                    FacesMessage message = new FacesMessage(FacesMessage.SEVERITY_INFO, "Workflow",
                            errorMessage);
                    FacesContext.getCurrentInstance().addMessage(null, message);
                }
            }
        }
        mLogger.info("taskReferences: " + taskReferences);
    }

    public TreeNode getTnSelectedNode() {
        return tnSelectedNode;
    }

    public void setTnSelectedNode(TreeNode tnSelectedNode) {
        mLogger.info("setTnSelectedNode. tnSelectedNode: " + tnSelectedNode);

        this.tnSelectedNode = tnSelectedNode;
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

    public TreeNode getTnRoot() {
        return tnRoot;
    }

    public void setTnRoot(TreeNode tnRoot) {
        this.tnRoot = tnRoot;
    }

    public String getWorkflowLabel() {
        return workflowLabel;
    }

    public void setWorkflowLabel(String workflowLabel) {
        this.workflowLabel = workflowLabel;
    }

    public String getTaskLabel() {
        return taskLabel;
    }

    public void setTaskLabel(String taskLabel) {
        this.taskLabel = taskLabel;
    }

    public String getTaskRetention() {
        return taskRetention;
    }

    public void setTaskRetention(String taskRetention) {
        this.taskRetention = taskRetention;
    }

    public String getTaskTitle() {
        return taskTitle;
    }

    public void setTaskTitle(String taskTitle) {
        this.taskTitle = taskTitle;
    }

    public String getTaskKeywords() {
        return taskKeywords;
    }

    public void setTaskKeywords(String taskKeywords) {
        this.taskKeywords = taskKeywords;
    }

    public String getTaskMD5FileChecksum() {
        return taskMD5FileChecksum;
    }

    public void setTaskMD5FileChecksum(String taskMD5FileChecksum) {
        this.taskMD5FileChecksum = taskMD5FileChecksum;
    }

    public Long getTaskFileSizeInBytes() {
        return taskFileSizeInBytes;
    }

    public void setTaskFileSizeInBytes(Long taskFileSizeInBytes) {
        this.taskFileSizeInBytes = taskFileSizeInBytes;
    }

    public String getTaskContentProviderName() {
        return taskContentProviderName;
    }

    public void setTaskContentProviderName(String taskContentProviderName) {
        this.taskContentProviderName = taskContentProviderName;
    }

    public String getTaskDeliveryFileName() {
        return taskDeliveryFileName;
    }

    public void setTaskDeliveryFileName(String taskDeliveryFileName) {
        this.taskDeliveryFileName = taskDeliveryFileName;
    }

    public Date getTaskStartPublishing() {
        return taskStartPublishing;
    }

    public void setTaskStartPublishing(Date taskStartPublishing) {
        this.taskStartPublishing = taskStartPublishing;
    }

    public Date getTaskEndPublishing() {
        return taskEndPublishing;
    }

    public void setTaskEndPublishing(Date taskEndPublishing) {
        this.taskEndPublishing = taskEndPublishing;
    }

    public String getTaskPullSourceURL() {
        return taskPullSourceURL;
    }

    public void setTaskPullSourceURL(String taskPullSourceURL) {
        this.taskPullSourceURL = taskPullSourceURL;
    }

    public String getTaskPushBinaryPathName() {
        return taskPushBinaryPathName;
    }

    public void setTaskPushBinaryPathName(String taskPushBinaryPathName) {
        this.taskPushBinaryPathName = taskPushBinaryPathName;
    }

    public String getTaskIngester() {
        return taskIngester;
    }

    public void setTaskIngester(String taskIngester) {
        this.taskIngester = taskIngester;
    }

    public String getTaskUserData() {
        return taskUserData;
    }

    public void setTaskUserData(String taskUserData) {
        this.taskUserData = taskUserData;
    }

    public String getTaskUniqueName() {
        return taskUniqueName;
    }

    public void setTaskUniqueName(String taskUniqueName) {
        this.taskUniqueName = taskUniqueName;
    }

    public String getTaskFileFormat() {
        return taskFileFormat;
    }

    public void setTaskFileFormat(String taskFileFormat) {
        this.taskFileFormat = taskFileFormat;
    }

    public List<String> getTaskFileFormatsList() {
        return taskFileFormatsList;
    }

    public void setTaskFileFormatsList(List<String> taskFileFormatsList) {
        this.taskFileFormatsList = taskFileFormatsList;
    }

    public String getJsonWorkflow() {
        return jsonWorkflow;
    }

    public void setJsonWorkflow(String jsonWorkflow) {
        this.jsonWorkflow = jsonWorkflow;
    }

    public String getTaskSourceDownloadType() {
        return taskSourceDownloadType;
    }

    public void setTaskSourceDownloadType(String taskSourceDownloadType) {
        this.taskSourceDownloadType = taskSourceDownloadType;
    }

    public String getTaskCutEndType() {
        return taskCutEndType;
    }

    public void setTaskCutEndType(String taskCutEndType) {
        this.taskCutEndType = taskCutEndType;
    }

    public String getGroupOfTaskExecutionType() {
        return groupOfTaskExecutionType;
    }

    public void setGroupOfTaskExecutionType(String groupOfTaskExecutionType) {
        this.groupOfTaskExecutionType = groupOfTaskExecutionType;
    }

    public TreeNode getTnWorkflow() {
        return tnWorkflow;
    }

    public void setTnWorkflow(TreeNode tnWorkflow) {
        this.tnWorkflow = tnWorkflow;
    }

    public String getNodeType() {
        return nodeType;
    }

    public void setNodeType(String nodeType) {
        this.nodeType = nodeType;
    }

    public Float getTaskStartTimeInSeconds() {
        return taskStartTimeInSeconds;
    }

    public void setTaskStartTimeInSeconds(Float taskStartTimeInSeconds) {
        this.taskStartTimeInSeconds = taskStartTimeInSeconds;
    }

    public Float getTaskEndTimeInSeconds() {
        return taskEndTimeInSeconds;
    }

    public void setTaskEndTimeInSeconds(Float taskEndTimeInSeconds) {
        this.taskEndTimeInSeconds = taskEndTimeInSeconds;
    }

    public Long getTaskFramesNumber() {
        return taskFramesNumber;
    }

    public void setTaskFramesNumber(Long taskFramesNumber) {
        this.taskFramesNumber = taskFramesNumber;
    }

    public String getTaskEncodingPriority() {
        return taskEncodingPriority;
    }

    public void setTaskEncodingPriority(String taskEncodingPriority) {
        this.taskEncodingPriority = taskEncodingPriority;
    }

    public List<String> getTaskEncodingPrioritiesList() {
        return taskEncodingPrioritiesList;
    }

    public void setTaskEncodingPrioritiesList(List<String> taskEncodingPrioritiesList) {
        this.taskEncodingPrioritiesList = taskEncodingPrioritiesList;
    }

    public String getTaskEncodingProfilesSetLabel() {
        return taskEncodingProfilesSetLabel;
    }

    public void setTaskEncodingProfilesSetLabel(String taskEncodingProfilesSetLabel) {
        this.taskEncodingProfilesSetLabel = taskEncodingProfilesSetLabel;
    }

    public String getTaskEncodingProfileLabel() {
        return taskEncodingProfileLabel;
    }

    public void setTaskEncodingProfileLabel(String taskEncodingProfileLabel) {
        this.taskEncodingProfileLabel = taskEncodingProfileLabel;
    }

    public String getTaskContentType() {
        return taskContentType;
    }

    public void setTaskContentType(String taskContentType) {
        this.taskContentType = taskContentType;
    }

    public List<String> getTaskContentTypesList() {
        return taskContentTypesList;
    }

    public void setTaskContentTypesList(List<String> taskContentTypesList) {
        this.taskContentTypesList = taskContentTypesList;
    }

    public String getTaskEncodingProfileType() {
        return taskEncodingProfileType;
    }

    public void setTaskEncodingProfileType(String taskEncodingProfileType) {
        this.taskEncodingProfileType = taskEncodingProfileType;
    }

    public String getTaskEmailAddress() {
        return taskEmailAddress;
    }

    public void setTaskEmailAddress(String taskEmailAddress) {
        this.taskEmailAddress = taskEmailAddress;
    }

    public String getTaskSubject() {
        return taskSubject;
    }

    public void setTaskSubject(String taskSubject) {
        this.taskSubject = taskSubject;
    }

    public String getTaskMessage() {
        return taskMessage;
    }

    public void setTaskMessage(String taskMessage) {
        this.taskMessage = taskMessage;
    }

    public List<WorkflowIssue> getWorkflowIssueList() {
        return workflowIssueList;
    }

    public void setWorkflowIssueList(List<WorkflowIssue> workflowIssueList) {
        this.workflowIssueList = workflowIssueList;
    }

    public IngestionResult getWorkflowRoot() {
        return workflowRoot;
    }

    public void setWorkflowRoot(IngestionResult workflowRoot) {
        this.workflowRoot = workflowRoot;
    }

    public List<IngestionResult> getIngestionJobList() {
        return ingestionJobList;
    }

    public void setIngestionJobList(List<IngestionResult> ingestionJobList) {
        this.ingestionJobList = ingestionJobList;
    }

    public String getTaskOverlayPositionXInPixel() {
        return taskOverlayPositionXInPixel;
    }

    public void setTaskOverlayPositionXInPixel(String taskOverlayPositionXInPixel) {
        this.taskOverlayPositionXInPixel = taskOverlayPositionXInPixel;
    }

    public String getTaskOverlayPositionYInPixel() {
        return taskOverlayPositionYInPixel;
    }

    public void setTaskOverlayPositionYInPixel(String taskOverlayPositionYInPixel) {
        this.taskOverlayPositionYInPixel = taskOverlayPositionYInPixel;
    }

    public String getTaskOverlayText() {
        return taskOverlayText;
    }

    public void setTaskOverlayText(String taskOverlayText) {
        this.taskOverlayText = taskOverlayText;
    }

    public String getTaskOverlayFontType() {
        return taskOverlayFontType;
    }

    public void setTaskOverlayFontType(String taskOverlayFontType) {
        this.taskOverlayFontType = taskOverlayFontType;
    }

    public Long getTaskOverlayFontSize() {
        return taskOverlayFontSize;
    }

    public void setTaskOverlayFontSize(Long taskOverlayFontSize) {
        this.taskOverlayFontSize = taskOverlayFontSize;
    }

    public String getTaskOverlayFontColor() {
        return taskOverlayFontColor;
    }

    public void setTaskOverlayFontColor(String taskOverlayFontColor) {
        this.taskOverlayFontColor = taskOverlayFontColor;
    }

    public Long getTaskOverlayTextPercentageOpacity() {
        return taskOverlayTextPercentageOpacity;
    }

    public void setTaskOverlayTextPercentageOpacity(Long taskOverlayTextPercentageOpacity) {
        this.taskOverlayTextPercentageOpacity = taskOverlayTextPercentageOpacity;
    }

    public Boolean getTaskOverlayBoxEnable() {
        return taskOverlayBoxEnable;
    }

    public void setTaskOverlayBoxEnable(Boolean taskOverlayBoxEnable) {
        this.taskOverlayBoxEnable = taskOverlayBoxEnable;
    }

    public String getTaskOverlayBoxColor() {
        return taskOverlayBoxColor;
    }

    public void setTaskOverlayBoxColor(String taskOverlayBoxColor) {
        this.taskOverlayBoxColor = taskOverlayBoxColor;
    }

    public Long getTaskOverlayBoxPercentageOpacity() {
        return taskOverlayBoxPercentageOpacity;
    }

    public void setTaskOverlayBoxPercentageOpacity(Long taskOverlayBoxPercentageOpacity) {
        this.taskOverlayBoxPercentageOpacity = taskOverlayBoxPercentageOpacity;
    }

    public Float getTaskFrameInstantInSeconds() {
        return taskFrameInstantInSeconds;
    }

    public void setTaskFrameInstantInSeconds(Float taskFrameInstantInSeconds) {
        this.taskFrameInstantInSeconds = taskFrameInstantInSeconds;
    }

    public Long getTaskFrameWidth() {
        return taskFrameWidth;
    }

    public void setTaskFrameWidth(Long taskFrameWidth) {
        this.taskFrameWidth = taskFrameWidth;
    }

    public Long getTaskFrameHeight() {
        return taskFrameHeight;
    }

    public void setTaskFrameHeight(Long taskFrameHeight) {
        this.taskFrameHeight = taskFrameHeight;
    }

    public Long getTaskFramePeriodInSeconds() {
        return taskFramePeriodInSeconds;
    }

    public void setTaskFramePeriodInSeconds(Long taskFramePeriodInSeconds) {
        this.taskFramePeriodInSeconds = taskFramePeriodInSeconds;
    }

    public Long getTaskFrameMaxFramesNumber() {
        return taskFrameMaxFramesNumber;
    }

    public void setTaskFrameMaxFramesNumber(Long taskFrameMaxFramesNumber) {
        this.taskFrameMaxFramesNumber = taskFrameMaxFramesNumber;
    }

    public Float getTaskFrameDurationOfEachSlideInSeconds() {
        return taskFrameDurationOfEachSlideInSeconds;
    }

    public void setTaskFrameDurationOfEachSlideInSeconds(Float taskFrameDurationOfEachSlideInSeconds) {
        this.taskFrameDurationOfEachSlideInSeconds = taskFrameDurationOfEachSlideInSeconds;
    }

    public String getMediaItemsSelectionMode() {
        return mediaItemsSelectionMode;
    }

    public void setMediaItemsSelectionMode(String mediaItemsSelectionMode) {
        this.mediaItemsSelectionMode = mediaItemsSelectionMode;
    }

    public void setMediaItemsList(List<MediaItem> mediaItemsList) {
        this.mediaItemsList = mediaItemsList;
    }

    public List<MediaItem> getMediaItemsList() {
        return mediaItemsList;
    }

    public Date getMediaItemsBegin() {
        return mediaItemsBegin;
    }

    public void setMediaItemsBegin(Date mediaItemsBegin) {
        this.mediaItemsBegin = mediaItemsBegin;
    }

    public Date getMediaItemsEnd() {
        return mediaItemsEnd;
    }

    public void setMediaItemsEnd(Date mediaItemsEnd) {
        this.mediaItemsEnd = mediaItemsEnd;
    }

    public String getMediaItemsContentType() {
        return mediaItemsContentType;
    }

    public void setMediaItemsContentType(String mediaItemsContentType) {
        this.mediaItemsContentType = mediaItemsContentType;
    }

    public List<String> getMediaItemsContentTypesList() {
        return mediaItemsContentTypesList;
    }

    public void setMediaItemsContentTypesList(List<String> mediaItemsContentTypesList) {
        this.mediaItemsContentTypesList = mediaItemsContentTypesList;
    }

    public Long getMediaItemsMaxMediaItemsNumber() {
        return mediaItemsMaxMediaItemsNumber;
    }

    public void setMediaItemsMaxMediaItemsNumber(Long mediaItemsMaxMediaItemsNumber) {
        this.mediaItemsMaxMediaItemsNumber = mediaItemsMaxMediaItemsNumber;
    }

    public Long getMediaItemsNumber() {
        return mediaItemsNumber;
    }

    public void setMediaItemsNumber(Long mediaItemsNumber) {
        this.mediaItemsNumber = mediaItemsNumber;
    }

    public String getTaskReferences() {
        return taskReferences;
    }

    public void setTaskReferences(String taskReferences) {
        this.taskReferences = taskReferences;
    }

    public String getPredefined() {
        return predefined;
    }

    public void setPredefined(String predefined) {
        this.predefined = predefined;
    }

    public String getData() {
        return data;
    }

    public void setData(String data) {
        this.data = data;
    }

    public Long getTimeInSecondsDecimalsPrecision() {
        return timeInSecondsDecimalsPrecision;
    }

    public void setTimeInSecondsDecimalsPrecision(Long timeInSecondsDecimalsPrecision) {
        this.timeInSecondsDecimalsPrecision = timeInSecondsDecimalsPrecision;
    }

    public String getMediaItemsToBeAddedOrReplaced() {
        return mediaItemsToBeAddedOrReplaced;
    }

    public void setMediaItemsToBeAddedOrReplaced(String mediaItemsToBeAddedOrReplaced) {
        this.mediaItemsToBeAddedOrReplaced = mediaItemsToBeAddedOrReplaced;
    }

    public String getIngestWorkflowErrorMessage() {
        return ingestWorkflowErrorMessage;
    }

    public void setIngestWorkflowErrorMessage(String ingestWorkflowErrorMessage) {
        this.ingestWorkflowErrorMessage = ingestWorkflowErrorMessage;
    }
}
