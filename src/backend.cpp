#include "backend.h"

#include <QDesktopServices>
#include <memory>
#include <QDir>
#include <QFileInfo>
#include <QSet>
#include <QSettings>
#include <QUrl>
#include <QtConcurrentMap>

#include "filepicker.h"

namespace {
// Plenty for a session of resizing, without letting snapshots of a large
// batch pile up without end.
constexpr int UndoLimit = 100;
}

Backend::Backend(FilePicker *picker, QObject *parent, const QString &settingsFile)
    : QObject(parent), m_picker(picker), m_settingsFile(settingsFile) {
    m_picker->setParent(this);
    connect(m_picker, &FilePicker::imagesSelected, this, [this](const QList<QUrl> &urls) {
        QStringList paths;
        for (const auto &url : urls)
            paths << url.toString();
        addPaths(paths);
    });
    connect(m_picker, &FilePicker::folderSelected, this, [this](const QUrl &url) {
        saveTo(url.isLocalFile() ? url.toLocalFile() : url.toString());
    });
    connect(m_picker, &FilePicker::failed, this, &Backend::setError);
    connect(&m_watcher, &QFutureWatcher<Resizer::Result>::progressValueChanged, this, &Backend::busyChanged);
    connect(&m_watcher, &QFutureWatcher<Resizer::Result>::finished, this, &Backend::finishSave);
}

Backend::~Backend() {
    // Let a running save finish its current files rather than leave them
    // half-written; the rest are cancelled.
    m_watcher.cancel();
    m_watcher.waitForFinished();
}

int Backend::selectedCount() const {
    return int(std::count_if(m_images.items().begin(), m_images.items().end(),
                             [](const auto &item) { return item.selected; }));
}

int Backend::changedCount() const {
    return int(std::count_if(m_images.items().begin(), m_images.items().end(),
                             [](const auto &item) { return item.targetWidth() != item.original.width(); }));
}

bool Backend::targets(const ImageList::Item &item) const {
    return item.selected || selectedCount() == 0;
}

int Backend::commonWidth() const {
    int width = 0;
    const bool all = selectedCount() == 0;
    for (const auto &item : m_images.items()) {
        if (!all && !item.selected)
            continue;
        if (width && width != item.targetWidth())
            return 0;
        width = item.targetWidth();
    }
    return width;
}

int Backend::heightFor(int width) const {
    int height = -1;
    const bool all = selectedCount() == 0;
    for (const auto &item : m_images.items()) {
        if (!all && !item.selected)
            continue;
        const int h = Resizer::heightFor(item.original, width);
        if (height != -1 && height != h)
            return -1;
        height = h;
    }
    return height;
}

double Backend::progress() const {
    const int range = m_watcher.progressMaximum() - m_watcher.progressMinimum();
    return range > 0 ? double(m_watcher.progressValue() - m_watcher.progressMinimum()) / range : 0;
}

void Backend::setError(const QString &message) {
    if (m_error == message)
        return;
    m_error = message;
    emit errorChanged();
}

void Backend::dismissError() { setError({}); }

void Backend::remember() {
    m_undo.append(m_images.items());
    if (m_undo.size() > UndoLimit)
        m_undo.removeFirst();
    m_redo.clear();
    m_dirty = true;
}

void Backend::restore(const Snapshot &snapshot) {
    m_images.setItems(snapshot);
    m_anchor = -1;
    m_dirty = true;
    emit changed();
}

void Backend::undo() {
    if (m_undo.isEmpty() || busy())
        return;
    m_redo.append(m_images.items());
    restore(m_undo.takeLast());
}

void Backend::redo() {
    if (m_redo.isEmpty() || busy())
        return;
    m_undo.append(m_images.items());
    restore(m_redo.takeLast());
}

void Backend::addImages() { m_picker->openImages(addFolder()); }

namespace {
// ~/.config/sizer/sizer.ini, unless a test asks for its own file.
std::unique_ptr<QSettings> settingsAt(const QString &file) {
    return file.isEmpty() ? std::make_unique<QSettings>(QSettings::IniFormat, QSettings::UserScope, "sizer", "sizer")
                          : std::make_unique<QSettings>(file, QSettings::IniFormat);
}
}

QString Backend::addFolder() const {
    return settingsAt(m_settingsFile)->value("addFolder").toString();
}

QString Backend::saveFolder() const {
    // Copies usually belong beside what they were made from, so start where
    // the selected images live, or the first image when none are selected.
    for (const auto &item : m_images.items())
        if (item.selected)
            return QFileInfo(item.path).absolutePath();
    return m_images.items().isEmpty() ? addFolder() : QFileInfo(m_images.items().first().path).absolutePath();
}

