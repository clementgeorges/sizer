#include "resizer.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QImage>
#include <QImageReader>
#include <QImageWriter>
#include <QSaveFile>
#include <cmath>

namespace Resizer {

namespace {
QString tr(const char *text) { return QCoreApplication::translate("Resizer", text); }

QSet<QByteArray> lower(const QList<QByteArray> &formats) {
    QSet<QByteArray> set;
    for (const auto &format : formats)
        set.insert(format.toLower());
    return set;
}

QByteArray normalizedSuffix(const QString &path) {
    const QByteArray suffix = QFileInfo(path).suffix().toLower().toUtf8();
    return suffix == "jpeg" ? "jpg" : suffix == "tif" ? "tiff" : suffix;
}
}

Probe probe(const QString &path) {
    QImageReader reader(path);
    reader.setAutoTransform(true);
    if (!reader.canRead())
        return {{}, reader.errorString()};
    QSize size = reader.size();
    if (!size.isValid()) {
        // A few formats only know their size after decoding.
        const QImage image = reader.read();
        if (image.isNull())
            return {{}, reader.errorString()};
        return {image.size(), {}};
    }
    // size() is the stored size; a photo taken in portrait is stored sideways
    // with a rotation tag, and the copy should be measured the way it shows.
    if (reader.transformation() & QImageIOHandler::TransformationRotate90)
        size.transpose();
    return {size, {}};
}

int heightFor(const QSize &original, int width) {
    if (original.width() <= 0 || width <= 0)
        return 0;
    return qMax(1, int(std::lround(double(original.height()) * width / original.width())));
}

bool isImageFile(const QString &path) {
    static const QSet<QByteArray> readable = lower(QImageReader::supportedImageFormats());
    const QByteArray suffix = QFileInfo(path).suffix().toLower().toUtf8();
    // SVG and PDF scale without loss; resizing them to pixels isn't this app's job.
    return QFileInfo(path).isFile() && readable.contains(suffix) && suffix != "svg" && suffix != "svgz";
}

QStringList imagesIn(const QString &folder) {
    QStringList images;
    const auto entries = QDir(folder).entryInfoList(QDir::Files | QDir::Readable, QDir::Name | QDir::IgnoreCase);
    for (const auto &entry : entries)
        if (isImageFile(entry.absoluteFilePath()))
            images << entry.absoluteFilePath();
    return images;
}

QByteArray outputFormat(const QString &source) {
    static const QSet<QByteArray> writable = lower(QImageWriter::supportedImageFormats());
    const QByteArray suffix = normalizedSuffix(source);
    return writable.contains(suffix) ? suffix : QByteArray("png");
}

QString destinationFor(const QString &source, const QString &folder, int width, QSet<QString> &taken) {
    const QFileInfo info(source);
    const QByteArray format = outputFormat(source);
    // Keep the original spelling of the extension (.JPG stays .JPG) unless
    // the format had to change.
    const QString suffix = format == normalizedSuffix(source) ? info.suffix() : QString::fromLatin1(format);
    const QString base = info.completeBaseName();
    const QDir dir(folder);
    const QString sourcePath = info.absoluteFilePath();

    auto free = [&](const QString &name) {
        const QString path = dir.absoluteFilePath(name);
        return !QFileInfo::exists(path) && !taken.contains(path) && path != sourcePath;
    };
    QString name = base + "." + suffix;
    if (!free(name)) {
        name = QStringLiteral("%1-%2w.%3").arg(base).arg(width).arg(suffix);
        for (int n = 2; !free(name); ++n)
            name = QStringLiteral("%1-%2w-%3.%4").arg(base).arg(width).arg(n).arg(suffix);
    }
    const QString path = dir.absoluteFilePath(name);
    taken.insert(path);
    return path;
}

Result resize(const Job &job) {
    Result result{job.source, job.destination, {}, {}};

    QImageReader reader(job.source);
    reader.setAutoTransform(true);
    QImage image = reader.read();
    if (image.isNull()) {
        result.error = tr("Could not read %1: %2").arg(QFileInfo(job.source).fileName(), reader.errorString());
        return result;
    }
    if (job.width <= 0) {
        result.error = tr("The width must be at least 1 pixel.");
        return result;
    }

    const QSize target(job.width, heightFor(image.size(), job.width));
    if (target != image.size())
        image = image.scaled(target, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);

    const QByteArray format = outputFormat(job.destination);
    // JPEG has no transparency; flatten onto white rather than black.
    if (format == "jpg" && image.hasAlphaChannel()) {
        QImage flat(image.size(), QImage::Format_RGB32);
        flat.fill(Qt::white);
        flat.setDevicePixelRatio(image.devicePixelRatio());
        QImage source = image.convertToFormat(QImage::Format_ARGB32_Premultiplied);
        for (int y = 0; y < flat.height(); ++y) {
            auto *out = reinterpret_cast<QRgb *>(flat.scanLine(y));
            const auto *in = reinterpret_cast<const QRgb *>(source.constScanLine(y));
            for (int x = 0; x < flat.width(); ++x) {
                const int a = qAlpha(in[x]);
                out[x] = qRgb(qRed(in[x]) + 255 - a, qGreen(in[x]) + 255 - a, qBlue(in[x]) + 255 - a);
            }
        }
        image = flat;
    }

    QSaveFile file(job.destination);
    if (!file.open(QIODevice::WriteOnly)) {
        result.error = tr("Could not write %1: %2").arg(job.destination, file.errorString());
        return result;
    }
    QImageWriter writer(&file, format);
    if (format == "jpg" || format == "webp")
        writer.setQuality(92);
    if (!writer.write(image)) {
        file.cancelWriting();
        result.error = tr("Could not write %1: %2").arg(job.destination, writer.errorString());
        return result;
    }
    if (!file.commit()) {
        result.error = tr("Could not write %1: %2").arg(job.destination, file.errorString());
        return result;
    }
    result.size = image.size();
    return result;
}

} // namespace Resizer
