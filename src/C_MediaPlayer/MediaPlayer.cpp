/**********************************************************************************
 * This file is part of Open Projection Program (OPP).
 *
 * Copyright (C) 2013 Catalogue Ouvert du Cinéma <dev@cinemaouvert.fr>
 *
 * Authors: Florian Mhun <florian.mhun@gmail.com>
 *          Cyril Naud <futuramath@gmail.com>
 *          Thomas Berthomé <thoberthome@laposte.net>
 *          Thibaud Lamarche <lamarchethibaud@hotmail.com>
 *
 * Open Projection Program is an initiative of Catalogue Ouvert du Cinéma.
 * The software was developed by four students of University of Poitiers
 * as school project.
 *
 * Open Projection Program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Open Projection Program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Open Projection Program. If not, see <http://www.gnu.org/licenses/>.
 **********************************************************************************/

#include "MediaPlayer.h"

#include <math.h>
#include <string.h>
#include <sstream>

#include <QApplication>
#include <QDesktopWidget>
#include <QSettings>
#include <QTime>
#include <QTimer>
#include <QDir>

#include <vlc/vlc.h>

#include "VLCApplication.h"
#include "media.h"
#include "videoview.h"
#include "mainwindow.h"
#include "PlaylistPlayer.h"
#include "mediasettings.h"
#include "playback.h"
#include "utils.h"

MediaPlayer::MediaPlayer(libvlc_instance_t *vlcInstance, QObject *parent) :
    QObject(parent),
    _inst(vlcInstance),
    _vlcMediaPlayer(NULL),
    _vlcBackMediaPlayer(NULL),
    _vlcEvents(NULL),
    _currentPlayback(NULL),
    _videoView(NULL),
    _videoBackView(NULL),
    _currentVolume(100),
    _currentGain(0),
    _currentAudioFadeOut(0),
    _isPaused(false),
    _hasInitMedia(true),
    _timer(NULL),
    _timerAudioFadeOut(NULL),
    _timerAudioFadeIn(NULL),
    _timerVideoFadeOut(NULL),
    _timerVideoFadeIn(NULL)
{
    QSettings settings("opp","opp");
    if(settings.value("VideoReturnMode").toString() == "none")
    {
        _bMode = NONE;
    }
    else  if(settings.value("VideoReturnMode").toString() == "pictures")
    {
        _bMode = SCREENSHOT;
    }
    else
    {
        _bMode = STREAMING;
    }
    if(settings.value("locateR").toBool())
    {
        std::stringstream ss;
            ss << QApplication::desktop()->screenGeometry().width();
        _sizeScreen ="screen-left="+ ss.str();
    }
    else
    {
        std::stringstream ss;
            ss << (QApplication::desktop()->screen()->width()-QApplication::desktop()->screenGeometry().width());
        _sizeScreen ="screen-width="+ ss.str();
    }
    _vlcBackMediaPlayer = libvlc_media_player_new(_inst);
    _vlcMediaPlayer = libvlc_media_player_new(_inst);
    _vlcEvents = libvlc_media_player_event_manager(_vlcMediaPlayer);

    libvlc_video_set_key_input(_vlcMediaPlayer, false);
    libvlc_video_set_mouse_input(_vlcMediaPlayer, false);

    connect(this, SIGNAL(vout(int)), this, SLOT(applyCurrentPlaybackSettings()));

    createCoreConnections();
}

MediaPlayer::~MediaPlayer()
{
    disconnect(this, SIGNAL(vout(int)), this, SLOT(applyCurrentPlaybackSettings()));

    removeCoreConnections();
    libvlc_media_player_release(_vlcMediaPlayer);
    libvlc_media_player_release(_vlcBackMediaPlayer);
    libvlc_vlm_release(_inst);

    if(_timerAudioFadeOut != NULL){
        delete(_timerAudioFadeOut);
    }
    if(_timerAudioFadeIn != NULL){
        delete(_timerAudioFadeIn);
    }
    if(_timerVideoFadeOut != NULL){
        delete(_timerVideoFadeOut);
    }
    if(_timerVideoFadeIn != NULL){
        delete(_timerVideoFadeIn);
    }
    if(_timer != NULL){
        delete (_timer);
    }
}

int MediaPlayer::currentLength() const
{
    return libvlc_media_player_get_length(_vlcMediaPlayer);
}

bool MediaPlayer::isPlaying() const
{
    return libvlc_media_player_is_playing(_vlcMediaPlayer);
}

bool MediaPlayer::isStopped() const
{
    if(!isPaused() && !isPlaying())
        return true;

    return false;
}

Media * MediaPlayer::media()
{
    if(_currentPlayback)
    {
        return _currentPlayback->media();
    }
    else
    {
        return NULL;
    }
}

void MediaPlayer::setBackMode(const BackMode &mode)
{
    _bMode = mode;
}

