Pebble.addEventListener("showConfiguration", () => {
  const config = localStorage.getItem("config") || '{"favourites": []}';
  const url = `http://192.168.1.111:8080/?config=${encodeURIComponent(config)}`;
  Pebble.openURL(url);
});

Pebble.addEventListener("webviewclosed", e => {
  const jsonConfig = decodeURIComponent(e.response);
  var config = JSON.parse(jsonConfig);
  localStorage.setItem("config", jsonConfig);
});

Pebble.addEventListener("ready", () => {
  const jsonConfig = localStorage.getItem("config");
  if (!jsonConfig) {
    Pebble.sendAppMessage({ notConfigured: true });
    return;
  }
});
