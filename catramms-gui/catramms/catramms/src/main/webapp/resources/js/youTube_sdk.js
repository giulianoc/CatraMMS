
var authorizationToken;

/**
*  On load, called to load the auth2 library and API client library.
*/
function handleClientLoad() {
    console.log("youTube handleClientLoad")
    gapi.load('client:auth2', initClient);
}

/**
*  Initializes the API client library and sets up sign-in state
*  listeners.
*/
function initClient()
{
    console.log("youTube initClient")

    // Client ID and API key from the Developer Console
    var CLIENT_ID = '700586767360-96om12ccsf16m41qijrdagkk0oqf2o7m.apps.googleusercontent.com';

    // Array of API discovery doc URLs for APIs used by the quickstart
    var DISCOVERY_DOCS = ["https://www.googleapis.com/discovery/v1/apis/youtube/v3/rest"];

    // Authorization scopes required by the API. If using multiple scopes,
    // separated them with spaces.
    var SCOPES = 'https://www.googleapis.com/auth/youtube https://www.googleapis.com/auth/youtube.upload';

    gapi.client.init({
        discoveryDocs: DISCOVERY_DOCS,
        clientId: CLIENT_ID,
        scope: SCOPES
    }).then(function () {
        // Listen for sign-in state changes.
        gapi.auth2.getAuthInstance().isSignedIn.listen(updateSigninStatus);

        // Handle the initial sign-in state.
        updateSigninStatus(gapi.auth2.getAuthInstance().isSignedIn.get());
    });
}

/**
*  Called when the signed in status changes, to update the UI
*  appropriately. After a sign-in, the API is called.
*/
function updateSigninStatus(isSignedIn)
{
    if (isSignedIn)
    {
        console.log("youTube Signed In")

        getChannel();
    }
    else
    {
        console.log("youTube Signed out")
    }
}

/**
*  Sign in the user upon button click.
*/
function handleAuthClick(event) {
    console.log("youTube handleAuthClick")
    gapi.auth2.getAuthInstance().signIn();

    authorizationToken = gapi.auth2.getAuthInstance().currentUser.get().getAuthResponse().id_token;
    console.log("handleAuthClick: authorizationToken: " + authorizationToken)

    taskUpdateAuthorizationToken_Post_On_YouTube([
        {name:'newAuthorizationToken',value:authorizationToken}
    ]);
}

/**
*  Sign out the user upon button click.
*/
function handleSignoutClick(event) {
    console.log("youTube handleSignoutClick")
    gapi.auth2.getAuthInstance().signOut();
}

/**
* Print files.
*/
function getChannel() {
    console.log("youTube getChannel")
    gapi.client.youtube.channels.list({
        'part': 'snippet,contentDetails,statistics',
        'forUsername': 'GoogleDevelopers'
    }).then(function(response) {
        var channel = response.result.items[0];
        console.log('youTube This channel\'s ID is ' + channel.id + '. ' +
            'Its title is \'' + channel.snippet.title + ', ' +
            'and it has ' + channel.statistics.viewCount + ' views.');
    });
}
