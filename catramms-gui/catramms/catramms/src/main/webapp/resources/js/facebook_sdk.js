
logInWithFacebook = function() {
    console.log('logInWithFacebook');

    var loginToBeDone = true;
    FB.getLoginStatus(function(response)
    {
        console.log('response.status: ' + response.status);
        if (response.status === 'connected')
        {
            // The user is logged in and has authenticated your
            // app, and response.authResponse supplies
            // the user's ID, a valid access token, a signed
            // request, and the time the access token
            // and signed request each expire.
            var uid = response.authResponse.userID;
            var accessToken = response.authResponse.accessToken;

            // FB.api('/oauth/access_token', 'get',
            //    { grant_type : 'fb_exchange_token', client_id : '1862418063793547', client_secret : '04a76f8e11e9dc70ea5975649a91574c', access_token : accessToken },
            //    function(response)
            // {
            //    console.log('Good to see you, ' + response.name + '.');
            // });

            console.log('Already connected (updateAccessToken)! accessToken: ' + accessToken);

            taskUpdateAccessToken_Post_On_Facebook([
                {name:'newAccessToken',value:accessToken}
            ]);

            loginToBeDone = false;
        }
        else if (response.status === 'authorization_expired')
        {
            // The user has signed into your application with
            // Facebook Login but must go through the login flow
            // again to renew data authorization. You might remind
            // the user they've used Facebook, or hide other options
            // to avoid duplicate account creation, but you should
            // collect a user gesture (e.g. click/touch) to launch the
            // login dialog so popup blocking is not triggered.
            console.log('status: authorization_expired');
        }
        else if (response.status === 'not_authorized')
        {
            // The user hasn't authorized your application.  They
            // must click the Login button, or you must call FB.login
            // in response to a user gesture, to launch a login dialog.
            console.log('status: not_authorized');
        }
        else
        {
            // The user isn't logged in to Facebook. You can launch a
            // login dialog with a user gesture, but the user may have
            // to log in to Facebook before authorizing your application.
            console.log('status: ???');
        }
    });

    console.log('loginToBeDone: ' + loginToBeDone);
    if (loginToBeDone)
    {
        FB.login(function(response) {
            if (response.authResponse)
            {
                console.log('Welcome!  Fetching your information.... ');
                // FB.api('/me', function(response)
                // {
                //    console.log('Good to see you, ' + response.name + '.');
                // });
            } else {
                console.log('User cancelled login or did not fully authorize.');
            }
        });
    }

    return false;
};

window.fbAsyncInit = function() {
    console.log('FB.init.');
    FB.init({
        appId            : '1862418063793547',
        autoLogAppEvents : true,
        xfbml            : true,
        version          : 'v3.2'
    });
};

(function(d, s, id){
    var js, fjs = d.getElementsByTagName(s)[0];
    if (d.getElementById(id))
    {
        return;
    }
    js = d.createElement(s);
    js.id = id;
    js.src = "https://connect.facebook.net/en_US/sdk.js";
    fjs.parentNode.insertBefore(js, fjs);
}(document, 'script', 'facebook-jssdk'));
