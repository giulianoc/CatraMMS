package com.media.components;

import javax.el.ValueExpression;
import javax.faces.component.FacesComponent;
import javax.faces.component.UIComponent;
import javax.faces.component.UINamingContainer;
import javax.faces.context.FacesContext;
import java.io.IOException;

/**
 * Created with IntelliJ IDEA.
 * User: multi
 * Date: 07/11/15
 * Time: 14:23
 * To change this template use File | Settings | File Templates.
 */
@FacesComponent("UIMediaComponent")
public class UIMediaComponent extends UINamingContainer {

    private static final String ELEMENT_ID = "media-player";
    private static final String ATTRIBUTE_AUTOPLAY = "autoplay";
    private static final String ATTRIBUTE_LOOP = "loop";
    private static final String ATTRIBUTE_MUTED = "muted";
    private static final String ATTRIBUTE_CONTROLS = "controls";
    private static final String ATTRIBUTE_CONTROLSLIST = "controlsList";
    private static final String ATTRIBUTE_CROSSORIGIN = "crossorigin";
    private static final String ATTRIBUTE_POSTER = "poster";
    private static final String ATTRIBUTE_WIDTH = "width";
    private static final String ATTRIBUTE_HEIGHT = "height";

    public String getElementId()
    {
        return ELEMENT_ID;
    }

    @Override
    public void encodeBegin(FacesContext context) throws IOException
    {
        super.encodeBegin(context);

        UIComponent element = findMediaElement();

        addAttributeIfTrue(element, ATTRIBUTE_AUTOPLAY, null);
        addAttributeIfTrue(element, ATTRIBUTE_LOOP, null);
        addAttributeIfTrue(element, ATTRIBUTE_MUTED, null);
        addAttributeIfTrue(element, ATTRIBUTE_CONTROLS, null);
        addAttributeIfTrue(element, ATTRIBUTE_CONTROLSLIST, "nodownload");

        addAttributeIfNotNull(element, ATTRIBUTE_CROSSORIGIN);
        addAttributeIfNotNull(element, ATTRIBUTE_POSTER);
        addAttributeIfNotNull(element, ATTRIBUTE_WIDTH);
        addAttributeIfNotNull(element, ATTRIBUTE_HEIGHT);
    }

    private void addAttributeIfNotNull(UIComponent component, String attributeName)
    {
        Object attributeValue = getAttributeValue(attributeName);
        if (attributeValue != null)
        {
            component.getPassThroughAttributes().put(attributeName, attributeValue);
        }
    }

    private void addAttributeIfTrue(UIComponent component, String attributeName, String attributeValue)
    {
        if (isAttributeTrue(attributeName))
        {
            component.getPassThroughAttributes().put(attributeName, attributeValue == null ? "true" : attributeValue);
        }
    }

    private UIComponent findMediaElement() throws IOException
    {
        UIComponent element = findComponent(getElementId());
        if (element == null)
        {
            throw new IOException("Media element with ID " + getElementId() + " could not be found");
        }

        return element;
    }

    private Object getAttributeValue(String name)
    {
        ValueExpression ve = getValueExpression(name);

        if (ve != null)
        {
            // Attribute is a value expression
            return ve.getValue(getFacesContext().getELContext());
        }
        else if (getAttributes().containsKey(name))
        {
            // Attribute is a fixed value
            return getAttributes().get(name);
        }
        else
        {
            // Attribute does not exist
            return null;
        }
    }

    private boolean isAttributeTrue(String attributeName)
    {
        boolean isBoolean = getAttributeValue(attributeName) instanceof Boolean;
        if (!isBoolean)
            return false;

        return ((boolean) getAttributeValue(attributeName)) == Boolean.TRUE;
    }
}
