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

    inline const QList<Playback*>& playbackList() { return _playbackList; }

    Playback* at(const int &index) const;

    void append(Playback *playback);

    void removeAt(int index);

    int indexOf(Playback *playback) const;

    int count() const;

    uint totalDuration() const;

private:
    QList<Playback*> _playbackList;
    
};

#endif // PLAYLIST_H