void MediaPlayer::setVideoView(VideoView *videoView)
{
    _videoView = videoView;

    if (_videoView) {
        _currentWId = _videoView->request();
    } else {
        _currentWId = 0;
    }

    if (_currentWId) {
        #if defined(Q_OS_WIN)
                libvlc_media_player_set_hwnd(_vlcMediaPlayer, (void *)_currentWId);
        #elif defined(Q_OS_MAC)
                libvlc_media_player_set_nsobject(_vlcMediaPlayer, (void *)_currentWId);
        #elif defined(Q_OS_UNIX)
                libvlc_media_player_set_xwindow(_vlcMediaPlayer, _currentWId);
        #endif
    }
}

void MediaPlayer::setVideoBackView(VideoView *videoView)
{
    _videoBackView = videoView;

    _currentWId = _videoBackView->request();

    if (_currentWId) {
        #if defined(Q_OS_WIN)
                libvlc_media_player_set_hwnd(_vlcBackMediaPlayer, (void *)_currentWId);
        #elif defined(Q_OS_MAC)
                libvlc_media_player_set_nsobject(_vlcBackMediaPlayer, (void *)_currentWId);
        #elif defined(Q_OS_UNIX)
                libvlc_media_player_set_xwindow(_vlcBackMediaPlayer, _currentWId);
        #endif
    }
}

void MediaPlayer::open(Playback *playback)
{
    if(playback != NULL && playback->mediaSettings() != NULL){
        if(_hasInitMedia)
            close(_currentPlayback);

        _currentPlayback = playback;
        _hasInitMedia = true;
        stopFaderOut();
        stopFaderIn();
        setVolume(_currentVolume);

        /** allows to send the new length even if the media player isn't playing */
        emit lengthChanged((int)libvlc_media_get_duration(playback->media()->core()));

        libvlc_media_player_set_media(_vlcMediaPlayer, playback->media()->core());
        MediaSettings* mediaSettings = _currentPlayback->mediaSettings();

        connect(mediaSettings, SIGNAL(gainChanged(float)), this, SLOT(setCurrentGain(float)));
        connect(mediaSettings, SIGNAL(ratioChanged(Ratio)), this, SLOT(setCurrentRatio(Ratio)));
        connect(mediaSettings, SIGNAL(gammaChanged(float)), this, SLOT(setCurrentGamma(float)));
        connect(mediaSettings, SIGNAL(contrastChanged(float)), this, SLOT(setCurrentContrast(float)));
        connect(mediaSettings, SIGNAL(brightnessChanged(float)), this, SLOT(setCurrentBrightness(float)));
        connect(mediaSettings, SIGNAL(saturationChanged(float)), this, SLOT(setCurrentSaturation(float)));
        connect(mediaSettings, SIGNAL(hueChanged(int)), this, SLOT(setCurrentHue(int)));
        connect(mediaSettings, SIGNAL(deinterlacingChanged(Deinterlacing)), this, SLOT(setCurrentDeinterlacing(Deinterlacing)));
        connect(mediaSettings, SIGNAL(subtitlesSyncChanged(double)), this, SLOT(setCurrentSubtitlesSync(double)));
        connect(mediaSettings, SIGNAL(subtitlesFileChanged(QString)), this, SLOT(setSubtitlesFile(QString)));
        connect(mediaSettings, SIGNAL(audioSyncChanged(double)), this, SLOT(setCurrentAudioSync(double)));
        connect(mediaSettings, SIGNAL(audioTrackChanged(int)), this, SLOT(setCurrentAudioTrack(int)));
        connect(mediaSettings, SIGNAL(videoTrackChanged(int)), this, SLOT(setCurrentVideoTrack(int)));
        connect(mediaSettings, SIGNAL(subtitlesTrackChanged(int)), this, SLOT(setCurrentSubtitlesTrack(int)));
        connect(mediaSettings, SIGNAL(subtitlesEncodeChanged(int)), this, SLOT(setCurrentSubtitlesEncode(int)));
        connect(mediaSettings, SIGNAL(cropChanged(int,int,int,int)), this, SLOT(applyCrop(int,int,int,int)));
        connect(mediaSettings, SIGNAL(audioFadeOutChanged(int)), this, SLOT(setCurrentAudioFadeOut(int)));
        connect(mediaSettings, SIGNAL(audioFadeInChanged(int)), this, SLOT(setCurrentAudioFadeIn(int)));
        connect(mediaSettings, SIGNAL(videoFadeOutChanged(int)), this, SLOT(setCurrentVideoFadeOut(int)));
        connect(mediaSettings, SIGNAL(videoFadeInChanged(int)), this, SLOT(setCurrentVideoFadeIn(int)));

        if(_currentPlayback->media()->isImage()){
            libvlc_media_player_set_time(_vlcMediaPlayer, 0);
        }
    }
}

