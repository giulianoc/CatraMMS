package com.catramms.utility.httpFetcher;

import org.apache.commons.httpclient.*;
import org.apache.commons.io.IOUtils;
import org.apache.commons.lang.StringUtils;
import org.apache.log4j.Logger;
import sun.net.www.protocol.http.HttpURLConnection;

import javax.net.ssl.*;
import javax.xml.bind.DatatypeConverter;
import java.io.*;
import java.net.URL;
import java.security.SecureRandom;


public class HttpFeedFetcher {

    private static final Logger mLogger = Logger.getLogger(HttpFeedFetcher.class);

    public static final String configFileName = "mpCommon.properties";

    static public String fetchGetHttpsJson(String url, int timeoutInSeconds, int maxRetriesNumber,
                                            String user, String password)
            throws Exception
    {
        // fetchWebPage
        mLogger.debug(String.format("fetchWebPage(%s) ", url));
        String result = "";
        // Date startTimestamp = new Date();
        if (StringUtils.isNotEmpty(url))
        {
            {
                SSLContext ctx = SSLContext.getInstance("SSL");
                ctx.init(new KeyManager[0], new TrustManager[] {new DefaultTrustManager()}, new SecureRandom());
                SSLContext.setDefault(ctx);
                HttpsURLConnection.setDefaultSSLSocketFactory(ctx.getSocketFactory());

                // Create all-trusting host name verifier
                HostnameVerifier allHostsValid = new HostnameVerifier() {
                    public boolean verify(String hostname, SSLSession session) {
                        return true;
                    }
                };

                // Install the all-trusting host verifier
                HttpsURLConnection.setDefaultHostnameVerifier(allHostsValid);
            }

            // GetMethod method = null;
            int retryIndex = 0;

            while(retryIndex < maxRetriesNumber)
            {
                retryIndex++;

                try
                {
                    mLogger.info("url: " + url);
                    URL uUrl = new URL(url);
                    HttpsURLConnection conn = (HttpsURLConnection) uUrl.openConnection();
                    conn.setConnectTimeout(timeoutInSeconds * 1000);
                    conn.setReadTimeout(timeoutInSeconds * 1000);

                    {
                        String encoded = DatatypeConverter.printBase64Binary((user + ":" + password).getBytes("utf-8"));
                        conn.setRequestProperty("Authorization", "Basic " + encoded);
                        mLogger.info("Add Header (user " + user + "). " + "Authorization: " + "Basic " + encoded);
                        // mLogger.info("Add Header (password " + password + "). " + "Authorization: " + "Basic " + encoded);
                    }

                    mLogger.info("conn.getResponseCode...");
                    int statusCode = conn.getResponseCode();

                    mLogger.info("conn.getResponseCode. statusCode: " + statusCode);
                    if (statusCode != HttpStatus.SC_OK && statusCode != HttpStatus.SC_CREATED)
                    {
                        mLogger.debug("Method failed: " + conn.getResponseMessage());

                        result = null;

                        throw new Exception("Method failed: " + conn.getResponseMessage());
                    }
                    else
                    {
                        // Read the response body.
                        // result = method.getResponseBodyAsString();
                        InputStream is = conn.getInputStream();
                        InputStreamReader isr = new InputStreamReader(is);

                        int numCharsRead;
                        char[] charArray = new char[1024 * 10];
                        StringBuffer sb = new StringBuffer();
                        while ((numCharsRead = isr.read(charArray)) > 0)
                            sb.append(charArray, 0, numCharsRead);

                        result = sb.toString();
                    }

                    mLogger.debug("result: " + result);
                }
                catch (HttpException e) {
                    String errorMessage = "URL: " + url
                            + ", Fatal protocol violation: " + e
                            + ", maxRetriesNumber: " + maxRetriesNumber
                            + ", retryIndex: " + (retryIndex - 1)
                            ;
                    mLogger.error(errorMessage);

                    if (retryIndex >= maxRetriesNumber)
                        throw e;
                    else
                        Thread.sleep(100);  // half second
                }
                catch (IOException e) {
                    String errorMessage = "URL: " + url
                            + ", Fatal transport error: " + e
                            + ", maxRetriesNumber: " + maxRetriesNumber
                            + ", retryIndex: " + (retryIndex - 1)
                            ;
                    mLogger.error(errorMessage);

                    if (retryIndex >= maxRetriesNumber)
                        throw e;
                    else
                        Thread.sleep(100);  // half second
                }
                catch (Exception e) {
                    String errorMessage = "URL: " + url
                            + ", Fatal transport error: " + e
                            + ", maxRetriesNumber: " + maxRetriesNumber
                            + ", retryIndex: " + (retryIndex - 1)
                            ;
                    mLogger.error(errorMessage);

                    if (retryIndex >= maxRetriesNumber)
                        throw e;
                    else
                        Thread.sleep(100);  // half second
                }
                /*
                finally {
                    // Release the connection.
                    if (method != null)
                        method.releaseConnection();
                }
                */
            }
        }

        // elapsed time saved in the calling method
        // mLogger.info("@fetchHttpsJson " + url + "@ elapsed (milliseconds): @" + (new Date().getTime() - startTimestamp.getTime()) + "@");

        return result;
    }

