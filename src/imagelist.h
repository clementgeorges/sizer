#pragma once

#include <QAbstractListModel>
#include <QSize>
#include <QVector>
#include <functional>

// The images in the batch, as the grid shows them. It only stores; the
// Backend decides what changes, so every change can be undone.
class ImageList : public QAbstractListModel {
    Q_OBJECT

public:
    struct Item {
        QString path;
        QSize original;
        int width = 0; // The new width; 0 keeps the original size.
        bool selected = false;

        int targetWidth() const { return width > 0 ? width : original.width(); }
        bool operator==(const Item &other) const {
            return path == other.path && original == other.original && width == other.width &&
                   selected == other.selected;
        }
    };

    enum Role {
        PathRole = Qt::UserRole + 1,
        SourceRole,
        NameRole,
        OriginalWidthRole,
        OriginalHeightRole,
        NewWidthRole,
        NewHeightRole,
        ChangedRole,
        EnlargedRole,
        SelectedRole
    };

    explicit ImageList(QObject *parent = nullptr) : QAbstractListModel(parent) {}

    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    const QVector<Item> &items() const { return m_items; }
    void setItems(const QVector<Item> &items);
    void append(const QVector<Item> &items);
    // Changes items in place and tells the view which rows changed.
    void update(const std::function<void(int, Item &)> &change);

private:
    QVector<Item> m_items;
};