void MediaPlayer::close(Playback *playback){
    if(playback != NULL && playback->mediaSettings() != NULL){
        _hasInitMedia = false;
        stopFaderOut();
        stopFaderIn();

        setVolume(_currentVolume);
        MediaSettings* mediaSettings = playback->mediaSettings();

        disconnect(mediaSettings, SIGNAL(gainChanged(float)), this, SLOT(setCurrentGain(float)));
        disconnect(mediaSettings, SIGNAL(ratioChanged(Ratio)), this, SLOT(setCurrentRatio(Ratio)));
        disconnect(mediaSettings, SIGNAL(gammaChanged(float)), this, SLOT(setCurrentGamma(float)));
        disconnect(mediaSettings, SIGNAL(contrastChanged(float)), this, SLOT(setCurrentContrast(float)));
        disconnect(mediaSettings, SIGNAL(brightnessChanged(float)), this, SLOT(setCurrentBrightness(float)));
        disconnect(mediaSettings, SIGNAL(saturationChanged(float)), this, SLOT(setCurrentSaturation(float)));
        disconnect(mediaSettings, SIGNAL(hueChanged(int)), this, SLOT(setCurrentHue(int)));
        disconnect(mediaSettings, SIGNAL(deinterlacingChanged(Deinterlacing)), this, SLOT(setCurrentDeinterlacing(Deinterlacing)));
        disconnect(mediaSettings, SIGNAL(subtitlesSyncChanged(double)), this, SLOT(setCurrentSubtitlesSync(double)));
        disconnect(mediaSettings, SIGNAL(subtitlesFileChanged(QString)), this, SLOT(setSubtitlesFile(QString)));
        disconnect(mediaSettings, SIGNAL(audioSyncChanged(double)), this, SLOT(setCurrentAudioSync(double)));
        disconnect(mediaSettings, SIGNAL(audioTrackChanged(int)), this, SLOT(setCurrentAudioTrack(int)));
        disconnect(mediaSettings, SIGNAL(videoTrackChanged(int)), this, SLOT(setCurrentVideoTrack(int)));
        disconnect(mediaSettings, SIGNAL(subtitlesTrackChanged(int)), this, SLOT(setCurrentSubtitlesTrack(int)));
        disconnect(mediaSettings, SIGNAL(subtitlesEncodeChanged(int)), this, SLOT(setCurrentSubtitlesEncode(int)));
        disconnect(mediaSettings, SIGNAL(cropChanged(int,int,int,int)), this, SLOT(applyCrop(int,int,int,int)));
        disconnect(mediaSettings, SIGNAL(audioFadeOutChanged(int)), this, SLOT(setCurrentAudioFadeOut(int)));
        disconnect(mediaSettings, SIGNAL(audioFadeInChanged(int)), this, SLOT(setCurrentAudioFadeIn(int)));
        disconnect(mediaSettings, SIGNAL(videoFadeOutChanged(int)), this, SLOT(setCurrentVideoFadeOut(int)));
        disconnect(mediaSettings, SIGNAL(videoFadeInChanged(int)), this, SLOT(setCurrentVideoFadeIn(int)));
    }
}

void MediaPlayer::initStream()
{
    const char* params[] = {"screen-fragment-size=16",
            _sizeScreen.c_str(),
            "screen-fps=25"};

    libvlc_vlm_add_broadcast(_inst, "mybroad",
        "screen:// --sout",
        "#transcode{vcodec=mp2v,acodec=none,ab=128}:standard{access=http,mux=ts,dst=127.0.0.1:8080/stream}",
        3, params, 1 , 0);

    Media *m = new Media("http://127.0.0.1:8080/stream", _inst, 0, false);
    libvlc_media_player_set_media(_vlcBackMediaPlayer, m->core());
}

void MediaPlayer::playStream()
{
    libvlc_vlm_play_media(_inst, "mybroad");
    libvlc_media_player_play(_vlcBackMediaPlayer);
}

void MediaPlayer::pauseStream()
{
    libvlc_media_player_pause(_vlcBackMediaPlayer);
    libvlc_vlm_pause_media(_inst, "mybroad");
}

void MediaPlayer::stopStream()
{
    libvlc_media_player_stop(_vlcBackMediaPlayer);
    libvlc_vlm_stop_media(_inst, "mybroad");
}

void MediaPlayer::playScreen()
{
    if(_timer == NULL){
        _timer = new QTimer();
    }

    if(!_currentPlayback->media()->isAudio() && !_currentPlayback->media()->isImage())
    {
        takeScreen();
        _timer->connect(_timer, SIGNAL(timeout()), this, SLOT(takeScreen()));
        _timer->start(20);
    }
}

void MediaPlayer::stopScreen()
{
    _timer->stop();
}

