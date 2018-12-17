package com.catramms.backing;

import com.catramms.backing.common.SessionUtils;
import com.catramms.backing.common.Workspace;
import com.catramms.backing.entity.EncodingJob;
import com.catramms.backing.entity.IngestionJob;
import com.catramms.utility.catramms.CatraMMS;
import org.apache.log4j.Logger;

import javax.faces.bean.ManagedBean;
import javax.faces.bean.ViewScoped;
import javax.faces.context.FacesContext;
import java.io.Serializable;
import java.text.SimpleDateFormat;

/**
 * Created with IntelliJ IDEA.
 * User: multi
 * Date: 27/09/15
 * Time: 20:28
 * To change this template use File | Settings | File Templates.
 */
@ManagedBean
@ViewScoped
public class EncodingJobDetails extends Workspace implements Serializable {

    // static because the class is Serializable
    private static final Logger mLogger = Logger.getLogger(EncodingJobDetails.class);

    private EncodingJob encodingJob = null;
    private Long encodingJobKey;


    public void init()
    {
        if (encodingJobKey == null)
        {
            String errorMessage = "encodingJobKey is null";
            mLogger.error(errorMessage);

            return;
        }

        try {
            Long userKey = SessionUtils.getUserKey();
            String apiKey = SessionUtils.getAPIKey();

            if (userKey == null || apiKey == null || apiKey.equalsIgnoreCase(""))
            {
                mLogger.warn("no input to require ingestionRoot"
                                + ", userKey: " + userKey
                                + ", apiKey: " + apiKey
                );
            }
            else
            {
                String username = userKey.toString();
                String password = apiKey;

                CatraMMS catraMMS = new CatraMMS();
                encodingJob = catraMMS.getEncodingJob(
                        username, password, encodingJobKey);
            }
        }
        catch (Exception e)
        {
            String errorMessage = "Exception: " + e;
            mLogger.error(errorMessage);

            encodingJob = null;
        }
    }

    public void increaseDecreasePriority(String increase)
    {
        mLogger.info("Received increaseDecreasePriority. increase: " + increase);

        try {
            Long userKey = SessionUtils.getUserKey();
            String apiKey = SessionUtils.getAPIKey();

            if (userKey == null || apiKey == null || apiKey.equalsIgnoreCase(""))
            {
                mLogger.warn("no input to require ingestionRoot"
                                + ", userKey: " + userKey
                                + ", apiKey: " + apiKey
                );
            }
            else
            {
                String username = userKey.toString();
                String password = apiKey;

                CatraMMS catraMMS = new CatraMMS();
                catraMMS.updateEncodingJobPriority(username, password, encodingJobKey,
                        increase.equalsIgnoreCase("true") ? encodingJob.getEncodingPriorityCode() + 1
                                : encodingJob.getEncodingPriorityCode() - 1);
            }
        }
        catch (Exception e)
        {
            String errorMessage = "Exception: " + e;
            mLogger.error(errorMessage);

            return;
        }

        try
        {
            String url = "encodingJobDetails.xhtml?encodingJobKey=" + encodingJobKey
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

    public void tryEncodingAgain()
    {
        mLogger.info("Received tryEncodingAgain");

        try {
            Long userKey = SessionUtils.getUserKey();
            String apiKey = SessionUtils.getAPIKey();

            if (userKey == null || apiKey == null || apiKey.equalsIgnoreCase(""))
            {
                mLogger.warn("no input to require ingestionRoot"
                        + ", userKey: " + userKey
                        + ", apiKey: " + apiKey
                );
            }
            else
            {
                String username = userKey.toString();
                String password = apiKey;

                CatraMMS catraMMS = new CatraMMS();
                catraMMS.updateEncodingJobTryAgain(username, password, encodingJobKey);
            }
        }
        catch (Exception e)
        {
            String errorMessage = "Exception: " + e;
            mLogger.error(errorMessage);

            return;
        }

        try
        {
            String url = "encodingJobDetails.xhtml?encodingJobKey=" + encodingJobKey
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

    public EncodingJob getEncodingJob() {
        return encodingJob;
    }

    public void setEncodingJob(EncodingJob encodingJob) {
        this.encodingJob = encodingJob;
    }

    public Long getEncodingJobKey() {
        return encodingJobKey;
    }

    public void setEncodingJobKey(Long encodingJobKey) {
        this.encodingJobKey = encodingJobKey;
    }
}
