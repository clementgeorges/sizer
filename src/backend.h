#pragma once

#include <QFutureWatcher>
#include <QObject>
#include <QVector>

#include "imagelist.h"
#include "resizer.h"

class FilePicker;

// The batch and everything done to it: adding images, choosing the new
// width, and writing the resized copies into a folder the user picks.
class Backend : public QObject {
    Q_OBJECT
    Q_PROPERTY(QObject *images READ images CONSTANT)
    Q_PROPERTY(int count READ count NOTIFY changed)
    Q_PROPERTY(int selectedCount READ selectedCount NOTIFY changed)
    Q_PROPERTY(int changedCount READ changedCount NOTIFY changed)
    // The width shared by every image the next change applies to, or 0 when
    // they differ.
    Q_PROPERTY(int commonWidth READ commonWidth NOTIFY changed)
    Q_PROPERTY(bool dirty READ dirty NOTIFY changed)
    Q_PROPERTY(bool canUndo READ canUndo NOTIFY changed)
    Q_PROPERTY(bool canRedo READ canRedo NOTIFY changed)
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)
    Q_PROPERTY(double progress READ progress NOTIFY busyChanged)
    Q_PROPERTY(QString error READ error NOTIFY errorChanged)

public:
    // The backend takes ownership of the picker. settingsFile is where the
    // last folder images were added from is kept; tests pass a temp file.
    explicit Backend(FilePicker *picker, QObject *parent = nullptr, const QString &settingsFile = {});
    ~Backend() override;

    QObject *images() { return &m_images; }
    const ImageList &list() const { return m_images; }
    int count() const { return int(m_images.items().size()); }
    int selectedCount() const;
    int changedCount() const;
    int commonWidth() const;
    bool dirty() const { return m_dirty && changedCount() > 0; }
    bool canUndo() const { return !m_undo.isEmpty(); }
    bool canRedo() const { return !m_redo.isEmpty(); }
    bool busy() const { return m_watcher.isRunning(); }
    double progress() const;
    QString error() const { return m_error; }

    // Opens the portal's file chooser to add images.
    Q_INVOKABLE void addImages();
    // Adds files, folders (their images, not subfolders), or file:// URLs.
    Q_INVOKABLE void addPaths(const QStringList &paths);

    // Clicking selects one image; with Ctrl it toggles, with Shift it extends
    // from the last clicked image.
    Q_INVOKABLE void select(int row, bool toggle = false, bool extend = false);
    Q_INVOKABLE void selectAll();
    Q_INVOKABLE void selectNone();
    Q_INVOKABLE void removeSelected();

    // Width changes apply to the selected images, or to all when none are.
    Q_INVOKABLE void setWidth(int width);
    Q_INVOKABLE void resetSize();
    // The height width gives when every target has the same shape, else -1.
    Q_INVOKABLE int heightFor(int width) const;

    // The folder Add Images starts in: where images were last added from,
    // remembered across sessions.
    QString addFolder() const;
    // The folder Save starts in: the one the images came from.
    QString saveFolder() const;

    // Asks where to save, then writes the resized copies there.
    Q_INVOKABLE void save();
    Q_INVOKABLE void saveTo(const QString &folder);
    Q_INVOKABLE void openFolder(const QString &folder);

    Q_INVOKABLE void undo();
    Q_INVOKABLE void redo();
    Q_INVOKABLE void dismissError();

signals:
    void changed();
    void busyChanged();
    void errorChanged();
    void saved(const QString &folder, int count);

private:
    using Snapshot = QVector<ImageList::Item>;

    void remember();
    void restore(const Snapshot &snapshot);
    bool targets(const ImageList::Item &item) const;
    void setError(const QString &message);
    void finishSave();

    ImageList m_images;
    FilePicker *m_picker;
    QVector<Snapshot> m_undo, m_redo;
    bool m_dirty = false;
    int m_anchor = -1;
    QString m_error;
    QString m_settingsFile;
    QString m_saveFolder;
    QFutureWatcher<Resizer::Result> m_watcher;
};