void MediaPlayer::takeScreen()
{
    unsigned* width = new unsigned(0);
    unsigned* height = new unsigned(0);
    libvlc_video_get_size(_vlcMediaPlayer, 0, width, height);

    // reduce the size to a suitable size
    unsigned denom;
    unsigned denomH = *height / (unsigned)((MainWindow *)((PlaylistPlayer *)this->parent())->parent())->screenBack()->height();
    unsigned denomW = *width / (unsigned)((MainWindow *)((PlaylistPlayer *)this->parent())->parent())->screenBack()->width();
    if(denomH < denomW)
        denom = denomH;
    else
        denom = denomW;

    if(denom > 1){
        *width = *width/denom;
        *height = *height/denom;
    }

    libvlc_video_take_snapshot(_vlcMediaPlayer, 0, "tmp.png", *width, *height);
    ((MainWindow *)((PlaylistPlayer *)this->parent())->parent())->setScreenshot("tmp.png");
    QFile("tmp.png").remove();
}

void MediaPlayer::play()
{
    switch (_bMode)
    {
        case SCREENSHOT:
            playScreen();
            break;
        case STREAMING:
            playStream();
            break;
        default:
            break;
    }

    libvlc_media_player_play(_vlcMediaPlayer);
    setVolume(_currentVolume);
    startAudioFadeOut(0);
    startAudioFadeIn();
    startVideoFadeOut(0);
    startVideoFadeIn();
    _isPaused = false;
}

void MediaPlayer::pause()
{
    switch (_bMode)
    {
    case STREAMING:
        pauseStream();
        break;
    default:
        break;
    }
    stopFaderOut();
    libvlc_media_player_set_pause(_vlcMediaPlayer, true);
    _isPaused = true;
}

void MediaPlayer::resume()
{
    switch (_bMode)
    {
    case SCREENSHOT:
        playScreen();
        break;
    case STREAMING:
        playStream();
        break;
    default:
        break;
    }

    libvlc_media_player_set_pause(_vlcMediaPlayer, false);
    setVolume(_currentVolume);
    startAudioFadeOut(libvlc_media_player_get_time(_vlcMediaPlayer));
    startVideoFadeOut(libvlc_media_player_get_time(_vlcMediaPlayer));
    _isPaused = false;
}

void MediaPlayer::stop()
{
    if (!_vlcMediaPlayer)
        return;

    _currentWId = 0;

    if(_currentPlayback != NULL){
        QString screenPath = "./screenshot/";
        screenPath = screenPath.replace("/",QDir::separator());
        screenPath +=  _currentPlayback->media()->getLocation().replace(QDir::separator(),"_").remove(":");
        screenPath += ".png";

        switch (_bMode)
        {
            case SCREENSHOT:
                stopScreen();
                ((MainWindow *)((PlaylistPlayer *)this->parent())->parent())->setScreenshot(screenPath);
                break;
            case STREAMING:
                stopStream();
                break;
            default:
                break;
        }
    }

    libvlc_media_player_stop(_vlcMediaPlayer);
    stopFaderOut();
    stopFaderIn();

    if(isPaused())
        emit end();

    _isPaused = false;
}

int MediaPlayer::currentTime() const
{
    return libvlc_media_player_get_time(_vlcMediaPlayer);
}

void MediaPlayer::setCurrentTime(int time)
{
    if(_currentPlayback){
        if(time > currentLength()){
            stop();
            close(_currentPlayback);
            return;
        }else if (time < 0){
            time = 0;
        }

        libvlc_media_player_set_time(_vlcMediaPlayer, time);
        startAudioFadeOut(time);
        startVideoFadeOut(time);

        /**
         * used because when the media player is in pause,
         * the libvlc doesn't send any event
         */
        if(isPaused() || isStopped())
            emit timeChanged(time);
    }
}

void MediaPlayer::setVolume(int volume)
{
    if(volume > 100)
        volume = 100;

    _currentVolume = volume;

    libvlc_audio_set_volume(_vlcMediaPlayer, ((float) volume) * powf(10.f, _currentGain/10.f) );
}

float MediaPlayer::position() const
{
    return libvlc_media_player_get_position(_vlcMediaPlayer);
}

int MediaPlayer::volume() const {
    return libvlc_audio_get_volume(_vlcMediaPlayer);
}

void MediaPlayer::setPosition(const float &position)
{
    libvlc_media_player_set_position(_vlcMediaPlayer, position);
}

int MediaPlayer::jumpForward(int timeMs)
{
    int currentT = currentTime();

    if(currentT){
        int newTime = currentT + timeMs;
        int currentL = currentLength();

        if(currentL >= newTime){
            setCurrentTime(newTime);
            return newTime;
        }else{
            setCurrentTime(currentL);
            stopFaderOut();
            return 0;
        }
    }

    stopFaderOut();
    return -1;
}

int MediaPlayer::jumpBack(int timeMs)
{
    int current = currentTime();

    if(current){
        int newTime = current - timeMs;

        if(newTime >= 0){
            setCurrentTime(newTime);
            return newTime;
        }else{
            setCurrentTime(0);
            stopFaderOut();
            return 0;
        }
    }

    stopFaderOut();
    return -1;
}

