function getTrackCoverURL() {
    return document.body
        .getElementsByClassName('entity-cover deco-entity-image-placeholder track-cover entity-cover_size_Large')[0]
        .firstChild.firstChild
        .src;
}

function getTrackAndURL() {
    const elem = document.body.getElementsByClassName('d-link deco-link track__title')[0]
    return [elem.textContent, elem.href]
}

function getArtistAndURL() {
    const elem = document.body.getElementsByClassName('d-artists d-artists__expanded')[0].firstChild
    return [elem.textContent, elem.href]
}

function sendData() {
    const trackCoverURL = getTrackCoverURL()
    const trackNameURL = getTrackAndURL()
    const artistURL = getArtistAndURL()
    var data = JSON.stringify({
        "track_cover_url": trackCoverURL,
        "track_name": trackNameURL[0],
        "track_url": trackNameURL[1],
        "artist_name": artistURL[0],
        "artist_url": artistURL[1]
    });
    var x = new XMLHttpRequest();
    x.open("POST", "http://localhost:13239");
    x.send(data);
}

setInterval(function() {
    sendData()
}, 2000)