
/* initialization of FB SDK (https://developers.facebook.com/docs/javascript/quickstart?locale=it_IT) */
window.fbAsyncInit = function() {
FB.init({
  appId            : '1862418063793547',
  autoLogAppEvents : true,
  xfbml            : true,
  version          : 'v3.2'
});
};

(function(d, s, id){
 var js, fjs = d.getElementsByTagName(s)[0];
 if (d.getElementById(id)) {return;}
 js = d.createElement(s); js.id = id;
 js.src = "https://connect.facebook.net/en_US/sdk.js";
 fjs.parentNode.insertBefore(js, fjs);
}(document, 'script', 'facebook-jssdk'));
