#include "playback.h"

Playback::Playback(Media *media, QObject *parent) :
    QObject(parent)
{
    _media = media;
    _mediaSettings = new MediaSettings();

    if (_media->videoTracks().count() > 0)
        _mediaSettings->setVideoTrack(_media->videoTracks().first());

    if (_media->audioTracks().count() > 0)
        _mediaSettings->setAudioTrack(_media->audioTracks().first());

    _mediaSettings->setOutMark(_media->duration());
}

Playback::Playback(const Playback &playback)
{
    _media=playback.media();
    _mediaSettings = new MediaSettings(*playback.mediaSettings());
}

Playback::~Playback()
{
    delete _mediaSettings;
}

Playback& Playback::operator =(const Playback &playback)
{
    if(this!=&playback) {
        _media=playback.media();
        _mediaSettings = new MediaSettings(*playback.mediaSettings());
    }
    return *this;
}