void MediaPlayer::goToBlack()
{
    // Disable the subtitle file
    removeCurrentSubtitlesFile();

    int nbEchantillon = 100;
    int timeLeft = 2500;

    // Audio
    float vol  = _currentVolume;
    float deltaA = vol / nbEchantillon;

    // Video
    float brightness  = _currentPlayback->mediaSettings()->brightness();
    float deltaV = brightness / nbEchantillon;

    for(float i = nbEchantillon; i>0; i--){
        int timeToWait =  timeLeft / nbEchantillon;

        if(!isPlaying()){
            return;
        }else{
            // Audio
            vol -= deltaA;
            libvlc_audio_set_volume(_vlcMediaPlayer, ((float) vol) * powf(10.f, _currentGain/10.f) );

            // Video
            brightness -= deltaV;
            setCurrentBrightness(brightness);
            setCurrentSaturation(brightness);
            setCurrentContrast(brightness);
        }
        wait(timeToWait);
    }

    stop();
    close(_currentPlayback);
    emit endGoToBlack();
}

void MediaPlayer::removeCurrentSubtitlesFile()
{
    setSubtitlesFile(qApp->applicationDirPath() + "/emptySubtitlesFile.srt");
}

/**********************************************************************\
                              SETTINGS
\**********************************************************************/

void MediaPlayer::applyCurrentPlaybackSettings()
{
    setCurrentGain(_currentPlayback->mediaSettings()->gain());

    setCurrentSubtitlesTrack(_currentPlayback->mediaSettings()->subtitlesTrack());
    setCurrentVideoTrack(_currentPlayback->mediaSettings()->videoTrack());
    setCurrentAudioTrack(_currentPlayback->mediaSettings()->audioTrack());

    setCurrentBrightness(_currentPlayback->mediaSettings()->brightness());
    setCurrentContrast(_currentPlayback->mediaSettings()->contrast());
    setCurrentDeinterlacing(_currentPlayback->mediaSettings()->deinterlacing());
    setCurrentGamma(_currentPlayback->mediaSettings()->gamma());
    setCurrentHue(_currentPlayback->mediaSettings()->hue());
    setCurrentRatio(_currentPlayback->mediaSettings()->ratio());
    setCurrentSaturation(_currentPlayback->mediaSettings()->saturation());
    setCurrentSubtitlesSync(_currentPlayback->mediaSettings()->subtitlesSync());
    setCurrentAudioSync(_currentPlayback->mediaSettings()->audioSync());
    setCurrentSubtitlesEncode(_currentPlayback->mediaSettings()->subtitlesEncode());

    applyCrop(_currentPlayback->mediaSettings()->cropTop(),
              _currentPlayback->mediaSettings()->cropLeft(),
              _currentPlayback->mediaSettings()->cropRight(),
              _currentPlayback->mediaSettings()->cropBot()
              );

    setCurrentAudioFadeOut(_currentPlayback->mediaSettings()->audioFadeOut());
    setCurrentAudioFadeIn(_currentPlayback->mediaSettings()->audioFadeIn());
    setCurrentVideoFadeOut(_currentPlayback->mediaSettings()->videoFadeOut());
    setCurrentVideoFadeIn(_currentPlayback->mediaSettings()->videoFadeIn());

    setSubtitlesFile(_currentPlayback->mediaSettings()->subtitlesFile());
}

void MediaPlayer::setCurrentGain(float gain)
{
    _currentGain = gain;
    setVolume(_currentVolume);
}

void MediaPlayer::setCurrentAudioFadeOut(int time)
{
    _currentAudioFadeOut = time;
}

void MediaPlayer::setCurrentAudioFadeIn(int time){
    _currentAudioFadeIn = time;

}

void MediaPlayer::setCurrentVideoFadeIn(int time){
    _currentVideoFadeIn = time;
}

void MediaPlayer::setCurrentVideoFadeOut(int time){
    _currentVideoFadeOut = time;
}

void MediaPlayer::setCurrentAudioTrack(const int &track)
{
    // call libvlc_video_set_audio() when media does not contains any audio track cause trouble
    if (_currentPlayback->media()->audioTracks().size() > 0)
        libvlc_audio_set_track(_vlcMediaPlayer, track);
}

void MediaPlayer::setCurrentVideoTrack(const int &track)
{
        libvlc_video_set_track(_vlcMediaPlayer, track);
}

void MediaPlayer::setCurrentSubtitlesTrack(const int &track)
{
    // call libvlc_video_set_spu() when media does not contains any subtitles track cause trouble
    if (_currentPlayback->media()->subtitlesTracks().size() > 0)
        libvlc_video_set_spu(_vlcMediaPlayer, track);
}

bool MediaPlayer::setSubtitlesFile(QString subtitleFile)
{
    if(!subtitleFile.isEmpty()){
        return libvlc_video_set_subtitle_file(_vlcMediaPlayer, subtitleFile.toStdString().c_str());
    }
    return false;
}

