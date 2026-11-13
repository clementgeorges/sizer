#include <QDir>
#include <QFile>
#include <QImage>
#include <QImageReader>
#include <QImageWriter>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest>

#include "backend.h"
#include "filepicker.h"
#include "resizer.h"
#include "theme.h"
#include "translator.h"

// Answers the backend's questions without a portal or a display.
class FakePicker : public FilePicker {
    Q_OBJECT
public:
    QString folder;
    QString suggested;
    QString addStart;
    void openImages(const QString &start) override { addStart = start; }
    void chooseFolder(const QString &start) override {
        suggested = start;
        emit folderSelected(QUrl::fromLocalFile(folder));
    }
};

class SizerTests : public QObject {
    Q_OBJECT

private slots:
    void heightKeepsTheShape()
    {
        QCOMPARE(Resizer::heightFor({4032, 3024}, 1200), 900);
        QCOMPARE(Resizer::heightFor({1080, 1920}, 1200), 2133);
        QCOMPARE(Resizer::heightFor({5000, 10}, 100), 1);
        QCOMPARE(Resizer::heightFor({0, 10}, 100), 0);
    }

    void probeMeasuresRotatedPhotosUpright()
    {
        QTemporaryDir dir;
        const QString path = dir.filePath("portrait.jpg");
        image(path, {400, 300}, QImageIOHandler::TransformationRotate90);
        QCOMPARE(Resizer::probe(path).size, QSize(300, 400));
        QVERIFY(!Resizer::probe(dir.filePath("missing.jpg")).error.isEmpty());
    }

    void resizeWritesAnUprightCopyAndLeavesTheOriginal()
    {
        QTemporaryDir dir;
        const QString source = dir.filePath("portrait.jpg");
        image(source, {400, 300}, QImageIOHandler::TransformationRotate90);
        const QByteArray before = contents(source);

        const auto result = Resizer::resize({source, dir.filePath("out.jpg"), 150});
        QVERIFY2(result.error.isEmpty(), qPrintable(result.error));
        QCOMPARE(result.size, QSize(150, 200));
        QImageReader reader(dir.filePath("out.jpg"));
        QCOMPARE(reader.read().size(), QSize(150, 200));
        QCOMPARE(contents(source), before);
    }

    void resizeReportsUnreadableFiles()
    {
        QTemporaryDir dir;
        write(dir.filePath("fake.png"), "not a png");
        const auto result = Resizer::resize({dir.filePath("fake.png"), dir.filePath("out.png"), 100});
        QVERIFY(!result.error.isEmpty());
        QVERIFY(!QFile::exists(dir.filePath("out.png")));
    }

    void destinationsNeverReplaceFiles()
    {
        QTemporaryDir dir;
        QDir(dir.path()).mkpath("a");
        QDir(dir.path()).mkpath("b");
        QDir(dir.path()).mkpath("out");
        const QString out = dir.filePath("out");
        write(out + "/taken.jpg", "x");

        QSet<QString> taken;
        QCOMPARE(Resizer::destinationFor(dir.filePath("a/photo.JPG"), out, 800, taken), out + "/photo.JPG");
        QCOMPARE(Resizer::destinationFor(dir.filePath("b/photo.JPG"), out, 800, taken), out + "/photo-800w.JPG");
        QCOMPARE(Resizer::destinationFor(dir.filePath("c/photo.JPG"), out, 800, taken), out + "/photo-800w-2.JPG");
        QCOMPARE(Resizer::destinationFor(dir.filePath("a/taken.jpg"), out, 800, taken), out + "/taken-800w.jpg");
        // Saving into the originals' own folder must not touch them.
        write(dir.filePath("a/self.png"), "x");
        QCOMPARE(Resizer::destinationFor(dir.filePath("a/self.png"), dir.filePath("a"), 640, taken),
                 dir.filePath("a/self-640w.png"));
        // Qt reads GIF but can't write it.
        QCOMPARE(Resizer::destinationFor(dir.filePath("a/anim.gif"), out, 800, taken), out + "/anim.png");
    }

