
jQuery(document).ready(function() {
    initPlayerListener();
});

function initPlayerListener() {
    var video = document.getElementsByTagName("video")[0];
    console.log("initPlayerListener. video: " + video);

    // video.addEventListener("loadedmetadata", onLoadedMetaData, false); // does not fire on WebKit nightly
    // video.addEventListener("loadeddata", onLoadedData, false);
    video.onplay = onPlay;
    video.onpause = onPause;

    // var updateVideoCurrentTimeCodeInterval = setInterval("updateVideoCurrentTimeCode()", 100);
}

function onPlay() {
    console.log("play");
}

function onPause() {
    console.log("pause");
}


function seekFrames(nr_of_frames) {
    var video = document.getElementsByTagName("video")[0];
    // var video = document.getElementsById("showVideoForm:binaryLinkPlayer");
    // var timecodeLabel = document.getElementsById("showVideoForm:timecode");
    // var timecodeLabel = $(".timecodeClass");
    // var video = jQuery(PrimeFaces.escapeClientId('showVideoForm:binaryLinkPlayer:media-player'));
    // var video = jQuery("#showVideoForm\\:binaryLinkPlayer\\:media-player");
    // var timecodeLabel = jQuery(".timecodeClass");
    // jQuery(".timecodeClass").value="timecodeText";
    // document.getElementById("#{p:component('timecodeHiddenId')}").value = "timecodeText";

    var framesPerSeconds = document.getElementById("showVideoForm:framesPerSeconds").innerHTML;
    console.log("framesPerSeconds: " + framesPerSeconds)

    if (video.paused == false) {
        video.pause();
    }

    //var currentFrames = Math.round(video.currentTime * framesPerSeconds);

    var currentFrames = video.currentTime * framesPerSeconds;
    console.log("currentFrames: " + currentFrames)

    var newPos = (currentFrames + nr_of_frames) / framesPerSeconds;
    newPos = newPos + 0.00001; // FIXES A SAFARI SEEK ISSUE. myVdieo.currentTime = 0.04 would give SMPTE 00:00:00:00 wheras it should give 00:00:00:01

    //var newPos = video.currentTime += 1/framesPerSeconds;
    //newPos = Math.round(newPos, 2) + 1/framesPerSeconds;

    // console.log("initial position: " + video.currentTime);
    // console.log("new position: " + newPos);

    video.currentTime = newPos; // TELL THE PLAYER TO GO HERE

    var seekError = newPos - video.currentTime;

    var smpteTimeCode = secondsToTimecode(video.currentTime, framesPerSeconds);

    updateVideoTimeCode([
        {name:'newTimeCode',value:smpteTimeCode}
    ]);

    // console.log("confirm new position: " + video.currentTime);
    console.log("seekError: " + seekError);

    // var timecodeText = video.currentTime + "(" + secondsToTimecode(video.currentTime, 25) + ")";
    // timecodeLabel.innerHTML = "timecodeText";

    //console.log("found_frame_nr: " + found_frame_nr + " (found_frame: "+found_frame+")");

    // $('#timecode_tracker').append("<font color='"+fontColor+"'>" + clickCounter + ";" + newPos + ';' + video.currentTime + ';'+found_frame+'</font><br/>');

}

function seekToTimecode(hh_mm_ss_ff, fps) {

    var video = document.getElementsByTagName("video")[0];

    if (video.paused == false) {
        video.pause();
    }

    var seekTime = timecodeToSeconds(hh_mm_ss_ff, fps);

    video.currentTime = seekTime;

}

//SMTE Time-code calculation functions
//=======================================================================================================

function timecodeToSeconds(hh_mm_ss_ff, fps) {
    var tc_array = hh_mm_ss_ff.split(":");
    var tc_hh = parseInt(tc_array[0]);
    var tc_mm = parseInt(tc_array[1]);
    var tc_ss = parseInt(tc_array[2]);
    var tc_ff = parseInt(tc_array[3]);
    var tc_in_seconds = ( tc_hh * 3600 ) + ( tc_mm * 60 ) + tc_ss + ( tc_ff / fps );
    return tc_in_seconds;

}

function secondsToTimecode(time, fps) {

    var hours = Math.floor(time / 3600) % 24;
    var minutes = Math.floor(time / 60) % 60;
    var seconds = Math.floor(time % 60);
    var frames = Math.floor(((time % 1)*fps).toFixed(3));

    var result = (hours < 10 ? "0" + hours : hours) + ":"
        + (minutes < 10 ? "0" + minutes : minutes) + ":"
        + (seconds < 10 ? "0" + seconds : seconds) + ":"
        + (frames < 10 ? "0" + frames : frames);

    return result;

}