void MediaPlayer::setCurrentRatio(Ratio ratio)
{
    libvlc_video_set_aspect_ratio(_vlcMediaPlayer, MediaSettings::ratioValues()[ratio].toUtf8().data());
}

void MediaPlayer::setCurrentDeinterlacing(Deinterlacing deinterlacing)
{
    libvlc_video_set_deinterlace(_vlcMediaPlayer, MediaSettings::deinterlacingValues()[deinterlacing].toUtf8().data());
}

void MediaPlayer::setCurrentSubtitlesSync(double sync)
{
    libvlc_video_set_spu_delay(_vlcMediaPlayer,(int64_t)(1000000*sync));
}

void MediaPlayer::setCurrentGamma(float gamma)
{
    libvlc_video_set_adjust_int(_vlcMediaPlayer,libvlc_adjust_Enable,1);
    libvlc_video_set_adjust_float(_vlcMediaPlayer,libvlc_adjust_Gamma, gamma);
}

void MediaPlayer::setCurrentContrast(float contrast)
{
    libvlc_video_set_adjust_int(_vlcMediaPlayer,libvlc_adjust_Enable,1);
    libvlc_video_set_adjust_float(_vlcMediaPlayer,libvlc_adjust_Contrast, contrast);
}

void MediaPlayer::setCurrentBrightness(float brightness)
{
    libvlc_video_set_adjust_int(_vlcMediaPlayer,libvlc_adjust_Enable,1);
    libvlc_video_set_adjust_float(_vlcMediaPlayer,libvlc_adjust_Brightness, brightness);
}

void MediaPlayer::setCurrentSaturation(float saturation)
{
    libvlc_video_set_adjust_int(_vlcMediaPlayer,libvlc_adjust_Enable,1);
    libvlc_video_set_adjust_float(_vlcMediaPlayer,libvlc_adjust_Saturation, saturation);
}

void MediaPlayer::setCurrentHue(int hue)
{
    libvlc_video_set_adjust_int(_vlcMediaPlayer,libvlc_adjust_Enable,1);
    libvlc_video_set_adjust_int(_vlcMediaPlayer,libvlc_adjust_Hue, hue);
}

void MediaPlayer::setCurrentAudioSync(double sync)
{
    libvlc_audio_set_delay(_vlcMediaPlayer,(int64_t)(1000000*sync));
}

void MediaPlayer::setCurrentSubtitlesEncode(int encode){
    if(encode > 0 && _currentPlayback != NULL){
        QString enc = MediaSettings::encodeValues()[encode];

        libvlc_media_add_option(_currentPlayback->media()->core(),
                                QString(":subsdec-encoding=" + enc).toLocal8Bit().data());

    }
}

/*****************************************************************************\
                            CORE CONNECTION
\*****************************************************************************/

void MediaPlayer::createCoreConnections()
{
    QList<libvlc_event_e> list;
    list << libvlc_MediaPlayerMediaChanged
         << libvlc_MediaPlayerNothingSpecial
         << libvlc_MediaPlayerOpening
         << libvlc_MediaPlayerBuffering
         << libvlc_MediaPlayerPlaying
         << libvlc_MediaPlayerPaused
         << libvlc_MediaPlayerStopped
         << libvlc_MediaPlayerForward
         << libvlc_MediaPlayerBackward
         << libvlc_MediaPlayerEndReached
         << libvlc_MediaPlayerEncounteredError
         << libvlc_MediaPlayerTimeChanged
         << libvlc_MediaPlayerPositionChanged
         << libvlc_MediaPlayerSeekableChanged
         << libvlc_MediaPlayerPausableChanged
         << libvlc_MediaPlayerTitleChanged
         << libvlc_MediaPlayerSnapshotTaken
         << libvlc_MediaPlayerLengthChanged
         << libvlc_MediaPlayerVout;

    foreach(const libvlc_event_e &event, list) {
        libvlc_event_attach(_vlcEvents, event, libvlc_callback, this);
    }
}

void MediaPlayer::removeCoreConnections()
{
    QList<libvlc_event_e> list;
    list << libvlc_MediaPlayerMediaChanged
         << libvlc_MediaPlayerNothingSpecial
         << libvlc_MediaPlayerOpening
         << libvlc_MediaPlayerBuffering
         << libvlc_MediaPlayerPlaying
         << libvlc_MediaPlayerPaused
         << libvlc_MediaPlayerStopped
         << libvlc_MediaPlayerForward
         << libvlc_MediaPlayerBackward
         << libvlc_MediaPlayerEndReached
         << libvlc_MediaPlayerEncounteredError
         << libvlc_MediaPlayerTimeChanged
         << libvlc_MediaPlayerPositionChanged
         << libvlc_MediaPlayerSeekableChanged
         << libvlc_MediaPlayerPausableChanged
         << libvlc_MediaPlayerTitleChanged
         << libvlc_MediaPlayerSnapshotTaken
         << libvlc_MediaPlayerLengthChanged
         << libvlc_MediaPlayerVout;

    foreach(const libvlc_event_e &event, list) {
        libvlc_event_detach(_vlcEvents, event, libvlc_callback, this);
    }
}

