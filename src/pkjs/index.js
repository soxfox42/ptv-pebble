Pebble.addEventListener("showConfiguration", () => {
  const config = localStorage.getItem("config") || '{"favourites": []}';
  const url = `http://192.168.1.111:8080/?config=${encodeURIComponent(config)}`;
  Pebble.openURL(url);
});

Pebble.addEventListener("webviewclosed", e => {
  localStorage.setItem("config", decodeURIComponent(e.response));
});

let appMessageQueue = [];

function sendNextAppMessage() {
  if (appMessageQueue.length === 0) return;

  const message = appMessageQueue.shift();
  Pebble.sendAppMessage(message, sendNextAppMessage);
}

Pebble.addEventListener("ready", () => {
  const jsonConfig = localStorage.getItem("config");
  if (!jsonConfig) {
    Pebble.sendAppMessage({ notConfigured: true });
    return;
  }

  const config = JSON.parse(jsonConfig);
  const queries = [];
  for (const favourite of config.favourites) {
    const query = `routeType=${favourite.routeType}&stopID=${favourite.stopID}`;
    if (!queries.includes(query)) {
      queries.push(query);
    }
  }

  let departures = [];
  let success = 0;
  for (const query of queries) {
    const request = new XMLHttpRequest();
    request.onload = function () {
      const result = JSON.parse(this.responseText);
      departures = departures.concat(result.departures);
      if (++success == queries.length) {
        processDepartures();
      }
    };
    request.open("GET", `https://core.soxfox.me/ptv/getDepartures?token=${config.token}&${query}`);
    request.send();
  }

  function processDepartures() {
    departures = departures.filter(
      departure =>
        new Date(departure.scheduled_departure_utc) > new Date()
        || (departure.estimated_depature_utc
          && new Date(departure.estimated_departure_utc) > new Date())
    );
    departures.sort((a, b) => {
      const aTime = new Date(a.estimated_departure_utc || a.scheduled_departure_utc);
      const bTime = new Date(b.estimated_departure_utc || b.scheduled_departure_utc);
      return aTime - bTime;
    });

    for (const favourite of config.favourites) {
      const departure = departures.find(
        departure => departure.stop_id === favourite.stopID
          && departure.direction_id === favourite.directionID
      );
      if (!departure) continue;
      const time = new Date(departure.scheduled_departure_utc);
      const timeValue = time.getHours() * 60 + time.getMinutes();
      const timeUntilDepart = new Date(departure.estimated_departure_utc) - new Date();
      appMessageQueue.push({
        name: favourite.name,
        time: timeValue,
        minutes: timeUntilDepart / 60000,
      });
    }

    sendNextAppMessage();
  }
});