    void addingSkipsWhatIsNotAnImage()
    {
        QTemporaryDir dir;
        image(dir.filePath("one.png"), {200, 100});
        image(dir.filePath("two.jpg"), {300, 300});
        write(dir.filePath("notes.txt"), "hello");

        Backend backend(new FakePicker, nullptr, m_settings.filePath("settings.ini"));
        backend.addPaths({dir.path(), dir.filePath("notes.txt"), QUrl::fromLocalFile(dir.filePath("one.png")).toString()});
        QCOMPARE(backend.count(), 2);
        QVERIFY(backend.error().contains("notes.txt"));
    }

    void widthAppliesToTheSelectionOrToAll()
    {
        QTemporaryDir dir;
        image(dir.filePath("a.png"), {200, 100});
        image(dir.filePath("b.png"), {400, 400});
        Backend backend(new FakePicker, nullptr, m_settings.filePath("settings.ini"));
        backend.addPaths({dir.filePath("a.png"), dir.filePath("b.png")});

        backend.setWidth(100);
        QCOMPARE(backend.commonWidth(), 100);
        QCOMPARE(backend.changedCount(), 2);
        QCOMPARE(backend.heightFor(100), -1);

        backend.select(1);
        backend.setWidth(50);
        QCOMPARE(backend.list().items().at(0).targetWidth(), 100);
        QCOMPARE(backend.list().items().at(1).targetWidth(), 50);
        QCOMPARE(backend.heightFor(50), 50);

        backend.resetSize();
        QCOMPARE(backend.list().items().at(1).targetWidth(), 400);
        QCOMPARE(backend.list().items().at(0).targetWidth(), 100);
    }

    void selectionTogglesAndExtends()
    {
        QTemporaryDir dir;
        for (const char *name : {"a.png", "b.png", "c.png", "d.png"})
            image(dir.filePath(name), {10, 10});
        Backend backend(new FakePicker, nullptr, m_settings.filePath("settings.ini"));
        backend.addPaths({dir.path()});

        backend.select(0);
        backend.select(2, false, true);
        QCOMPARE(backend.selectedCount(), 3);
        backend.select(1, true);
        QCOMPARE(backend.selectedCount(), 2);
        backend.selectAll();
        QCOMPARE(backend.selectedCount(), 4);
        backend.removeSelected();
        QCOMPARE(backend.count(), 0);
        backend.undo();
        QCOMPARE(backend.count(), 4);
    }

    void undoAndRedoWidths()
    {
        QTemporaryDir dir;
        image(dir.filePath("a.png"), {200, 100});
        Backend backend(new FakePicker, nullptr, m_settings.filePath("settings.ini"));
        backend.addPaths({dir.filePath("a.png")});
        backend.setWidth(100);
        backend.setWidth(50);

        backend.undo();
        QCOMPARE(backend.commonWidth(), 100);
        backend.undo();
        QCOMPARE(backend.commonWidth(), 200);
        backend.redo();
        QCOMPARE(backend.commonWidth(), 100);
        QVERIFY(backend.canRedo());
    }

    void saveWritesEveryImageAndClearsDirty()
    {
        QTemporaryDir dir;
        QDir(dir.path()).mkpath("out");
        image(dir.filePath("a.png"), {200, 100});
        image(dir.filePath("b.jpg"), {400, 400});
        auto *picker = new FakePicker;
        picker->folder = dir.filePath("out");
        Backend backend(picker, nullptr, dir.filePath("settings.ini"));
        backend.addPaths({dir.filePath("a.png"), dir.filePath("b.jpg")});
        backend.setWidth(100);
        QVERIFY(backend.dirty());

        QSignalSpy saved(&backend, &Backend::saved);
        backend.save();
        QCOMPARE(picker->suggested, dir.path());
        QVERIFY(saved.wait(5000));
        QCOMPARE(saved.first().at(1).toInt(), 2);
        QCOMPARE(QImageReader(dir.filePath("out/a.png")).size(), QSize(100, 50));
        QCOMPARE(QImageReader(dir.filePath("out/b.jpg")).size(), QSize(100, 100));
        QVERIFY(!backend.dirty());
        QVERIFY(backend.error().isEmpty());
    }

