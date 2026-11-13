#pragma once

#include <QSet>
#include <QSize>
#include <QString>
#include <QStringList>

// The resizing itself, with no UI: the window and the command line both use
// these, and the tests call them directly.
namespace Resizer {

struct Probe {
    QSize size;    // As the image is shown, after its EXIF rotation.
    QString error; // Empty when the image can be read.
};

struct Job {
    QString source;
    QString destination;
    int width = 0;
};

struct Result {
    QString source;
    QString destination;
    QSize size;
    QString error;
};

Probe probe(const QString &path);
// The height that keeps the aspect ratio at the given width, never below 1.
int heightFor(const QSize &original, int width);
bool isImageFile(const QString &path);
// The images directly inside a folder, sorted by name.
QStringList imagesIn(const QString &folder);
// The format the copy is written in: the original's when Qt can write it,
// otherwise PNG (GIF, for one, can be read but not written).
QByteArray outputFormat(const QString &source);
// A free path in folder for the resized copy of source. It keeps the file
// name when nothing is there, and otherwise adds the width ("beach-1200w.jpg")
// and a counter, so neither an original nor an earlier export is replaced.
// taken holds paths already handed out in the same batch.
QString destinationFor(const QString &source, const QString &folder, int width, QSet<QString> &taken);
// Reads, scales, and writes one image. The copy is written beside its
// destination and renamed into place, so a failure leaves nothing half-done.
Result resize(const Job &job);

} // namespace Resizer
