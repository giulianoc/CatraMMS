package com.catramms.backing;

import com.catramms.backing.common.SessionUtils;
import com.catramms.backing.common.Workspace;
import com.catramms.backing.entity.IngestionWorkflow;
import com.catramms.backing.entity.MediaItem;
import com.catramms.backing.entity.WorkspaceDetails;
import com.catramms.utility.catramms.CatraMMS;
import org.apache.log4j.Logger;

import javax.annotation.PostConstruct;
import javax.faces.application.FacesMessage;
import javax.faces.bean.ManagedBean;
import javax.faces.bean.ViewScoped;
import javax.faces.context.FacesContext;
import javax.servlet.http.HttpSession;
import java.io.Serializable;
import java.text.SimpleDateFormat;
import java.util.ArrayList;
import java.util.Calendar;
import java.util.Date;
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
public class IngestionWorkflows extends Workspace implements Serializable {

    // static because the class is Serializable
    private static final Logger mLogger = Logger.getLogger(IngestionWorkflows.class);

    private int autoRefreshPeriodInSeconds;
    private boolean autoRefresh;
    private Date begin;
    private Date end;

    private boolean ascending;
    private Long maxIngestionWorkflowsNumber = new Long(100);

    private Long ingestionWorkflowsNumber = new Long(0);
    private List<IngestionWorkflow> ingestionWorkflowsList = new ArrayList<>();

    @PostConstruct
    public void init()
    {
        mLogger.debug("init");

        ascending = false;

        autoRefresh = true;
        autoRefreshPeriodInSeconds = 30;

        {
            Calendar calendar = Calendar.getInstance();

            calendar.add(Calendar.DAY_OF_MONTH, -1);

            calendar.set(Calendar.HOUR_OF_DAY, 0);
            calendar.set(Calendar.MINUTE, 0);
            calendar.set(Calendar.SECOND, 0);
            calendar.set(Calendar.MILLISECOND, 0);

            begin = calendar.getTime();
        }

        {
            Calendar calendar = Calendar.getInstance();

            calendar.add(Calendar.HOUR_OF_DAY, 5);

            calendar.set(Calendar.MINUTE, 0);
            calendar.set(Calendar.SECOND, 0);
            calendar.set(Calendar.MILLISECOND, 0);

            end = calendar.getTime();
        }
    }

    public void beginChanged()
    {
        SimpleDateFormat simpleDateFormat = new SimpleDateFormat("yyyy-MM-dd HH:mm:ss");

        mLogger.debug("beginChanged. begin: " + simpleDateFormat.format(begin) + ", end: " + simpleDateFormat.format(end));
        if (begin.getTime() >= end.getTime())
        {
            Calendar calendar = Calendar.getInstance();

            calendar.setTime(begin);

            calendar.add(Calendar.DAY_OF_MONTH, 1);

            end.setTime(calendar.getTime().getTime());

            // really face message is useless because of the redirection
            FacesMessage message = new FacesMessage(FacesMessage.SEVERITY_ERROR,
                    "Traffic System Update", "BeginDate cannot be later the EndDate");
            FacesContext context = FacesContext.getCurrentInstance();
            context.addMessage(null, message);
        }
        /*
        else if ((end.getTime() - begin.getTime()) / 1000 > maxDaysDisplayed * 24 * 3600)
        {
            Calendar calendar = Calendar.getInstance();

            calendar.setTime(begin);

            calendar.add(Calendar.DAY_OF_MONTH, maxDaysDisplayed);

            end.setTime(calendar.getTime().getTime());

            // really face message is useless because of the redirection
            FacesMessage message = new FacesMessage(FacesMessage.SEVERITY_ERROR,
                "Traffic System Update", "Max " + maxDaysDisplayed + " days can be displayed");
            FacesContext context = FacesContext.getCurrentInstance();
            context.addMessage(null, message);
        }
        */

        mLogger.debug("beginChanged. begin: " + simpleDateFormat.format(begin) + ", end: " + simpleDateFormat.format(end));

        fillList(true);
    }