    void foldersAreRememberedFromWhereImagesCameFrom()
    {
        QTemporaryDir dir;
        QDir(dir.path()).mkpath("holiday");
        QDir(dir.path()).mkpath("work");
        QDir(dir.path()).mkpath("out");
        image(dir.filePath("holiday/a.png"), {20, 10});
        image(dir.filePath("work/b.png"), {20, 10});
        const QString settings = dir.filePath("settings.ini");

        {
            auto *picker = new FakePicker;
            picker->folder = dir.filePath("out");
            Backend backend(picker, nullptr, settings);
            backend.addPaths({dir.filePath("holiday/a.png")});
            backend.addPaths({dir.filePath("work/b.png")});
            // Save starts beside the images, even after saving somewhere else.
            QSignalSpy saved(&backend, &Backend::saved);
            backend.save();
            QCOMPARE(picker->suggested, dir.filePath("holiday"));
            QVERIFY(saved.wait(5000));
            backend.save();
            QCOMPARE(picker->suggested, dir.filePath("holiday"));
            QVERIFY(saved.wait(5000));
            backend.select(1);
            backend.save();
            QCOMPARE(picker->suggested, dir.filePath("work"));
            QVERIFY(saved.wait(5000));
        }

        // A new session starts adding where the last images came from.
        auto *picker = new FakePicker;
        Backend backend(picker, nullptr, settings);
        backend.addImages();
        QCOMPARE(picker->addStart, dir.filePath("work"));
        QCOMPARE(backend.saveFolder(), dir.filePath("work"));
    }

    void saveRefusesAFolderItCannotWrite()
    {
        QTemporaryDir dir;
        image(dir.filePath("a.png"), {20, 10});
        Backend backend(new FakePicker, nullptr, m_settings.filePath("settings.ini"));
        backend.addPaths({dir.filePath("a.png")});
        backend.saveTo(dir.filePath("missing"));
        QVERIFY(!backend.busy());
        QVERIFY(!backend.error().isEmpty());
    }

    void themeFallsBack()
    {
        QTemporaryDir dir;
        Theme theme(dir.path());
        QCOMPARE(theme.accent(), QString("#FFD60A"));
        QCOMPARE(theme.accentForeground(), QString("black"));
    }

    void themeReadsAccent()
    {
        QTemporaryDir dir;
        QDir(dir.path()).mkpath("theme");
        QFile colors(dir.path() + "/theme/colors.toml");
        QVERIFY(colors.open(QIODevice::WriteOnly));
        colors.write("accent = \"#112233\"\n");
        colors.close();

        Theme theme(dir.path());
        QCOMPARE(theme.accent(), QString("#112233"));
        QCOMPARE(theme.accentForeground(), QString("white"));
    }

    void themeReadsThePalette()
    {
        QTemporaryDir dir;
        QDir(dir.path()).mkpath("theme");
        write(dir.path() + "/theme/colors.toml",
              "mode = \"light\"\naccent = \"#1e66f5\"\nbackground = \"#eff1f5\"\n"
              "lighter_background = \"#dce0e8\"\nforeground = \"#4c4f69\"\nmuted = \"#acb0be\"\n"
              "red = \"#d20f39\"\nyellow = \"#df8e1d\"\n");

        Theme theme(dir.path());
        QVERIFY(!theme.dark());
        QCOMPARE(theme.background(), QString("#eff1f5"));
        QCOMPARE(theme.surface(), QString("#dce0e8"));
        QCOMPARE(theme.foreground(), QString("#4c4f69"));
        QCOMPARE(theme.muted(), QString("#acb0be"));
        QCOMPARE(theme.error(), QString("#d20f39"));
        // No orange in this theme, so warnings fall back to yellow.
        QCOMPARE(theme.warning(), QString("#df8e1d"));
    }

