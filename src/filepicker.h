#pragma once

// The picker interface follows Omacut (MIT, David Heinemeier Hansson): the
// app talks to this, the portal implements it, and tests inject a fake.

#include <QList>
#include <QObject>
#include <QUrl>

class FilePicker : public QObject {
    Q_OBJECT

public:
    explicit FilePicker(QObject *parent = nullptr) : QObject(parent) {}
    virtual ~FilePicker() = default;

    // Asks for any number of images to add, starting in start.
    virtual void openImages(const QString &start) = 0;
    // Asks for the folder the resized copies go into, starting in suggested.
    virtual void chooseFolder(const QString &suggested) = 0;

signals:
    void imagesSelected(const QList<QUrl> &urls);
    void folderSelected(const QUrl &url);
    void failed(const QString &message);
};
