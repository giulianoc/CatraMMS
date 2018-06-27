/*

changes 

AUG 10 2012
-Bug fix: removed unnecessary Math.round() causing Safari to seek to wrong frames
-Patch:   added newPos = newPos + 0.00001 to correct for Safari seeking to correct frame. ie when setting myVideo.currentTime to 0.04 Safari *should* seek to SMPTE 00:00:00:01 but instead remains stuck at 00:00:00:00
          seeking to 0.40001 forces Safari to seek to SMPTE 00:00:00:01. Also this trick works fine in Chrome, so solution pratically found.

*/

var updateVideoCurrentTimeCodeInterval;
var loadedmetadata = false;
var updateReadyStateInterval;
var readyState;
var video;
var duration;

var FPS = 29.97;

$(document).ready(function() {
	init();
});

function init() {
	setupGlue();		
	$('#which_video').change(function() {
		selectVideo( $("#which_video").val() );
	});
			
}

function setupGlue() {
	video = $('video')[0];
	initEventListeners();
	updateReadyStateInterval = setInterval("updateReadyState()", 100);
	updateVideoCurrentTimeCodeInterval = setInterval(
			"updateVideoCurrentTimeCode()", FPS);
}

function updateReadyState() {

	var HAVE_NOTHING = 0;
	var HAVE_METADATA = 1;
	var HAVE_CURRENT_DATA = 2;
	var HAVE_FUTURE_DATA = 3;
	var HAVE_ENOUGH_DATA = 4;

	if (video.readyState == HAVE_NOTHING) {
		$("#metaData").html("video.readyState = HAVE_NOTHING");
	} else if (video.readyState > HAVE_NOTHING) {

		var readyStateInfo = "<b>Meta data loaded</b><br/>";
		readyStateInfo += "duration: " + parseFloat(video.duration.toFixed(2))
		+ " seconds.<br/>";
		$("#metaData").html(readyStateInfo);
		/*
		var duration = $('#duration').get(0);
		var vid_duration = Math.round(video.duration);
		duration.firstChild.nodeValue = vid_duration;
		clearInterval(t);
		 */
		//clearInterval(updateReadyStateInterval);
	}

}

function initEventListeners() {
	video.addEventListener("loadedmetadata", onLoadedMetaData, false); // does not fire on WebKit nightly
	video.addEventListener("loadeddata", onLoadedData, false);
	//video.addEventListener("timeupdate", onTimeUpdate, false);
	video.addEventListener("play", onPlay, false);
}

function onLoadedMetaData() {
	// duration is available
	$("#metaData").append("Meta data loaded...<br/>");
}

function onLoadedData() {
	$("#metaData").append("Data loaded...<br/>");
}

function onTimeUpdate() {
	//	updateVideoCurrentTimeCode();
	// works fine in all browsers, but it's a tad slow- still using a setinterval to make it update faster
}

function onPlay() {

}


	
function updateVideoCurrentTimeCode() {

	var fixedTimecode = video.currentTime;
	fixedTimecode = parseFloat(fixedTimecode.toFixed(2));

	var SMPTE_time = secondsToTimecode(video.currentTime, FPS);
	$("#currentTimeCode").html(SMPTE_time);

	var videoInfo = "<b>Video info:</b><br/>";
	videoInfo += "currentTime: " + video.currentTime + "<br/>";
	videoInfo += "fixedTimecode: " + fixedTimecode + "<br/>";
	videoInfo += "srcVideo: " + video.currentSrc + "<br/>";
	$("#videoInfo").html(videoInfo);

	// UNABLE to see if HTML5 picked the .mp4 / .webm / .ogv version by querying the .src attribute...
	//var video_src = "<b>video source used:</b><br/>" + video.source.src;
	//$("#videoSource").html = video_src;
	

}

