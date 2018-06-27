package com.catramms.backing;

import com.catramms.backing.common.SessionUtils;
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
public class IngestionWorkflowDetails implements Serializable {

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

        try {
            userKey = SessionUtils.getUserKey();
            apiKey = SessionUtils.getAPIKey();

            if (userKey == null || apiKey == null || apiKey.equalsIgnoreCase(""))
            {
                mLogger.warn("no input to require ingestionRoot"
                                + ", userKey: " + userKey
                                + ", apiKey: " + apiKey
                );
            }
            else
            {
                fillIngestionWorkflow();
            }
        }
        catch (Exception e)
        {
            String errorMessage = "Exception: " + e;
            mLogger.error(errorMessage);
        }
    }

    public void fillIngestionWorkflow()
    {
        try {
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
