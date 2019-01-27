

PrimeFaces.locales['my'] = {
    firstDay : 1
}

function on_start() {
    /* console.log("on_start") */
    document.body.style.cursor='wait';

    PF('globalAjaxProgressBarDialog').show();
}

function on_complete() {
    console.log("on_complete")
    document.body.style.cursor='default';

    PF('globalAjaxProgressBarDialog').hide();
}

var updaterTimer;
function initUpdaterTimer(callback, periodInSeconds) {
    console.log("initUpdaterTimer. periodInSeconds: ", periodInSeconds * 1000);
    updaterTimer = setInterval(callback, periodInSeconds * 1000);
}

function stopUpdaterTimer() {
    console.log("stopUpdaterTimer");
    clearInterval(updaterTimer);
}

function setZIndexTieredMenu() {

    console.log("setZIndexTieredMenu");

    var tieredMenu = jQuery(".tieredMenuClass");

    tieredMenu.css("z-index", "2000");

}

function setZIndex(classes) {

    for (classIndex = 0; classIndex < classes.length; classIndex++)
    {
        /* console.log("setZIndex for " + classes[classIndex]); */

        var widget = jQuery(classes[classIndex]);

        widget.css("z-index", "2000");
    }
}