    static public String fetchPostHttpsJson(String url, int timeoutInSeconds, int maxRetriesNumber,
                                            String user, String password, String postBodyRequest)
            throws Exception
    {
        return fetchBodyHttpsJson("POST", url, timeoutInSeconds, maxRetriesNumber, user, password, postBodyRequest);
    }

    static public String fetchPutHttpsJson(String url, int timeoutInSeconds, int maxRetriesNumber,
                                            String user, String password, String postBodyRequest)
            throws Exception
    {
        return fetchBodyHttpsJson("PUT", url, timeoutInSeconds, maxRetriesNumber, user, password, postBodyRequest);
    }

    static public String fetchDeleteHttpsJson(String url, int timeoutInSeconds, int maxRetriesNumber,
                                           String user, String password)
            throws Exception
    {
        String postBodyRequest = null;
        return fetchBodyHttpsJson("DELETE", url, timeoutInSeconds, maxRetriesNumber, user, password, postBodyRequest);
    }

    static private String fetchBodyHttpsJson(String httpMethod, String url, int timeoutInSeconds, int maxRetriesNumber,
                                        String user, String password, String postBodyRequest)
            throws Exception
    {
        // fetchWebPage
        mLogger.debug(String.format("fetchWebPage(%s) ", url));
        String result = "";
        // Date startTimestamp = new Date();
        if (StringUtils.isNotEmpty(url))
        {
            {
                SSLContext ctx = SSLContext.getInstance("SSL");
                ctx.init(new KeyManager[0], new TrustManager[] {new DefaultTrustManager()}, new SecureRandom());
                SSLContext.setDefault(ctx);
                HttpsURLConnection.setDefaultSSLSocketFactory(ctx.getSocketFactory());

                // Create all-trusting host name verifier
                HostnameVerifier allHostsValid = new HostnameVerifier() {
                    public boolean verify(String hostname, SSLSession session) {
                        return true;
                    }
                };

                // Install the all-trusting host verifier
                HttpsURLConnection.setDefaultHostnameVerifier(allHostsValid);
            }

            // GetMethod method = null;
            int retryIndex = 0;

            while(retryIndex < maxRetriesNumber)
            {
                retryIndex++;

                try
                {
                    /*
                    method = new GetMethod(url);

                    // Provide custom retry handler is necessary
                    method.getParams().setParameter(HttpMethodParams.RETRY_HANDLER, new DefaultHttpMethodRetryHandler(3, false));
                    // method.addRequestHeader("X-Inline", "describedby");
                    // Credentials credentials = new UsernamePasswordCredentials("admin", "admin");

                    HttpClient httpClient = new HttpClient();
                    // httpClient.getState().setCredentials(AuthScope.ANY, credentials);

                    // Execute the method.
                    int statusCode = httpClient.executeMethod(method);
                    */

                    mLogger.info("url: " + url);
                    URL uUrl = new URL(url);
                    HttpsURLConnection conn = (HttpsURLConnection) uUrl.openConnection();
                    conn.setConnectTimeout(timeoutInSeconds * 1000);
                    conn.setReadTimeout(timeoutInSeconds * 1000);
                    /*
                    conn.setHostnameVerifier(new HostnameVerifier() {
                        @Override
                        public boolean verify(String arg0, SSLSession arg1) {
                            return true;
                        }
                    });
                    */

                    if (user != null && password != null)
                    {
                        String encoded = DatatypeConverter.printBase64Binary((user + ":" + password).getBytes("utf-8"));
                        conn.setRequestProperty("Authorization", "Basic " + encoded);
                        mLogger.info("Add Header (user " + user + "). " + "Authorization: " + "Basic " + encoded);
                        mLogger.info("Add Header (password " + password + "). " + "Authorization: " + "Basic " + encoded);
                    }

                    conn.setDoOutput(true); // false because I do not need to append any data to this request
                    conn.setRequestMethod(httpMethod);
                    {
                        int clength;
                        byte[] bytes = null;
                        if (postBodyRequest != null)
                        {
                            bytes = postBodyRequest.getBytes("UTF-8");
                            clength = bytes.length;
                        }
                        else
                        {
                            clength = 0;
                        }

                        if (clength > 0)
                        {
                            conn.setRequestProperty("Content-Type", "application/json");
                            mLogger.info("Header. " + "Content-Type: " + "application/json");
                        }

                        conn.setRequestProperty("Content-Length", String.valueOf(clength));
                        mLogger.info("Header. " + "Content-Length: " + String.valueOf(clength));

                        conn.setDoInput(true); // false means the response is ignored

                        if (clength > 0)
                        {
                            // con.getOutputStream().write(postBodyRequest.getBytes(), 0, clength);
                            OutputStream outputStream = conn.getOutputStream();
                            outputStream.write(bytes);
                            outputStream.flush();
                            outputStream.close();
                        }
                    }

                    mLogger.info("conn.getResponseCode...");
                    int statusCode = conn.getResponseCode();

                    mLogger.info("conn.getResponseCode. statusCode: " + statusCode);
                    if (statusCode != HttpStatus.SC_OK && statusCode != HttpStatus.SC_CREATED)
                    {
                        mLogger.debug("Method failed: " + conn.getResponseMessage());

                        result = null;

                        throw new Exception("Method failed: " + conn.getResponseMessage());
                    }
                    else
                    {
                        // Read the response body.
                        // result = method.getResponseBodyAsString();
                        InputStream is = conn.getInputStream();
                        InputStreamReader isr = new InputStreamReader(is);

                        int numCharsRead;
                        char[] charArray = new char[1024 * 10];
                        StringBuffer sb = new StringBuffer();
                        while ((numCharsRead = isr.read(charArray)) > 0)
                            sb.append(charArray, 0, numCharsRead);

                        result = sb.toString();
                    }

                    mLogger.debug("result: " + result);
                }
                catch (HttpException e) {
                    String errorMessage = "URL: " + url
                            + ", Fatal protocol violation: " + e
                            + ", maxRetriesNumber: " + maxRetriesNumber
                            + ", retryIndex: " + (retryIndex - 1)
                            ;
                    mLogger.error(errorMessage);

                    if (retryIndex >= maxRetriesNumber)
                        throw e;
                    else
                        Thread.sleep(100);  // half second
                }
                catch (IOException e) {
                    String errorMessage = "URL: " + url
                            + ", Fatal transport error: " + e
                            + ", maxRetriesNumber: " + maxRetriesNumber
                            + ", retryIndex: " + (retryIndex - 1)
                            ;
                    mLogger.error(errorMessage);

                    if (retryIndex >= maxRetriesNumber)
                        throw e;
                    else
                        Thread.sleep(100);  // half second
                }
                catch (Exception e) {
                    String errorMessage = "URL: " + url
                            + ", Fatal transport error: " + e
                            + ", maxRetriesNumber: " + maxRetriesNumber
                            + ", retryIndex: " + (retryIndex - 1)
                            ;
                    mLogger.error(errorMessage);

                    if (retryIndex >= maxRetriesNumber)
                        throw e;
                    else
                        Thread.sleep(100);  // half second
                }
                /*
                finally {
                    // Release the connection.
                    if (method != null)
                        method.releaseConnection();
                }
                */
            }
        }

        // elapsed time saved in the calling method
        // mLogger.info("@fetchHttpsJson " + url + "@ elapsed (milliseconds): @" + (new Date().getTime() - startTimestamp.getTime()) + "@");

        return result;
    }