void MediaPlayer::libvlc_callback(const libvlc_event_t *event, void *data)
{
    MediaPlayer *player = (MediaPlayer *)data;

    switch(event->type)
    {
    case libvlc_MediaPlayerMediaChanged:
        emit player->mediaChanged(event->u.media_player_media_changed.new_media);
        break;
    case libvlc_MediaPlayerNothingSpecial:
        emit player->nothingSpecial();
        break;
    case libvlc_MediaPlayerOpening:
        emit player->opening();
        break;
    case libvlc_MediaPlayerBuffering:
        emit player->buffering(event->u.media_player_buffering.new_cache);
        break;
    case libvlc_MediaPlayerPlaying:
        emit player->playing();
        break;
    case libvlc_MediaPlayerPaused:
        emit player->paused();
        break;
    case libvlc_MediaPlayerStopped:
        emit player->stopped();
        break;
    case libvlc_MediaPlayerForward:
        emit player->forward();
        break;
    case libvlc_MediaPlayerBackward:
        emit player->backward();
        break;
    case libvlc_MediaPlayerEndReached:
        emit player->end();
        break;
    case libvlc_MediaPlayerEncounteredError:
        emit player->error();
        break;
    case libvlc_MediaPlayerTimeChanged:
        emit player->timeChanged(event->u.media_player_time_changed.new_time);
        break;
    case libvlc_MediaPlayerPositionChanged:
        emit player->positionChanged(event->u.media_player_position_changed.new_position);
        break;
    case libvlc_MediaPlayerSeekableChanged:
        emit player->seekableChanged(event->u.media_player_seekable_changed.new_seekable);
        break;
    case libvlc_MediaPlayerPausableChanged:
        emit player->pausableChanged(event->u.media_player_pausable_changed.new_pausable);
        break;
    case libvlc_MediaPlayerTitleChanged:
        emit player->titleChanged(event->u.media_player_title_changed.new_title);
        break;
    case libvlc_MediaPlayerSnapshotTaken:
        emit player->snapshotTaken(event->u.media_player_snapshot_taken.psz_filename);
        break;
    case libvlc_MediaPlayerLengthChanged:
        emit player->lengthChanged(event->u.media_player_length_changed.new_length);
        break;
    case libvlc_MediaPlayerVout:
        emit player->vout(event->u.media_player_vout.new_count);
        break;
    default:
        break;
    }

    if (event->type >= libvlc_MediaPlayerNothingSpecial &&
        event->type <= libvlc_MediaPlayerEncounteredError) {
        emit player->stateChanged();
    }
}

void MediaPlayer::applyCrop(int cropTop,int cropLeft, int cropRight, int cropBot){
    QString plus =("+");
    QString val =
            QString::number(cropLeft)  +plus+
            QString::number(cropTop)   +plus+
            QString::number(cropRight) +plus+
            QString::number(cropBot);
    libvlc_video_set_crop_geometry(core(),val.toStdString().c_str());
}

/****************************/
/***      FADERS          ***/
/****************************/

void MediaPlayer::startAudioFadeOut(int time){
    if(_currentPlayback->mediaSettings()->audioFadeOut() > 0){
        setVolume(_currentVolume);
        stopFader(_timerAudioFadeOut);
        if(_timerAudioFadeOut == NULL){
             _timerAudioFadeOut = new QTimer();
             _timerAudioFadeOut->setSingleShot(true);
             _timerAudioFadeOut->connect(_timerAudioFadeOut, SIGNAL(timeout()), this, SLOT(audioFadeOut()));
        }

        startFaderOut(_timerAudioFadeOut, time, _currentPlayback->mediaSettings()->audioFadeOut());
    }
}

void MediaPlayer::startAudioFadeIn(){
    if(_currentPlayback->mediaSettings()->audioFadeIn() > 0){
        stopFader(_timerAudioFadeIn);
        if(_timerAudioFadeIn == NULL){
             _timerAudioFadeIn = new QTimer();
             _timerAudioFadeIn->setSingleShot(true);
             _timerAudioFadeIn->connect(_timerAudioFadeIn, SIGNAL(timeout()), this, SLOT(audioFadeIn()));
        }

        startFaderIn(_timerAudioFadeIn);
    }
}
void MediaPlayer::startVideoFadeOut(int time){
    if(_currentPlayback->mediaSettings()->videoFadeOut() > 0){
        setCurrentBrightness(_currentPlayback->mediaSettings()->brightness());
        stopFader(_timerVideoFadeOut);
        if(_timerVideoFadeOut == NULL){
             _timerVideoFadeOut = new QTimer();
             _timerVideoFadeOut->setSingleShot(true);
             _timerVideoFadeOut->connect(_timerVideoFadeOut, SIGNAL(timeout()), this, SLOT(videoFadeOut()));
        }

        startFaderOut(_timerVideoFadeOut, time, _currentPlayback->mediaSettings()->videoFadeOut());
    }
}

