#ifndef MEDIALISTMODEL_H
#define MEDIALISTMODEL_H

#include <QAbstractTableModel>
#include <QList>

#include "media.h"

//class Media;

class MediaListModel : public QAbstractTableModel
{
    Q_OBJECT
public:
    enum Columns { Name = 0, Duration = 1, Used = 2, Location = 3 };

    MediaListModel(QObject *parent = 0);

    inline QList<Media>& mediaList() { return _mediaList; }

    int columnCount(const QModelIndex &parent) const;

    int rowCount(const QModelIndex &parent = QModelIndex()) const;

    Qt::ItemFlags flags(const QModelIndex &index) const;

    QVariant headerData(int section, Qt::Orientation orientation, int role) const;

    QVariant data(const QModelIndex &index, int role) const;

    bool removeRows(int position, int rows, const QModelIndex &index = QModelIndex());

    bool addMedia(const Media &media);

private:
    QList<Media> _mediaList;
};

#endif // MEDIALISTMODEL_H
