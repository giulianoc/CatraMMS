
/*
jQuery(document).ready(function() {
    initPlayerListener();
});
*/

var intervalRewind;
var updateVideoCurrentTimeCodeInterval;
var editVideo;

function initPlayerListener(localEditVideo) {
    console.log("initPlayerListener. editVideo: " + localEditVideo);
    editVideo = localEditVideo;
    var video = document.getElementsByTagName("video")[0];

    // video.addEventListener("loadedmetadata", onLoadedMetaData, false); // does not fire on WebKit nightly
    // video.addEventListener("loadeddata", onLoadedData, false);
    video.onplay = onPlay;
    video.onpause = onPause;

    // video.ontimeupdate = onTimeUpdate;
}


function onPlay() {
    console.log("play");

    var video = document.getElementsByTagName("video")[0];

    // video.playbackRate = 1.0;

    clearInterval(intervalRewind);

    updateVideoCurrentTimeCodeInterval = setInterval(onTimeUpdate, 100);
}

function onPause() {
    console.log("pause");

    var video = document.getElementsByTagName("video")[0];

    if (typeof intervalRewind === 'undefined')
    {
        // we are NOT doing 'rewind'
        console.log("we are NOT doing rewind");

        video.playbackRate = 1.0;

        // clearInterval(intervalRewind);
    }

    clearInterval(updateVideoCurrentTimeCodeInterval);
}

function onTimeUpdate()
{
    console.log("onTimeUpdate");

    if (editVideo)
    {
        var timeCode = document.getElementById("showVideoForm:timecode");
        var timeCodeHidden = document.getElementById("showVideoForm:timecodeHidden");
        var video = document.getElementsByTagName("video")[0];

        var framesPerSeconds = document.getElementById("showVideoForm:framesPerSeconds").innerHTML;
        var smpteTimeCode = secondsToTimecode(video.currentTime, framesPerSeconds);

        // console.log(smpteTimeCode);
        timeCode.innerHTML = smpteTimeCode;
        timeCodeHidden.value = smpteTimeCode;
    }
}

function speed(playSpeed) {
    console.log("speed", playSpeed);

    var video = document.getElementsByTagName("video")[0];

    video.playbackRate = playSpeed;

    if (video.paused == true)
        video.play();
}

function rewind(playSpeed)
{
    console.log("rewind", playSpeed);
    var video = document.getElementsByTagName("video")[0];

    video.pause();

   var framesPerSeconds = document.getElementById("showVideoForm:framesPerSeconds").innerHTML;
    framesPerSeconds /= playSpeed;
    console.log("framesPerSeconds: " + framesPerSeconds)

    console.log("intervalRewind", intervalRewind)
    if (typeof intervalRewind !== 'undefined')
        clearInterval(intervalRewind);
    intervalRewind = setInterval(
        function(){
            video.playbackRate = playSpeed;
            if(video.currentTime <= 0){
                clearInterval(intervalRewind);
                video.pause();
            }
            else{
                // video.currentTime += -.1;
                var decrement = 1/framesPerSeconds
                console.log("decrement", decrement);
                video.currentTime -= decrement;
                onTimeUpdate();
            }
        },
        playSpeed == 1 ? 100 : 400);   // 1000 / framesPerSeconds); 1000/25 = 40 and it is too fast, the player is not able to refresh the picture
}

function seekFrames(nr_of_frames) {
    console.log("seekFrames");

    var video = document.getElementsByTagName("video")[0];

    var framesPerSeconds = document.getElementById("showVideoForm:framesPerSeconds").innerHTML;
    console.log("framesPerSeconds: " + framesPerSeconds)

    if (typeof intervalRewind !== 'undefined')
        clearInterval(intervalRewind);
    if (video.paused == false)
        video.pause();

    var currentFrames = video.currentTime * framesPerSeconds;
    console.log("currentFrames: " + currentFrames)

    var newPos = (currentFrames + nr_of_frames) / framesPerSeconds;
    // newPos = newPos + 0.00001; // FIXES A SAFARI SEEK ISSUE. myVdieo.currentTime = 0.04 would give SMPTE 00:00:00:00 wheras it should give 00:00:00:01

    video.currentTime = newPos; // TELL THE PLAYER TO GO HERE

    var seekError = newPos - video.currentTime;
    console.log("seekError: " + seekError);

    onTimeUpdate();

        /*
    var smpteTimeCode = secondsToTimecode(video.currentTime, framesPerSeconds);

    updateVideoTimeCode([
        {name:'newTimeCode',value:smpteTimeCode}
    ]);
    */
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
        + (seconds < 10 ? "0" + seconds : seconds) + "."
        + (frames < 10 ? "0" + frames : frames);

    return result;

}
