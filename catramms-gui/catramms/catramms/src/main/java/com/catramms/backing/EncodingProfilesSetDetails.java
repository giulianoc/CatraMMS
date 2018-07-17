package com.catramms.backing;

import com.catramms.backing.common.SessionUtils;
import com.catramms.backing.common.Workspace;
import com.catramms.backing.entity.EncodingProfile;
import com.catramms.backing.entity.EncodingProfilesSet;
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
public class EncodingProfilesSetDetails extends Workspace implements Serializable {

    // static because the class is Serializable
    private static final Logger mLogger = Logger.getLogger(EncodingProfilesSetDetails.class);

    private EncodingProfilesSet encodingProfilesSet = null;
    private Long encodingProfilesSetKey;


    public void init()
    {
        if (encodingProfilesSetKey == null)
        {
            String errorMessage = "encodingProfilesSetKey is null";
            mLogger.error(errorMessage);

            return;
        }

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
                encodingProfilesSet = catraMMS.getEncodingProfilesSet(
                        username, password, encodingProfilesSetKey);
            }
        }
        catch (Exception e)
        {
            String errorMessage = "Exception: " + e;
            mLogger.error(errorMessage);

            encodingProfilesSet = null;
        }
    }

    public EncodingProfilesSet getEncodingProfilesSet() {
        return encodingProfilesSet;
    }

    public void setEncodingProfilesSet(EncodingProfilesSet encodingProfilesSet) {
        this.encodingProfilesSet = encodingProfilesSet;
    }

    public Long getEncodingProfilesSetKey() {
        return encodingProfilesSetKey;
    }

    public void setEncodingProfilesSetKey(Long encodingProfilesSetKey) {
        this.encodingProfilesSetKey = encodingProfilesSetKey;
    }
}
