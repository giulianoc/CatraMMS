package com.catramms.backing.webFilter;

import org.apache.log4j.Logger;

import java.io.IOException;
import javax.servlet.Filter;
import javax.servlet.FilterChain;
import javax.servlet.FilterConfig;
import javax.servlet.ServletException;
import javax.servlet.ServletRequest;
import javax.servlet.ServletResponse;
import javax.servlet.annotation.WebFilter;
import javax.servlet.http.HttpServletRequest;
import javax.servlet.http.HttpServletResponse;
import javax.servlet.http.HttpSession;

@WebFilter(filterName = "AuthFilter", urlPatterns = { "*.xhtml" })
public class AuthorizationFilter implements Filter {

    private static final Logger mLogger = Logger.getLogger(AuthorizationFilter.class);

    public AuthorizationFilter() {
    }

    @Override
    public void init(FilterConfig filterConfig) throws ServletException {

    }

    @Override
    public void doFilter(ServletRequest request, ServletResponse response,
                         FilterChain chain) throws IOException, ServletException {
        try
        {

            HttpServletRequest reqt = (HttpServletRequest) request;
            HttpServletResponse resp = (HttpServletResponse) response;
            HttpSession ses = reqt.getSession(false);

            String reqURI = reqt.getRequestURI();
            if (reqURI.indexOf("/login.xhtml") >= 0
                    || (ses != null && ses.getAttribute("username") != null)
                    || reqURI.indexOf("/public/") >= 0
                    || reqURI.contains("javax.faces.resource"))
            {
                chain.doFilter(request, response);
            }
            else
            {
                String originURI = reqt.getRequestURI() + (reqt.getQueryString() != null ? ("?" + reqt.getQueryString()) : "");
                mLogger.info("originURI: " + originURI);
                mLogger.info("reqt.getContextPath: " + reqt.getContextPath());
                mLogger.info("resp.getStatus: " + resp.getStatus());
                mLogger.info("resp.isCommitted: " + resp.isCommitted());
                resp.sendRedirect(reqt.getContextPath() + "/login.xhtml"
                        + "?originURI=" + java.net.URLEncoder.encode(originURI, "UTF-8"));
            }
        } catch (Exception e) {
            System.out.println(e.getMessage());
        }
    }

    @Override
    public void destroy() {

    }
}