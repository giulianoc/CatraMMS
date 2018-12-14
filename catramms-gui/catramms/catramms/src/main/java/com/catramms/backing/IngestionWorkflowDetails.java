package com.catramms.backing;

import com.catramms.backing.common.SessionUtils;
import com.catramms.backing.common.Workspace;
import com.catramms.backing.entity.IngestionJob;
import com.catramms.backing.entity.IngestionWorkflow;
import com.catramms.backing.entity.MediaItem;
import com.catramms.utility.catramms.CatraMMS;
import org.apache.log4j.Logger;

import javax.faces.bean.ManagedBean;
import javax.faces.bean.ViewScoped;
import java.io.Serializable;

/**
 * Created with IntelliJ IDEA.
 * User: multi
 * Date: 27/09/15
 * Time: 20:28
 * To change this template use File | Settings | File Templates.
 */
@ManagedBean
@ViewScoped
public class IngestionWorkflowDetails extends Workspace implements Serializable {

    // static because the class is Serializable
    private static final Logger mLogger = Logger.getLogger(IngestionWorkflowDetails.class);

    private int autoRefreshPeriodInSeconds;
    private boolean autoRefresh;

    private IngestionWorkflow ingestionWorkflow = null;
    private Long ingestionRootKey;

    private Long userKey;
    private String apiKey;

    public void init()
    {
        if (ingestionRootKey == null)
        {
            String errorMessage = "ingestionRootKey is null";
            mLogger.error(errorMessage);

            return;
        }

        autoRefresh = true;
        autoRefreshPeriodInSeconds = 30;

        fillIngestionWorkflow();
    }

    public void fillIngestionWorkflow()
    {
        try
        {
            userKey = SessionUtils.getUserKey();
            apiKey = SessionUtils.getAPIKey();

            if (userKey == null || apiKey == null || apiKey.equalsIgnoreCase(""))
            {
                String errorMessage = "no input to require ingestionRoot"
                                + ", userKey: " + userKey
                                + ", apiKey: " + apiKey
                ;
                mLogger.error(errorMessage);

                throw new Exception(errorMessage);
            }

            String username = userKey.toString();
            String password = apiKey;

            CatraMMS catraMMS = new CatraMMS();
            ingestionWorkflow = catraMMS.getIngestionWorkflow(
                    username, password, ingestionRootKey);
        }
        catch (Exception e)
        {
            String errorMessage = "Exception: " + e;
            mLogger.error(errorMessage);
        }
    }

    public String getIngestionJobStyleClass(int rowId)
    {
        String styleClass = "";

        String status = ingestionWorkflow.getIngestionJobList().get(rowId).getStatus();

        if (status.equalsIgnoreCase("End_TaskSuccess"))
            styleClass = "successFullColor";
        else if (status.equalsIgnoreCase("End_NotToBeExecuted"))
            styleClass = "successFullColor";
        else if (status.startsWith("End_"))
            styleClass = "failureColor";
        else if (status.equalsIgnoreCase("Start_TaskQueued"))
            styleClass = "toBeProcessedColor";
        else
            styleClass = "processingColor";

        return styleClass;
    }

    public IngestionWorkflow getIngestionWorkflow() {
        return ingestionWorkflow;
    }

    public void setIngestionWorkflow(IngestionWorkflow ingestionWorkflow) {
        this.ingestionWorkflow = ingestionWorkflow;
    }

    public Long getIngestionRootKey() {
        return ingestionRootKey;
    }

    public void setIngestionRootKey(Long ingestionRootKey) {
        this.ingestionRootKey = ingestionRootKey;
    }

    public int getAutoRefreshPeriodInSeconds() {
        return autoRefreshPeriodInSeconds;
    }

    public void setAutoRefreshPeriodInSeconds(int autoRefreshPeriodInSeconds) {
        this.autoRefreshPeriodInSeconds = autoRefreshPeriodInSeconds;
    }

    public boolean isAutoRefresh() {
        return autoRefresh;
    }

    public void setAutoRefresh(boolean autoRefresh) {
        this.autoRefresh = autoRefresh;
    }
}
