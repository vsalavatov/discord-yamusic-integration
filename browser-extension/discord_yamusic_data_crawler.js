function trackInfoContainerElement() {
    return document.getElementsByClassName('track track_type_player')[0];
}

function getTrackCoverURL() {
    return trackInfoContainerElement().getElementsByClassName('track-cover')[0]
        .firstChild.firstChild
        .src;
}

function getTrackAndURL() {
    const elem = trackInfoContainerElement().getElementsByClassName('track__title')[0]
    return [elem.textContent, elem.href]
}

function getArtistAndURL() {
    const elem = trackInfoContainerElement().getElementsByClassName('track__artists')[0]
        .firstChild.firstChild
    return [elem.textContent, elem.href]
}

function getSongState() {
    const left = document.body.getElementsByClassName('progress__left')[0]
    const right = document.body.getElementsByClassName('progress__right')[0]
    return [left.textContent, right.textContent]
}

function sendData() {
    const trackCoverURL = getTrackCoverURL()
    const trackNameURL = getTrackAndURL()
    const artistURL = getArtistAndURL()
    const songState = getSongState()
    var data = JSON.stringify({
        "track_cover_url": trackCoverURL,
        "track_name": trackNameURL[0],
        "track_url": trackNameURL[1],
        "artist_name": artistURL[0],
        "artist_url": artistURL[1],
        "song_state_now": songState[0],
        "song_state_total": songState[1]
    });
    var x = new XMLHttpRequest();
    x.open("POST", "http://localhost:13239");
    x.send(data);
}

setInterval(function() {
    sendData()
}, 1000)