    public void endChanged()
    {
        SimpleDateFormat simpleDateFormat = new SimpleDateFormat("yyyy-MM-dd HH:mm:ss");

        mLogger.debug("endChanged. begin: " + simpleDateFormat.format(begin) + ", end: " + simpleDateFormat.format(end));
        if (begin.getTime() >= end.getTime())
        {
            Calendar calendar = Calendar.getInstance();

            calendar.setTime(end);

            calendar.add(Calendar.DAY_OF_MONTH, -1);

            begin.setTime(calendar.getTime().getTime());

            // really face message is useless because of the redirection
            FacesMessage message = new FacesMessage(FacesMessage.SEVERITY_ERROR,
                "Traffic System Update", "BeginDate cannot be later the EndDate");
            FacesContext context = FacesContext.getCurrentInstance();
            context.addMessage(null, message);
        }
        /*
        else if ((end.getTime() - begin.getTime()) / 1000 > maxDaysDisplayed * 24 * 3600)
        {
            Calendar calendar = Calendar.getInstance();

            calendar.setTime(end);

            calendar.add(Calendar.DAY_OF_MONTH, -maxDaysDisplayed);

            begin.setTime(calendar.getTime().getTime());

            // really face message is useless because of the redirection
            FacesMessage message = new FacesMessage(FacesMessage.SEVERITY_ERROR,
                "Traffic System Update", "Max " + maxDaysDisplayed + " days can be displayed");
            FacesContext context = FacesContext.getCurrentInstance();
            context.addMessage(null, message);
        }
        */

        mLogger.debug("endChanged. begin: " + simpleDateFormat.format(begin) + ", end: " + simpleDateFormat.format(end));

        fillList(true);
    }

    public void fillList(boolean toBeRedirected)
    {
        SimpleDateFormat simpleDateFormat = new SimpleDateFormat("yyyy-MM-dd HH:mm:ss");

        mLogger.info("fillList"
                + ", toBeRedirected: " + toBeRedirected
                + ", maxIngestionWorkflowsNumber: " + maxIngestionWorkflowsNumber
                + ", autoRefresh: " + autoRefresh
                + ", begin: " + simpleDateFormat.format(begin)
            + ", end: " + simpleDateFormat.format(end));

        if (toBeRedirected)
        {
            try
            {
                SimpleDateFormat simpleDateFormat_1 = new SimpleDateFormat("yyyy-MM-dd'T'HH:mm:ss");

                String url = "ingestionWorkflows.xhtml?maxIngestionWorkflowsNumber=" + maxIngestionWorkflowsNumber
                        + "&ascending=" + ascending
                        + "&autoRefresh=" + autoRefresh
                        + "&begin=" + simpleDateFormat_1.format(begin)
                        + "&end=" + simpleDateFormat_1.format(end)
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
            {
                try
                {
                    Long userKey = SessionUtils.getUserKey();
                    String apiKey = SessionUtils.getAPIKey();

                    if (userKey == null || apiKey == null || apiKey.equalsIgnoreCase(""))
                    {
                        mLogger.warn("no input to require ingestionWorkflows"
                                + ", userKey: " + userKey
                                + ", apiKey: " + apiKey
                        );
                    }
                    else
                    {
                        String username = userKey.toString();
                        String password = apiKey;

                        ingestionWorkflowsList.clear();

                        CatraMMS catraMMS = new CatraMMS();
                        ingestionWorkflowsNumber = catraMMS.getIngestionWorkflows(
                                username, password, maxIngestionWorkflowsNumber,
                                begin, end, ascending, ingestionWorkflowsList);
                    }
                }
                catch (Exception e)
                {
                    String errorMessage = "Exception: " + e;
                    mLogger.error(errorMessage);
                }
            }
        }
    }

    public Date getBegin() {
        return begin;
    }

    public void setBegin(Date begin) {
        this.begin = begin;
    }

    public Date getEnd() {
        return end;
    }

    public void setEnd(Date end) {
        this.end = end;
    }

    public boolean isAutoRefresh() {
        return autoRefresh;
    }

    public void setAutoRefresh(boolean autoRefresh) {
        this.autoRefresh = autoRefresh;
    }

    public int getAutoRefreshPeriodInSeconds() {
        return autoRefreshPeriodInSeconds;
    }

    public void setAutoRefreshPeriodInSeconds(int autoRefreshPeriodInSeconds) {
        this.autoRefreshPeriodInSeconds = autoRefreshPeriodInSeconds;
    }

    public Long getMaxIngestionWorkflowsNumber() {
        return maxIngestionWorkflowsNumber;
    }

    public void setMaxIngestionWorkflowsNumber(Long maxIngestionWorkflowsNumber) {
        this.maxIngestionWorkflowsNumber = maxIngestionWorkflowsNumber;
    }

    public Long getIngestionWorkflowsNumber() {
        return ingestionWorkflowsNumber;
    }

    public void setIngestionWorkflowsNumber(Long ingestionWorkflowsNumber) {
        this.ingestionWorkflowsNumber = ingestionWorkflowsNumber;
    }

    public boolean isAscending() {
        return ascending;
    }

    public void setAscending(boolean ascending) {
        this.ascending = ascending;
    }

    public List<IngestionWorkflow> getIngestionWorkflowsList() {
        return ingestionWorkflowsList;
    }

    public void setIngestionWorkflowsList(List<IngestionWorkflow> ingestionWorkflowsList) {
        this.ingestionWorkflowsList = ingestionWorkflowsList;
    }
}