void MediaPlayer::startVideoFadeIn(){
    if(_currentPlayback->mediaSettings()->videoFadeIn() > 0){
        stopFader(_timerVideoFadeIn);
        if(_timerVideoFadeIn == NULL){
             _timerVideoFadeIn = new QTimer();
             _timerVideoFadeIn->setSingleShot(true);
             _timerVideoFadeIn->connect(_timerVideoFadeIn, SIGNAL(timeout()), this, SLOT(videoFadeIn()));
        }

        startFaderIn(_timerVideoFadeIn);
    }
}

void MediaPlayer::stopFaderOut(){
    stopFader(_timerAudioFadeOut);
    stopFader(_timerVideoFadeOut);
}

void MediaPlayer::stopFaderIn(){
    stopFader(_timerAudioFadeIn);
    stopFader(_timerVideoFadeIn);
}

void MediaPlayer::startFaderOut(QTimer* timer, int time, int timeToFade){
    int duration = _currentPlayback->mediaSettings()->outMark() - _currentPlayback->mediaSettings()->inMark();
    duration = (duration <= 0) ? 0 : _currentPlayback->media()->duration();

    int launch = duration - timeToFade;
    if(time > 0)
        launch -= time - _currentPlayback->mediaSettings()->inMark() ;

    launch = launch < 0 ? 0 : launch;

    timer->start(launch);
}

void MediaPlayer::startFaderIn(QTimer* timer){
    timer->start();
}

void MediaPlayer::audioFadeOut(){
    float vol  = _currentVolume;
    float delta = vol / 100;

    int duration = _currentPlayback->mediaSettings()->outMark();
    if(duration <= 0){
        duration = _currentPlayback->media()->duration();

        if(duration == 0)
            return;
    }

    int timeLeft = (duration - currentTime() );
    int timeToWait =  timeLeft / 100;

    for(float i = 100; i>0; i--){
        if(!isPlaying() || (timeLeft-500) >_currentPlayback->mediaSettings()->audioFadeOut()){
            return;
        }else{
            vol -= delta;
            libvlc_audio_set_volume(_vlcMediaPlayer, ((float) vol) * powf(10.f, _currentGain/10.f) );
        }
        wait(timeToWait);
    }
}

void MediaPlayer::audioFadeIn(){
    libvlc_audio_set_volume(_vlcMediaPlayer, ((float) 0) * powf(10.f, _currentGain/10.f) );
    float vol  = 0;
    float delta = _currentVolume / 40;

    for(float i = 40; i>0; i--){
        int timeToWait =  _currentPlayback->mediaSettings()->audioFadeIn() / 40;
        wait(timeToWait);

        if(!isPlaying()){
            return;
        }else{
            vol += delta;
            libvlc_audio_set_volume(_vlcMediaPlayer, ((float) vol) * powf(10.f, _currentGain/10.f) );
        }
    }
    libvlc_audio_set_volume(_vlcMediaPlayer, ((float) _currentVolume) * powf(10.f, _currentGain/10.f) );

}
void MediaPlayer::videoFadeOut(){
    int nbEchantillon = 100;
    float brightness  = _currentPlayback->mediaSettings()->brightness();
    float delta = brightness / nbEchantillon;

    int duration = _currentPlayback->mediaSettings()->outMark();
    if(duration <=0)
        duration = _currentPlayback->media()->duration();

    if(duration == 0)
        return;

    int timeLeft = duration - currentTime();
    int timeToWait =  timeLeft / nbEchantillon;

    for(float i = nbEchantillon; i>0; i--){
        if(!isPlaying() || (timeLeft-500) >_currentPlayback->mediaSettings()->videoFadeOut()){
            return;
        }else{
            brightness -= delta;
            setCurrentBrightness(brightness);
            setCurrentSaturation(brightness);
            setCurrentContrast(brightness);
        }
        wait(timeToWait);
    }
}
void MediaPlayer::videoFadeIn(){
    float brightness  = 0;
    setCurrentBrightness(brightness);
    float delta = _currentPlayback->mediaSettings()->brightness() / 40;

    for(float i = 40; i>0; i--){
        int timeToWait =  _currentPlayback->mediaSettings()->videoFadeIn() / 40;
        wait(timeToWait);

        if(!isPlaying()){
            return;
        }else{
            brightness += delta;
            setCurrentBrightness(brightness);
        }
    }
    setCurrentBrightness(_currentPlayback->mediaSettings()->brightness());
}

void MediaPlayer::stopFader(QTimer *timer){
    if(timer != NULL){
        timer->stop();
    }
}

