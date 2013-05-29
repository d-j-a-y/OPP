#ifndef PLAYLISTMODEL_H
#define PLAYLISTMODEL_H

#include <QAbstractTableModel>

class Media;

class PlaylistModel : public QAbstractTableModel
{
    Q_OBJECT
public:
    enum Columns { Title = 0, Duration = 1, Video = 2, Audio = 3, Subtitles = 4, TestPattern = 5, Gain = 6, Status = 7 };

    PlaylistModel(QList<Media> &mediaList, QObject *parent = 0);

    int columnCount(const QModelIndex &parent) const;

    int rowCount(const QModelIndex &parent = QModelIndex()) const;

    Qt::ItemFlags flags(const QModelIndex &index) const;

    QVariant headerData(int section, Qt::Orientation orientation, int role) const;

    QVariant data(const QModelIndex &index, int role) const;

private:
    QList<Media> &_mediaList;
};


#endif // PLAYLISTMODEL_H