    static public void fetchPostHttpBinary(String url, int timeoutInSeconds, int maxRetriesNumber,
                                            String user, String password, File binaryPathName)
            throws Exception
    {
        // fetchWebPage
        mLogger.debug(String.format("fetchWebPage(%s) ", url));
        // Date startTimestamp = new Date();
        if (StringUtils.isNotEmpty(url))
        {
            // GetMethod method = null;
            int retryIndex = 0;

            while(retryIndex < maxRetriesNumber)
            {
                retryIndex++;

                try
                {
                    /*
                    method = new GetMethod(url);

                    // Provide custom retry handler is necessary
                    method.getParams().setParameter(HttpMethodParams.RETRY_HANDLER, new DefaultHttpMethodRetryHandler(3, false));
                    // method.addRequestHeader("X-Inline", "describedby");
                    // Credentials credentials = new UsernamePasswordCredentials("admin", "admin");

                    HttpClient httpClient = new HttpClient();
                    // httpClient.getState().setCredentials(AuthScope.ANY, credentials);

                    // Execute the method.
                    int statusCode = httpClient.executeMethod(method);
                    */

                    mLogger.info("url: " + url);
                    URL uUrl = new URL(url);
                    HttpURLConnection conn = (HttpURLConnection) uUrl.openConnection();
                    conn.setConnectTimeout(timeoutInSeconds * 1000);
                    conn.setReadTimeout(timeoutInSeconds * 1000);

                    {
                        String encoded = DatatypeConverter.printBase64Binary((user + ":" + password).getBytes("utf-8"));
                        conn.setRequestProperty("Authorization", "Basic " + encoded);
                        mLogger.info("Add Header (user " + user + "). " + "Authorization: " + "Basic " + encoded);
                        mLogger.info("Add Header (password " + password + "). " + "Authorization: " + "Basic " + encoded);
                    }

                    conn.setDoOutput(true); // false because I do not need to append any data to this request
                    conn.setRequestMethod("POST");
                    {
                        long clength = binaryPathName.length();

                        conn.setRequestProperty("Content-Length", String.valueOf(clength));
                        mLogger.info("Header. " + "Content-Length: " + String.valueOf(clength));

                        conn.setDoInput(true); // false means the response is ignored

                        OutputStream outputStream = null;
                        InputStream inputStream = null;
                        try {
                            outputStream = conn.getOutputStream();
                            inputStream = new FileInputStream(binaryPathName);

                            IOUtils.copy(inputStream, outputStream);
                        }
                        catch (Exception ex)
                        {
                            mLogger.error("Exception: " + ex);
                        }
                        finally {
                            if (inputStream != null)
                                IOUtils.closeQuietly(inputStream);
                            if (outputStream != null)
                                IOUtils.closeQuietly(outputStream);
                        }
                    }

                    mLogger.info("conn.getResponseCode...");
                    int statusCode = conn.getResponseCode();

                    mLogger.info("conn.getResponseCode. statusCode: " + statusCode);
                    if (statusCode != HttpStatus.SC_OK && statusCode != HttpStatus.SC_CREATED)
                    {
                        mLogger.debug("Method failed: " + conn.getResponseMessage());

                        throw new Exception("Method failed: " + conn.getResponseMessage());
                    }

                    mLogger.debug("POST successful. statusCode: " + statusCode);
                }
                catch (HttpException e) {
                    String errorMessage = "URL: " + url
                            + ", Fatal protocol violation: " + e
                            + ", maxRetriesNumber: " + maxRetriesNumber
                            + ", retryIndex: " + (retryIndex - 1)
                            ;
                    mLogger.error(errorMessage);

                    if (retryIndex >= maxRetriesNumber)
                        throw e;
                    else
                        Thread.sleep(100);  // half second
                }
                catch (IOException e) {
                    String errorMessage = "URL: " + url
                            + ", Fatal transport error: " + e
                            + ", maxRetriesNumber: " + maxRetriesNumber
                            + ", retryIndex: " + (retryIndex - 1)
                            ;
                    mLogger.error(errorMessage);

                    if (retryIndex >= maxRetriesNumber)
                        throw e;
                    else
                        Thread.sleep(100);  // half second
                }
                catch (Exception e) {
                    String errorMessage = "URL: " + url
                            + ", Fatal transport error: " + e
                            + ", maxRetriesNumber: " + maxRetriesNumber
                            + ", retryIndex: " + (retryIndex - 1)
                            ;
                    mLogger.error(errorMessage);

                    if (retryIndex >= maxRetriesNumber)
                        throw e;
                    else
                        Thread.sleep(100);  // half second
                }
                /*
                finally {
                    // Release the connection.
                    if (method != null)
                        method.releaseConnection();
                }
                */
            }
        }

        // elapsed time saved in the calling method
        // mLogger.info("@fetchHttpsJson " + url + "@ elapsed (milliseconds): @" + (new Date().getTime() - startTimestamp.getTime()) + "@");
    }
}