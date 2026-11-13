#include "imagelist.h"

#include <QFileInfo>
#include <QUrl>

#include "resizer.h"

int ImageList::rowCount(const QModelIndex &parent) const {
    return parent.isValid() ? 0 : int(m_items.size());
}

QVariant ImageList::data(const QModelIndex &index, int role) const {
    if (!index.isValid() || index.row() >= m_items.size())
        return {};
    const Item &item = m_items.at(index.row());
    switch (role) {
    case PathRole: return item.path;
    case SourceRole: return QUrl::fromLocalFile(item.path);
    case NameRole: return QFileInfo(item.path).fileName();
    case OriginalWidthRole: return item.original.width();
    case OriginalHeightRole: return item.original.height();
    case NewWidthRole: return item.targetWidth();
    case NewHeightRole: return Resizer::heightFor(item.original, item.targetWidth());
    case ChangedRole: return item.targetWidth() != item.original.width();
    case EnlargedRole: return item.targetWidth() > item.original.width();
    case SelectedRole: return item.selected;
    }
    return {};
}

QHash<int, QByteArray> ImageList::roleNames() const {
    return {
        {PathRole, "path"},
        {SourceRole, "source"},
        {NameRole, "name"},
        {OriginalWidthRole, "originalWidth"},
        {OriginalHeightRole, "originalHeight"},
        {NewWidthRole, "newWidth"},
        {NewHeightRole, "newHeight"},
        {ChangedRole, "changed"},
        {EnlargedRole, "enlarged"},
        {SelectedRole, "selected"},
    };
}

void ImageList::setItems(const QVector<Item> &items) {
    beginResetModel();
    m_items = items;
    endResetModel();
}

void ImageList::append(const QVector<Item> &items) {
    if (items.isEmpty())
        return;
    beginInsertRows({}, int(m_items.size()), int(m_items.size() + items.size() - 1));
    m_items += items;
    endInsertRows();
}

void ImageList::update(const std::function<void(int, Item &)> &change) {
    for (int row = 0; row < m_items.size(); ++row) {
        const Item before = m_items.at(row);
        change(row, m_items[row]);
        if (!(before == m_items.at(row)))
            emit dataChanged(index(row), index(row));
    }
}
