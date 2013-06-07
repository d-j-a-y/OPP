#include "media.h"

#include "global.h"
#include "application.h"
#include <unistd.h>
#include <QThread>

Media::Media(const QString &location, libvlc_instance_t *vlcInstance, QObject *parent) :
    QObject(parent),
    _usageCount(0)
{
    initMedia(location);
    _vlcMedia = libvlc_media_new_path(vlcInstance, location.toLocal8Bit().data());

    // PATCH : We need to play the media to get the duration
    libvlc_media_player_t *fakeplayer = libvlc_media_player_new(vlcInstance);
    VLC_LAST_ERROR();
    libvlc_media_player_set_media(fakeplayer, _vlcMedia);
    libvlc_media_player_play(fakeplayer);
    usleep(100000);

    libvlc_track_description_t* trackAudio = libvlc_audio_get_track_description (fakeplayer);
    qDebug()<<"Audio tracks";
    while(trackAudio)
    {
        _audioTracks.append(trackAudio->psz_name);
        qDebug()<<trackAudio->psz_name;
        trackAudio=trackAudio->p_next;
    }
    qDebug()<<"Video tracks";
    libvlc_track_description_t* trackVideo = libvlc_video_get_track_description (fakeplayer);
    while(trackVideo)
    {
        _videoTracks.append(trackVideo->psz_name);
        qDebug()<<trackVideo->psz_name;
        trackVideo=trackVideo->p_next;
    }
    qDebug()<<"Subtitles tracks";
    libvlc_track_description_t* trackSubtitles = libvlc_video_get_spu_description (fakeplayer);
    while(trackSubtitles)
    {
        _subtitlesTracks.append(trackSubtitles->psz_name);
        qDebug()<<trackSubtitles->psz_name;
        trackSubtitles=trackSubtitles->p_next;
    }


    libvlc_media_player_stop(fakeplayer);
    libvlc_media_player_release(fakeplayer);
}

Media::Media(const Media &media)
{
    initMedia(media._location);
    _usageCount = media._usageCount;
    _vlcMedia = libvlc_media_duplicate(media._vlcMedia);
}

Media::~Media()
{
    libvlc_media_release(_vlcMedia);
}

void Media::initMedia(const QString &location)
{
    _location = location;
    _fileInfo = QFileInfo(location);
}

Media & Media::operator=(const Media &media)
{
    if (this != &media) {
        _location = media._location;
        _fileInfo = QFileInfo(_location);
        _vlcMedia = libvlc_media_duplicate(media._vlcMedia);
//        _usageCount = media._usageCount;
    }
    return *this;
}

bool Media::operator==(const Media &media) const
{
    return _location == media.location();
}

uint Media::duration() const
{
    return libvlc_media_get_duration(_vlcMedia);
}

QString Media::name() const
{
    return _fileInfo.fileName();
}

bool Media::exists() const
{
    return _fileInfo.exists();
}

void Media::usageCountAdd(int count)
{
    _usageCount += count;
    emit usageCountChanged();
}

bool Media::isUsed() const
{
    return _usageCount > 0;
}

QList<const char*> Media::audioTracks() const
{
    return _audioTracks;
}

QList<const char*> Media::videoTracks() const
{
    return _videoTracks;
}

QList<const char*> Media::subtitlesTracks() const
{
    return _subtitlesTracks;
}