    void themeFallsBackToADarkPalette()
    {
        QTemporaryDir dir;
        Theme theme(dir.path());
        QVERIFY(theme.dark());
        QCOMPARE(theme.background(), QString("#0e0e10"));
    }

    void themeFollowsHyprlandRounding()
    {
        QTemporaryDir dir;
        const QString response = dir.filePath("response.json"), command = dir.filePath("hyprctl");
        auto write = [](const QString &path, const QByteArray &data) {
            QFile file(path);
            return file.open(QIODevice::WriteOnly) && file.write(data) == data.size();
        };
        QVERIFY(write(command, "#!/bin/sh\ncat '" + response.toUtf8() + "'\n"));
        QVERIFY(QFile::setPermissions(command, QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner));
        QVERIFY(write(response, "{\"int\":0}"));

        Theme theme(dir.path(), nullptr, command);
        QCOMPARE(theme.radius(), 0);
        QVERIFY(write(response, "{\"int\":6}"));
        QTRY_COMPARE(theme.radius(), 6);
    }

    void translatorFallsBackToEnglish()
    {
        QTemporaryDir dir;
        Translator translator(dir.path() + "/language", dir.path());
        QCOMPARE(translator.language(), QString("en"));
        QVERIFY(translator.translate("Main", "Save").isNull());
    }

    void translatorFollowsTheLanguageFile()
    {
        QTemporaryDir dir;
        write(dir.path() + "/language", "pt-BR\n");
        write(dir.path() + "/pt.json", R"({"Save": "Guardar", "Open": "Abrir"})");
        write(dir.path() + "/pt-BR.json", R"({"Save": "Salvar"})");

        Translator translator(dir.path() + "/language", dir.path());
        QCOMPARE(translator.language(), QString("pt-BR"));
        QCOMPARE(translator.translate("Main", "Save"), QString("Salvar"));
        QCOMPARE(translator.translate("Main", "Open"), QString("Abrir"));
        QVERIFY(translator.translate("Main", "Quit").isNull());
    }

    void translatorPicksPluralForms()
    {
        QTemporaryDir dir;
        write(dir.path() + "/language", "ru\n");
        write(dir.path() + "/en.json", R"({"%n clips": {"one": "%n clip", "other": "%n clips"}})");
        write(dir.path() + "/ru.json", R"({"%n clips": {"one": "%n клип", "few": "%n клипа", "many": "%n клипов", "other": "%n клипа"}})");

        Translator translator(dir.path() + "/language", dir.path());
        QCOMPARE(translator.translate("Main", "%n clips", nullptr, 21), QString("%n клип"));
        QCOMPARE(translator.translate("Main", "%n clips", nullptr, 22), QString("%n клипа"));
        QCOMPARE(translator.translate("Main", "%n clips", nullptr, 25), QString("%n клипов"));
        QCOMPARE(Translator::pluralCategory("ar", 11), QString("many"));
        QCOMPARE(Translator::pluralCategory("ja", 1), QString("other"));
    }

private:
    // Backends in these tests remember folders here, never in ~/.config.
    QTemporaryDir m_settings;

    static void image(const QString &path, const QSize &size,
                      QImageIOHandler::Transformations transformation = QImageIOHandler::TransformationNone)
    {
        QImage picture(size, QImage::Format_RGB32);
        picture.fill(Qt::darkCyan);
        QImageWriter writer(path);
        writer.setTransformation(transformation);
        QVERIFY2(writer.write(picture), qPrintable(writer.errorString()));
    }

    static QByteArray contents(const QString &path)
    {
        QFile file(path);
        return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
    }

    static void write(const QString &path, const QByteArray &contents)
    {
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write(contents);
    }
};

QTEST_MAIN(SizerTests)
#include "sizer_tests.moc"
