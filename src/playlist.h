#ifndef PLAYLIST_H
#define PLAYLIST_H

#include <QObject>
#include "media.h"
#include "playback.h"

class Playlist : public QObject
{
    Q_OBJECT
public:
    Playlist(QObject *parent = 0);
    virtual ~Playlist();

    inline QList<Playback>& playbackList() {return _playbackList;}

    const Playback & at(const int &index) const;

    void append(const Playback &playback);

    int count() const;

private:
    QList<Playback> _playbackList;
    
};

#endif // PLAYLIST_H