function seekToTimecode(hh_mm_ss_ff, fps) {

	if (video.paused == false) {
		video.pause();
	}

	var seekTime = timecodeToSeconds(hh_mm_ss_ff, fps);
	var str_seekInfo = "video was at: " + video.currentTime + "<br/>";
	str_seekInfo += "seeking to (fixed): <b>" + seekTime + "</b><br/>";
	video.currentTime = seekTime;
	str_seekInfo += "seek done, got: " + video.currentTime + "<br/>";
	$("#seekInfo").html(str_seekInfo);

}


function togglePlay() {
	var video = document.getElementsByTagName("video")[0];
	video.pause();
}

var clickCounter = 0;

function seekFrames(nr_of_frames, fps) {

	clickCounter++;

	var div_seekInfo = document.getElementById('seekInfo');

	if (video.paused == false) {
		video.pause();
	}

	//var currentFrames = Math.round(video.currentTime * fps); 
	
	var currentFrames = video.currentTime * fps;
	
	var newPos = (currentFrames + nr_of_frames) / fps;
	newPos = newPos + 0.00001; // FIXES A SAFARI SEEK ISSUE. myVdieo.currentTime = 0.04 would give SMPTE 00:00:00:00 wheras it should give 00:00:00:01

	//var newPos = video.currentTime += 1/fps;
	//newPos = Math.round(newPos, 2) + 1/fps; 

	var str_seekInfo = "seeking to (fixed): <b>" + newPos + "</b><br/>";
	
	console.log("video.currentTime = " + newPos);
	video.currentTime = newPos; // TELL THE PLAYER TO GO HERE
	
	str_seekInfo += "seek done, got: " + video.currentTime + "<br/>";
	var seek_error = newPos - video.currentTime;
	str_seekInfo += "seek error: " + seek_error + "<br/>";

	div_seekInfo.innerHTML = str_seekInfo;
	
	// track calculated value in logger
	
	//console.log("SMPTE_time: " + SMPTE_time);
	
	
	// check found timecode frame
	var found_frame = $("#currentTimeCode").text();
	found_frame_split = found_frame.split(":");
	
	found_frame_nr = Number(found_frame_split[3]);
	
	//console.log("found_frame_nr: " + found_frame_nr + " (found_frame: "+found_frame+")");
	
	var fontColor = "#000";
	if ( found_frame_nr+1 != clickCounter) {
		fontColor = "#F00";	
	}
	
	$('#timecode_tracker').append("<font color='"+fontColor+"'>" + clickCounter + ";" + newPos + ';' + video.currentTime + ';'+found_frame+'</font><br/>');
	


}

function getDigits(val) {
	var fullVal = parseFloat(val);
	var newVal = fullVal - Math.floor(parseFloat(fullVal));
	newVal = newVal.toFixed(2);
	return newVal;
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

var video_01 = 	{	"mp4"	: { video : "video/perfect-timecoded.mp4", type : "video/mp4" },
                    "webm"	: { video : "video/perfect-timecoded.webm", type : "video/webm" }
				}

var video_02 = 	{	"mp4"	: { video : "video/perfect-timecoded.mp4", type : "video/mp4" },
                    "webm"	: { video : "video/perfect-timecoded.webm", type : "video/webm" }
				}



function selectVideo(which_video) {
/* ADDING VIDEO SELECTION STUFF TO ALLOW USER TO SELECT IE9 COMPATIBLE VIDEO */
	
	if ( !which_video ) {
		return;
	}

	var selected_video_json;
	switch(which_video)
	{
	case "old_video":
	 selected_video_json = video_01;
	  break;
	case "new_video":
	  selected_video_json = video_02;
	  break;
	default:
	  // whatevs
	}
	
	// HACKERDEHACK
	replace_str = "<video id='myVideo' width='640' height='368' controls='controls'>";
	replace_str += "<source src='"+selected_video_json.mp4.video+"' type='"+selected_video_json.mp4.type+"' />";
	replace_str += "<source src='"+selected_video_json.webm.video+"' type='"+selected_video_json.webm.type+"' />";
	replace_str += "</video>";

	$('#videoHolder').html(replace_str);
	
	setupGlue();
}

function toggleLog() {
	$('#changelog').toggle();
	$('#toggleLogDiv').toggle();
}