void Backend::addPaths(const QStringList &paths) {
    QStringList files;
    for (const auto &entry : paths) {
        const QUrl url(entry);
        const QString path = url.isLocalFile() ? url.toLocalFile() : entry;
        const QFileInfo info(path);
        if (info.isDir())
            files += Resizer::imagesIn(info.absoluteFilePath());
        else
            files << info.absoluteFilePath();
    }

    QSet<QString> present;
    for (const auto &item : m_images.items())
        present.insert(item.path);

    QVector<ImageList::Item> added;
    QStringList skipped;
    for (const auto &file : files) {
        if (present.contains(file))
            continue;
        const auto probe = Resizer::probe(file);
        if (!probe.error.isEmpty() || !QFileInfo(file).isFile()) {
            skipped << QFileInfo(file).fileName();
            continue;
        }
        present.insert(file);
        added.append({file, probe.size, 0, false});
    }

    if (!skipped.isEmpty())
        setError(tr("These aren't images Sizer can read: %1").arg(skipped.join(", ")));
    if (added.isEmpty())
        return;
    settingsAt(m_settingsFile)->setValue("addFolder", QFileInfo(added.first().path).absolutePath());
    remember();
    m_images.append(added);
    emit changed();
}

void Backend::select(int row, bool toggle, bool extend) {
    if (row < 0 || row >= count())
        return;
    if (extend && m_anchor >= 0) {
        const int from = qMin(m_anchor, row), to = qMax(m_anchor, row);
        m_images.update([&](int r, auto &item) { item.selected = (toggle && item.selected) || (r >= from && r <= to); });
    } else if (toggle) {
        m_images.update([&](int r, auto &item) { if (r == row) item.selected = !item.selected; });
        m_anchor = row;
    } else {
        m_images.update([&](int r, auto &item) { item.selected = r == row; });
        m_anchor = row;
    }
    emit changed();
}

void Backend::selectAll() {
    m_images.update([](int, auto &item) { item.selected = true; });
    emit changed();
}

void Backend::selectNone() {
    m_images.update([](int, auto &item) { item.selected = false; });
    m_anchor = -1;
    emit changed();
}

void Backend::removeSelected() {
    if (selectedCount() == 0 || busy())
        return;
    remember();
    Snapshot kept;
    for (const auto &item : m_images.items())
        if (!item.selected)
            kept.append(item);
    m_images.setItems(kept);
    m_anchor = -1;
    emit changed();
}

void Backend::setWidth(int width) {
    if (width <= 0 || count() == 0 || busy())
        return;
    bool differs = false;
    for (const auto &item : m_images.items())
        differs |= targets(item) && item.targetWidth() != width;
    if (!differs)
        return;
    remember();
    const bool all = selectedCount() == 0;
    m_images.update([&](int, auto &item) {
        if (all || item.selected)
            item.width = width == item.original.width() ? 0 : width;
    });
    emit changed();
}

void Backend::resetSize() {
    if (busy())
        return;
    bool differs = false;
    for (const auto &item : m_images.items())
        differs |= targets(item) && item.width != 0;
    if (!differs)
        return;
    remember();
    const bool all = selectedCount() == 0;
    m_images.update([&](int, auto &item) {
        if (all || item.selected)
            item.width = 0;
    });
    emit changed();
}

void Backend::save() {
    if (busy())
        return;
    if (count() == 0) {
        setError(tr("Add some images first."));
        return;
    }
    m_picker->chooseFolder(saveFolder());
}

void Backend::saveTo(const QString &folder) {
    if (busy() || count() == 0)
        return;
    const QFileInfo info(folder);
    if (!info.isDir() || !info.isWritable()) {
        setError(tr("Can't save into %1: the folder isn't writable.").arg(folder));
        return;
    }
    dismissError();

    // Names are settled up front, so two "photo.jpg" from different folders
    // can't race for the same destination.
    QSet<QString> taken;
    QList<Resizer::Job> jobs;
    for (const auto &item : m_images.items()) {
        const int width = item.targetWidth();
        jobs.append({item.path, Resizer::destinationFor(item.path, info.absoluteFilePath(), width, taken), width});
    }
    m_saveFolder = info.absoluteFilePath();
    m_watcher.setFuture(QtConcurrent::mapped(jobs, &Resizer::resize));
    emit busyChanged();
}

void Backend::finishSave() {
    QStringList failures;
    int written = 0;
    if (!m_watcher.isCanceled()) {
        const auto results = m_watcher.future().results();
        for (const auto &result : results) {
            if (result.error.isEmpty())
                ++written;
            else
                failures << result.error;
        }
    }
    emit busyChanged();
    if (!failures.isEmpty())
        setError(tr("%n images could not be saved.", "", int(failures.size())) + "\n" + failures.join("\n"));
    if (written > 0) {
        if (failures.isEmpty())
            m_dirty = false;
        emit changed();
        emit saved(m_saveFolder, written);
    }
}

void Backend::openFolder(const QString &folder) {
    if (!QDesktopServices::openUrl(QUrl::fromLocalFile(folder)))
        setError(tr("Could not open %1.").arg(folder));
}
