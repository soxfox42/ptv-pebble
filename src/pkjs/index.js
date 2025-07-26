Pebble.addEventListener("showConfiguration", () => {
  console.log("Opening configuration page...");
  var url = "http://192.168.1.111:8080/";
  Pebble.openURL(url);
});

Pebble.addEventListener("webviewclosed", e => {
  console.log("Configuration window closed.", typeof e.response);
  var config = JSON.parse(decodeURIComponent(e.response));
  console.log("Configuration closed with: " + JSON.stringify(config));
});